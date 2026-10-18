// SPDX-License-Identifier: GPL-3.0-or-later
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <vector>
#include "../src/work_stub.h"

namespace ws=industrial::work;
extern "C" void RunWorkStub(void*,void*,void*,std::uint64_t*);
extern "C" void RunWorkStub2(void*,void*,void*,std::uint64_t*);

static std::uint32_t Bits(float f) { std::uint32_t b; std::memcpy(&b,&f,4); return b; }
static float Float(std::uint64_t b) { float f; std::uint32_t u=static_cast<std::uint32_t>(b); std::memcpy(&f,&u,4); return f; }

struct Case {
    const char* name;
    float work=0.0f;            // summed by the real workers
    int kind=-1, workers=60;    // kind -1: the site's own building type
    bool descriptorNull=false;
    int compare=3;              // game +0x5BC, compared with zero by the container facility's relocated instruction
    bool expectVirtual=false; float expectedWork=0;
};

int main() {
    auto block=static_cast<BYTE*>(VirtualAlloc(nullptr,8192,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
    if (!block) return 2;
    const BYTE continuation[]={0x9C,0x41,0x5B,0xC3}; // pushfq; pop r11; ret
    std::memcpy(block+0x1800,continuation,sizeof(continuation));
    alignas(8) static volatile LONG64 ticks[ws::kSiteCount]{};
    bool ok=true;
    for (int site=0;site<ws::kSiteCount;++site) {
        const auto& s=ws::kSites[site];
        BYTE* code=block+0x400*site;
        ws::Stub stub;
        if (!ws::BuildWorkStub(stub,s,reinterpret_cast<std::uintptr_t>(code),reinterpret_cast<std::uintptr_t>(block+0x1800),
                reinterpret_cast<std::uintptr_t>(&ticks[site]))) return 3;
        std::memcpy(code,stub.bytes,stub.size);
        std::printf("%s work stub size=%zu/%zu\n",s.label,stub.size,sizeof(stub.bytes));
    }
    FlushInstructionCache(GetCurrentProcess(),block,8192);

    auto run=[&](int site,Case c) {
        const auto& s=ws::kSites[site];
        std::vector<BYTE> building(0x1000),descriptor(0x900),game(0x1000);
        void* d=c.descriptorNull?nullptr:descriptor.data(); std::memcpy(building.data()+0x318,&d,8);
        int kind=c.kind<0?s.buildingType:c.kind;
        std::memcpy(descriptor.data()+0x360,&kind,4); std::memcpy(descriptor.data()+0x80C,&c.workers,4);
        std::memcpy(game.data()+0x5BC,&c.compare,4);
        std::uint64_t fieldPointer=0x0BADF00D12345678ULL; std::memcpy(building.data()+0xE68,&fieldPointer,8);
        auto before=building; auto beforeDescriptor=descriptor; auto beforeGame=game;
        std::uint64_t o[16]{}; o[0]=Bits(c.work);
        LONG64 oldTicks=ticks[site];
        RunWorkStub(block+0x400*site,building.data(),game.data(),o);
        bool container=s.buildingReg==3;
        std::uint64_t work=container?o[1]:o[2], other=container?o[2]:o[1];
        bool relocated;
        if (container) { // cmp [r14+0x5bc],r12d decides the flags
            const std::uint64_t ZF=1u<<6, SF=1u<<7, OF=1u<<11;
            relocated=((o[3]&ZF)!=0)==(c.compare==0) && (((o[3]&SF)!=0)!=((o[3]&OF)!=0))==(c.compare<0) &&
                      o[8]==0x1111222233334444ULL;
        } else {         // mov rcx,[rsi+0xe68]
            relocated=o[8]==fieldPointer;
        }
        bool preserved=o[4]==reinterpret_cast<std::uintptr_t>(building.data()) && o[5]==reinterpret_cast<std::uintptr_t>(building.data()) &&
            o[6]==reinterpret_cast<std::uintptr_t>(game.data()) && o[7]==0 && o[9]==0x5555666677778888ULL && o[10]==0 &&
            other==Bits(c.work) && building==before && descriptor==beforeDescriptor && game==beforeGame;
        bool ok=relocated && preserved && (c.expectVirtual ? Float(work)==c.expectedWork && ticks[site]-oldTicks==1
                                                           : work==Bits(c.work) && ticks[site]-oldTicks==0);
        std::printf("%s %s %s path=%s work=%.2f relocated=%s preserved=%s\n",ok?"PASS":"FAIL",s.label,c.name,
                    ticks[site]-oldTicks?"virtual":"original",Float(work),relocated?"ok":"WRONG",preserved?"yes":"NO");
        return ok;
    };
    auto virt=[&](int site,Case c,float expected) { c.expectVirtual=true; c.expectedWork=expected; return run(site,c); };
    constexpr int C=0, F=1;
    ok &= virt(C,{"no-staff-gets-60",0.0f},60.0f);
    ok &= virt(C,{"negative-zero-work-gets-60",-0.0f},60.0f);
    ok &= virt(C,{"no-staff-flags-equal",0.0f,-1,60,false,0},60.0f);
    ok &= virt(C,{"no-staff-flags-less",0.0f,-1,60,false,-2},60.0f);
    ok &= virt(C,{"staff-1000-boundary",0.0f,-1,1000},1000.0f);
    ok &= run(C,{"real-workers-keep-original",3.5f});
    ok &= run(C,{"tiny-real-work-keeps-original",1e-30f});
    ok &= run(C,{"descriptor-null",0.0f,-1,60,true});
    ok &= run(C,{"other-building-type",0.0f,0x2B});
    ok &= run(C,{"no-staff-defined",0.0f,-1,0});
    ok &= run(C,{"negative-staff",0.0f,-1,-5});
    ok &= run(C,{"implausible-staff",0.0f,-1,1001});
    ok &= run(C,{"real-workers-flags-equal",2.0f,-1,60,false,0});
    ok &= virt(F,{"small-field-gets-75",0.0f,-1,75},75.0f);
    ok &= virt(F,{"medium-field-gets-150",0.0f,-1,150},150.0f);
    ok &= virt(F,{"big-field-gets-250",-0.0f,-1,250},250.0f);
    ok &= run(F,{"field-real-workers",12.25f,-1,75});
    ok &= run(F,{"field-descriptor-null",0.0f,-1,75,true});
    ok &= run(F,{"farm-type-not-a-field",0.0f,9,75});
    ok &= run(F,{"field-no-staff-defined",0.0f,-1,0});

    // Control tower (site 2) and water well (site 3), through the second harness.
    struct Case2 { const char* name; float work; int kind; int workers, professors; bool descriptorNull; int compare;
                   bool expectVirtual; float expected; };
    auto run2=[&](int site,Case2 c) {
        const auto& s=ws::kSites[site];
        std::vector<BYTE> building(0x1000),descriptor(0x900),game(0x1000);
        void* d=c.descriptorNull?nullptr:descriptor.data(); std::memcpy(building.data()+0x318,&d,8);
        int kind=c.kind<0?s.buildingType:c.kind;
        std::memcpy(descriptor.data()+0x360,&kind,4);
        std::memcpy(descriptor.data()+0x80C,&c.workers,4); std::memcpy(descriptor.data()+0x810,&c.professors,4);
        std::memcpy(game.data()+0x5BC,&c.compare,4);
        auto before=building; auto beforeDescriptor=descriptor; auto beforeGame=game;
        std::uint64_t o[16]{}; o[0]=Bits(c.work);
        LONG64 oldTicks=ticks[site];
        RunWorkStub2(block+0x400*site,building.data(),game.data(),o);
        bool tower=s.workXmm==8;
        std::uint64_t work=tower?o[1]:o[2], other=tower?o[2]:o[1];
        const std::uint64_t ZF=1u<<6, SF=1u<<7, OF=1u<<11;
        bool flags=((o[3]&ZF)!=0)==(c.compare==0) && (((o[3]&SF)!=0)!=((o[3]&OF)!=0))==(c.compare<0);
        auto gamePtr=reinterpret_cast<std::uintptr_t>(game.data());
        float staff=static_cast<float>(tower?c.professors:c.workers);
        bool extras;
        if (c.expectVirtual) extras=tower? o[6]==static_cast<std::uint32_t>(c.professors) && o[11]==0xDEADBEEF
                                         : o[6]==gamePtr && o[11]==Bits(c.expected);
        else                 extras=o[6]==gamePtr && o[11]==0xDEADBEEF;
        bool preserved=o[4]==reinterpret_cast<std::uintptr_t>(building.data()) && o[5]==reinterpret_cast<std::uintptr_t>(building.data()) &&
            o[7]==gamePtr && o[8]==0 && o[9]==0x1111222233334444ULL && o[10]==0x5555666677778888ULL && o[12]==0 &&
            other==Bits(c.work) && building==before && descriptor==beforeDescriptor && game==beforeGame;
        bool ok=flags && extras && preserved && (c.expectVirtual ? Float(work)==c.expected && c.expected==staff && ticks[site]-oldTicks==1
                                                                 : work==Bits(c.work) && ticks[site]-oldTicks==0);
        std::printf("%s %s %s path=%s work=%.2f flags=%s extras=%s preserved=%s\n",ok?"PASS":"FAIL",s.label,c.name,
                    ticks[site]-oldTicks?"virtual":"original",Float(work),flags?"ok":"WRONG",extras?"ok":"WRONG",preserved?"yes":"NO");
        return ok;
    };
    constexpr int T=2, W=3;
    ok &= run2(T,{"tower-no-staff-gets-20-engineers",0.0f,-1,0,20,false,3,true,20.0f});
    ok &= run2(T,{"dlc3-tower-gets-15",-0.0f,-1,0,15,false,3,true,15.0f});
    ok &= run2(T,{"tower-flags-equal",0.0f,-1,0,20,false,0,true,20.0f});
    ok &= run2(T,{"tower-flags-less",0.0f,-1,0,20,false,-1,true,20.0f});
    ok &= run2(T,{"tower-real-engineers",4.5f,-1,0,20,false,3,false,0});
    ok &= run2(T,{"tower-uses-engineers-not-workers",0.0f,-1,20,0,false,3,false,0});
    ok &= run2(T,{"tower-descriptor-null",0.0f,-1,0,20,true,3,false,0});
    ok &= run2(T,{"tower-other-type",0.0f,0x2F,0,20,false,3,false,0});
    ok &= run2(T,{"tower-implausible-staff",0.0f,-1,0,1001,false,3,false,0});
    ok &= run2(W,{"well-no-staff-gets-7",0.0f,-1,7,0,false,3,true,7.0f});
    ok &= run2(W,{"small-well-gets-10",-0.0f,-1,10,0,false,3,true,10.0f});
    ok &= run2(W,{"well-flags-equal",0.0f,-1,8,0,false,0,true,8.0f});
    ok &= run2(W,{"well-real-workers",2.5f,-1,7,0,false,3,false,0});
    ok &= run2(W,{"surface-water-no-staff-defined",0.0f,-1,0,0,false,3,false,0});
    ok &= run2(W,{"well-descriptor-null",0.0f,-1,7,0,true,3,false,0});
    ok &= run2(W,{"well-other-type",0.0f,0x5A,7,0,false,3,false,0});
    ws::Stub bad;
    ws::Site wrong=ws::kSites[0]; wrong.buildingReg=1;
    ws::Site wrongStaff=ws::kSites[0]; wrongStaff.staffOffset=0x814;
    ws::Site wrongCount=ws::kSites[2]; wrongCount.countReg=4;
    ws::Site wrongSpill=ws::kSites[3]; wrongSpill.spillRsp=0x80;
    bool invalid=!ws::BuildWorkStub(bad,ws::kSites[0],0x1000,0x2000,0) && !ws::BuildWorkStub(bad,wrong,0x1000,0x2000,0x3000) &&
                 !ws::BuildWorkStub(bad,wrongStaff,0x1000,0x2000,0x3000) && !ws::BuildWorkStub(bad,wrongCount,0x1000,0x2000,0x3000) &&
                 !ws::BuildWorkStub(bad,wrongSpill,0x1000,0x2000,0x3000);
    std::printf("%s invalid-work-config\n",invalid?"PASS":"FAIL");
    bool constants=true;
    for (const auto& s:ws::kSites)
        constants &= s.site>=s.contextAt && s.site+sizeof(s.original)<=s.contextAt+s.contextSize &&
                     !std::memcmp(s.originalInContext(),s.original,sizeof(s.original));
    std::printf("%s work-patch-constants-consistent\n",constants?"PASS":"FAIL");
    VirtualFree(block,0,MEM_RELEASE);
    return ok && invalid && constants?0:1;
}
