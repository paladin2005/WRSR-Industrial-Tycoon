// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

namespace industrial {
constexpr std::uint32_t kTimestamp = 0x6A3EB6AD;
constexpr std::uint32_t kLaborFunction = 0x175A00;
constexpr std::uint32_t kLaborSite = 0x175CF7;
constexpr std::uint8_t kOriginal[5] = {0xF3,0x0F,0x10,0x7D,0x87};
constexpr std::uint8_t kContext[12] = {
    0xF3,0x0F,0x10,0x7D,0x87,0x4C,0x8D,0x86,0x08,0x14,0x01,0x00};
constexpr std::uint8_t kFunctionHead[17] = {
    0x48,0x8B,0xC4,0x48,0x89,0x58,0x18,0x48,0x89,0x70,0x20,
    0x55,0x57,0x41,0x54,0x41,0x56};

// 0.3 used 245 bytes and the power plant branch passed 256; 0.10 reached 461 with name tables.
constexpr std::size_t kStubCapacity=1024;
// Parser values of $SUBTYPE_SHIP and $SUBTYPE_RAIL, written to descriptor+0x364
// (0x10FAE4 and 0x1187EB).
constexpr std::uint8_t kSubtypeShip=0x13, kSubtypeRail=0x1A;
// Building types admitted with any localized name and any staff, so Workshop buildings that use
// $NAME_STR (and therefore no base-game name id) are covered too.
constexpr std::uint8_t kAdmittedTypes[]={
    0x06,   // factories, through the factory routine 0x1D1E80, the waste chain included since 0.15
            // (incinerators, waste separation, recycling, bio-waste fertiliser, toxic waste treatment),
            // for games with waste management enabled
    0x07,   // mines (wood, gravel, coal, iron, uranium, bauxite, oil); tick 0x1B3700 calls this function
            // at 0x1B392D, and wood multiplies the factor by the staff at 0x1B3D4D
    0x11,   // power plants, through the factory routine 0x1D1E80 (fuel, cooling and weather untouched)
    0x28,   // production lines (road, rail, ship and aircraft factories), tick 0x1CA020
    0x6D,   // scrapyards, tick 0x14B920 (calls this function at 0x14BAA4)
    0x61,   // water treatment plants and
    0x62,   // sewage treatment plants (0.16, water management): the dispatcher picks both at 0x13DD24-0x13DD38
            // (`sub ecx,0x61; cmp ecx,1; ja`) and calls 0x1CE9A0, which calls this function at 0x1CEA80 and
            // feeds the factor to the same production plan as factories (0x1797A0, 0x1760E0, 0x17A7B0)
};
struct Stub { std::uint8_t bytes[kStubCapacity]{}; std::size_t size=0; };

inline bool RelativeJump(std::uint8_t out[5],std::uintptr_t at,std::uintptr_t to) {
    auto delta=static_cast<std::int64_t>(to)-static_cast<std::int64_t>(at+5);
    if (delta<INT32_MIN || delta>INT32_MAX) return false;
    auto rel=static_cast<std::int32_t>(delta);
    out[0]=0xE9;
    std::memcpy(out+1,&rel,4);
    return true;
}

// Entry inside the game's work-factor function: RBX=building, RBP=frame.
// Only [RBP+0x6F] changes on a match. All GPRs and flags survive. The stolen
// MOVSS executes exactly once, with its original SSE semantics.
inline bool BuildStub(Stub& out,std::uintptr_t address,
                      std::uintptr_t continuation,std::uintptr_t counter) {
    out={};
    bool overflow=false;
    // Never writes past the buffer: an oversized stub is reported, not emitted.
    auto emit=[&](std::initializer_list<std::uint8_t> values) {
        for (auto value:values) {
            if (out.size<kStubCapacity) out.bytes[out.size]=value; else overflow=true;
            ++out.size;
        }
    };
    constexpr std::size_t kMaxFixups=32;
    std::size_t exits[kMaxFixups]{}, matches[kMaxFixups]{};
    std::size_t exitCount=0, matchCount=0;
    auto branch=[&](std::uint8_t condition,std::size_t* list,std::size_t& count) {
        emit({0x0F,condition,0,0,0,0});
        if (count<kMaxFixups) list[count++]=out.size-4; else overflow=true;
    };
    auto exitIf=[&](std::uint8_t condition) { branch(condition,exits,exitCount); };
    auto applyIf=[&](std::uint8_t condition) { branch(condition,matches,matchCount); };
    emit({0x9C,0x50}); // pushfq; push rax
    emit({0x48,0x8B,0x83,0x18,0x03,0x00,0x00}); // building->type
    emit({0x48,0x85,0xC0}); exitIf(0x84);
    // Ship and rail sections: every building whose subtype is SHIP or RAIL (drydock, ship scrapyards,
    // train factories; the other ones have no staff).
    emit({0x83,0xB8,0x64,0x03,0x00,0x00,kSubtypeShip}); applyIf(0x84);
    emit({0x83,0xB8,0x64,0x03,0x00,0x00,kSubtypeRail}); applyIf(0x84);
    for (auto type:kAdmittedTypes) { emit({0x83,0xB8,0x60,0x03,0x00,0x00,type}); applyIf(0x84); }
    emit({0xE9,0,0,0,0}); // anything else stays original
    if (exitCount<kMaxFixups) exits[exitCount++]=out.size-4; else overflow=true;
    auto apply=out.size;
    emit({0xC7,0x45,0x6F,0x00,0x00,0x80,0x3F}); // local work factor=1.0
    emit({0x48,0xB8}); // movabs rax, hit counter
    for (int i=0;i<8;++i) emit({static_cast<std::uint8_t>(counter>>(8*i))});
    emit({0xF0,0x48,0xFF,0x00}); // lock inc qword [rax]
    auto restore=out.size;
    emit({0x58,0x9D});
    for (auto value:kOriginal) emit({value});
    std::uint8_t jump[5];
    if (!RelativeJump(jump,address+out.size,continuation)) return false;
    for (auto value:jump) emit({value});
    if (overflow) return false;
    auto resolve=[&](std::size_t operand,std::size_t target) {
        if (operand+4>kStubCapacity || target>kStubCapacity) { overflow=true; return; }
        auto distance=static_cast<std::int32_t>(target)-static_cast<std::int32_t>(operand+4);
        std::memcpy(out.bytes+operand,&distance,4);
    };
    for (std::size_t i=0;i<matchCount;++i) resolve(matches[i],apply);
    for (std::size_t i=0;i<exitCount;++i) resolve(exits[i],restore);
    return !overflow && out.size<=sizeof(out.bytes);
}
}
