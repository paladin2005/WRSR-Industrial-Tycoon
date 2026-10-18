// SPDX-License-Identifier: GPL-3.0-or-later
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <vector>
#include <limits>
#include "../src/fire_stub.h"

namespace fs=industrial::fire;
extern "C" void RunFireStub(void*,void*,void*,std::uint64_t*);

static std::uint32_t Bits(float f) { std::uint32_t b; std::memcpy(&b,&f,4); return b; }
static float Float(std::uint64_t b) { float f; std::uint32_t u=static_cast<std::uint32_t>(b); std::memcpy(&f,&u,4); return f; }

int main() {
    auto block=static_cast<BYTE*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
    if (!block) return 2;
    const BYTE normalRet[]={0x41,0xBB,0x01,0x00,0x00,0x00,0xC3};  // mov r11d,1; ret
    const BYTE virtualRet[]={0x41,0xBB,0x02,0x00,0x00,0x00,0xC3}; // mov r11d,2; ret
    std::memcpy(block+0x800,normalRet,sizeof(normalRet));
    std::memcpy(block+0x900,virtualRet,sizeof(virtualRet));
    alignas(8) static volatile LONG64 ticks=0;
    fs::Stub stub;
    if (!fs::BuildExtinguishStub(stub,reinterpret_cast<std::uintptr_t>(block),reinterpret_cast<std::uintptr_t>(block+0x800),
            reinterpret_cast<std::uintptr_t>(block+0x900),reinterpret_cast<std::uintptr_t>(&ticks),4)) return 3;
    std::memcpy(block,stub.bytes,stub.size); FlushInstructionCache(GetCurrentProcess(),block,4096);
    std::printf("fire extinguish stub size=%zu/%zu\n",stub.size,sizeof(stub.bytes));

    // crew: real crew count; skill: fire skill value of the selected truck; index: truck in the list
    auto run=[&](const char* name,int crew,float skill,int index,bool expectVirtual,float expectedCrew) {
        std::vector<BYTE> building(0x1000),frame(0x400);
        std::vector<BYTE> types[3],skills[3],vehicles[3];
        void* list[3];
        for (int i=0;i<3;++i) {
            types[i].assign(0x9000,0); skills[i].assign(0x40,0); vehicles[i].assign(0x2000,0);
            int skillType=31; float value=i==index?skill:77.0f; // other trucks must not be read
            std::memcpy(skills[i].data(),&skillType,4); std::memcpy(skills[i].data()+4,&value,4);
            void* s=skills[i].data(); std::memcpy(types[i].data()+0x85E0,&s,8);
            void* t=types[i].data(); std::memcpy(vehicles[i].data()+0x1708,&t,8);
            list[i]=vehicles[i].data();
        }
        void* l=list; std::memcpy(building.data()+0xC70,&l,8);
        BYTE* rbp=frame.data()+0x100; int before=11; std::memcpy(rbp+0x138,&before,4);
        auto beforeBuilding=building;
        std::uint64_t o[16]{}; o[0]=static_cast<std::uint32_t>(crew); o[1]=index; o[2]=before; o[3]=Bits(2.5f);
        LONG64 oldTicks=ticks;
        RunFireStub(block,building.data(),rbp,o);
        int after; std::memcpy(&after,rbp+0x138,4);
        bool common=o[72/8]==static_cast<std::uint32_t>(crew) && o[80/8]==reinterpret_cast<std::uintptr_t>(building.data()) &&
            o[88/8]==static_cast<std::uint64_t>(index) && o[96/8]==reinterpret_cast<std::uintptr_t>(rbp) &&
            o[104/8]==0x1111222233334444ULL && o[112/8]==0x5555666677778888ULL && o[120/8]==0x1234AAAABBBBCCCCULL &&
            building==beforeBuilding;
        bool ok;
        if (expectVirtual)
            ok=common && o[4]==2 && Float(o[5])==expectedCrew && Float(o[6])==2.5f*expectedCrew &&
               o[7]==0 && o[8]==static_cast<std::uint64_t>(before+1) && after==before+1 && ticks-oldTicks==1;
        else
            ok=common && o[4]==1 && Float(o[5])==static_cast<float>(crew) && Float(o[6])==2.5f &&
               o[7]==~0ULL && o[8]==static_cast<std::uint64_t>(before) && after==before && ticks-oldTicks==0;
        std::printf("%s %s path=%s crew=%.2f rate=%.2f counter=%d/%d preserved=%s\n",ok?"PASS":"FAIL",name,
                    o[4]==2?"virtual":o[4]==1?"normal":"NONE",Float(o[5]),Float(o[6]),after,static_cast<int>(o[8]),common?"yes":"NO");
        return ok;
    };
    bool ok=true;
    ok &= run("real-crew-3-original-path",3,9.0f,0,false,0);
    ok &= run("real-crew-1-original-path",1,9.0f,1,false,0);
    ok &= run("no-crew-uses-truck-skill-9",0,9.0f,0,true,9.0f);
    ok &= run("no-crew-third-truck-skill-6",0,6.0f,2,true,6.0f);
    ok &= run("no-crew-skill-1-boundary",0,1.0f,0,true,1.0f);
    ok &= run("no-crew-skill-100-boundary",0,100.0f,0,true,100.0f);
    ok &= run("no-crew-skill-below-1-default",0,0.5f,0,true,4.0f);
    ok &= run("no-crew-skill-zero-default",0,0.0f,0,true,4.0f);
    ok &= run("no-crew-skill-negative-default",0,-3.0f,0,true,4.0f);
    ok &= run("no-crew-skill-huge-default",0,150.0f,0,true,4.0f);
    ok &= run("no-crew-skill-nan-default",0,std::numeric_limits<float>::quiet_NaN(),0,true,4.0f);
    ok &= run("no-crew-skill-inf-default",0,std::numeric_limits<float>::infinity(),0,true,4.0f);
    fs::Stub bad;
    bool invalid=!fs::BuildExtinguishStub(bad,0x1000,0x2000,0x3000,0x4000,0) &&
                 !fs::BuildExtinguishStub(bad,0x1000,0x2000,0x3000,0x4000,21) &&
                 !fs::BuildExtinguishStub(bad,0x1000,0x2000,0x3000,0,4);
    std::printf("%s invalid-fire-config\n",invalid?"PASS":"FAIL");
    bool dispatch=sizeof(fs::kDispatchContext)==31 && fs::kDispatchContext[26]==fs::kDispatchOriginal[0] &&
                  fs::kDispatchContext[27]==fs::kDispatchOriginal[1] && fs::kDispatchContextAt+26==fs::kDispatchSite;
    bool extinguish=fs::kExtinguishContextAt+11==fs::kExtinguishSite &&
                    !std::memcmp(fs::kExtinguishContext+11,fs::kExtinguishOriginal,sizeof(fs::kExtinguishOriginal)) &&
                    fs::kExtinguishSite+sizeof(fs::kExtinguishOriginal)==fs::kExtinguishNormal;
    std::printf("%s fire-patch-constants-consistent\n",dispatch&&extinguish?"PASS":"FAIL");
    VirtualFree(block,0,MEM_RELEASE);
    return ok && invalid && dispatch && extinguish?0:1;
}
