// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "labor_stub.h"

namespace industrial::construction {
constexpr std::uint32_t kFunction=0x159070;
constexpr std::uint32_t kSite=0x15A39D;
constexpr std::uint8_t kHead[]={0x48,0x8B,0xC4,0x44,0x88,0x48,0x20,0xF3,0x0F,0x11,0x50,0x18};
constexpr std::uint8_t kOriginal[]={0x66,0x0F,0x6E,0xF6,0x0F,0x5B,0xF6};
constexpr std::uint8_t kContext[]={0x66,0x0F,0x6E,0xF6,0x0F,0x5B,0xF6,0x45,0x8B,0xC7,0x44,0x89,0x7C,0x24,0x50};
// 0.5 used 481 of 512 bytes; the table-driven 0.6 stub needs more room, and 0.10 passes 1024
// with the waste storage names.
constexpr std::size_t kCapacity=2048;

// One counter per admitted family, reported separately in the log.
enum Family : int {
    kStorageFarm=0, kElectricBuilding=1, kPowerLine=2, kOffice=3, kPipe=4,
    kRoadInfra=5, kRoadBuilding=6, kService=7, kShip=8, kRail=9, kCargo=10, kConveyor=11, kDecoration=12,
    kIndustry=13, kAirport=14, kBorder=15, kWaste=16, kWater=17, kFamilies=18
};
// One more counter after the families: renovations (admitted sites in maintenance mode, 0.18).
constexpr int kRenovationCounter=kFamilies, kCounterSlots=kFamilies+1;
// Localized names of the train passenger stations (every base, CWC and DLC3 size); bus
// stops share their type 1 and declare no subtype either.
constexpr std::uint32_t kTrainStationNames[]={1802,1804};
// Accessories share $TYPE_PEDESTRIAN_BRIDGE (0x50) with pedestrian infrastructure: fence gates,
// pergolas and colonnades are admitted, while pedestrian bridges, the pedestrian tunnel entry and
// the metro entrances (city infrastructure, not decoration) stay out by name.
constexpr std::uint8_t kPedestrianBridgeType=0x50;
constexpr std::uint32_t kPedestrianInfrastructureNames[]={1078,1079,1089,1828,1829};
// Parser value of $SUBTYPE_CABLEWAY (0x10FC24): cableway stations and poles.
constexpr std::uint8_t kSubtypeCableway=0x20;
// Diagnostic histogram of zero-worker infrastructure sites that were NOT admitted,
// indexed by (road type << 3) | sublevel for road type < 16 and sublevel < 8.
constexpr int kDiagTypes=16, kDiagSublevels=8, kDiagSlots=kDiagTypes*kDiagSublevels;
// Zero-worker building sites that were NOT admitted, by building type < 128, stored
// after the infrastructure slots in the same table.
constexpr int kDiagBuildingTypes=128, kDiagBuildingBase=kDiagSlots;
constexpr int kDiagTableSlots=kDiagSlots+kDiagBuildingTypes;

struct TypeFamily { std::uint8_t type; std::uint8_t family; };

// Buildings admitted with any localized name, by the parser value of descriptor+0x360
// (verified in the type parser 0x10E1F0; see dev.py construction_verify).
constexpr TypeFamily kBuildingTypes[]={
    {0x11,kElectricBuilding}, // $TYPE_POWERPLANT
    {0x12,kElectricBuilding}, // $TYPE_SUBSTATION
    {0x13,kElectricBuilding}, // $TYPE_TRANSFORMATOR (switches, transformers)
    {0x1F,kElectricBuilding}, // $TYPE_ELETRIC_EXPORT (also the foreign power connection)
    {0x20,kElectricBuilding}, // $TYPE_ELETRIC_IMPORT
    {0x3C,kElectricBuilding}, // $TYPE_COOLING_TOWER
    {0x23,kElectricBuilding}, // $TYPE_RAIL_TRAFO (rail, trolleybus, tram, metro)
    // Offices that need no staff to operate once built. The rail construction office
    // (0x1C) is left out: it needs 60/170 workers to run and rails stay excluded.
    {0x0C,kOffice},           // $TYPE_CONSTRUCTION_OFFICE (all sizes, helicopter)
    {0x6A,kOffice},           // $TYPE_DEMOLITION_OFFICE (all sizes, helicopter)
    // Road section buildings.
    {0x00,kRoadBuilding},     // $TYPE_CARGO_STATION (road, bulk, liquid; rail ones share the type)
    {0x0E,kRoadBuilding},     // $TYPE_ROADDEPO (road, tram, trolleybus depots)
    {0x22,kRoadBuilding},     // $TYPE_GAS_STATION
    {0x26,kRoadBuilding},     // $TYPE_PARKING
    {0x6C,kRoadBuilding},     // $TYPE_REPAIR_OFFICE (vehicle repair stations, every size; works without staff: repair_stub.h)
    // Services the industry itself needs. Fire stations: needed when fires are enabled,
    // because industries burn; their operation without staff is in industrial_tycoon.dll.
    {0x1A,kService},          // $TYPE_FIRESTATION (base, CWC and DLC3 sizes, helicopter ones)
    // Ship section: ship depots have no subtype; every other ship building is admitted by
    // its SHIP subtype (kSubtypeShip), checked before this table.
    {0x2A,kShip},             // $TYPE_SHIP_DOCK (small and big)
    // Rail section: train factories and rail end stations carry the RAIL subtype (checked
    // before this table); train passenger stations are admitted by name; rail cargo stations
    // (0x00), rail gas and coaling stations (0x22) and traction transformers (0x23) are above.
    {0x0F,kRail},             // $TYPE_RAILDEPO
    {0x1C,kRail},             // $TYPE_CONSTRUCTION_OFFICE_RAIL (works without staff: rail_stub.h)
    {0x34,kRail},             // $TYPE_DISTRIBUTION_OFFICE_RAIL
    // Cargo & storage section (storages are handled below, cargo stations above).
    {0x2B,kCargo},            // $TYPE_DISTRIBUTION_OFFICE (road, every size)
    {0x2C,kCargo},            // $TYPE_CONTAINER_FACILITY (works without staff: container_stub.h)
    {0x2D,kCargo},            // $TYPE_FORKLIFT_GARAGE (forklift station and forklift crossing)
    // Conveyor section: conveyor and bulk engines/transfers and their overpasses; pipeline pumping
    // stations share the type. Cableway poles (same type) are counted by their subtype above.
    {0x0B,kConveyor},         // $TYPE_ENGINE
    // Trees & accessories section: decoration for industrial areas too.
    {0x21,kDecoration},       // $TYPE_MONUMENT (base, CWC, DLC3 and the DLC4 world monuments)
    // Industries and storages, with any name so Workshop buildings ($NAME_STR) are covered too. Since
    // 0.17 the waste chain is included (waste factories, waste and container storages, the landfill),
    // for games with waste management enabled.
    {0x06,kIndustry},         // $TYPE_FACTORY (every factory, the waste chain included)
    {0x05,kStorageFarm},      // $TYPE_STORAGE (every storage, the waste ones and the landfill included)
    {0x08,kIndustry},         // $TYPE_FIELD (hand-worked fields; they work without staff: work_stub.h)
    {0x09,kStorageFarm},      // $TYPE_FARM (every size, base and DLC3)
    {0x07,kIndustry},         // $TYPE_MINE_* (wood, gravel, coal, iron, uranium, bauxite, oil)
    {0x28,kIndustry},         // $TYPE_PRODUCTION_LINE (road, rail, ship and aircraft factories)
    {0x6D,kIndustry},         // $TYPE_SCRAPYARD
    // Airport section (0.17); the cargo terminal is a cargo station (above), airport passenger terminals
    // stay out with the other passenger stations.
    {0x2E,kAirport},          // $TYPE_AIRPLANE_GATE (airport road connection)
    {0x2F,kAirport},          // $TYPE_AIRPLANE_PARKING (airplane stands, heliports)
    {0x30,kAirport},          // $TYPE_AIRPLANE_TOWER (control tower; works without staff: work_stub.h)
    // Border (0.17): where the republic sells and buys.
    {0x14,kBorder},           // $TYPE_CUSTOMHOUSE (road customs, every size; air customs)
    {0x6B,kBorder},           // $TYPE_FOREIGN_PIPELINE_EXPORT (foreign pipeline connection)
    // Waste management (0.17), for games with it enabled.
    {0x31,kWaste},            // $TYPE_GARBAGE_OFFICE (technical services, DLC3 excavation office)
    {0x69,kWaste},            // $TYPE_TRASH_CONTAINER
    // Water and sewage (0.17), for games with water management enabled.
    {0x5A,kWater},            // $TYPE_WATER_PUMP (pumping stations, reservoirs)
    {0x5B,kWater},            // $TYPE_WATER_ENDSTATION
    {0x5C,kWater},            // $TYPE_MINE_WATER and _SURFACE (wells work without staff: work_stub.h)
    {0x61,kWater},            // $TYPE_WATER_TREATMENT
    {0x5D,kWater},            // $TYPE_SEWAGE_PUMP
    {0x5E,kWater},            // $TYPE_SEWAGE_ENDSTATION
    {0x5F,kWater},            // $TYPE_SEWAGE_DISCHARGE
    {0x62,kWater},            // $TYPE_SEWAGE_TREATMENT
};
// Road object+0x12C (SOVIET_ROADTYPE) admitted at temporary infrastructure sites.
// Overhead/underground, lit/unlit, bridge and tunnel variants share their type.
constexpr TypeFamily kRoadTypes[]={
    {8,kPowerLine},  // ELETRIC_HIGH
    {9,kPowerLine},  // ELETRIC_LOW
    {0,kRoadInfra},  // ROAD (every road class, street lights, bridges, tunnels)
    {3,kRoadInfra},  // ROAD_SECONDARY (road surfaces and squares, "second_road_*" tools)
    {4,kRoadInfra},  // PEDESTRIAN
    {5,kCargo},      // CONNECTION (factory connections)
    {12,kCargo},     // CABLEWAY
    {6,kConveyor},   // CONVEYOR (conveyors, variant 0, and bulk conveyors, variant 5: tool 0x3AE610)
    {7,kPipe},       // PIPE (oil pipelines, sublevel 0, and steam pipes, sublevel 5: tool 0x3AEFD0)
    {15,kDecoration},// FENCE (every fence model; tool [game+0xF8B0], see 0x16BCFA)
    {13,kAirport},   // AIRPORT (runways, taxiways)
    {11,kWater},     // WATER (water pipes)
    {16,kWater},     // PLUMBING (sewage pipes)
};

struct Stub { std::uint8_t bytes[kCapacity]{}; std::size_t size=0; };

// At this site RDI is the building, ESI the accepted worker count and XMM7
// their summed productivity. Original machinery and material code follows.
// Infrastructure sites are temporary construction buildings whose +0x5D0
// points to the road object being built; only the road types above are admitted there. Every other rejected road type and building type is
// counted for diagnostics.
// Renovation (0.18): with building wear enabled, the construction office renovates a completed building
// through this same routine in maintenance mode ([rbp+0x240]=1, set at 0x1591A3 when the global setting
// 0x9D54CC is on, the building's +0x879 is set, +0x604 >= 1 and its first phase is kind 0x12 or 0x13).
// The routine then takes its progress from 0x1AA120 (the done fraction of the renovation phase) into
// XMM15 (0x159274) instead of +0x604, counts the workers of the renovation list (+0xC20; for kind 0x13
// also the building's own people) and applies their work like for a construction site. With
// `renovation`, a maintenance-mode site is admitted like a construction site of the same building,
// checking XMM15 < 1 instead of +0x604; without it, maintenance mode stays original.
inline bool BuildStub(Stub& out,std::uintptr_t address,std::uintptr_t continuation,
                      std::uintptr_t counters,std::uintptr_t diagnostics,int virtualWorkers,bool renovation=true) {
    if (virtualWorkers<1 || virtualWorkers>500 || !counters || !diagnostics) return false;
    out={};
    bool overflow=false;
    auto emit=[&](std::initializer_list<std::uint8_t> v) {
        for (auto b:v) { if (out.size<kCapacity) out.bytes[out.size]=b; else overflow=true; ++out.size; }
    };
    auto emitBytes=[&](const void* p,std::size_t n) {
        auto b=static_cast<const std::uint8_t*>(p);
        for (std::size_t i=0;i<n;++i) emit({b[i]});
    };
    constexpr std::size_t kMaxFixups=128; // 0.17 passes 64 family jumps (types, road types, subtypes, names)
    std::size_t exits[kMaxFixups]{}; std::size_t exitCount=0;
    struct Fixup { std::size_t operand; int family; };
    Fixup familyJumps[kMaxFixups]{}; std::size_t familyCount=0;
    auto branch=[&](std::uint8_t cc) { emit({0x0F,cc,0,0,0,0}); return out.size-4; };
    auto exitIf=[&](std::uint8_t cc) {
        auto at=branch(cc);
        if (exitCount<kMaxFixups) exits[exitCount++]=at; else overflow=true;
    };
    auto exitJump=[&]() {
        emit({0xE9,0,0,0,0});
        if (exitCount<kMaxFixups) exits[exitCount++]=out.size-4; else overflow=true;
    };
    auto toFamily=[&](std::uint8_t cc,int family) { // conditional jump to a family label
        auto at=branch(cc);
        if (familyCount<kMaxFixups) familyJumps[familyCount++]={at,family}; else overflow=true;
    };
    auto resolve=[&](std::size_t operand,std::size_t target) {
        if (operand+4>kCapacity || target>kCapacity) { overflow=true; return; }
        auto rel=static_cast<std::int32_t>(target)-static_cast<std::int32_t>(operand+4);
        std::memcpy(out.bytes+operand,&rel,4);
    };

    emit({0x9C,0x50,0x51}); // preserve flags, RAX and RCX
    emit({0x85,0xF6}); exitIf(0x85); // real workers: leave them alone
    emit({0x48,0x85,0xFF}); exitIf(0x84);
    emit({0x80,0xBD,0x40,0x02,0x00,0x00,0x00}); // maintenance mode (renovation)?
    std::size_t constructionMode=0, toFunded=0;
    if (!renovation) exitIf(0x85);
    else {
        constructionMode=branch(0x84);
        emit({0x66,0x44,0x0F,0x7E,0xF8});            // movd eax,xmm15  renovation progress (0x1AA120)
        emit({0x3D,0x00,0x00,0x80,0x3F}); exitIf(0x83); // >=1, negative, NaN: nothing to renovate
        emit({0xE9,0,0,0,0}); toFunded=out.size-4;
        resolve(constructionMode,out.size);
    }
    emit({0x8B,0x87,0x04,0x06,0x00,0x00}); // progress float bits
    emit({0x3D,0x00,0x00,0x80,0x3F}); exitIf(0x83); // >=1, negative, NaN
    if (renovation) resolve(toFunded,out.size);
    emit({0x66,0x83,0xBF,0x08,0x06,0x00,0x00,0x00}); exitIf(0x85); // funded RUB/USD

    // Infrastructure site: admitted road types only.
    emit({0x48,0x8B,0x87,0xD0,0x05,0x00,0x00}); // rax=[rdi+0x5D0] road object
    emit({0x48,0x85,0xC0}); auto buildingPath=branch(0x84);
    emit({0x8B,0x88,0x24,0x01,0x00,0x00}); // ecx=road sublevel
    emit({0x8B,0x80,0x2C,0x01,0x00,0x00}); // eax=road type
    for (auto& r:kRoadTypes) { emit({0x83,0xF8,r.type}); toFamily(0x84,r.family); }
    auto rejected=out.size; // diagnostics: count (type, sublevel) when both are small
    emit({0x83,0xF8,kDiagTypes}); exitIf(0x83);
    emit({0x83,0xF9,kDiagSublevels}); exitIf(0x83);
    emit({0xC1,0xE0,0x03}); // shl eax,3
    emit({0x09,0xC8});      // or eax,ecx
    emit({0x48,0xB9}); emitBytes(&diagnostics,8); // movabs rcx, table
    emit({0xF0,0x48,0xFF,0x04,0xC1}); // lock inc qword [rcx+rax*8]
    exitJump();

    // Building site: subtypes, types admitted with any name, then the name-filtered types.
    auto building=out.size;
    emit({0x48,0x8B,0x87,0x18,0x03,0x00,0x00}); // rax=descriptor
    emit({0x48,0x85,0xC0}); exitIf(0x84);
    emit({0x8B,0x88,0x60,0x03,0x00,0x00}); // ecx=building type
    emit({0x83,0xB8,0x64,0x03,0x00,0x00,kSubtypeShip}); toFamily(0x84,kShip); // subtype SHIP
    emit({0x83,0xB8,0x64,0x03,0x00,0x00,kSubtypeRail}); toFamily(0x84,kRail); // subtype RAIL
    emit({0x83,0xB8,0x64,0x03,0x00,0x00,kSubtypeCableway}); toFamily(0x84,kCargo); // subtype CABLEWAY
    for (auto& b:kBuildingTypes) { emit({0x83,0xF9,b.type}); toFamily(0x84,b.family); }
    emit({0x83,0xF9,0x01}); auto notPassenger=branch(0x85); // passenger station
    for (auto name:kTrainStationNames) {
        emit({0x81,0xB8,0x00,0x02,0x00,0x00}); emitBytes(&name,4); toFamily(0x84,kRail);
    }
    emit({0xE9,0,0,0,0}); auto passengerRejected=out.size-4;
    auto pedestrian=out.size;
    emit({0x81,0xF9,kPedestrianBridgeType,0x00,0x00,0x00}); auto notPedestrian=branch(0x85); // accessories
    std::size_t pedestrianRejected[sizeof(kPedestrianInfrastructureNames)/sizeof(kPedestrianInfrastructureNames[0])]{};
    std::size_t pedestrianCount=0;
    for (auto name:kPedestrianInfrastructureNames) {
        emit({0x81,0xB8,0x00,0x02,0x00,0x00}); emitBytes(&name,4); pedestrianRejected[pedestrianCount++]=branch(0x84);
    }
    emit({0xE9,0,0,0,0}); auto accessoryAdmitted=out.size-4;
    auto buildingRejected=out.size; // diagnostics: count the building type when it is small
    emit({0x81,0xF9}); emitBytes(&kDiagBuildingTypes,4); exitIf(0x83); // unsigned: negatives too
    std::uint64_t buildingDiag=diagnostics+8ull*kDiagBuildingBase;
    emit({0x48,0xB8}); emitBytes(&buildingDiag,8); // movabs rax, building slots
    emit({0xF0,0x48,0xFF,0x04,0xC8}); // lock inc qword [rax+rcx*8]
    exitJump();

    // Family labels: select the family counter in RCX, then the common admitted path.
    std::size_t familyLabel[kFamilies]{};
    std::size_t toAdmitted[kFamilies]{};
    for (int f=0;f<kFamilies;++f) {
        familyLabel[f]=out.size;
        std::uint64_t at=counters+8ull*static_cast<unsigned>(f);
        emit({0x48,0xB9}); emitBytes(&at,8); // movabs rcx, &counters[f]
        emit({0xE9,0,0,0,0}); toAdmitted[f]=out.size-4;
    }

    auto admitted=out.size;
    emit({0x8B,0x87,0x2C,0x06,0x00,0x00}); // configured site worker limit
    emit({0x85,0xC0}); exitIf(0x8E);
    emit({0x3D,0x88,0x13,0x00,0x00}); exitIf(0x87); // sanity: <=5000
    emit({0x3D}); emitBytes(&virtualWorkers,4);
    auto capped=branch(0x8E);
    emit({0xB8}); emitBytes(&virtualWorkers,4);
    auto apply=out.size;
    emit({0x8B,0xF0}); // esi=virtual count (local, never a citizen list)
    emit({0xF3,0x0F,0x2A,0xFE}); // cvtsi2ss xmm7,esi; preserve upper lanes
    emit({0xF0,0x48,0xFF,0x01}); // lock inc qword [rcx]
    std::size_t notRenovation=0;
    if (renovation) {             // renovations are also counted on their own
        emit({0x80,0xBD,0x40,0x02,0x00,0x00,0x00}); notRenovation=branch(0x84);
        std::uint64_t at=counters+8ull*kRenovationCounter;
        emit({0x48,0xB8}); emitBytes(&at,8); // movabs rax, &counters[kRenovationCounter]
        emit({0xF0,0x48,0xFF,0x00});         // lock inc qword [rax]
    }
    auto restore=out.size;
    if (renovation) resolve(notRenovation,restore);
    emit({0x59,0x58,0x9D});
    emitBytes(kOriginal,sizeof(kOriginal));
    std::uint8_t jump[5];
    if (!RelativeJump(jump,address+out.size,continuation)) return false;
    emitBytes(jump,5);
    if (overflow) return false;

    resolve(buildingPath,building);
    resolve(notPassenger,pedestrian);
    resolve(notPedestrian,buildingRejected);
    for (std::size_t i=0;i<pedestrianCount;++i) resolve(pedestrianRejected[i],buildingRejected);
    if (familyCount<kMaxFixups) familyJumps[familyCount++]={accessoryAdmitted,kDecoration}; else overflow=true;
    resolve(passengerRejected,buildingRejected);
    for (std::size_t i=0;i<familyCount;++i) resolve(familyJumps[i].operand,familyLabel[familyJumps[i].family]);
    for (int f=0;f<kFamilies;++f) resolve(toAdmitted[f],admitted);
    resolve(capped,apply);
    for (std::size_t i=0;i<exitCount;++i) resolve(exits[i],restore);
    return !overflow && out.size<=kCapacity;
}
}
