// SPDX-License-Identifier: GPL-3.0-or-later
// Fire service without staff: fire trucks leave without firefighters and put out
// fires with a virtual crew. Both patch points were verified on WRSR 1.1.1.9.
#pragma once
#include "labor_stub.h"

namespace industrial::fire {
// --- Dispatch (station selection for a fire, 0x253060, fire-station branch) ---
// Road fire trucks were accepted only if the station's people list (+0xBC0..+0xBC8)
// was not empty: `test rax,rax; jne accept`. The patch makes that jump unconditional
// (helicopters were already accepted without people).
constexpr std::uint32_t kDispatchSite=0x25373E;
constexpr std::uint8_t kDispatchOriginal[]={0x75,0x09};
constexpr std::uint8_t kDispatchPatched[]={0xEB,0x09};
constexpr std::uint32_t kDispatchContextAt=0x253724;
constexpr std::uint8_t kDispatchContext[]={
    0x45,0x85,0xC9,0x7E,0x17,                   // test r9d,r9d; jle 0x253740
    0x48,0x8B,0x87,0xC8,0x0B,0x00,0x00,         // mov rax,[rdi+0xbc8]
    0x48,0x2B,0x87,0xC0,0x0B,0x00,0x00,         // sub rax,[rdi+0xbc0]
    0x48,0xC1,0xF8,0x03,                        // sar rax,3
    0x48,0x85,0xC0,                             // test rax,rax
    0x75,0x09,                                  // jne 0x253749   <- patched
    0x45,0x85,0xDB};                            // test r11d,r11d

// --- Extinguishing (building fire update 0x13EC60) ---
// For each fire truck/ladder (skill 31/32) at a burning building the game multiplies
// the extinguishing rate by the crew count (vehicle +0x4C8..+0x4D0) and skips the
// truck when that count is 0. The hook replaces the crew count by the truck's own
// fire skill value (the capacity the station uses, skill entry +4) only when the
// truck carries nobody, skips the per-firefighter fatigue loop and continues at
// the fire-point reduction.
constexpr std::uint32_t kExtinguishSite=0x13F165;
constexpr std::uint8_t kExtinguishOriginal[]={0x66,0x0F,0x6E,0xCB,0x0F,0x5B,0xC9}; // movd xmm1,ebx; cvtdq2ps xmm1,xmm1
constexpr std::uint32_t kExtinguishContextAt=0x13F15A;
constexpr std::uint8_t kExtinguishContext[]={
    0x0F,0x28,0xF8,                                   // movaps xmm7,xmm0
    0xF3,0x0F,0x59,0x3D,0x13,0xAB,0x7C,0x00,          // mulss xmm7,[rip+...]
    0x66,0x0F,0x6E,0xCB,0x0F,0x5B,0xC9,               // stolen
    0xF3,0x0F,0x59,0xF9,                              // mulss xmm7,xmm1
    0x45,0x33,0xD2,                                   // xor r10d,r10d
    0x85,0xDB};                                       // test ebx,ebx
constexpr std::uint32_t kExtinguishNormal=0x13F16C;   // back after the stolen bytes
constexpr std::uint32_t kExtinguishVirtual=0x13F22E;  // `or r12d,-1`: fire-point reduction
constexpr std::uint8_t kVirtualContext[]={0x41,0x83,0xCC,0xFF};
constexpr std::size_t kCapacity=256;

struct Stub { std::uint8_t bytes[kCapacity]{}; std::size_t size=0; };

// Registers at the site: RSI burning building, R15 vehicle index in its +0xC70 list,
// EBX crew count, XMM7 rate, R12D trucks-fighting counter, RBP frame (+0x138 copy).
// Only RAX, XMM1 and flags are written on the virtual path; the game reloads RAX and
// sets XMM1/flags itself on the normal path.
inline bool BuildExtinguishStub(Stub& out,std::uintptr_t address,std::uintptr_t normal,
                                std::uintptr_t virtualPath,std::uintptr_t counter,int defaultCrew) {
    if (defaultCrew<1 || defaultCrew>20 || !counter) return false;
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
    float crew=static_cast<float>(defaultCrew);
    std::uint32_t crewBits; std::memcpy(&crewBits,&crew,4);

    emit({0x85,0xDB});                                  // test ebx,ebx
    auto toNormal=branch(0x85);                         // real crew: original code
    emit({0x48,0x8B,0x86,0x70,0x0C,0x00,0x00});         // mov rax,[rsi+0xc70]
    emit({0x4A,0x8B,0x04,0xF8});                        // mov rax,[rax+r15*8]   vehicle
    emit({0x48,0x8B,0x80,0x08,0x17,0x00,0x00});         // mov rax,[rax+0x1708]  vehicle type
    emit({0x48,0x8B,0x80,0xE0,0x85,0x00,0x00});         // mov rax,[rax+0x85e0]  skills
    emit({0x8B,0x40,0x04});                             // mov eax,[rax+4]       fire skill (float)
    emit({0x3D,0x00,0x00,0x80,0x3F});                   // cmp eax,1.0f
    auto low=branch(0x82);                              // jb  default (also +0)
    emit({0x3D,0x00,0x00,0xC8,0x42});                   // cmp eax,100.0f
    auto ok=branch(0x86);                               // jbe have (negative/NaN/inf fall through)
    auto useDefault=out.size;
    emit({0xB8}); emitBytes(&crewBits,4);               // mov eax,defaultCrew
    auto have=out.size;
    emit({0x66,0x0F,0x6E,0xC8});                        // movd xmm1,eax
    emit({0xF3,0x0F,0x59,0xF9});                        // mulss xmm7,xmm1   (as at 0x13F16C)
    emit({0x45,0x33,0xD2});                             // xor r10d,r10d     (as at 0x13F170)
    emit({0x41,0xFF,0xC4});                             // inc r12d          (as at 0x13F17B)
    emit({0x44,0x89,0xA5,0x38,0x01,0x00,0x00});         // mov [rbp+0x138],r12d
    emit({0x48,0xB8}); emitBytes(&counter,8);           // movabs rax,counter
    emit({0xF0,0x48,0xFF,0x00});                        // lock inc qword [rax]
    jumpTo(virtualPath);
    auto normalPath=out.size;
    emitBytes(kExtinguishOriginal,sizeof(kExtinguishOriginal));
    jumpTo(normal);
    if (overflow) return false;
    resolve(toNormal,normalPath); resolve(low,useDefault); resolve(ok,have);
    return !overflow && out.size<=kCapacity;
}
}
