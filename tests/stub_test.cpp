// SPDX-License-Identifier: GPL-3.0-or-later
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <vector>
#include "../src/labor_stub.h"

extern "C" void RunLaborStub(void*,void*,void*,std::uint64_t*);

int main() {
    auto block=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,4096,
        MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
    if (!block) return 2;
    block[2048]=0xC3;
    alignas(8) volatile LONG64 hits=0;
    industrial::Stub stub;
    if (!industrial::BuildStub(stub,reinterpret_cast<std::uintptr_t>(block),
        reinterpret_cast<std::uintptr_t>(block+2048),reinterpret_cast<std::uintptr_t>(&hits))) return 3;
    std::memcpy(block,stub.bytes,stub.size);
    FlushInstructionCache(GetCurrentProcess(),block,4096);
    std::printf("labor stub size=%zu/%zu\n",stub.size,sizeof(stub.bytes));
    auto run=[&](const char* name,int kind,int label,int workers,int professors,bool nullType,bool matches,int mineType=0,int subtype=0) {
        std::vector<std::uint8_t> descriptor(0x900),building(0x400),frame(0x300);
        auto put=[&](int offset,int value) { std::memcpy(descriptor.data()+offset,&value,4); };
        put(0x360,kind); put(0x200,label); put(0x80C,workers); put(0x810,professors); put(0x368,mineType); put(0x364,subtype);
        void* type=nullType?nullptr:descriptor.data();
        std::memcpy(building.data()+0x318,&type,8);
        auto rbp=frame.data()+0x100;
        float before=matches?0.0f:0.35f,originalLoad=12.5f;
        std::memcpy(rbp+0x6F,&before,4);
        std::memcpy(rbp-0x79,&originalLoad,4);
        std::uint64_t observed[7]{};
        LONG64 oldHits=hits;
        RunLaborStub(block,building.data(),rbp,observed);
        float after,loaded;
        std::memcpy(&after,rbp+0x6F,4);
        std::memcpy(&loaded,reinterpret_cast<char*>(observed)+40,4);
        std::uint32_t upper[3]{};
        std::memcpy(upper,reinterpret_cast<char*>(observed)+44,12);
        bool ok=after==(matches?1.0f:before) && loaded==originalLoad &&
            hits-oldHits==(matches?1:0) && observed[0]==observed[1] &&
            observed[2]==0x123456789ABCDEF0ULL && observed[3]==reinterpret_cast<std::uintptr_t>(building.data()) &&
            observed[4]==reinterpret_cast<std::uintptr_t>(rbp) && upper[0]==0 && upper[1]==0 && upper[2]==0;
        std::printf("%s %s factor=%.2f counter_delta=%lld flags=%s\n",ok?"PASS":"FAIL",name,
            after,hits-oldHits,observed[0]==observed[1]?"preserved":"CHANGED");
        return ok;
    };
    bool ok=true;
    // Since 0.11 factories (type 6), mines (7), production lines (40) and scrapyards (109) are admitted
    // with any name and staff, so Workshop buildings using $NAME_STR are covered; since 0.15 the waste chain
    // factories too (waste management). Subtypes SHIP/RAIL and power plants (17) keep their rules.
    constexpr int ship=industrial::kSubtypeShip, road=0x19, rail=0x1A, airplane=0x21;
    ok &= run("refinery",6,6053,500,0,false,true);
    ok &= run("refinery-any-staff",6,6053,100,0,false,true);
    ok &= run("refinery-with-professors",6,6053,500,1,false,true);
    ok &= run("chemical-plant",6,6054,60,0,false,true);
    ok &= run("steel-mill",6,6353,500,0,false,true);
    ok &= run("uranium-conversion-with-professors",6,6316,80,40,false,true);
    ok &= run("workshop-factory-without-name-id",6,0,50,0,false,true);
    ok &= run("workshop-factory-unknown-name",6,123456,50,0,false,true);
    for (int name:{6101,6102,6104,6105,7573,7002,6351,6352,6158,6170,6168,6172,6169,6171})
        ok &= run("section-factory",6,name,30,0,false,true);
    for (int name:{7557,7558,7561,7562,7563,7564,7565,7570,7571,7572,7598})
        ok &= run("waste-chain-factory",6,name,25,0,false,true);
    ok &= run("factory-null-descriptor",6,6053,500,0,true,false);
    ok &= run("woodcutting",7,7001,10,0,false,true,4);
    ok &= run("dlc3-woodcutting-5-workers",7,7001,5,0,false,true,4);
    ok &= run("gravel-quarry",7,6155,40,0,false,true,3);
    ok &= run("workshop-gravel-mine-name-str",7,0,50,0,false,true,3);
    ok &= run("coal-mine",7,6159,220,0,false,true,1);
    ok &= run("iron-mine",7,6160,250,0,false,true,2);
    ok &= run("uranium-mine",7,6314,100,0,false,true,5);
    ok &= run("mine-null-descriptor",7,7001,10,0,true,false,4);
    ok &= run("road-vehicles",40,2020,400,100,false,true,0,road);
    ok &= run("road-vehicles-changed-staff",40,2020,500,99,false,true,0,road);
    ok &= run("train-factory",40,2021,400,100,false,true,0,rail);
    ok &= run("dlc3-train-factory",40,2021,200,50,false,true,0,rail);
    ok &= run("drydock",40,6391,400,100,false,true,0,ship);
    ok &= run("aircraft-factory",40,2087,500,300,false,true,0,airplane);
    ok &= run("workshop-production-line",40,0,100,20,false,true,0,0);
    ok &= run("ship-scrapyard",109,6316,150,0,false,true,0,ship);
    ok &= run("cwc-ship-scrapping-facility-big",109,26364,280,0,false,true,0,ship);
    ok &= run("airplane-scrapyard",109,6316,80,0,false,true,0,airplane);
    ok &= run("production-line-null-descriptor",40,2020,400,100,true,false);
    // Power plants: parser type 17, any name; staff values from the base-game files.
    ok &= run("coal-power-plant",17,6300,20,0,false,true);
    ok &= run("nuclear-power-plant-double",17,6319,80,40,false,true);
    ok &= run("wind-power-plant-no-staff",17,6337,0,0,false,true);
    ok &= run("power-plant-null-descriptor",17,6300,20,0,true,false);
    // Not admitted: services, heating, electric distribution and other types.
    // 0.16: water and sewage treatment plants (water management), any name and staff.
    ok &= run("water-treatment-big",0x61,7531,10,0,false,true);
    ok &= run("water-treatment-small",0x61,7530,5,0,false,true);
    ok &= run("workshop-water-treatment-no-name-id",0x61,0,8,0,false,true);
    ok &= run("sewage-treatment-big",0x62,7533,20,0,false,true);
    ok &= run("sewage-treatment-small",0x62,7532,10,0,false,true);
    ok &= run("water-treatment-null-descriptor",0x61,7531,10,0,true,false);
    ok &= run("water-well-uses-work-site-not-labor",0x5C,7501,7,0,false,false);
    ok &= run("sewage-discharge-not-admitted",0x5F,7536,0,0,false,false);
    ok &= run("heating-plant-not-admitted",70,6303,30,0,false,false);
    ok &= run("substation-not-admitted",18,6302,0,0,false,false);
    ok &= run("hospital-not-admitted",25,6260,50,10,false,false);
    ok &= run("field-type-uses-work-site-not-labor",8,6108,75,0,false,false);
    ok &= run("storage-not-admitted",5,6203,0,0,false,false);
    ok &= run("ship-subtype-on-other-type",1,6373,0,0,false,true,0,ship);
    ok &= run("rail-subtype-on-end-station",96,1836,0,0,false,true,0,rail);
    std::uint8_t jump[5];
    bool range=!industrial::RelativeJump(jump,0x1000,0x100000000ULL);
    std::printf("%s rejects-out-of-range-jump\n",range?"PASS":"FAIL");
    VirtualFree(block,0,MEM_RELEASE);
    return ok&&range?0:1;
}
