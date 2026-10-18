// SPDX-License-Identifier: GPL-3.0-or-later
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <vector>
#include <limits>
#include "../src/construction_stub.h"

namespace ic=industrial::construction;
extern "C" void RunConstructionStub(void*,void*,void*,std::uint64_t*);

struct Case {
    const char* name;
    int kind=5, label=6203;          // descriptor type / localized name
    float progress=0.25f;
    int cap=100, actual=0;           // site worker limit / real workers
    bool maintenance=false;
    int funded=0;                    // 1 RUB, 2 USD
    bool nullType=false;
    int roadType=-1;                 // other than -1: temporary infrastructure site (+0x5D0)
    int expected=0;                  // ESI after the stub
    int family=-1;                   // counter that must advance
    int sublevel=0;                  // road object +0x124
    int diag=-1;                     // diagnostic slot that must advance (rejected infrastructure)
    int subtype=0;                   // descriptor+0x364
    int bdiag=-1;                    // building type whose diagnostic slot must advance
    float renovation=2.0f;           // XMM15 in maintenance mode: renovation progress from 0x1AA120 (>=1: nothing to do)
};

int main() {
    auto block=static_cast<BYTE*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
    if (!block) return 2;
    block[2048]=0xC3;
    alignas(8) static volatile LONG64 counts[ic::kCounterSlots]{};
    alignas(8) static volatile LONG64 diag[ic::kDiagTableSlots]{};
    ic::Stub stub;
    if (!ic::BuildStub(stub,reinterpret_cast<std::uintptr_t>(block),
        reinterpret_cast<std::uintptr_t>(block+2048),reinterpret_cast<std::uintptr_t>(&counts[0]),
        reinterpret_cast<std::uintptr_t>(&diag[0]),50)) return 3;
    std::memcpy(block,stub.bytes,stub.size); FlushInstructionCache(GetCurrentProcess(),block,4096);
    std::printf("construction stub size=%zu/%zu\n",stub.size,sizeof(stub.bytes));
    BYTE* current=block; // stub under test (the renovation-disabled variant is swapped in below)
    auto run=[&](Case c) {
        if (c.actual) c.expected=c.actual;
        std::vector<BYTE> descriptor(0x900),building(0x1200),frame(0x600),road(0x400);
        auto put=[&](std::vector<BYTE>& v,size_t at,int x) { std::memcpy(v.data()+at,&x,4); };
        put(descriptor,0x360,c.kind); put(descriptor,0x200,c.label); put(descriptor,0x364,c.subtype);
        void* td=c.nullType?nullptr:descriptor.data(); std::memcpy(building.data()+0x318,&td,8);
        if (c.roadType!=-1) {
            put(road,0x12C,c.roadType); put(road,0x124,c.sublevel);
            void* r=road.data(); std::memcpy(building.data()+0x5D0,&r,8);
        }
        std::memcpy(building.data()+0x604,&c.progress,4); put(building,0x62C,c.cap);
        building[0x608]=c.funded==1; building[0x609]=c.funded==2;
        BYTE* rbp=frame.data()+0x100; rbp[0x240]=c.maintenance;
        auto beforeBuilding=building; auto beforeDescriptor=descriptor; auto beforeFrame=frame; auto beforeRoad=road;
        std::uint64_t observed[16]{}; observed[0]=c.actual;
        std::uint32_t renovationBits; std::memcpy(&renovationBits,&c.renovation,4); observed[14]=renovationBits;
        float productivity=c.actual*0.85f; std::memcpy(reinterpret_cast<BYTE*>(observed)+8,&productivity,4);
        LONG64 oldCounts[ic::kCounterSlots],oldDiag[ic::kDiagTableSlots];
        for (int i=0;i<ic::kCounterSlots;++i) oldCounts[i]=counts[i];
        for (int i=0;i<ic::kDiagTableSlots;++i) oldDiag[i]=diag[i];
        RunConstructionStub(current,building.data(),rbp,observed);
        float x6,x7; std::memcpy(&x6,reinterpret_cast<BYTE*>(observed)+72,4);
        std::memcpy(&x7,reinterpret_cast<BYTE*>(observed)+88,4);
        bool applied=c.expected!=c.actual;
        bool countersOk=true;
        for (int i=0;i<ic::kFamilies;++i)
            countersOk &= counts[i]-oldCounts[i]==(applied && i==c.family?1:0);
        countersOk &= counts[ic::kRenovationCounter]-oldCounts[ic::kRenovationCounter]==(applied && c.maintenance?1:0);
        int buildingSlot=c.bdiag>=0?ic::kDiagBuildingBase+c.bdiag:-1;
        for (int i=0;i<ic::kDiagTableSlots;++i)
            countersOk &= diag[i]-oldDiag[i]==(i==c.diag||i==buildingSlot?1:0);
        bool ok=observed[2]==observed[3] && observed[4]==0x123456789ABCDEF0ULL &&
            observed[5]==reinterpret_cast<std::uintptr_t>(current) && observed[6]==reinterpret_cast<std::uintptr_t>(rbp) &&
            static_cast<std::uint32_t>(observed[15])==renovationBits &&
            observed[7]==reinterpret_cast<std::uintptr_t>(building.data()) && observed[8]==static_cast<std::uint64_t>(c.expected) &&
            observed[13]==0x0FEDCBA987654321ULL &&
            x6==c.expected && x7==(applied?static_cast<float>(c.expected):productivity) && countersOk &&
            building==beforeBuilding && descriptor==beforeDescriptor && frame==beforeFrame && road==beforeRoad;
        for (int i=1;i<4;++i) {
            std::uint32_t u6,u7;
            std::memcpy(&u6,reinterpret_cast<BYTE*>(observed)+72+i*4,4);
            std::memcpy(&u7,reinterpret_cast<BYTE*>(observed)+88+i*4,4);
            ok &= u6==0 && u7==0xFFFFFFFF;
        }
        std::printf("%s %s workers=%llu labor=%.2f flags=%s rcx=%s counters=%s memory=%s\n",ok?"PASS":"FAIL",c.name,
                    observed[8],x7,observed[2]==observed[3]?"preserved":"CHANGED",
                    observed[13]==0x0FEDCBA987654321ULL?"preserved":"CHANGED",countersOk?"ok":"WRONG",
                    building==beforeBuilding&&road==beforeRoad?"unchanged":"CHANGED");
        return ok;
    };
    constexpr int SF=ic::kStorageFarm, EB=ic::kElectricBuilding, PL=ic::kPowerLine, OF=ic::kOffice,
                  RI=ic::kRoadInfra, RB=ic::kRoadBuilding, SV=ic::kService, SH=ic::kShip, RL=ic::kRail, CG=ic::kCargo;
    // A zero-worker building site of a type that is not admitted advances its diagnostic slot.
    auto rejectedType=[&](Case c) { c.bdiag=c.kind; return run(c); };
    auto withSubtype=[&](Case c,int subtype) { c.subtype=subtype; return run(c); };
    bool ok=true;
    // 0.2 behaviour, unchanged.
    ok &= run({"open-storage-zero-staff",5,6203,0.25f,100,0,false,0,false,-1,50,SF});
    ok &= run({"covered-storage-zero-staff",5,6202,0.1f,100,0,false,0,false,-1,50,SF});
    ok &= run({"site-limit-respected",5,6203,0.25f,12,0,false,0,false,-1,12,SF});
    ok &= run({"zero-site-limit",5,6203,0.25f,0});
    ok &= run({"negative-site-limit",5,6203,0.25f,-1});
    ok &= run({"invalid-site-limit",5,6203,0.25f,5001});
    ok &= run({"existing-workers",5,6203,0.25f,100,3});
    ok &= run({"completed-building",5,6203,1.0f});
    ok &= run({"unstarted-progress",5,6203,0.0f,100,0,false,0,false,-1,50,SF});
    ok &= run({"negative-progress",5,6203,-0.1f});
    ok &= run({"invalid-progress",5,6203,std::numeric_limits<float>::quiet_NaN()});
    ok &= run({"maintenance",5,6203,0.25f,100,0,true});
    ok &= run({"funded-rub",5,6203,0.25f,100,0,false,1});
    ok &= run({"funded-usd",5,6203,0.25f,100,0,false,2});
    ok &= run({"open-storage-big-admitted-since-0.10",5,6204,0.25f,100,0,false,0,false,-1,50,SF});
    ok &= run({"free-open-storage",5,6226,1.0f});
    ok &= run({"any-name-factory-admitted-since-0.15",6,6203,0.25f,100,0,false,0,false,-1,50,ic::kIndustry});
    ok &= run({"null-descriptor",5,6203,0.25f,100,0,false,0,true});
    ok &= rejectedType({"reject-old-wrong-storage-type",1,6203});
    ok &= run({"small-farm-stalled-at-24-percent",9,6115,0.24f,64,0,false,0,false,-1,50,SF});
    ok &= run({"small-farm-site-limit",9,6115,0.24f,32,0,false,0,false,-1,32,SF});
    ok &= run({"completed-small-farm",9,6115,1.0f,64});
    ok &= run({"medium-farm-admitted-since-0.13",9,6116,0.24f,64,0,false,0,false,-1,50,SF});
    ok &= run({"any-name-storage-admitted-since-0.10",5,6115,0.24f,64,0,false,0,false,-1,50,SF});
    ok &= run({"any-name-farm-admitted-since-0.13",9,6203,0.24f,64,0,false,0,false,-1,50,SF});
    // 0.3: electric buildings by parser type, any localized name.
    ok &= run({"substation",18,6302,0.4f,60,0,false,0,false,-1,50,EB});
    ok &= run({"transformer",19,6301,0.4f,60,0,false,0,false,-1,50,EB});
    ok &= run({"high-switch",19,6305,0.4f,60,0,false,0,false,-1,50,EB});
    ok &= run({"low-priority-switch",19,6341,0.4f,60,0,false,0,false,-1,50,EB});
    ok &= run({"coal-power-plant",17,6300,0.4f,120,0,false,0,false,-1,50,EB});
    ok &= run({"nuclear-power-plant",17,6319,0.4f,200,0,false,0,false,-1,50,EB});
    ok &= run({"wind-power-plant-small-limit",17,6337,0.4f,8,0,false,0,false,-1,8,EB});
    ok &= run({"cooling-tower",60,9060,0.4f,60,0,false,0,false,-1,50,EB});
    ok &= run({"border-export",31,6419,0.4f,60,0,false,0,false,-1,50,EB});
    ok &= run({"border-import",32,6418,0.4f,60,0,false,0,false,-1,50,EB});
    ok &= run({"substation-with-real-workers",18,6302,0.4f,60,4});
    ok &= run({"substation-completed",18,6302,1.0f,60});
    ok &= run({"substation-funded",18,6302,0.4f,60,0,false,1});
    ok &= run({"substation-maintenance",18,6302,0.4f,60,0,true});
    ok &= run({"substation-zero-limit",18,6302,0.4f,0});
    ok &= rejectedType({"heating-plant-excluded",70,6303,0.4f,60});
    ok &= run({"rail-trafo-admitted-since-0.5",35,6420,0.4f,60,0,false,0,false,-1,50,EB});
    ok &= run({"water-pump-admitted-since-0.17",90,7521,0.4f,60,0,false,0,false,-1,50,ic::kWater});
    // 0.3: infrastructure sites (descriptor ignored, road object type decides).
    ok &= run({"power-line-high",13,0,0.3f,40,0,false,0,false,8,40,PL});
    ok &= run({"power-line-low",13,0,0.3f,80,0,false,0,false,9,50,PL});
    ok &= run({"power-line-null-descriptor",13,0,0.3f,80,0,false,0,true,9,50,PL});
    ok &= run({"power-line-real-workers",13,0,0.3f,80,2,false,0,false,8});
    ok &= run({"power-line-funded",13,0,0.3f,80,0,false,2,false,8});
    ok &= run({"power-line-completed",13,0,1.0f,80,0,false,0,false,8});
    ok &= run({"power-line-zero-limit",13,0,0.3f,0,0,false,0,false,9});
    ok &= run({"road-admitted-since-0.6",13,0,0.3f,80,0,false,0,false,0,50,RI,0});
    ok &= run({"rail-excluded",13,0,0.3f,80,0,false,0,false,1,0,-1,0,8});
    ok &= run({"road-surface-admitted-since-0.6",13,0,0.3f,80,0,false,0,false,3,50,RI,0});
    ok &= run({"oil-pipe-admitted-since-0.11",13,0,0.3f,80,0,false,0,false,7,50,ic::kPipe,0});
    ok &= run({"water-pipe-admitted-since-0.17",13,0,0.3f,80,0,false,0,false,11,50,ic::kWater,0});
    ok &= run({"heating-pipe-excluded",13,0,0.3f,80,0,false,0,false,14,0,-1,0,112});
    ok &= run({"tram-track-excluded",13,0,0.3f,80,0,false,0,false,2,0,-1,0,16});
    ok &= run({"unknown-road-type-10-excluded",13,0,0.3f,80,0,false,0,false,10,0,-1,0,80});
    ok &= run({"road-on-electric-descriptor-counts-as-road",18,6302,0.3f,80,0,false,0,false,0,50,RI,0});
    // 0.4: construction and demolition offices by parser type, any localized name.
    ok &= run({"construction-office-24-vehicles",12,6151,0.2f,120,0,false,0,false,-1,50,OF});
    ok &= run({"construction-office-helicopter",12,26314,0.2f,120,0,false,0,false,-1,50,OF});
    ok &= run({"construction-office-small-limit",12,6151,0.2f,20,0,false,0,false,-1,20,OF});
    ok &= run({"demolition-office-big",106,26333,0.2f,120,0,false,0,false,-1,50,OF});
    ok &= run({"demolition-office-helicopter",106,26337,0.2f,120,0,false,0,false,-1,50,OF});
    ok &= run({"construction-office-real-workers",12,6151,0.2f,120,5});
    ok &= run({"construction-office-funded",12,6151,0.2f,120,0,false,1});
    ok &= run({"construction-office-completed",12,6151,1.0f,120});
    ok &= run({"construction-office-zero-limit",12,6151,0.2f,0});
    ok &= run({"rail-construction-office-admitted-since-0.9",28,6152,0.2f,120,0,false,0,false,-1,50,RL});
    ok &= run({"rail-on-office-descriptor-excluded",12,6151,0.2f,120,0,false,0,false,1,0,-1,0,8});
    // 0.5: steam (cooling) pipes, traction transformers and the diagnostic table (every pipe since 0.11).
    constexpr int ST=ic::kPipe;
    ok &= run({"steam-pipe",13,0,0.3f,80,0,false,0,false,7,50,ST,5});
    ok &= run({"steam-pipe-small-limit",13,0,0.3f,10,0,false,0,false,7,10,ST,5});
    ok &= run({"steam-pipe-real-workers",13,0,0.3f,80,3,false,0,false,7,0,-1,5});
    ok &= run({"steam-pipe-funded",13,0,0.3f,80,0,false,1,false,7,0,-1,5});
    ok &= run({"steam-pipe-completed",13,0,1.0f,80,0,false,0,false,7,0,-1,5});
    ok &= run({"steam-pipe-zero-limit",13,0,0.3f,0,0,false,0,false,7,0,-1,5});
    ok &= run({"pipe-sublevel-3-admitted-since-0.11",13,0,0.3f,80,0,false,0,false,7,50,ic::kPipe,3});
    ok &= run({"sublevel-5-rail-excluded",13,0,0.3f,80,0,false,0,false,1,0,-1,5,13});
    ok &= run({"sublevel-5-water-admitted-since-0.17",13,0,0.3f,80,0,false,0,false,11,50,ic::kWater,5});
    ok &= run({"power-line-underground-sublevel",13,0,0.3f,80,0,false,0,false,9,50,PL,2});
    ok &= run({"diag-road-type-out-of-range",13,0,0.3f,80,0,false,0,false,17,0,-1,0,-1});
    ok &= run({"diag-negative-road-type",13,0,0.3f,80,0,false,0,false,-5+0,0,-1,0,-1});
    ok &= run({"diag-sublevel-out-of-range",13,0,0.3f,80,0,false,0,false,1,0,-1,8,-1});
    ok &= run({"rail-traction-transformer",35,6310,0.3f,60,0,false,0,false,-1,50,EB});
    ok &= run({"trolleybus-transformer",35,1609,0.3f,60,0,false,0,false,-1,50,EB});
    ok &= run({"rail-construction-office-small-limit",28,6152,0.3f,20,0,false,0,false,-1,20,RL});
    // 0.6: road section (roads of every class, lit, bridges/tunnels share type 0) and its buildings.
    ok &= run({"road-lit-sublevel-3",13,0,0.3f,80,0,false,0,false,0,50,RI,3});
    ok &= run({"road-small-limit",13,0,0.3f,6,0,false,0,false,0,6,RI,0});
    ok &= run({"road-high-sublevel",13,0,0.3f,80,0,false,0,false,0,50,RI,7});
    ok &= run({"pedestrian-path",13,0,0.3f,80,0,false,0,false,4,50,RI,0});
    ok &= run({"road-real-workers",13,0,0.3f,80,4,false,0,false,0,0,-1,0});
    ok &= run({"road-funded",13,0,0.3f,80,0,false,2,false,0,0,-1,0});
    ok &= run({"road-completed",13,0,1.0f,80,0,false,0,false,0,0,-1,0});
    ok &= run({"road-zero-limit",13,0,0.3f,0,0,false,0,false,0,0,-1,0});
    ok &= run({"road-maintenance",13,0,0.3f,80,0,true,0,false,0,0,-1,0});
    ok &= run({"rail-still-excluded",13,0,0.3f,80,0,false,0,false,1,0,-1,0,8});
    ok &= run({"tram-track-still-excluded",13,0,0.3f,80,0,false,0,false,2,0,-1,0,16});
    ok &= run({"conveyor-admitted-since-0.11",13,0,0.3f,80,0,false,0,false,6,50,ic::kConveyor,0});
    ok &= run({"factory-connection-admitted-since-0.10",13,0,0.3f,80,0,false,0,false,5,50,CG,0});
    ok &= run({"road-cargo-station",0,6111,0.3f,80,0,false,0,false,-1,50,RB});
    ok &= run({"road-depot",14,6130,0.3f,80,0,false,0,false,-1,50,RB});
    ok &= run({"gas-station",34,6125,0.3f,80,0,false,0,false,-1,50,RB});
    ok &= run({"parking",38,6129,0.3f,80,0,false,0,false,-1,50,RB});
    // 0.16: vehicle repair stations (type 108), every size; they repair without staff in industrial_tycoon.dll.
    ok &= run({"repair-station-small",108,26330,0.3f,80,0,false,0,false,-1,50,RB});
    ok &= run({"repair-station-medium",108,26338,0.3f,80,0,false,0,false,-1,50,RB});
    ok &= run({"repair-station-large",108,26331,0.3f,80,0,false,0,false,-1,50,RB});
    ok &= run({"repair-service-office",108,6151,0.3f,80,0,false,0,false,-1,50,RB});
    ok &= run({"repair-station-horse-dlc3",108,90013,0.3f,40,0,false,0,false,-1,40,RB});
    ok &= run({"repair-station-real-workers",108,26330,0.3f,80,4});
    ok &= run({"repair-station-completed",108,26330,1.0f,80});
    ok &= run({"road-depot-funded",14,6130,0.3f,80,0,false,1});
    ok &= rejectedType({"passenger-station-excluded",1,6110,0.3f,80});
    ok &= rejectedType({"waiting-station-excluded",96,6112,0.3f,80});
    ok &= rejectedType({"car-dealer-excluded",41,6140,0.3f,80});
    // 0.7: fire stations (every size: base 40/4, CWC 50/8 and 30/2, DLC3 30/5, 30/2, 20/1, 20/2).
    ok &= run({"fire-station",26,6259,0.3f,60,0,false,0,false,-1,50,SV});
    ok &= run({"fire-station-small-limit",26,6259,0.3f,20,0,false,0,false,-1,20,SV});
    ok &= run({"fire-station-real-workers",26,6259,0.3f,60,7});
    ok &= run({"fire-station-funded",26,6259,0.3f,60,0,false,2});
    ok &= rejectedType({"hospital-still-excluded",25,6260,0.3f,60});
    ok &= rejectedType({"police-still-excluded",73,6261,0.3f,60});
    // 0.8: ship section (subtype SHIP 0x13 on any type, checked before the type table) and ship docks.
    constexpr int ship=industrial::kSubtypeShip;
    ok &= withSubtype({"harbor-cargo-small",0,6371,0.3f,80,0,false,0,false,-1,50,SH},ship);
    ok &= withSubtype({"harbor-container",0,6395,0.3f,80,0,false,0,false,-1,50,SH},ship);
    ok &= withSubtype({"harbor-passenger-small",1,6373,0.3f,80,0,false,0,false,-1,50,SH},ship);
    ok &= withSubtype({"drydock",40,6391,0.3f,120,0,false,0,false,-1,50,SH},ship);
    ok &= withSubtype({"ship-scrapyard",109,6316,0.3f,120,0,false,0,false,-1,50,SH},ship);
    ok &= withSubtype({"ship-building-small-limit",0,6372,0.3f,15,0,false,0,false,-1,15,SH},ship);
    ok &= run({"ship-dock-big",42,6389,0.3f,80,0,false,0,false,-1,50,SH});
    ok &= run({"ship-dock-small",42,6388,0.3f,80,0,false,0,false,-1,50,SH});
    ok &= withSubtype({"harbor-real-workers",0,6371,0.3f,80,3},ship);
    ok &= withSubtype({"harbor-funded",0,6371,0.3f,80,0,false,1},ship);
    ok &= withSubtype({"harbor-completed",0,6371,1.0f,80},ship);
    ok &= withSubtype({"harbor-maintenance",0,6371,0.3f,80,0,true},ship);
    ok &= withSubtype({"harbor-zero-limit",0,6371,0.3f,0},ship);
    ok &= withSubtype({"harbor-null-descriptor",0,6371,0.3f,80,0,false,0,true},ship);
    ok &= withSubtype({"ship-subtype-on-rail-site-excluded",0,6371,0.3f,80,0,false,0,false,1,0,-1,0,8},ship);
    ok &= withSubtype({"road-vehicle-factory-admitted-since-0.15",40,2020,0.3f,120,0,false,0,false,-1,50,ic::kIndustry},0x19);
    ok &= withSubtype({"airplane-scrapyard-admitted-since-0.15",109,6316,0.3f,120,0,false,0,false,-1,50,ic::kIndustry},0x21);
    ok &= withSubtype({"airplane-passenger-terminal-still-excluded",1,6386,0.3f,80,0,false,0,false,-1,0,-1,0,-1,0,1},0x21);
    // 0.8: building diagnostics cover types 0..127 only.
    ok &= rejectedType({"diag-building-type-127",127,1,0.3f,80});
    ok &= run({"diag-building-type-out-of-range",128,1,0.3f,80});
    ok &= run({"diag-negative-building-type",-3,1,0.3f,80});
    ok &= run({"diag-not-counted-for-admitted-type-at-zero-limit",26,6259,0.3f,0});
    // 0.9: rail section. Rail cargo stations (0), rail gas and coaling stations (0x22) were already in.
    constexpr int railSub=industrial::kSubtypeRail;
    ok &= run({"rail-depot",15,1601,0.3f,80,0,false,0,false,-1,50,RL});
    ok &= run({"rail-depot-big",15,1602,0.3f,80,0,false,0,false,-1,50,RL});
    ok &= run({"rail-construction-office-big",28,6152,0.3f,170,0,false,0,false,-1,50,RL});
    ok &= run({"rail-distribution-office",52,6219,0.3f,80,0,false,0,false,-1,50,RL});
    ok &= run({"train-station",1,1802,0.3f,80,0,false,0,false,-1,50,RL});
    ok &= run({"train-station-small",1,1804,0.3f,80,0,false,0,false,-1,50,RL});
    ok &= withSubtype({"train-factory",40,2021,0.3f,120,0,false,0,false,-1,50,RL},railSub);
    ok &= withSubtype({"rail-end-station",96,1836,0.3f,80,0,false,0,false,-1,50,RL},railSub);
    ok &= run({"rail-cargo-station-still-road-building-family",0,1812,0.3f,80,0,false,0,false,-1,50,RB});
    ok &= run({"coaling-station-gas-station-type",34,6345,0.3f,80,0,false,0,false,-1,50,RB});
    ok &= run({"rail-depot-real-workers",15,1601,0.3f,80,4});
    ok &= run({"rail-depot-funded",15,1601,0.3f,80,0,false,2});
    ok &= run({"rail-depot-completed",15,1601,1.0f,80});
    ok &= run({"rail-depot-zero-limit",15,1601,0.3f,0});
    ok &= rejectedType({"bus-stop-still-excluded",1,1805,0.3f,80});
    ok &= withSubtype({"airport-terminal-still-excluded",1,6386,0.3f,80,0,false,0,false,-1,0,-1,0,-1,0,1},0x21);
    ok &= withSubtype({"bus-end-station-still-excluded",96,1823,0.3f,80,0,false,0,false,-1,0,-1,0,-1,0,96},0x19);
    ok &= withSubtype({"tram-stop-still-excluded",1,34810,0.3f,80,0,false,0,false,-1,0,-1,0,-1,0,1},0x24);
    ok &= withSubtype({"metro-station-still-excluded",1,1826,0.3f,80,0,false,0,false,-1,0,-1,0,-1,0,1},0x25);
    ok &= run({"road-distribution-office-admitted-since-0.10",43,6211,0.3f,80,0,false,0,false,-1,50,CG});
    ok &= withSubtype({"train-station-name-with-ship-subtype-counts-as-ship",1,1802,0.3f,80,0,false,0,false,-1,50,SH},industrial::kSubtypeShip);
    ok &= withSubtype({"rail-subtype-on-rail-segment-still-diverted",0,0,0.3f,80,0,false,0,false,1,0,-1,0,8},railSub);
    // 0.10: cargo & storage. Every storage but the waste ones; cargo buildings; cableway; factory connections.
    constexpr int cable=ic::kSubtypeCableway;
    ok &= run({"warehouse-dlc3",5,6202,0.3f,80,0,false,0,false,-1,50,SF});
    ok &= run({"open-storage-combined",5,6201,0.3f,80,0,false,0,false,-1,50,SF});
    ok &= run({"grain-silo",5,6107,0.3f,80,0,false,0,false,-1,50,SF});
    ok &= run({"meat-cold-storage",5,6106,0.3f,80,0,false,0,false,-1,50,SF});
    ok &= withSubtype({"space-for-vehicles",5,2037,0.3f,80,0,false,0,false,-1,50,SF},29);
    ok &= run({"gravel-storage",5,6157,0.3f,80,0,false,0,false,-1,50,SF});
    ok &= run({"cement-silo",5,2158,0.3f,80,0,false,0,false,-1,50,SF});
    ok &= run({"oil-storage",5,6052,0.3f,80,0,false,0,false,-1,50,SF});
    ok &= run({"livestock-storage",5,2157,0.3f,80,0,false,0,false,-1,50,SF});
    ok &= run({"storage-site-limit",5,6107,0.3f,30,0,false,0,false,-1,30,SF});
    for (int waste:{7553,7554,7555,7556,7597,7578,7579,26360}) { // admitted since 0.17 (waste management)
        Case c{"waste-storage-or-landfill",5,waste,0.3f,80,0,false,0,false,-1,50,SF}; ok &= run(c);
    }
    ok &= run({"storage-real-workers",5,6107,0.3f,80,2});
    ok &= run({"storage-funded",5,6107,0.3f,80,0,false,1});
    ok &= run({"container-packing-facility",44,6216,0.3f,80,0,false,0,false,-1,50,CG});
    ok &= run({"container-unpacking-facility",44,6217,0.3f,80,0,false,0,false,-1,50,CG});
    ok &= run({"forklift-station",45,6205,0.3f,80,0,false,0,false,-1,50,CG});
    ok &= run({"forklift-crossing",45,6206,0.3f,80,0,false,0,false,-1,50,CG});
    ok &= run({"distribution-office-small",43,6210,0.3f,80,0,false,0,false,-1,50,CG});
    ok &= run({"distribution-office-dlc3-no-name",43,0,0.3f,80,0,false,0,false,-1,50,CG});
    ok &= withSubtype({"cableway-passenger-station",1,6501,0.3f,80,0,false,0,false,-1,50,CG},cable);
    ok &= withSubtype({"cableway-pole",11,6504,0.3f,80,0,false,0,false,-1,50,CG},cable);
    ok &= withSubtype({"cableway-gravel-loading-counts-as-cargo",0,6505,0.3f,80,0,false,0,false,-1,50,CG},cable);
    ok &= run({"cableway-segment",13,0,0.3f,80,0,false,0,false,12,50,CG,0});
    ok &= run({"factory-connection-sublevel-2",13,0,0.3f,80,0,false,0,false,5,50,CG,2});
    // 0.11: conveyor section and every pipe.
    constexpr int CV=ic::kConveyor;
    ok &= run({"conveyor-engine",11,6167,0.3f,80,0,false,0,false,-1,50,CV});
    ok &= run({"conveyor-engine-overpass",11,6173,0.3f,80,0,false,0,false,-1,50,CV});
    ok &= run({"bulk-engine",11,6182,0.3f,80,0,false,0,false,-1,50,CV});
    ok &= run({"bulk-engine-overpass",11,6174,0.3f,80,0,false,0,false,-1,50,CV});
    ok &= run({"pipeline-pumping-station",11,6056,0.3f,80,0,false,0,false,-1,50,CV});
    ok &= run({"conveyor-segment",13,0,0.3f,80,0,false,0,false,6,50,CV,0});
    ok &= run({"bulk-conveyor-segment",13,0,0.3f,80,0,false,0,false,6,50,CV,5});
    ok &= run({"conveyor-segment-small-limit",13,0,0.3f,12,0,false,0,false,6,12,CV,0});
    ok &= run({"conveyor-real-workers",13,0,0.3f,80,2,false,0,false,6,0,-1,0});
    ok &= run({"conveyor-funded",13,0,0.3f,80,0,false,1,false,6,0,-1,0});
    ok &= run({"oil-pipeline-segment",13,0,0.3f,80,0,false,0,false,7,50,ST,0});
    ok &= run({"water-pipe-admitted-since-0.17-cargo-section",13,0,0.3f,80,0,false,0,false,11,50,ic::kWater,0});
    ok &= run({"heating-pipe-still-excluded",13,0,0.3f,80,0,false,0,false,14,0,-1,0,112});
    ok &= run({"fence-admitted-since-0.12",13,0,0.3f,80,0,false,0,false,15,50,ic::kDecoration,0});
    ok &= run({"cargo-real-workers",45,6205,0.3f,80,3});
    ok &= run({"cargo-completed",45,6205,1.0f,80});
    ok &= run({"trash-container-admitted-since-0.17",105,7550,0.3f,80,0,false,0,false,-1,50,ic::kWaste});
    // 0.12: trees & accessories. Monuments, fences, fence gates, pergolas and colonnades.
    constexpr int DC=ic::kDecoration;
    ok &= run({"monument-statue",33,6363,0.3f,80,0,false,0,false,-1,50,DC});
    ok &= run({"dlc4-world-monument",33,90018,0.3f,80,0,false,0,false,-1,50,DC});
    ok &= run({"monument-without-name",33,0,0.3f,80,0,false,0,false,-1,50,DC});
    ok &= run({"pyramid-museum-monument",33,6041,0.3f,80,0,false,0,false,-1,50,DC});
    ok &= run({"monument-small-limit",33,6363,0.3f,9,0,false,0,false,-1,9,DC});
    ok &= run({"fence-gate",80,26315,0.3f,80,0,false,0,false,-1,50,DC});
    ok &= run({"fence-gate-rail",80,26366,0.3f,80,0,false,0,false,-1,50,DC});
    ok &= run({"fence-gate-park-vases",80,26367,0.3f,80,0,false,0,false,-1,50,DC});
    ok &= run({"pergola",80,90044,0.3f,80,0,false,0,false,-1,50,DC});
    ok &= run({"colonnade-without-name",80,0,0.3f,80,0,false,0,false,-1,50,DC});
    for (int name:{1078,1079,1089,1828,1829}) {
        Case c{"pedestrian-or-metro-entrance-excluded",80,name,0.3f,80}; ok &= rejectedType(c);
    }
    ok &= run({"fence-segment-sublevel-4",13,0,0.3f,80,0,false,0,false,15,50,DC,4});
    ok &= run({"fence-real-workers",13,0,0.3f,80,3,false,0,false,15,0,-1,0});
    ok &= run({"monument-funded",33,6363,0.3f,80,0,false,2});
    ok &= run({"plumbing-admitted-since-0.17",13,0,0.3f,80,0,false,0,false,16,50,ic::kWater,0});
    ok &= rejectedType({"attraction-still-excluded",51,6038,0.3f,80});
    ok &= rejectedType({"sport-still-excluded",24,6258,0.3f,80});
    // 0.13: food, alcohol & crops. Farms of every size, fields, and the section's factories by name.
    constexpr int IND=ic::kIndustry;
    ok &= run({"large-farm",9,6117,0.3f,80,0,false,0,false,-1,50,SF});
    ok &= run({"farm-dlc3",9,6103,0.3f,80,0,false,0,false,-1,50,SF});
    ok &= run({"field-small",8,6108,0.3f,80,0,false,0,false,-1,50,IND});
    ok &= run({"field-big",8,6109,0.3f,80,0,false,0,false,-1,50,IND});
    ok &= run({"food-factory",6,6101,0.3f,120,0,false,0,false,-1,50,IND});
    ok &= run({"distillery",6,6102,0.3f,120,0,false,0,false,-1,50,IND});
    ok &= run({"livestock-farm",6,6104,0.3f,120,0,false,0,false,-1,50,IND});
    ok &= run({"slaughterhouse",6,6105,0.3f,120,0,false,0,false,-1,50,IND});
    ok &= run({"synthesized-fertiliser",6,7573,0.3f,120,0,false,0,false,-1,50,IND});
    ok &= run({"refinery-construction",6,6053,0.3f,120,0,false,0,false,-1,50,IND});
    ok &= run({"factory-small-limit",6,6101,0.3f,25,0,false,0,false,-1,25,IND});
    ok &= run({"bio-waste-fertiliser-admitted-since-0.17",6,7572,0.3f,80,0,false,0,false,-1,50,IND});
    ok &= run({"steel-mill-admitted-since-0.15",6,6353,0.3f,80,0,false,0,false,-1,50,IND});
    ok &= run({"factory-real-workers",6,6101,0.3f,120,5});
    ok &= run({"factory-funded",6,6101,0.3f,120,0,false,1});
    ok &= run({"field-completed",8,6108,1.0f,80});
    // 0.14: miscellaneous section. Its factories by name, the woodcutting post and the gravel quarry.
    for (int name:{7002,6351,6352,6158,6170,6168,6172,6169,6171}) {
        Case c{"miscellaneous-factory",6,name,0.3f,120,0,false,0,false,-1,50,IND}; ok &= run(c);
    }
    ok &= run({"woodcutting-post",7,7001,0.3f,80,0,false,0,false,-1,50,IND});
    ok &= run({"gravel-quarry",7,6155,0.3f,80,0,false,0,false,-1,50,IND});
    ok &= run({"quarry-small-limit",7,6155,0.3f,18,0,false,0,false,-1,18,IND});
    ok &= run({"coal-mine-admitted-since-0.15",7,6159,0.3f,80,0,false,0,false,-1,50,IND});
    ok &= run({"gravel-recycling-admitted-since-0.17",6,7565,0.3f,80,0,false,0,false,-1,50,IND});
    ok &= run({"quarry-real-workers",7,6155,0.3f,80,6});
    // 0.15: industries by type, any name, so Workshop buildings using $NAME_STR are covered.
    ok &= run({"workshop-gravel-mine-v2-name-str",7,0,0.3f,50,0,false,0,false,-1,50,IND});
    ok &= run({"workshop-factory-unknown-name",6,123456,0.3f,80,0,false,0,false,-1,50,IND});
    ok &= run({"workshop-production-line",40,0,0.3f,80,0,false,0,false,-1,50,IND});
    ok &= run({"aircraft-factory",40,2087,0.3f,80,0,false,0,false,-1,50,IND});
    ok &= run({"iron-mine",7,6160,0.3f,80,0,false,0,false,-1,50,IND});
    ok &= run({"oil-well",7,6051,0.3f,80,0,false,0,false,-1,50,IND});
    for (int waste:{7557,7558,7561,7562,7563,7564,7565,7570,7571,7572,7598}) { // admitted since 0.17
        Case c{"waste-chain-factory",6,waste,0.3f,80,0,false,0,false,-1,50,IND}; ok &= run(c);
    }
    // 0.17: airports, border, waste management and water/sewage.
    constexpr int AP=ic::kAirport, BD=ic::kBorder, WS=ic::kWaste, WT=ic::kWater;
    ok &= run({"airport-road-connection",46,9046,0.3f,80,0,false,0,false,-1,50,AP});
    for (int name:{6383,6384,6385,6396,6397,26311,26316,70144}) {
        Case c{"airplane-stand-or-heliport",47,name,0.3f,80,0,false,0,false,-1,50,AP}; ok &= run(c);
    }
    ok &= run({"control-tower",48,9048,0.3f,80,0,false,0,false,-1,50,AP});
    ok &= run({"runway",13,0,0.3f,80,0,false,0,false,13,50,AP,0});
    ok &= run({"taxiway-small-limit",13,0,0.3f,20,0,false,0,false,13,20,AP,1});
    ok &= run({"runway-real-workers",13,0,0.3f,80,4,false,0,false,13});
    for (int name:{6400,6401,6402}) {
        Case c{"road-customs",20,name,0.3f,80,0,false,0,false,-1,50,BD}; ok &= run(c);
    }
    ok &= withSubtype({"air-customs",20,6420,0.3f,80,0,false,0,false,-1,50,BD},0x21);
    ok &= run({"foreign-pipeline-connection",107,9107,0.3f,80,0,false,0,false,-1,50,BD});
    ok &= run({"technical-services",49,6218,0.3f,80,0,false,0,false,-1,50,WS});
    ok &= run({"technical-services-dlc3-excavation-office",49,6218,0.3f,30,0,false,0,false,-1,30,WS});
    for (int kind:{90,91,92,93,94,95,97,98}) {
        Case c{"water-or-sewage-building",kind,7500,0.3f,80,0,false,0,false,-1,50,WT}; ok &= run(c);
    }
    ok &= run({"sewage-pipe",13,0,0.3f,80,0,false,0,false,16,50,WT,0});
    ok &= run({"water-treatment-real-workers",97,7531,0.3f,80,3});
    ok &= rejectedType({"heating-endstation-still-excluded",71,7511,0.3f,80});
    ok &= rejectedType({"heating-switch-still-excluded",72,7512,0.3f,80});
    ok &= rejectedType({"pollution-meter-still-excluded",36,7540,0.3f,80});
    // 0.18: renovation of completed buildings with building wear on (maintenance mode, progress in XMM15).
    auto renovate=[&](Case c,float progress) { c.maintenance=true; c.renovation=progress; return run(c); };
    ok &= renovate({"renovation-storage",5,6203,1.0f,100,0,false,0,false,-1,50,SF},0.4f);
    ok &= renovate({"renovation-factory-small-limit",6,6053,1.0f,20,0,false,0,false,-1,20,IND},0.0f);
    ok &= renovate({"renovation-power-plant",17,6300,1.0f,80,0,false,0,false,-1,50,EB},0.75f);
    ok &= renovate({"renovation-road",13,0,1.0f,80,0,false,0,false,0,50,RI,0},0.5f);
    ok &= renovate({"renovation-harbor-ship-subtype",0,6371,1.0f,80,0,false,0,false,-1,50,SH,0,-1,ship},0.2f);
    ok &= renovate({"renovation-completed",5,6203,1.0f,100},1.0f);
    ok &= renovate({"renovation-progress-above-one",5,6203,1.0f,100},1.5f);
    ok &= renovate({"renovation-negative-progress",5,6203,1.0f,100},-0.1f);
    ok &= renovate({"renovation-nan-progress",5,6203,1.0f,100},std::numeric_limits<float>::quiet_NaN());
    ok &= renovate({"renovation-real-workers",5,6203,1.0f,100,6},0.4f);
    ok &= renovate({"renovation-funded",5,6203,1.0f,100,0,false,2},0.4f);
    ok &= renovate({"renovation-zero-limit",5,6203,1.0f,0},0.4f);
    ok &= renovate({"renovation-null-descriptor-on-building",5,6203,1.0f,100,0,false,0,true},0.4f);
    { Case c{"renovation-citizen-building-not-admitted",25,6260,1.0f,60}; c.bdiag=25; ok &= renovate(c,0.4f); }
    ok &= run({"construction-mode-ignores-xmm15",5,6203,0.25f,100,0,false,0,false,-1,50,SF,0,-1,0,-1,0.9f});
    ok &= run({"construction-mode-completed-site",5,6203,1.0f,100,0,false,0,false,-1,0,-1,0,-1,0,-1,0.2f});
    // renovation=0 in the INI: maintenance mode stays original, construction is unchanged.
    {
        auto block2=static_cast<BYTE*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
        if (!block2) return 4;
        block2[2048]=0xC3;
        ic::Stub off;
        if (!ic::BuildStub(off,reinterpret_cast<std::uintptr_t>(block2),reinterpret_cast<std::uintptr_t>(block2+2048),
                reinterpret_cast<std::uintptr_t>(&counts[0]),reinterpret_cast<std::uintptr_t>(&diag[0]),50,false)) return 5;
        std::memcpy(block2,off.bytes,off.size); FlushInstructionCache(GetCurrentProcess(),block2,4096);
        std::printf("construction stub without renovation size=%zu/%zu\n",off.size,sizeof(off.bytes));
        current=block2;
        ok &= renovate({"renovation-disabled-storage",5,6203,1.0f,100},0.4f);
        ok &= run({"renovation-disabled-construction-unchanged",5,6203,0.25f,100,0,false,0,false,-1,50,SF});
        current=block;
        VirtualFree(block2,0,MEM_RELEASE);
    }
    ic::Stub rejected;
    bool invalid=!ic::BuildStub(rejected,0x1000,0x2000,0x3000,0x4000,0) &&
                 !ic::BuildStub(rejected,0x1000,0x2000,0x3000,0x4000,501) &&
                 !ic::BuildStub(rejected,0x1000,0x2000,0,0x4000,50) &&
                 !ic::BuildStub(rejected,0x1000,0x2000,0x3000,0,50);
    std::printf("%s invalid-worker-or-counter-config\n",invalid?"PASS":"FAIL");
    VirtualFree(block,0,MEM_RELEASE);
    return ok && invalid?0:1;
}
