// SPDX-License-Identifier: GPL-3.0-or-later
// Asphalt deliveries for virtual workers: construction offices plan asphalt for a road site with
// no asphalt paver and no real workers when the construction patch will work that site.
// The patch point was verified on WRSR 1.1.1.9.
#pragma once
#include "construction_stub.h"

namespace industrial::asphalt {
// Office delivery planner 0x14EB10 (RBP game, RBX site, R14 resource; callers 0x14FCAA, 0x1502B6,
// 0x15053A). For an infrastructure site (+0x5D0 set) that asks for asphalt ([game+0xC298]+8) in an
// asphalt phase (phase kind 8, phase index from 0x14DDE0, phases at +0x610, 0x21B8 bytes each), the
// planner skips the delivery (0x14F110) when the trip value from 0x1544A0 exceeds 35, or when no
// assigned office has an asphalt paver (0x1528E0 walks the site's offices +0x880..+0x888) and none
// can bring real workers (0x1521F0 with the workers resource [game+0xC2C8]). Virtual workers are
// only added later, at 0x15A39D, so without this patch the asphalt is never planned and the site
// waits. The patch replaces the final `je 0x14F110`; the trip limit and the paver check stay.
constexpr std::uint32_t kSite=0x14EC50;
constexpr std::uint8_t kOriginal[]={0x0F,0x84,0xBA,0x04,0x00,0x00}; // je 0x14F110
constexpr std::uint32_t kContextAt=0x14EBEF;
constexpr std::uint8_t kContext[]={
    0x48,0x8B,0x85,0x98,0xC2,0x00,0x00,               // mov rax,[rbp+0xc298]  construction resources
    0x4C,0x3B,0x70,0x08,                              // cmp r14,[rax+8]       asphalt?
    0x75,0x5A,                                        // jne 0x14EC56
    0x48,0x8B,0xD3,0x48,0x8B,0xCD,                    // mov rdx,rbx; mov rcx,rbp
    0xE8,0xD9,0xF1,0xFF,0xFF,                         // call 0x14DDE0         current phase index
    0x85,0xC0,0x78,0x4B,                              // test eax,eax; js 0x14EC56
    0x48,0x98,0x48,0x69,0xC8,0xB8,0x21,0x00,0x00,     // cdqe; imul rcx,rax,0x21b8
    0x48,0x8B,0x83,0x10,0x06,0x00,0x00,               // mov rax,[rbx+0x610]   phases
    0x83,0x7C,0x01,0x04,0x08,                         // cmp dword [rcx+rax+4],8  asphalt phase
    0x75,0x34,                                        // jne 0x14EC56
    0x44,0x0F,0x2F,0x25,0xA2,0xBD,0x7B,0x00,          // comiss xmm12,[0x90A9CC]  35.0
    0x0F,0x87,0xE0,0x04,0x00,0x00,                    // ja 0x14F110           trip limit (kept)
    0x48,0x8B,0xD3,0xE8,0xA8,0x3C,0x00,0x00,          // mov rdx,rbx; call 0x1528E0  asphalt paver?
    0x84,0xC0,0x75,0x1A,                              // test al,al; jne 0x14EC56
    0x4C,0x8B,0x85,0xC8,0xC2,0x00,0x00,               // mov r8,[rbp+0xc2c8]   workers resource
    0x48,0x8B,0xD3,0x48,0x8B,0xCD,                    // mov rdx,rbx; mov rcx,rbp
    0xE8,0xA2,0x35,0x00,0x00,                         // call 0x1521F0         real workers?
    0x84,0xC0,                                        // test al,al
    0x0F,0x84,0xBA,0x04,0x00,0x00};                   // je 0x14F110           <- patched
constexpr std::uint32_t kAdmit=0x14EC56;  // comiss xmm6,xmm11: plan the delivery
constexpr std::uint32_t kReject=0x14F110; // skip this resource for the site
// The first version uses 213 bytes; keep room for later checks.
constexpr std::size_t kCapacity=512;

struct Stub { std::uint8_t bytes[kCapacity]{}; std::size_t size=0; };

// Entry flags come from `test al,al` on the result of 0x1521F0. RAX and flags are dead at both
// targets (0x14EC56 starts with comiss; 0x14F110 writes RAX before reading it), so the stub uses
// only RAX. A delivery is admitted for the cases the construction stub gives virtual workers to:
// an admitted road type, a site worker limit of 1..5000 and a site not funded with RUB or USD.
inline bool BuildStub(Stub& out,std::uintptr_t address,std::uintptr_t admit,std::uintptr_t reject,std::uintptr_t counter) {
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
    constexpr std::size_t kMaxFixups=32;
    std::size_t toReject[kMaxFixups]{}; std::size_t rejectCount=0;
    std::size_t toType[kMaxFixups]{}; std::size_t typeCount=0;
    auto branch=[&](std::uint8_t cc) { emit({0x0F,cc,0,0,0,0}); return out.size-4; };
    auto rejectIf=[&](std::uint8_t cc) {
        auto at=branch(cc);
        if (rejectCount<kMaxFixups) toReject[rejectCount++]=at; else overflow=true;
    };
    auto resolve=[&](std::size_t operand,std::size_t target) {
        if (operand+4>kCapacity || target>kCapacity) { overflow=true; return; }
        auto rel=static_cast<std::int32_t>(target)-static_cast<std::int32_t>(operand+4);
        std::memcpy(out.bytes+operand,&rel,4);
    };

    auto realWorkers=branch(0x85);                      // jne: an office can bring real workers
    emit({0x48,0x8B,0x83,0xD0,0x05,0x00,0x00});        // mov rax,[rbx+0x5d0]  road object
    emit({0x48,0x85,0xC0}); rejectIf(0x84);
    emit({0x8B,0x80,0x2C,0x01,0x00,0x00});              // mov eax,[rax+0x12c]  road type
    for (auto& r:construction::kRoadTypes) {
        emit({0x83,0xF8,r.type});
        auto at=branch(0x84);
        if (typeCount<kMaxFixups) toType[typeCount++]=at; else overflow=true;
    }
    emit({0xE9,0,0,0,0});
    if (rejectCount<kMaxFixups) toReject[rejectCount++]=out.size-4; else overflow=true;
    auto typeOk=out.size;
    emit({0x66,0x83,0xBB,0x08,0x06,0x00,0x00,0x00}); rejectIf(0x85); // funded RUB/USD
    emit({0x8B,0x83,0x2C,0x06,0x00,0x00});              // mov eax,[rbx+0x62c]  site worker limit
    emit({0x85,0xC0}); rejectIf(0x8E);
    emit({0x3D,0x88,0x13,0x00,0x00}); rejectIf(0x87);   // sanity: <=5000
    emit({0x48,0xB8}); emitBytes(&counter,8);           // movabs rax,counter
    emit({0xF0,0x48,0xFF,0x00});                        // lock inc qword [rax]
    auto toAdmit=out.size;
    std::uint8_t jump[5];
    if (!RelativeJump(jump,address+out.size,admit)) return false;
    emitBytes(jump,5);
    auto rejected=out.size;
    if (!RelativeJump(jump,address+out.size,reject)) return false;
    emitBytes(jump,5);
    if (overflow) return false;
    resolve(realWorkers,toAdmit);
    for (std::size_t i=0;i<typeCount;++i) resolve(toType[i],typeOk);
    for (std::size_t i=0;i<rejectCount;++i) resolve(toReject[i],rejected);
    return !overflow && out.size<=kCapacity;
}
}
