// SPDX-License-Identifier: GPL-3.0-or-later
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <vector>
#include "../src/repair_stub.h"

namespace rp=industrial::repair;
extern "C" void RunRepairStub(void*,void*,void*,std::uint64_t*);

static float Float(std::uint64_t b) { float f; std::uint32_t u=static_cast<std::uint32_t>(b); std::memcpy(&f,&u,4); return f; }

enum Path { kNone=0, kLoop=1, kFactor=2, kDone=3 };

int main() {
    auto block=static_cast<BYTE*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
    if (!block) return 2;
    const BYTE loopRet[]={0x41,0xBB,0x01,0x00,0x00,0x00,0xC3};   // mov r11d,1; ret
    const BYTE factorRet[]={0x41,0xBB,0x02,0x00,0x00,0x00,0xC3}; // mov r11d,2; ret
    const BYTE doneRet[]={0x41,0xBB,0x03,0x00,0x00,0x00,0xC3};   // mov r11d,3; ret
    std::memcpy(block+0x800,loopRet,sizeof(loopRet));
    std::memcpy(block+0x900,factorRet,sizeof(factorRet));
    std::memcpy(block+0xA00,doneRet,sizeof(doneRet));
    alignas(8) static volatile LONG64 ticks=0;
    rp::Stub stub;
    if (!rp::BuildRateStub(stub,reinterpret_cast<std::uintptr_t>(block),reinterpret_cast<std::uintptr_t>(block+0x800),
            reinterpret_cast<std::uintptr_t>(block+0x900),reinterpret_cast<std::uintptr_t>(block+0xA00),
            reinterpret_cast<std::uintptr_t>(&ticks))) return 3;
    std::memcpy(block,stub.bytes,stub.size); FlushInstructionCache(GetCurrentProcess(),block,4096);
    std::printf("repair rate stub size=%zu/%zu\n",stub.size,sizeof(stub.bytes));

    // start: first person for this job (EDI, >= 0); people: people present; perJob: ESI;
    // type/workers: descriptor; expect: path and virtual rate.
    auto run=[&](const char* name,int start,int people,int perJob,int type,int workers,bool nullDescriptor,
                 Path expect,float rate) {
        std::vector<BYTE> station(0x1000),descriptor(0x900),game(0x1000);
        if (!nullDescriptor) { void* d=descriptor.data(); std::memcpy(station.data()+0x318,&d,8); }
        std::memcpy(descriptor.data()+0x360,&type,4); std::memcpy(descriptor.data()+0x80C,&workers,4);
        auto beforeStation=station; auto beforeDescriptor=descriptor;
        std::uint64_t o[16]{};
        o[0]=static_cast<std::uint32_t>(start); o[1]=static_cast<std::uint64_t>(people); o[2]=static_cast<std::uint32_t>(perJob);
        LONG64 oldTicks=ticks;
        RunRepairStub(block,station.data(),game.data(),o);
        bool preserved=o[5]==reinterpret_cast<std::uintptr_t>(station.data()) && o[6]==reinterpret_cast<std::uintptr_t>(game.data()) &&
            o[7]==static_cast<std::uint32_t>(perJob) && o[8]==static_cast<std::uint32_t>(start) &&
            o[9]==0x1111222233334444ULL && o[10]==0x5555666677778888ULL && o[11]==0x1234AAAABBBBCCCCULL &&
            o[12]==0x0BADC0DE0BADC0DEULL && o[13]==0x0BADC0DE0BADC0DEULL &&
            station==beforeStation && descriptor==beforeDescriptor;
        bool ok=preserved && o[3]==static_cast<std::uint64_t>(expect) &&
            (expect==kFactor? Float(o[4])==rate && ticks-oldTicks==1 : o[4]==0 && ticks-oldTicks==0);
        const char* paths[]={"NONE","loop","factor","done"};
        std::printf("%s %s path=%s rate=%.1f preserved=%s\n",ok?"PASS":"FAIL",name,
                    o[3]<4?paths[o[3]]:"?",Float(o[4]),preserved?"yes":"NO");
        return ok;
    };
    constexpr int R=rp::kRepairType;
    bool ok=true;
    // People present: the original paths are kept.
    ok &= run("people-left-original-loop",0,3,6,R,30,false,kLoop,0);
    ok &= run("people-left-second-job-original-loop",2,3,6,R,30,false,kLoop,0);
    ok &= run("people-all-serving-earlier-jobs",3,3,6,R,30,false,kDone,0);
    ok &= run("people-beyond-start",8,3,6,R,30,false,kDone,0);
    // Nobody present: the virtual staff is the station's $WORKERS_NEEDED, shared by the jobs in order.
    ok &= run("empty-first-job",0,0,6,R,30,false,kFactor,6.0f);
    ok &= run("empty-second-job",6,0,6,R,30,false,kFactor,6.0f);
    ok &= run("empty-capped-by-people-left",24,0,10,R,30,false,kFactor,6.0f);
    ok &= run("empty-job-larger-than-station",0,0,50,R,30,false,kFactor,30.0f);
    ok &= run("empty-all-virtual-staff-used",30,0,4,R,30,false,kDone,0);
    ok &= run("empty-beyond-virtual-staff",40,0,4,R,30,false,kDone,0);
    ok &= run("empty-zero-per-job-takes-one",0,0,0,R,30,false,kFactor,1.0f);
    ok &= run("empty-negative-per-job-takes-one",0,0,-3,R,30,false,kFactor,1.0f);
    ok &= run("small-station-15",0,0,5,R,15,false,kFactor,5.0f);
    ok &= run("large-station-80",70,0,20,R,80,false,kFactor,10.0f);
    ok &= run("dlc3-horse-station-10",0,0,10,R,10,false,kFactor,10.0f);
    ok &= run("workers-1-boundary",0,0,4,R,1,false,kFactor,1.0f);
    ok &= run("workers-1000-boundary",0,0,2000,R,1000,false,kFactor,1000.0f);
    // Not admitted: other types, missing descriptor, unusable staff values.
    ok &= run("other-type-scrapyard",0,0,6,0x6D,30,false,kDone,0);
    ok &= run("other-type-factory",0,0,6,6,30,false,kDone,0);
    ok &= run("null-descriptor",0,0,6,R,30,true,kDone,0);
    ok &= run("workers-zero",0,0,6,R,0,false,kDone,0);
    ok &= run("workers-negative",0,0,6,R,-5,false,kDone,0);
    ok &= run("workers-1001",0,0,6,R,1001,false,kDone,0);
    rp::Stub bad;
    bool invalid=!rp::BuildRateStub(bad,0x1000,0x2000,0x3000,0x4000,0);
    std::printf("%s invalid-repair-config\n",invalid?"PASS":"FAIL");
    bool rate=rp::kRateContextAt+0x4F==rp::kRateSite &&
              !std::memcmp(rp::kRateContext+0x4F,rp::kRateOriginal,sizeof(rp::kRateOriginal)) &&
              rp::kRateSite+sizeof(rp::kRateOriginal)==rp::kRateLoop &&
              rp::kRateFactorContextAt+2==rp::kRateFactor && rp::kRateFactorContextAt+0x24==rp::kRateDone &&
              // jge rel32 0xD4 from the end of the site lands on the epilogue.
              rp::kRateLoop+0xD4==rp::kRateDone;
    bool tick=rp::kTickContextAt+0x1E==rp::kTickSite && sizeof(rp::kTickOriginal)==sizeof(rp::kTickPatched) &&
              !std::memcmp(rp::kTickContext+0x1E,rp::kTickOriginal,sizeof(rp::kTickOriginal));
    std::printf("%s repair-patch-constants-consistent\n",rate&&tick?"PASS":"FAIL");
    VirtualFree(block,0,MEM_RELEASE);
    return ok && invalid && rate && tick?0:1;
}
