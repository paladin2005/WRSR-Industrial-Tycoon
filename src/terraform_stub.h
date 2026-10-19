// SPDX-License-Identifier: GPL-3.0-or-later
// Terraforming with virtual workers: the realistic terrain tools work the ground by hand when no
// excavator or bulldozer is free and no unemployed citizens live nearby.
// The patch point was verified on WRSR 1.1.1.9.
#pragma once
#include "labor_stub.h"

namespace industrial::terraform {
// Terrain work planner 0x3A6FD0 (RCX game, RDX selected terrain points, R8D skill: 3 excavator,
// 0x17 bulldozer, R9D vehicle limit; callers 0x2CB5C3, 0x3AA7C2, 0x3AA7E7, 0x516F54). It copies the
// points (12 bytes each) to [game+0xD2D8..0xD2E0] and selects free vehicles with the skill near
// them into [game+0xD2F0..0xD2F8], adding their skill power to [game+0xD374]. When it selected
// none, it counts unemployed citizens of nearby residential buildings and labour halls into
// [game+0xD338] (at most one more than the points) and adds count/3 to the power. With no vehicle
// and no citizen the callers show "You need a free excavator/bulldozer ... (or unemployed
// citizens ...)" and leave the ground as it is. The tools then change the terrain gradually each
// frame from that power and count, as the game does for citizens; the paid modes skip the planner.
// The patch replaces `mov eax,0x55555556`, the start of the count/3 step that the citizen search
// and its empty-building-list branch (0x3A7621) both reach.
constexpr std::uint32_t kSite=0x3A77E3;
constexpr std::uint8_t kOriginal[]={0xB8,0x56,0x55,0x55,0x55}; // mov eax,0x55555556
constexpr std::uint32_t kContextAt=0x3A77B3;
constexpr std::uint8_t kContext[]={
    0x48,0x63,0x86,0x38,0xD3,0x00,0x00,               // movsxd rax,[rsi+0xd338]  citizens counted
    0x48,0x3B,0xC2,                                   // cmp rax,rdx              points
    0x77,0x1F,                                        // ja 0x3A77DE              enough citizens
    0xFF,0xC7,                                        // inc edi                  next building
    0x48,0x8B,0x86,0x10,0x1B,0x01,0x00,               // mov rax,[rsi+0x11b10]
    0x48,0x2B,0x86,0x08,0x1B,0x01,0x00,               // sub rax,[rsi+0x11b08]
    0x48,0xC1,0xF8,0x03,                              // sar rax,3
    0x8B,0xCF,                                        // mov ecx,edi
    0x48,0x3B,0xC8,                                   // cmp rcx,rax
    0x0F,0x82,0x62,0xFE,0xFF,0xFF,                    // jb 0x3A7640              building loop
    0x4C,0x8B,0x74,0x24,0x38,                         // mov r14,[rsp+0x38]
    0xB8,0x56,0x55,0x55,0x55,                         // mov eax,0x55555556       <- patched
    0xF7,0xAE,0x38,0xD3,0x00,0x00,                    // imul dword [rsi+0xd338]
    0x8B,0xC2,0xC1,0xE8,0x1F,0x03,0xD0,               // mov eax,edx; shr eax,31; add edx,eax  (count/3)
    0x66,0x0F,0x6E,0xC2,0x0F,0x5B,0xC0,               // movd xmm0,edx; cvtdq2ps xmm0,xmm0
    0xF3,0x0F,0x58,0x86,0x74,0xD3,0x00,0x00,          // addss xmm0,[rsi+0xd374]  power
    0xF3,0x0F,0x11,0x86,0x74,0xD3,0x00,0x00};         // movss [rsi+0xd374],xmm0
constexpr std::uint32_t kContinuation=0x3A77E8;   // imul dword [rsi+0xd338]
// The first version uses 113 bytes; keep room for later checks.
constexpr std::size_t kCapacity=256;

struct Stub { std::uint8_t bytes[kCapacity]{}; std::size_t size=0; };

// Registers at the site: RSI game. RAX, RCX, RDX and flags are dead (the replayed instruction and
// the imul after it overwrite RAX, RDX and flags; RCX is written before any read on every path).
// With no vehicle selected, the citizen count is raised to min(workers, points); real citizens
// that already reach it are kept. The game then turns the count into power (count/3) as usual.
inline bool BuildStub(Stub& out,std::uintptr_t address,std::uintptr_t continuation,std::uintptr_t counter,int workers) {
    if (workers<1 || workers>1000 || !counter) return false;
    out={};
    bool overflow=false;
    auto emit=[&](std::initializer_list<std::uint8_t> v) {
        for (auto b:v) { if (out.size<kCapacity) out.bytes[out.size]=b; else overflow=true; ++out.size; }
    };
    auto emitBytes=[&](const void* p,std::size_t n) {
        auto b=static_cast<const std::uint8_t*>(p);
        for (std::size_t i=0;i<n;++i) emit({b[i]});
    };
    constexpr std::size_t kMaxFixups=8;
    std::size_t toOriginal[kMaxFixups]{}; std::size_t originalCount=0;
    auto branch=[&](std::uint8_t cc) { emit({0x0F,cc,0,0,0,0}); return out.size-4; };
    auto originalIf=[&](std::uint8_t cc) {
        auto at=branch(cc);
        if (originalCount<kMaxFixups) toOriginal[originalCount++]=at; else overflow=true;
    };
    auto resolve=[&](std::size_t operand,std::size_t target) {
        if (operand+4>kCapacity || target>kCapacity) { overflow=true; return; }
        auto rel=static_cast<std::int32_t>(target)-static_cast<std::int32_t>(operand+4);
        std::memcpy(out.bytes+operand,&rel,4);
    };

    emit({0x48,0x8B,0x86,0xF8,0xD2,0x00,0x00});        // mov rax,[rsi+0xd2f8]  selected vehicles end
    emit({0x48,0x2B,0x86,0xF0,0xD2,0x00,0x00});        // sub rax,[rsi+0xd2f0]
    emit({0x48,0x83,0xF8,0x08}); originalIf(0x83);      // jae: a vehicle works the ground
    emit({0x48,0x8B,0x86,0xE0,0xD2,0x00,0x00});        // mov rax,[rsi+0xd2e0]  points end
    emit({0x48,0x2B,0x86,0xD8,0xD2,0x00,0x00});        // sub rax,[rsi+0xd2d8]
    originalIf(0x8E);                                   // jle: no points
    emit({0x31,0xD2});                                  // xor edx,edx
    emit({0xB9,0x0C,0x00,0x00,0x00});                   // mov ecx,12
    emit({0x48,0xF7,0xF1});                             // div rcx              points
    emit({0x48,0x3D}); emitBytes(&workers,4);           // cmp rax,workers
    auto fewPoints=branch(0x86);                        // jbe: one worker per point at most
    emit({0xB8}); emitBytes(&workers,4);                // mov eax,workers
    auto capped=out.size;
    emit({0x39,0x86,0x38,0xD3,0x00,0x00}); originalIf(0x8D); // jge: real citizens are enough
    emit({0x89,0x86,0x38,0xD3,0x00,0x00});              // mov [rsi+0xd338],eax
    emit({0x48,0xB8}); emitBytes(&counter,8);           // movabs rax,counter
    emit({0xF0,0x48,0xFF,0x00});                        // lock inc qword [rax]
    auto original=out.size;
    emitBytes(kOriginal,sizeof(kOriginal));
    std::uint8_t jump[5];
    if (!RelativeJump(jump,address+out.size,continuation)) return false;
    emitBytes(jump,5);
    if (overflow) return false;
    resolve(fewPoints,capped);
    for (std::size_t i=0;i<originalCount;++i) resolve(toOriginal[i],original);
    return !overflow && out.size<=kCapacity;
}
}
