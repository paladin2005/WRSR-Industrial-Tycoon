// SPDX-License-Identifier: GPL-3.0-or-later
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <vector>
#include "../src/terraform_stub.h"

namespace tf=industrial::terraform;
extern "C" void RunTerraformStub(void*,void*,std::uint64_t*);

struct Case {
    const char* name;
    long long points=0;          // terrain points selected (12 bytes each); negative: corrupt list
    int vehicles=0;              // vehicles selected by the planner
    int citizens=0;              // unemployed citizens already counted ([game+0xD338])
    int expectCount=0;           // [game+0xD338] after the stub
    int expectCounted=0;
};

static std::uint64_t Markers[]={0x1111222233334444ULL,0x5555666677778888ULL,0x0FEDCBA987654321ULL,0x1234AAAABBBBCCCCULL,
    0x0ABCDEF012345678ULL,0x2222333344445555ULL,0x7777666655554444ULL,0x3333444455556666ULL,0x6666555544443333ULL};

int main() {
    auto block=static_cast<BYTE*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
    if (!block) return 2;
    block[0x800]=0xC3; // continuation: return to the harness
    alignas(8) static volatile LONG64 ticks=0;
    auto build=[&](int workers) {
        tf::Stub stub;
        if (!tf::BuildStub(stub,reinterpret_cast<std::uintptr_t>(block),reinterpret_cast<std::uintptr_t>(block+0x800),
                reinterpret_cast<std::uintptr_t>(&ticks),workers)) return false;
        std::memcpy(block,stub.bytes,stub.size); FlushInstructionCache(GetCurrentProcess(),block,4096);
        std::printf("terraform stub size=%zu/%zu (workers %d)\n",stub.size,sizeof(stub.bytes),workers);
        return true;
    };
    std::vector<BYTE> points(12*200000);
    std::vector<void*> fleet(8);
    auto run=[&](Case c) {
        std::vector<BYTE> game(0xE000);
        auto ptr=[&](size_t at,const void* p) { std::memcpy(game.data()+at,&p,8); };
        BYTE* first=points.data()+12*100000;
        ptr(0xD2D8,first); ptr(0xD2E0,first+12*c.points);
        ptr(0xD2F0,fleet.data()); ptr(0xD2F8,fleet.data()+c.vehicles);
        std::memcpy(game.data()+0xD338,&c.citizens,4);
        float power=7.0f; std::memcpy(game.data()+0xD374,&power,4);
        auto before=game;
        std::uint64_t o[16]{};
        LONG64 old=ticks;
        RunTerraformStub(block,game.data(),o);
        int count; std::memcpy(&count,game.data()+0xD338,4);
        std::memcpy(before.data()+0xD338,&c.expectCount,4);
        bool preserved=o[1]==reinterpret_cast<std::uintptr_t>(game.data()) && o[2]==Markers[0] && o[3]==Markers[1] &&
            o[4]==Markers[2] && o[5]==Markers[3] && o[6]==Markers[4] && o[7]==Markers[5] && o[8]==Markers[6] &&
            o[9]==Markers[7] && o[10]==Markers[8] && o[11]==0 && o[12]==0x4444333322221111ULL && o[13]==0x5A5A5A5AA5A5A5A5ULL &&
            game==before;
        bool ok=preserved && static_cast<std::uint32_t>(o[0])==0x55555556u && count==c.expectCount && ticks-old==c.expectCounted;
        std::printf("%s %s citizens=%d counted=%lld preserved=%s\n",ok?"PASS":"FAIL",c.name,count,
                    static_cast<long long>(ticks-old),preserved?"yes":"NO");
        return ok;
    };
    auto add=[&](Case c,int expect) { c.expectCount=expect; c.expectCounted=1; return run(c); };
    auto keep=[&](Case c) { c.expectCount=c.citizens; c.expectCounted=0; return run(c); };
    bool ok=build(100);
    // No vehicle and no citizen: a virtual crew of min(workers, points), the game turns it into power.
    ok &= add({"no-vehicle-no-citizen-large-area",500},100);
    ok &= add({"small-area",20},20);
    ok &= add({"one-point",1},1);
    ok &= add({"points-equal-workers",100},100);
    ok &= add({"points-above-workers",101},100);
    ok &= add({"huge-area",100000},100);
    // A few real citizens are topped up to the virtual crew; enough real citizens are kept.
    ok &= add({"few-citizens-topped-up",500,0,5},100);
    ok &= add({"citizens-below-small-area",20,0,3},20);
    ok &= keep({"citizens-equal-crew",500,0,100});
    ok &= keep({"citizens-fill-area",100,0,101});
    // Vehicles selected: the planner never reaches the citizen step; checked anyway.
    ok &= keep({"one-vehicle",500,1});
    ok &= keep({"three-vehicles",500,3});
    ok &= keep({"vehicle-and-citizens",500,2,4});
    // No points or a corrupt list: left as it is.
    ok &= keep({"no-points",0});
    ok &= keep({"corrupt-points",-3});
    ok &= build(1);
    ok &= add({"workers-1",500},1);
    ok &= build(1000);
    ok &= add({"workers-1000-area-500",500},500);
    ok &= add({"workers-1000-area-5000",5000},1000);
    tf::Stub bad;
    bool invalid=!tf::BuildStub(bad,0x1000,0x2000,0x3000,0) && !tf::BuildStub(bad,0x1000,0x2000,0x3000,1001) &&
        !tf::BuildStub(bad,0x1000,0x2000,0,100);
    std::printf("%s invalid-terraform-config\n",invalid?"PASS":"FAIL");
    std::size_t at=tf::kSite-tf::kContextAt;
    bool constants=at+sizeof(tf::kOriginal)<=sizeof(tf::kContext) &&
        !std::memcmp(tf::kContext+at,tf::kOriginal,sizeof(tf::kOriginal)) &&
        tf::kSite+sizeof(tf::kOriginal)==tf::kContinuation;
    std::printf("%s terraform-patch-constants-consistent\n",constants?"PASS":"FAIL");
    VirtualFree(block,0,MEM_RELEASE);
    return ok && invalid && constants?0:1;
}
