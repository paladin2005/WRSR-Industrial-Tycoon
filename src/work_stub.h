// SPDX-License-Identifier: GPL-3.0-or-later
// Buildings whose tick sums the work of the people present instead of calling the labor function
// 0x175A00: when nobody worked, they get the work of their full staff. Every site below was
// verified on WRSR 1.1.1.9.
#pragma once
#include "labor_stub.h"

namespace industrial::work {
// Each tick walks the people at the building (+0xBC0..+0xBC8) and sums their work (at most 1.0
// per person) into an XMM register; the empty-list branch and the end of the loop meet at the
// hooked instruction, which is relocated after the stub so it runs last on every path.
struct Site {
    const char* key;            // INI key
    const char* label;          // log name
    std::uint32_t site;
    std::uint8_t original[7];   // relocated instruction
    std::uint32_t contextAt;
    const std::uint8_t* context;
    std::size_t contextSize;
    std::uint8_t buildingReg;   // 3 RBX, 6 RSI
    std::uint8_t workXmm;       // register holding the summed work
    std::uint8_t buildingType;  // descriptor+0x360
    std::uint16_t staffOffset;  // descriptor field with the staff: 0x80C $WORKERS_NEEDED, 0x810 $PROFESORS_NEEDED
    std::int8_t countReg;       // register holding the number of people who worked (-1: none)
    std::int16_t spillRsp;      // [rsp+disp8] that also holds the summed work (-1: none)
    std::uint32_t continuation() const { return site+sizeof(original); }
    const std::uint8_t* originalInContext() const { return context+(site-contextAt); }
};

// Container facilities: tick 0x1C27F0 (dispatcher 0x13E329, type 0x2C). Work per person:
// efficiency 0x1BCAE0 x (qualification 0x18FE20 x 0.7 x productivity 0x824290 + productivity x
// 0.3), up to [facility+0xDEC] people, summed into XMM13; the rest of the tick spends it on
// containers (0x1C2B4F, 0x1C2F04, 0x1C2F72). The relocated compare sets the flags for the `jle`.
constexpr std::uint8_t kContainerContext[]={
    0xF3,0x44,0x0F,0x10,0x15,0x17,0x80,0x74,0x00,     // movss xmm10,[rip+...]  (60.0)
    0x44,0x0F,0x28,0x9C,0x24,0xA0,0x00,0x00,0x00,     // movaps xmm11,[rsp+0xa0]
    0x45,0x39,0xA6,0xBC,0x05,0x00,0x00,               // relocated: cmp [r14+0x5bc],r12d
    0x7E,0x13,                                        // jle 0x1C2A86
    0x48,0x8B,0xD3,                                   // mov rdx,rbx
    0xE8,0x05,0x78,0xFE,0xFF};                        // call 0x1AA280
// Fields (hand-worked kolkhoz fields, 75/150/250 workers): tick 0x16E3B0 (dispatcher 0x13E0D9,
// type 8). Productivity 0x824290 of each rested person summed into XMM6; the tick then advances
// the field by XMM6 / [field+0xE30] (0x16E564).
constexpr std::uint8_t kFieldContext[]={
    0x8B,0xCF,                                        // mov ecx,edi
    0x48,0x3B,0xC8,                                   // cmp rcx,rax
    0x72,0x83,                                        // jb 0x16E480  (people loop)
    0x48,0x8B,0x8E,0x68,0x0E,0x00,0x00,               // relocated: mov rcx,[rsi+0xe68]
    0x48,0x2B,0x8E,0x60,0x0E,0x00,0x00};              // sub rcx,[rsi+0xe60]

// Control towers (15/20 engineers, no workers): tick 0x1C23C0 (dispatcher 0x13E2A9, type 0x30). Work per
// engineer (efficiency x (qualification x c x productivity + productivity x c)) summed into XMM8 and the
// people counted in R14D, both 0 with nobody there; the tick then raises or lowers the tower's operating
// level (+0x11A8, capped at $PROFESORS_NEEDED) from them and stores XMM8 at +0xDDC. The empty-list branch
// (0x1C249B) and the end of the loop meet at 0x1C25B5; the hook is the first instruction after the
// restores, where the relocated compare sets the flags for the `jle`.
constexpr std::uint8_t kTowerContext[]={
    0x44,0x0F,0x28,0xAC,0x24,0x80,0x00,0x00,0x00,     // movaps xmm13,[rsp+0x80]   (meeting point 0x1C25B5)
    0x44,0x0F,0x28,0x9C,0x24,0xA0,0x00,0x00,0x00,     // movaps xmm11,[rsp+0xa0]
    0x48,0x8B,0xB4,0x24,0x28,0x01,0x00,0x00,          // mov rsi,[rsp+0x128]
    0x44,0x39,0xBD,0xBC,0x05,0x00,0x00,               // relocated: cmp [rbp+0x5bc],r15d
    0x7E,0x17,                                        // jle 0x1C25EF
    0x48,0x8B,0xD3,                                   // mov rdx,rbx
    0xE8,0xA0,0x7C,0xFE,0xFF};                        // call 0x1AA280
// Water wells (7..10 workers, used with water management): tick 0x189030 (dispatcher 0x13C93B, type 0x5C)
// calls 0x1B1290 (its only caller), which sums each person's work (productivity x efficiency x
// qualification) into XMM15 and keeps it at [rsp+0x74] too; both are read later. The empty-list branch
// (0x1B13E1) and the end of the loop meet at the relocated compare.
constexpr std::uint8_t kWellContext[]={
    0x48,0x3B,0xC8,                                   // cmp rcx,rax
    0x0F,0x82,0x33,0xFF,0xFF,0xFF,                    // jb 0x1B13F2  (people loop)
    0xF3,0x44,0x0F,0x11,0x7C,0x24,0x74,               // movss [rsp+0x74],xmm15
    0x45,0x39,0xBE,0xBC,0x05,0x00,0x00,               // relocated: cmp [r14+0x5bc],r15d
    0x7E,0x1A,                                        // jle 0x1B14E9
    0x48,0x8B,0xD6,                                   // mov rdx,rsi
    0xE8,0xA9,0x8D,0xFF,0xFF};                        // call 0x1AA280

constexpr Site kSites[]={
    {"container_facility","container facility",0x1C2A6A,{0x45,0x39,0xA6,0xBC,0x05,0x00,0x00},
     0x1C2A58,kContainerContext,sizeof(kContainerContext),3,13,0x2C,0x80C,-1,-1},
    {"fields","field",0x16E4FD,{0x48,0x8B,0x8E,0x68,0x0E,0x00,0x00},
     0x16E4F6,kFieldContext,sizeof(kFieldContext),6,6,0x08,0x80C,-1,-1},
    {"control_tower","control tower",0x1C25CF,{0x44,0x39,0xBD,0xBC,0x05,0x00,0x00},
     0x1C25B5,kTowerContext,sizeof(kTowerContext),3,8,0x30,0x810,14,-1},
    {"water_well","water well",0x1B14C6,{0x45,0x39,0xBE,0xBC,0x05,0x00,0x00},
     0x1B14B6,kWellContext,sizeof(kWellContext),6,15,0x5C,0x80C,-1,0x74},
};
constexpr int kSiteCount=sizeof(kSites)/sizeof(kSites[0]);
constexpr std::size_t kCapacity=256;

struct Stub { std::uint8_t bytes[kCapacity]{}; std::size_t size=0; };

// At every site RAX and the flags are dead (the relocated instruction or the code right after it
// sets them before any read). The virtual path writes the low lane of the work register, plus the
// site's people counter and stack copy of the work when it has them. Virtual staff: the building's
// staff field (descriptor+staffOffset), each person adding 1.0.
inline bool BuildWorkStub(Stub& out,const Site& s,std::uintptr_t address,std::uintptr_t continuation,
                          std::uintptr_t counter) {
    if (!counter || (s.buildingReg!=3 && s.buildingReg!=6) || s.workXmm>15 ||
        (s.staffOffset!=0x80C && s.staffOffset!=0x810) || s.countReg<-1 || s.countReg>15 || s.countReg==4 ||
        s.spillRsp<-1 || s.spillRsp>0x7F) return false;
    out={};
    bool overflow=false;
    auto emit=[&](std::initializer_list<std::uint8_t> v) {
        for (auto b:v) { if (out.size<kCapacity) out.bytes[out.size]=b; else overflow=true; ++out.size; }
    };
    auto emitBytes=[&](const void* p,std::size_t n) {
        auto b=static_cast<const std::uint8_t*>(p);
        for (std::size_t i=0;i<n;++i) emit({b[i]});
    };
    std::size_t toOriginal[8]{}; std::size_t originalCount=0;
    auto originalIf=[&](std::uint8_t cc) {
        emit({0x0F,cc,0,0,0,0});
        if (originalCount<8) toOriginal[originalCount++]=out.size-4; else overflow=true;
    };
    auto xmmModrm=static_cast<std::uint8_t>(0xC0|((s.workXmm&7)<<3)); // reg=xmm, rm=eax
    auto emitXmmOp=[&](std::uint8_t prefix,std::uint8_t op) {          // prefix [REX.R] 0F op modrm
        if (s.workXmm>=8) emit({prefix,0x44,0x0F,op,xmmModrm}); else emit({prefix,0x0F,op,xmmModrm});
    };

    emitXmmOp(0x66,0x7E);                               // movd eax,xmmN
    emit({0xA9,0xFF,0xFF,0xFF,0x7F}); originalIf(0x85); // real work (anything but +-0): original
    emit({0x48,0x8B,static_cast<std::uint8_t>(0x80|s.buildingReg),0x18,0x03,0x00,0x00}); // mov rax,[bld+0x318]
    emit({0x48,0x85,0xC0}); originalIf(0x84);
    emit({0x83,0xB8,0x60,0x03,0x00,0x00,s.buildingType}); originalIf(0x85);
    emit({0x8B,0x80,static_cast<std::uint8_t>(s.staffOffset),static_cast<std::uint8_t>(s.staffOffset>>8),0x00,0x00}); // mov eax,[rax+staff]
    emit({0x83,0xF8,0x01}); originalIf(0x8C);           // no staff defined
    emit({0x3D,0xE8,0x03,0x00,0x00}); originalIf(0x8F); // implausible staff
    emitXmmOp(0xF3,0x2A);                               // cvtsi2ss xmmN,eax
    if (s.countReg>=0) {                                // mov rNd,eax  people who worked
        auto rm=static_cast<std::uint8_t>(0xC0|(s.countReg&7));
        if (s.countReg>=8) emit({0x41,0x89,rm}); else emit({0x89,rm});
    }
    if (s.spillRsp>=0) {                                // movss [rsp+disp8],xmmN
        auto modrm=static_cast<std::uint8_t>(0x44|((s.workXmm&7)<<3));
        auto disp=static_cast<std::uint8_t>(s.spillRsp);
        if (s.workXmm>=8) emit({0xF3,0x44,0x0F,0x11,modrm,0x24,disp}); else emit({0xF3,0x0F,0x11,modrm,0x24,disp});
    }
    emit({0x48,0xB8}); emitBytes(&counter,8);           // movabs rax,counter
    emit({0xF0,0x48,0xFF,0x00});                        // lock inc qword [rax]
    auto original=out.size;
    emitBytes(s.original,sizeof(s.original));
    std::uint8_t jump[5];
    if (!RelativeJump(jump,address+out.size,continuation)) return false;
    emitBytes(jump,5);
    if (overflow) return false;
    for (std::size_t i=0;i<originalCount;++i) {
        auto rel=static_cast<std::int32_t>(original)-static_cast<std::int32_t>(toOriginal[i]+4);
        std::memcpy(out.bytes+toOriginal[i],&rel,4);
    }
    return out.size<=kCapacity;
}
}
