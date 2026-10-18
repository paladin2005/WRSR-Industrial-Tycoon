// SPDX-License-Identifier: GPL-3.0-or-later
// Rail construction without staff: the rail construction office's track vehicles work
// at rail sites with a virtual crew and refuel at the office with nobody there.
// Both patch points were verified on WRSR 1.1.1.9.
#pragma once
#include "labor_stub.h"

namespace industrial::rail {
// --- Track crew (rail branch of the construction routine 0x159070) ---
// Rail segments (road type 1) are diverted at 0x1594AD: they are worked by the first vehicle
// in the site's +0xC70 list (RSI). 0x66BD60 refills that vehicle's crew (+0x4C8..+0x4D0) from
// the people present at its rail construction office (home +0x4F8, type 0x1C, people list
// +0xBC0), shared by vehicle capacity. The crew's summed productivity (XMM9) is then scaled by
// the vehicle factor at 0x15965E; with nobody at the office it stays 0 and the site waits.
// Skill 27 vehicles are automatic machines (0x66BD30) and never reach this point.
constexpr std::uint32_t kTrackSite=0x159657;
constexpr std::uint8_t kTrackOriginal[]={0x48,0x8B,0x86,0x08,0x17,0x00,0x00}; // mov rax,[rsi+0x1708]
constexpr std::uint32_t kTrackContextAt=0x159650;
constexpr std::uint8_t kTrackContext[]={
    0x41,0xB5,0x01,                                   // mov r13b,1   (end of the crew loop)
    0x8B,0x5C,0x24,0x40,                              // mov ebx,[rsp+0x40]
    0x48,0x8B,0x86,0x08,0x17,0x00,0x00,               // stolen: mov rax,[rsi+0x1708]
    0x48,0x8B,0x80,0xE0,0x85,0x00,0x00,               // mov rax,[rax+0x85e0]
    0xF3,0x44,0x0F,0x59,0x48,0x0C};                   // mulss xmm9,[rax+0xc]
constexpr std::uint32_t kTrackContinuation=0x15965E;

// --- Refueling at the office (rail office tick 0x1403F0) ---
// Parked office vehicles are refueled (0x142430) only while the office has more people present
// than the vehicle's index, unless the vehicle is automatic (skill 27): `ja refuel` at 0x142345.
// The patch makes that jump unconditional.
constexpr std::uint32_t kRefuelSite=0x142345;
constexpr std::uint8_t kRefuelOriginal[]={0x77,0x2A};
constexpr std::uint8_t kRefuelPatched[]={0xEB,0x2A};
constexpr std::uint32_t kRefuelContextAt=0x142330;
constexpr std::uint8_t kRefuelContext[]={
    0x49,0x8B,0x87,0xC8,0x0B,0x00,0x00,               // mov rax,[r15+0xbc8]
    0x49,0x2B,0x87,0xC0,0x0B,0x00,0x00,               // sub rax,[r15+0xbc0]
    0x48,0xC1,0xF8,0x03,                              // sar rax,3
    0x48,0x3B,0xC1,                                   // cmp rax,rcx  (people vs vehicle index)
    0x77,0x2A,                                        // ja refuel    <- patched
    0x48,0x8B,0x8B,0x08,0x17,0x00,0x00};              // mov rcx,[rbx+0x1708]

constexpr std::uint8_t kRailOfficeType=0x1C;
constexpr std::uint8_t kSkillRailCrane=22, kSkillTrackLayer=26, kSkillRailCrane2=28;
// The first version uses 254 bytes; keep room for later checks.
constexpr std::size_t kCapacity=512;

struct Stub { std::uint8_t bytes[kCapacity]{}; std::size_t size=0; };

// Registers at the site: RSI vehicle, RDI site, XMM9 summed crew productivity. RAX and flags
// are dead (the stolen instruction reloads RAX; the game sets flags at 0x159671). The virtual
// path also writes the low lane of XMM9 and saves/restores RCX and RDX on the stack.
// Virtual crew: the vehicle's capacity (skill +8, the value the game shares people by), scaled
// by office staff / office vehicle capacity (+0xDEC) when the office could not fill them all.
inline bool BuildTrackStub(Stub& out,std::uintptr_t address,std::uintptr_t continuation,
                           std::uintptr_t counter,int defaultCrew) {
    if (defaultCrew<1 || defaultCrew>200 || !counter) return false;
    out={};
    bool overflow=false;
    auto emit=[&](std::initializer_list<std::uint8_t> v) {
        for (auto b:v) { if (out.size<kCapacity) out.bytes[out.size]=b; else overflow=true; ++out.size; }
    };
    auto emitBytes=[&](const void* p,std::size_t n) {
        auto b=static_cast<const std::uint8_t*>(p);
        for (std::size_t i=0;i<n;++i) emit({b[i]});
    };
    constexpr std::size_t kMaxFixups=16;
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

    emit({0x48,0x8B,0x86,0xD0,0x04,0x00,0x00});        // mov rax,[rsi+0x4d0]
    emit({0x48,0x2B,0x86,0xC8,0x04,0x00,0x00});        // sub rax,[rsi+0x4c8]
    originalIf(0x85);                                   // real crew: original code
    emit({0x48,0x8B,0x86,0xF8,0x04,0x00,0x00});        // mov rax,[rsi+0x4f8]  home building
    emit({0x48,0x85,0xC0}); originalIf(0x84);
    emit({0x48,0x8B,0x80,0x18,0x03,0x00,0x00});        // mov rax,[rax+0x318]  descriptor
    emit({0x48,0x85,0xC0}); originalIf(0x84);
    emit({0x83,0xB8,0x60,0x03,0x00,0x00,kRailOfficeType}); originalIf(0x85);
    emit({0x48,0x8B,0x86,0x08,0x17,0x00,0x00});        // mov rax,[rsi+0x1708] vehicle type
    emit({0x48,0x8B,0x80,0xE0,0x85,0x00,0x00});        // mov rax,[rax+0x85e0] skills
    emit({0x48,0x85,0xC0}); originalIf(0x84);
    emit({0x83,0x38,kSkillRailCrane}); auto crane=branch(0x84);
    emit({0x83,0x38,kSkillTrackLayer}); auto layer=branch(0x84);
    emit({0x83,0x38,kSkillRailCrane2}); originalIf(0x85);
    auto crew=out.size;
    emit({0x51,0x52});                                  // push rcx; push rdx
    emit({0xF3,0x0F,0x2C,0x40,0x08});                   // cvttss2si eax,[rax+8]  capacity
    emit({0x83,0xF8,0x01}); auto low=branch(0x8C);      // <1 (also NaN): fallback
    emit({0x3D,0xE8,0x03,0x00,0x00}); auto capOk=branch(0x8E); // <=1000
    auto fallback=out.size;
    emit({0xB8}); emitBytes(&defaultCrew,4);            // mov eax,defaultCrew
    auto haveCap=out.size;
    emit({0x48,0x8B,0x8E,0xF8,0x04,0x00,0x00});        // mov rcx,[rsi+0x4f8]  office
    emit({0x48,0x8B,0x91,0x18,0x03,0x00,0x00});        // mov rdx,[rcx+0x318]
    emit({0x8B,0x92,0x0C,0x08,0x00,0x00});              // mov edx,[rdx+0x80c]  office staff
    emit({0x8B,0x89,0xEC,0x0D,0x00,0x00});              // mov ecx,[rcx+0xdec]  office vehicle capacity
    emit({0x85,0xD2}); auto noStaff=branch(0x8E);       // staff<=0: keep capacity
    emit({0x81,0xFA,0x10,0x27,0x00,0x00}); auto hugeStaff=branch(0x87); // staff>10000
    emit({0x39,0xD1}); auto enough=branch(0x8E);        // capacity<=staff: everyone fits
    emit({0x0F,0xAF,0xC2});                             // imul eax,edx
    emit({0x99});                                       // cdq
    emit({0xF7,0xF9});                                  // idiv ecx
    emit({0x83,0xF8,0x01}); auto atLeastOne=branch(0x8D);
    emit({0xB8,0x01,0x00,0x00,0x00});                   // mov eax,1
    auto scaled=out.size;
    emit({0xF3,0x44,0x0F,0x2A,0xC8});                   // cvtsi2ss xmm9,eax
    emit({0x48,0xB9}); emitBytes(&counter,8);           // movabs rcx,counter
    emit({0xF0,0x48,0xFF,0x01});                        // lock inc qword [rcx]
    emit({0x5A,0x59});                                  // pop rdx; pop rcx
    auto original=out.size;
    emitBytes(kTrackOriginal,sizeof(kTrackOriginal));
    std::uint8_t jump[5];
    if (!RelativeJump(jump,address+out.size,continuation)) return false;
    emitBytes(jump,5);
    if (overflow) return false;
    resolve(crane,crew); resolve(layer,crew);
    resolve(low,fallback); resolve(capOk,haveCap);
    resolve(noStaff,scaled); resolve(hugeStaff,scaled); resolve(enough,scaled); resolve(atLeastOne,scaled);
    for (std::size_t i=0;i<originalCount;++i) resolve(toOriginal[i],original);
    return !overflow && out.size<=kCapacity;
}
}
