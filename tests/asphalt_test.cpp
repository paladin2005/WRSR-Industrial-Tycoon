// SPDX-License-Identifier: GPL-3.0-or-later
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <vector>
#include "../src/asphalt_stub.h"

namespace as=industrial::asphalt;
extern "C" void RunAsphaltStub(void*,void*,void*,std::uint64_t*);

struct Case {
    const char* name;
    int realWorkers=0;          // al from 0x1521F0: an office can bring real workers
    int roadType=0;             // -1: no road object
    int limit=50;               // site worker limit +0x62C
    int rub=0, usd=0;           // funded with money (+0x608, +0x609)
    int expectPath=2;           // 1 plan the delivery, 2 skip it
    int expectCount=0;
};

int main() {
    auto block=static_cast<BYTE*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
    if (!block) return 2;
    const BYTE admitRet[]={0x41,0xBB,0x01,0x00,0x00,0x00,0xC3}, rejectRet[]={0x41,0xBB,0x02,0x00,0x00,0x00,0xC3};
    std::memcpy(block+0x800,admitRet,sizeof(admitRet));
    std::memcpy(block+0x900,rejectRet,sizeof(rejectRet));
    alignas(8) static volatile LONG64 plans=0;
    as::Stub stub;
    if (!as::BuildStub(stub,reinterpret_cast<std::uintptr_t>(block),reinterpret_cast<std::uintptr_t>(block+0x800),
            reinterpret_cast<std::uintptr_t>(block+0x900),reinterpret_cast<std::uintptr_t>(&plans))) return 3;
    std::memcpy(block,stub.bytes,stub.size); FlushInstructionCache(GetCurrentProcess(),block,4096);
    std::printf("asphalt delivery stub size=%zu/%zu\n",stub.size,sizeof(stub.bytes));

    auto run=[&](Case c) {
        std::vector<BYTE> site(0x1000),road(0x400),game(0x400);
        if (c.roadType>=0) {
            void* r=road.data(); std::memcpy(site.data()+0x5D0,&r,8);
            std::memcpy(road.data()+0x12C,&c.roadType,4);
        }
        std::memcpy(site.data()+0x62C,&c.limit,4);
        site[0x608]=static_cast<BYTE>(c.rub); site[0x609]=static_cast<BYTE>(c.usd);
        auto beforeSite=site; auto beforeRoad=road; auto beforeGame=game;
        std::uint64_t o[16]{}; o[0]=static_cast<std::uint64_t>(c.realWorkers);
        LONG64 old=plans;
        RunAsphaltStub(block,site.data(),game.data(),o);
        bool preserved=o[2]==reinterpret_cast<std::uintptr_t>(site.data()) && o[3]==reinterpret_cast<std::uintptr_t>(game.data()) &&
            o[4]==0x1111222233334444ULL && o[5]==0x5555666677778888ULL && o[6]==0x0FEDCBA987654321ULL &&
            o[7]==0x1234AAAABBBBCCCCULL && o[8]==0x0ABCDEF012345678ULL && o[9]==0x7777666655554444ULL && o[10]==0 &&
            site==beforeSite && road==beforeRoad && game==beforeGame;
        bool ok=preserved && static_cast<int>(o[1])==c.expectPath && plans-old==c.expectCount;
        std::printf("%s %s path=%s counted=%lld preserved=%s\n",ok?"PASS":"FAIL",c.name,
                    o[1]==1?"plan":o[1]==2?"skip":"NONE",static_cast<long long>(plans-old),preserved?"yes":"NO");
        return ok;
    };
    auto plan=[&](Case c) { c.expectPath=1; c.expectCount=1; return run(c); };
    bool ok=true;
    // An office that can bring real workers keeps the original decision, without counting.
    ok &= run({"real-workers-plan-as-before",1,0,50,0,0,1,0});
    ok &= run({"real-workers-even-without-road",1,-1,0,0,0,1,0});
    // No paver and no real workers: planned when the construction stub will add virtual workers.
    ok &= plan({"road",0,0});
    ok &= plan({"road-surface-square",0,3});
    ok &= plan({"footpath",0,4});
    ok &= plan({"airport-runway",0,13});
    ok &= plan({"limit-1",0,0,1});
    ok &= plan({"limit-5000",0,0,5000});
    ok &= run({"rail-not-admitted",0,1});
    ok &= run({"tram-not-admitted",0,2});
    ok &= run({"heating-not-admitted",0,14});
    ok &= run({"road-type-256",0,256});
    ok &= run({"no-road-object",0,-1});
    ok &= run({"limit-0",0,0,0});
    ok &= run({"limit-negative",0,0,-1});
    ok &= run({"limit-5001",0,0,5001});
    ok &= run({"funded-rub",0,0,50,1});
    ok &= run({"funded-usd",0,0,50,0,1});
    bool every=true;
    for (auto& r:industrial::construction::kRoadTypes) {
        char name[48]; std::snprintf(name,sizeof(name),"admitted-road-type-%d",r.type);
        Case c{name,0,r.type}; every&=plan(c);
    }
    ok &= every;
    as::Stub bad;
    bool invalid=!as::BuildStub(bad,0x1000,0x2000,0x3000,0);
    std::printf("%s invalid-asphalt-config\n",invalid?"PASS":"FAIL");
    bool constants=as::kContextAt+sizeof(as::kContext)-sizeof(as::kOriginal)==as::kSite &&
        !std::memcmp(as::kContext+sizeof(as::kContext)-sizeof(as::kOriginal),as::kOriginal,sizeof(as::kOriginal)) &&
        as::kSite+sizeof(as::kOriginal)==as::kAdmit &&
        as::kSite+sizeof(as::kOriginal)+static_cast<std::int32_t>(as::kOriginal[2]|as::kOriginal[3]<<8)==as::kReject;
    std::printf("%s asphalt-patch-constants-consistent\n",constants?"PASS":"FAIL");
    VirtualFree(block,0,MEM_RELEASE);
    return ok && invalid && constants?0:1;
}
