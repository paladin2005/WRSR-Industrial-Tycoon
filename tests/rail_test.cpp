// SPDX-License-Identifier: GPL-3.0-or-later
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <vector>
#include <limits>
#include "../src/rail_stub.h"

namespace rs=industrial::rail;
extern "C" void RunTrackStub(void*,void*,void*,std::uint64_t*);

static std::uint32_t Bits(float f) { std::uint32_t b; std::memcpy(&b,&f,4); return b; }
static float Float(std::uint64_t b) { float f; std::uint32_t u=static_cast<std::uint32_t>(b); std::memcpy(&f,&u,4); return f; }

struct Case {
    const char* name;
    int crew=0;                 // real crew members in the vehicle
    int home=0x1C;              // home building type; -1: no home building
    int staff=60, officeCapacity=100;
    int skill=22; float capacity=70.0f;
    bool skillsNull=false, descriptorNull=false;
    bool expectVirtual=false; float expectedCrew=0;
};

int main() {
    auto block=static_cast<BYTE*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
    if (!block) return 2;
    block[0x800]=0xC3; // continuation: back to the harness
    alignas(8) static volatile LONG64 ticks=0;
    rs::Stub stub;
    if (!rs::BuildTrackStub(stub,reinterpret_cast<std::uintptr_t>(block),reinterpret_cast<std::uintptr_t>(block+0x800),
            reinterpret_cast<std::uintptr_t>(&ticks),20)) return 3;
    std::memcpy(block,stub.bytes,stub.size); FlushInstructionCache(GetCurrentProcess(),block,4096);
    std::printf("rail track stub size=%zu/%zu\n",stub.size,sizeof(stub.bytes));

    auto run=[&](Case c) {
        std::vector<BYTE> vehicle(0x2000),type(0x9000),skills(0x40),office(0x1000),descriptor(0x900),site(0x1000),people(0x100);
        auto put=[&](std::vector<BYTE>& v,size_t at,const void* p,size_t n) { std::memcpy(v.data()+at,p,n); };
        auto ptr=[&](std::vector<BYTE>& v,size_t at,void* p) { put(v,at,&p,8); };
        BYTE* crewBegin=people.data(); BYTE* crewEnd=people.data()+8*c.crew;
        ptr(vehicle,0x4C8,crewBegin); ptr(vehicle,0x4D0,crewEnd);
        ptr(vehicle,0x4F8,c.home<0?nullptr:office.data());
        ptr(vehicle,0x1708,type.data());
        ptr(type,0x85E0,c.skillsNull?nullptr:skills.data());
        float power=85.0f,factor=1.0f;
        put(skills,0,&c.skill,4); put(skills,4,&power,4); put(skills,8,&c.capacity,4); put(skills,0xC,&factor,4);
        ptr(office,0x318,c.descriptorNull?nullptr:descriptor.data());
        put(office,0xDEC,&c.officeCapacity,4);
        put(descriptor,0x360,&c.home,4); put(descriptor,0x80C,&c.staff,4);
        auto before=vehicle; auto beforeType=type; auto beforeSkills=skills; auto beforeOffice=office; auto beforeDescriptor=descriptor;
        std::uint64_t o[16]{}; o[0]=Bits(0.25f);
        LONG64 oldTicks=ticks;
        RunTrackStub(block,vehicle.data(),site.data(),o);
        bool preserved=o[2]==reinterpret_cast<std::uintptr_t>(type.data()) && // stolen mov rax,[rsi+0x1708]
            o[3]==0x1111222233334444ULL && o[4]==0x5555666677778888ULL &&
            o[5]==reinterpret_cast<std::uintptr_t>(vehicle.data()) && o[6]==reinterpret_cast<std::uintptr_t>(site.data()) &&
            o[7]==0x0FEDCBA987654321ULL && o[8]==0x1234AAAABBBBCCCCULL && o[9]==0x0ABCDEF012345678ULL && o[10]==0 &&
            vehicle==before && type==beforeType && skills==beforeSkills && office==beforeOffice && descriptor==beforeDescriptor;
        bool ok=preserved && (c.expectVirtual ? Float(o[1])==c.expectedCrew && ticks-oldTicks==1
                                              : Float(o[1])==0.25f && ticks-oldTicks==0);
        std::printf("%s %s path=%s crew=%.2f preserved=%s\n",ok?"PASS":"FAIL",c.name,
                    ticks-oldTicks?"virtual":"original",Float(o[1]),preserved?"yes":"NO");
        return ok;
    };
    auto virt=[&](Case c,float expected) { c.expectVirtual=true; c.expectedCrew=expected; return run(c); };
    bool ok=true;
    // Office staff 60 shared by 100 capacity: each vehicle gets 60% of its capacity, as when full.
    ok &= virt({"rail-crane-70-shared",0,0x1C,60,100,22,70.0f},42.0f);
    ok &= virt({"track-layer-30-shared",0,0x1C,60,100,26,30.0f},18.0f);
    ok &= virt({"dlc3-rail-crane2-15-fits",0,0x1C,20,15,28,15.0f},15.0f);
    ok &= virt({"big-office-enough-staff",0,0x1C,170,130,22,70.0f},70.0f);
    ok &= virt({"office-capacity-unknown",0,0x1C,60,0,22,70.0f},70.0f);
    ok &= virt({"office-without-staff-definition",0,0x1C,0,100,22,70.0f},70.0f);
    ok &= virt({"office-negative-capacity",0,0x1C,60,-5,26,30.0f},30.0f);
    ok &= virt({"share-rounds-up-to-one",0,0x1C,1,100,22,10.0f},1.0f);
    ok &= virt({"capacity-zero-uses-fallback",0,0x1C,60,100,22,0.0f},12.0f);
    ok &= virt({"capacity-nan-uses-fallback",0,0x1C,60,100,22,std::numeric_limits<float>::quiet_NaN()},12.0f);
    ok &= virt({"capacity-huge-uses-fallback",0,0x1C,60,30,22,5000.0f},20.0f);
    ok &= virt({"capacity-1000-boundary",0,0x1C,60,30,22,1000.0f},1000.0f);
    ok &= virt({"huge-staff-not-scaled",0,0x1C,20000,30000,22,70.0f},70.0f);
    ok &= run({"real-crew-keeps-original",3,0x1C,60,100,22,70.0f});
    ok &= run({"no-home-building",0,-1});
    ok &= run({"home-is-road-office",0,0x0C});
    ok &= run({"home-descriptor-null",0,0x1C,60,100,22,70.0f,false,true});
    ok &= run({"skills-null",0,0x1C,60,100,22,70.0f,true});
    ok &= run({"automatic-machine-skill-27",0,0x1C,60,100,27,260.0f});
    ok &= run({"fire-truck-skill-31",0,0x1C,60,100,31,4.0f});
    ok &= run({"road-crane-skill-21",0,0x1C,60,100,21,40.0f});
    rs::Stub bad;
    bool invalid=!rs::BuildTrackStub(bad,0x1000,0x2000,0x3000,0) && !rs::BuildTrackStub(bad,0x1000,0x2000,0x3000,201) &&
                 !rs::BuildTrackStub(bad,0x1000,0x2000,0,20);
    std::printf("%s invalid-rail-config\n",invalid?"PASS":"FAIL");
    bool constants=rs::kTrackContextAt+7==rs::kTrackSite &&
        !std::memcmp(rs::kTrackContext+7,rs::kTrackOriginal,sizeof(rs::kTrackOriginal)) &&
        rs::kTrackSite+sizeof(rs::kTrackOriginal)==rs::kTrackContinuation &&
        rs::kRefuelContextAt+21==rs::kRefuelSite && rs::kRefuelContext[21]==rs::kRefuelOriginal[0] &&
        rs::kRefuelContext[22]==rs::kRefuelOriginal[1];
    std::printf("%s rail-patch-constants-consistent\n",constants?"PASS":"FAIL");
    VirtualFree(block,0,MEM_RELEASE);
    return ok && invalid && constants?0:1;
}
