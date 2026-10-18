// SPDX-License-Identifier: GPL-3.0-or-later
// Vehicle repair stations without staff: repairs progress with a virtual staff equal to
// the station's $WORKERS_NEEDED when nobody is there. Both patch points were verified
// on WRSR 1.1.1.9.
#pragma once
#include "labor_stub.h"

namespace industrial::repair {
constexpr std::uint8_t kRepairType=0x6C; // $TYPE_REPAIR_OFFICE (parser 0x10EE6B)

// --- Repair rate (0x1AD790, called only from the repair station tick 0x14C5B0 at 0x14C992) ---
// rate(game, station, first person, people per job): sums the productivity of up to ESI people
// of the station's people list (+0xBC0..+0xBC8) starting at EDI. With no person left from EDI
// (`cmp edi,eax; jge`) it returns 0, so a station without people never repairs. The hook takes
// that jump: with an empty list it sets XMM7 to the virtual people left for this job and
// continues at the game's building condition factor; otherwise it keeps the original paths.
constexpr std::uint32_t kRateSite=0x1AD7DF;
constexpr std::uint8_t kRateOriginal[]={0x0F,0x8D,0xD4,0x00,0x00,0x00}; // jge 0x1AD8B9
constexpr std::uint32_t kRateContextAt=0x1AD790;
constexpr std::uint8_t kRateContext[]={
    0x4C,0x8B,0xDC,0x53,0x55,0x56,                   // mov r11,rsp; push rbx; push rbp; push rsi
    0x48,0x83,0xEC,0x60,                             // sub rsp,0x60
    0x0F,0x29,0x7C,0x24,0x40,                        // movaps [rsp+0x40],xmm7
    0x41,0x8B,0xF1,                                  // mov esi,r9d            people per job
    0x45,0x0F,0x29,0x4B,0xA8,                        // movaps [r11-0x58],xmm9
    0x45,0x0F,0x57,0xC9,                             // xorps xmm9,xmm9
    0x41,0x0F,0x28,0xF9,                             // movaps xmm7,xmm9       rate = 0
    0x48,0x8B,0xDA,                                  // mov rbx,rdx            station
    0x48,0x8B,0xE9,                                  // mov rbp,rcx            game
    0x48,0x85,0xD2,0x0F,0x84,0x19,0x01,0x00,0x00,    // test rdx,rdx; je 0x1AD8D7
    0x48,0x8B,0x82,0xC8,0x0B,0x00,0x00,              // mov rax,[rdx+0xbc8]
    0x49,0x89,0x7B,0x08,                             // mov [r11+8],rdi
    0x33,0xFF,0x45,0x85,0xC0,0x41,0x0F,0x49,0xF8,    // edi = max(0, r8d)      first person
    0x48,0x2B,0x82,0xC0,0x0B,0x00,0x00,              // sub rax,[rdx+0xbc0]
    0x48,0xC1,0xF8,0x03,                             // sar rax,3              people present
    0x3B,0xF8,                                       // cmp edi,eax
    0x0F,0x8D,0xD4,0x00,0x00,0x00,                   // jge 0x1AD8B9   <- hook
    0x4D,0x89,0x73,0x10};                            // mov [r11+0x10],r14     loop setup
constexpr std::uint32_t kRateLoop=0x1AD7E5;   // people left from EDI: original loop
constexpr std::uint32_t kRateFactorContextAt=0x1AD895;
constexpr std::uint8_t kRateFactorContext[]={
    0x76,0x22,                                       // jbe 0x1AD8B9           (rate <= 0)
    0x83,0xBD,0xBC,0x05,0x00,0x00,0x00,0x7E,0x19,    // cmp [rbp+0x5bc],0; jle 0x1AD8B9
    0x48,0x8B,0xD3,0xE8,0xD8,0xC9,0xFF,0xFF,         // mov rdx,rbx; call 0x1AA280 (condition factor)
    0xF3,0x0F,0x10,0x2D,0xA8,0xC6,0x75,0x00,         // movss xmm5,[rip+...]
    0x0F,0x2F,0xE8,0x76,0x04,                        // comiss xmm5,xmm0; jbe
    0xF3,0x0F,0x59,0xF8,                             // mulss xmm7,xmm0
    0x48,0x8B,0xBC,0x24,0x80,0x00,0x00,0x00,         // mov rdi,[rsp+0x80]     epilogue
    0x0F,0x28,0xC7,                                  // movaps xmm0,xmm7       returns the rate
    0x0F,0x28,0x7C,0x24,0x40};                       // movaps xmm7,[rsp+0x40]
constexpr std::uint32_t kRateFactor=0x1AD897; // `cmp [rbp+0x5bc],0`: condition factor, then epilogue
constexpr std::uint32_t kRateDone=0x1AD8B9;   // epilogue: returns XMM7

// --- Tick (0x14C5B0) ---
// With nobody present the job loop ran only if the station flag +0x11C1 was set (set to 1 when
// a repair station is created, 0x2D7251; it also arms the idle timer of each job). The patch
// lets the job loop run with an empty people list, as it does whenever people are present.
constexpr std::uint32_t kTickSite=0x14C8A8;
constexpr std::uint8_t kTickOriginal[]={0x0F,0x85,0x8A,0x02,0x00,0x00}; // jne 0x14CB38
constexpr std::uint8_t kTickPatched[]={0x66,0x0F,0x1F,0x44,0x00,0x00};  // 6-byte nop
constexpr std::uint32_t kTickContextAt=0x14C88A;
constexpr std::uint8_t kTickContext[]={
    0x48,0x8B,0x83,0xC8,0x0B,0x00,0x00,              // mov rax,[rbx+0xbc8]
    0x48,0x2B,0x83,0xC0,0x0B,0x00,0x00,              // sub rax,[rbx+0xbc0]
    0x48,0xC1,0xF8,0x03,0x48,0x85,0xC0,              // sar rax,3; test rax,rax
    0x75,0x0D,                                       // jne 0x14C8AE           people present
    0x80,0xBB,0xC1,0x11,0x00,0x00,0x01,              // cmp byte [rbx+0x11c1],1
    0x0F,0x85,0x8A,0x02,0x00,0x00,                   // jne 0x14CB38   <- patched
    0x48,0x8B,0x8B,0xF8,0x0F,0x00,0x00};             // mov rcx,[rbx+0xff8]    job loop
constexpr std::size_t kCapacity=256;

struct Stub { std::uint8_t bytes[kCapacity]{}; std::size_t size=0; };

// Registers at the site: flags from `cmp edi,eax`, EDI first person (>= 0), EAX people present,
// ESI people per job, RBX station, RBP game, XMM7 = 0. Only RAX, RCX, flags and XMM7 are
// written: RAX and RCX are dead at the factor and the epilogue, and the factor sets the flags.
inline bool BuildRateStub(Stub& out,std::uintptr_t address,std::uintptr_t loop,std::uintptr_t factor,
                          std::uintptr_t done,std::uintptr_t counter) {
    if (!counter) return false;
    out={};
    bool overflow=false;
    auto emit=[&](std::initializer_list<std::uint8_t> v) {
        for (auto b:v) { if (out.size<kCapacity) out.bytes[out.size]=b; else overflow=true; ++out.size; }
    };
    auto emitBytes=[&](const void* p,std::size_t n) {
        auto b=static_cast<const std::uint8_t*>(p);
        for (std::size_t i=0;i<n;++i) emit({b[i]});
    };
    auto branch=[&](std::uint8_t cc) { emit({0x0F,cc,0,0,0,0}); return out.size-4; };
    auto resolve=[&](std::size_t operand,std::size_t target) {
        if (operand+4>kCapacity || target>kCapacity) { overflow=true; return; }
        auto rel=static_cast<std::int32_t>(target)-static_cast<std::int32_t>(operand+4);
        std::memcpy(out.bytes+operand,&rel,4);
    };
    auto jumpTo=[&](std::uintptr_t target) {
        std::uint8_t jump[5];
        if (!RelativeJump(jump,address+out.size,target)) { overflow=true; return; }
        emitBytes(jump,5);
    };

    auto toLoop=branch(0x8C);                           // jl: people left from EDI, original loop
    emit({0x85,0xC0});                                  // test eax,eax
    auto busy=branch(0x85);                             // people present but all serving earlier jobs
    emit({0x48,0x8B,0x83,0x18,0x03,0x00,0x00});         // mov rax,[rbx+0x318]   descriptor
    emit({0x48,0x85,0xC0});                             // test rax,rax
    auto none=branch(0x84);
    emit({0x83,0xB8,0x60,0x03,0x00,0x00,kRepairType});  // cmp dword [rax+0x360],0x6C
    auto other=branch(0x85);
    emit({0x8B,0x80,0x0C,0x08,0x00,0x00});              // mov eax,[rax+0x80c]   $WORKERS_NEEDED
    emit({0x83,0xF8,0x01});                             // cmp eax,1
    auto few=branch(0x8C);                              // jl
    emit({0x3D,0xE8,0x03,0x00,0x00});                   // cmp eax,1000
    auto many=branch(0x8F);                             // jg
    emit({0x2B,0xC7});                                  // sub eax,edi           virtual people left
    auto spent=branch(0x8E);                            // jle: earlier jobs took them all
    emit({0xB9,0x01,0x00,0x00,0x00});                   // mov ecx,1
    emit({0x3B,0xF1});                                  // cmp esi,ecx
    emit({0x0F,0x4D,0xCE});                             // cmovge ecx,esi        the game takes at least one
    emit({0x3B,0xC8});                                  // cmp ecx,eax
    emit({0x0F,0x4F,0xC8});                             // cmovg ecx,eax         at most the people left
    emit({0xF3,0x0F,0x2A,0xF9});                        // cvtsi2ss xmm7,ecx     productivity 1.0 each
    emit({0x48,0xB8}); emitBytes(&counter,8);           // movabs rax,counter
    emit({0xF0,0x48,0xFF,0x00});                        // lock inc qword [rax]
    jumpTo(factor);
    auto original=out.size;
    jumpTo(done);
    auto people=out.size;
    jumpTo(loop);
    if (overflow) return false;
    resolve(toLoop,people);
    for (auto b:{busy,none,other,few,many,spent}) resolve(b,original);
    return !overflow && out.size<=kCapacity;
}
}
