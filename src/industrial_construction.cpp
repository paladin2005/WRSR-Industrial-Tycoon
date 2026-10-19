// SPDX-License-Identifier: GPL-3.0-or-later
#include "../vendor/TesmioLoader/src/tesmio_plugin.h"
#include "construction_stub.h"
#include "rail_stub.h"
#include "asphalt_stub.h"
#include "terraform_stub.h"

// The single version string: plugin info and the startup log both use it.
#define CONSTRUCTION_VERSION "0.20.0"

namespace ic=industrial::construction;
alignas(8) static volatile LONG64 g_counts[ic::kCounterSlots]{};
alignas(8) static volatile LONG64 g_diag[ic::kDiagTableSlots]{};
alignas(8) static volatile LONG64 g_trackTicks=0;
alignas(8) static volatile LONG64 g_asphaltPlans=0;
alignas(8) static volatile LONG64 g_terraformTicks=0;
static volatile LONG g_stopping=0;
static int g_workers=50;
static bool g_renovation=true;
static bool g_railWanted=true, g_railValid=false, g_railInstalled=false;
static int g_trackCrew=20;
static bool g_asphaltValid=false, g_asphaltInstalled=false;
static bool g_terraformWanted=true, g_terraformValid=false, g_terraformInstalled=false;
static int g_terraformWorkers=100;

static DWORD WINAPI ReportActivity(void*) {
    LONG64 reported[ic::kCounterSlots]{};
    LONG64 reportedTrack=0, reportedAsphalt=0, reportedTerraform=0;
    bool diagnosed[ic::kDiagTableSlots]{};
    while (!InterlockedCompareExchange(&g_stopping,0,0)) {
        Sleep(5000);
        if (InterlockedCompareExchange(&g_stopping,0,0)) break;
        LONG64 now[ic::kCounterSlots];
        bool changed=false;
        for (int i=0;i<ic::kCounterSlots;++i) {
            now[i]=InterlockedCompareExchange64(&g_counts[i],0,0);
            changed|=now[i]!=reported[i];
        }
        if (changed) {
            Logf("industrial_construction labor substitutions storage/farm=%lld electric-buildings=%lld power-lines=%lld offices=%lld pipes=%lld roads=%lld road-buildings=%lld services=%lld ships=%lld rail-buildings=%lld cargo=%lld conveyors=%lld decorations=%lld industries=%lld airports=%lld border=%lld waste=%lld water=%lld; renovations=%lld (materials use original game code)",
                 now[ic::kStorageFarm],now[ic::kElectricBuilding],now[ic::kPowerLine],now[ic::kOffice],now[ic::kPipe],
                 now[ic::kRoadInfra],now[ic::kRoadBuilding],now[ic::kService],now[ic::kShip],now[ic::kRail],now[ic::kCargo],now[ic::kConveyor],now[ic::kDecoration],now[ic::kIndustry],
                 now[ic::kAirport],now[ic::kBorder],now[ic::kWaste],now[ic::kWater],now[ic::kRenovationCounter]);
            for (int i=0;i<ic::kCounterSlots;++i) reported[i]=now[i];
        }
        LONG64 track=InterlockedCompareExchange64(&g_trackTicks,0,0);
        if (track!=reportedTrack) {
            Logf("industrial_construction rail track: work ticks with a virtual crew=%lld",track);
            reportedTrack=track;
        }
        LONG64 asphalt=InterlockedCompareExchange64(&g_asphaltPlans,0,0);
        if (asphalt!=reportedAsphalt) {
            Logf("industrial_construction asphalt: deliveries planned for virtual workers=%lld",asphalt);
            reportedAsphalt=asphalt;
        }
        LONG64 terraform=InterlockedCompareExchange64(&g_terraformTicks,0,0);
        if (terraform!=reportedTerraform) {
            Logf("industrial_construction terraforming: tool updates with virtual workers=%lld",terraform);
            reportedTerraform=terraform;
        }
        // Each rejected (road type, sublevel) pair is reported once, to identify stalled sites.
        for (int i=0;i<ic::kDiagSlots;++i) {
            if (diagnosed[i] || !InterlockedCompareExchange64(&g_diag[i],0,0)) continue;
            diagnosed[i]=true;
            Logf("industrial_construction diagnostic: zero-worker infrastructure site not admitted: road type %d sublevel %d",
                 i/ic::kDiagSublevels,i%ic::kDiagSublevels);
        }
        // Each rejected building type is reported once as well.
        for (int i=ic::kDiagBuildingBase;i<ic::kDiagTableSlots;++i) {
            if (diagnosed[i] || !InterlockedCompareExchange64(&g_diag[i],0,0)) continue;
            diagnosed[i]=true;
            Logf("industrial_construction diagnostic: zero-worker building site not admitted: building type %d",i-ic::kDiagBuildingBase);
        }
    }
    return 0;
}

static bool WriteCode(BYTE* at,const void* bytes,std::size_t n) {
    DWORD oldProtection;
    if (!VirtualProtect(at,n,PAGE_EXECUTE_READWRITE,&oldProtection)) return false;
    std::memcpy(at,bytes,n);
    FlushInstructionCache(GetCurrentProcess(),at,n);
    DWORD unused;
    if (!VirtualProtect(at,n,oldProtection,&unused)) Logf("industrial_construction warning: code page protection restoration failed");
    return true;
}

static bool RailContextMatches() {
    namespace r=industrial::rail;
    return g_exeSize>r::kTrackContextAt+sizeof(r::kTrackContext) && g_exeSize>r::kRefuelContextAt+sizeof(r::kRefuelContext) &&
        !std::memcmp(g_exeBase+r::kTrackContextAt,r::kTrackContext,sizeof(r::kTrackContext)) &&
        !std::memcmp(g_exeBase+r::kRefuelContextAt,r::kRefuelContext,sizeof(r::kRefuelContext));
}

// Optional module: a failure here leaves the construction patch installed and the rail code original.
static void InstallRailConstruction() {
    namespace r=industrial::rail;
    if (!g_railWanted) { Logf("industrial_construction rail construction disabled in INI"); return; }
    if (!g_railValid || !RailContextMatches()) {
        Logf("industrial_construction rail construction skipped: rail instructions differ"); return;
    }
    BYTE* site=g_exeBase+r::kTrackSite;
    BYTE* cave=AllocNear(site,r::kCapacity);
    if (!cave) { Logf("industrial_construction rail construction skipped: no nearby code memory"); return; }
    r::Stub stub;
    BYTE patch[sizeof(r::kTrackOriginal)]={0x90,0x90,0x90,0x90,0x90,0x90,0x90};
    if (!r::BuildTrackStub(stub,reinterpret_cast<std::uintptr_t>(cave),
            reinterpret_cast<std::uintptr_t>(g_exeBase+r::kTrackContinuation),
            reinterpret_cast<std::uintptr_t>(&g_trackTicks),g_trackCrew) ||
        !industrial::RelativeJump(patch,reinterpret_cast<std::uintptr_t>(site),reinterpret_cast<std::uintptr_t>(cave))) {
        VirtualFree(cave,0,MEM_RELEASE);
        Logf("industrial_construction rail construction skipped: stub does not fit or jump is out of range"); return;
    }
    std::memcpy(cave,stub.bytes,stub.size);
    FlushInstructionCache(GetCurrentProcess(),cave,stub.size);
    // Track crew first, so a refueled vehicle can always work once it reaches a site.
    if (!WriteCode(site,patch,sizeof(patch))) {
        VirtualFree(cave,0,MEM_RELEASE);
        Logf("industrial_construction rail construction skipped: unable to write track crew patch"); return;
    }
    if (!WriteCode(g_exeBase+r::kRefuelSite,r::kRefuelPatched,sizeof(r::kRefuelPatched))) {
        Logf("industrial_construction rail construction partial: track crew installed, refueling left original"); return;
    }
    g_railInstalled=true;
    Logf("industrial_construction rail construction INSTALLED: rail office vehicles work and refuel without staff; virtual crew = vehicle capacity scaled by office staff (fallback %d)",g_trackCrew);
}

static bool AsphaltContextMatches() {
    namespace a=industrial::asphalt;
    return g_exeSize>a::kContextAt+sizeof(a::kContext) &&
        !std::memcmp(g_exeBase+a::kContextAt,a::kContext,sizeof(a::kContext));
}

// Part of the construction patch: without it, offices never plan asphalt for road sites that
// only virtual workers will lay. A failure here leaves the planner original.
static void InstallAsphaltDelivery() {
    namespace a=industrial::asphalt;
    if (!g_asphaltValid || !AsphaltContextMatches()) {
        Logf("industrial_construction asphalt delivery skipped: office planner instructions differ"); return;
    }
    BYTE* site=g_exeBase+a::kSite;
    BYTE* cave=AllocNear(site,a::kCapacity);
    if (!cave) { Logf("industrial_construction asphalt delivery skipped: no nearby code memory"); return; }
    a::Stub stub;
    BYTE patch[sizeof(a::kOriginal)]={0x90,0x90,0x90,0x90,0x90,0x90};
    if (!a::BuildStub(stub,reinterpret_cast<std::uintptr_t>(cave),reinterpret_cast<std::uintptr_t>(g_exeBase+a::kAdmit),
            reinterpret_cast<std::uintptr_t>(g_exeBase+a::kReject),reinterpret_cast<std::uintptr_t>(&g_asphaltPlans)) ||
        !industrial::RelativeJump(patch,reinterpret_cast<std::uintptr_t>(site),reinterpret_cast<std::uintptr_t>(cave))) {
        VirtualFree(cave,0,MEM_RELEASE);
        Logf("industrial_construction asphalt delivery skipped: stub does not fit or jump is out of range"); return;
    }
    std::memcpy(cave,stub.bytes,stub.size);
    FlushInstructionCache(GetCurrentProcess(),cave,stub.size);
    if (!WriteCode(site,patch,sizeof(patch))) {
        VirtualFree(cave,0,MEM_RELEASE);
        Logf("industrial_construction asphalt delivery skipped: unable to write planner patch"); return;
    }
    g_asphaltInstalled=true;
    Logf("industrial_construction asphalt delivery INSTALLED: offices plan asphalt for road sites with virtual workers when no asphalt paver or real workers are available (stub %u/%u bytes)",
         static_cast<unsigned>(stub.size),static_cast<unsigned>(a::kCapacity));
}

static bool TerraformContextMatches() {
    namespace t=industrial::terraform;
    return g_exeSize>t::kContextAt+sizeof(t::kContext) &&
        !std::memcmp(g_exeBase+t::kContextAt,t::kContext,sizeof(t::kContext));
}

// Optional module: a failure here leaves the terrain planner original.
static void InstallTerraforming() {
    namespace t=industrial::terraform;
    if (!g_terraformWanted) { Logf("industrial_construction terraforming disabled in INI"); return; }
    if (!g_terraformValid || !TerraformContextMatches()) {
        Logf("industrial_construction terraforming skipped: terrain planner instructions differ"); return;
    }
    BYTE* site=g_exeBase+t::kSite;
    BYTE* cave=AllocNear(site,t::kCapacity);
    if (!cave) { Logf("industrial_construction terraforming skipped: no nearby code memory"); return; }
    t::Stub stub;
    BYTE patch[sizeof(t::kOriginal)]{};
    if (!t::BuildStub(stub,reinterpret_cast<std::uintptr_t>(cave),reinterpret_cast<std::uintptr_t>(g_exeBase+t::kContinuation),
            reinterpret_cast<std::uintptr_t>(&g_terraformTicks),g_terraformWorkers) ||
        !industrial::RelativeJump(patch,reinterpret_cast<std::uintptr_t>(site),reinterpret_cast<std::uintptr_t>(cave))) {
        VirtualFree(cave,0,MEM_RELEASE);
        Logf("industrial_construction terraforming skipped: stub does not fit or jump is out of range"); return;
    }
    std::memcpy(cave,stub.bytes,stub.size);
    FlushInstructionCache(GetCurrentProcess(),cave,stub.size);
    if (!WriteCode(site,patch,sizeof(patch))) {
        VirtualFree(cave,0,MEM_RELEASE);
        Logf("industrial_construction terraforming skipped: unable to write planner patch"); return;
    }
    g_terraformInstalled=true;
    Logf("industrial_construction terraforming INSTALLED: terrain tools work by hand with up to %d virtual workers when no excavator, bulldozer or unemployed citizen is available (stub %u/%u bytes)",
         g_terraformWorkers,static_cast<unsigned>(stub.size),static_cast<unsigned>(t::kCapacity));
}

extern "C" __declspec(dllexport) unsigned TsmPluginApiVersion() { return TSM_API_VERSION; }
extern "C" __declspec(dllexport) int TsmPluginInit(const TsmHost* host,TsmPluginInfo* info) {
    TsmBind(host);
    info->name="Industrial Tycoon - construction without workers (storages, farms, electricity, offices, roads, fire stations, ships, rail, cargo, conveyors, pipes, decorations, industries)";
    info->version=CONSTRUCTION_VERSION "-construction-experimental";
    if (!H->configInt("plugins\\industrial_construction.ini","industrial_construction","enabled",1)) return 1;
    g_workers=H->configInt("plugins\\industrial_construction.ini","industrial_construction","virtual_workers",50);
    if (g_workers<1 || g_workers>500 || !ReadablePtr(g_exeBase,0x1000)) return 1;
    g_renovation=H->configInt("plugins\\industrial_construction.ini","industrial_construction","renovation",1)!=0;
    g_railWanted=H->configInt("plugins\\industrial_construction.ini","industrial_construction","rail_construction",1)!=0;
    g_trackCrew=H->configInt("plugins\\industrial_construction.ini","industrial_construction","virtual_track_crew",20);
    if (g_trackCrew<1 || g_trackCrew>200) g_trackCrew=20;
    g_terraformWanted=H->configInt("plugins\\industrial_construction.ini","industrial_construction","terraforming",1)!=0;
    g_terraformWorkers=H->configInt("plugins\\industrial_construction.ini","industrial_construction","virtual_terraform_workers",100);
    if (g_terraformWorkers<1 || g_terraformWorkers>1000) g_terraformWorkers=100;
    auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(g_exeBase);
    if (dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<0 || dos->e_lfanew>0x800) return 1;
    auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(g_exeBase+dos->e_lfanew);
    if (nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64 ||
        nt->FileHeader.TimeDateStamp!=industrial::kTimestamp ||
        g_exeSize<ic::kSite+sizeof(ic::kContext)) {
        Logf("industrial_construction refused: unsupported executable"); return 1;
    }
    if (std::memcmp(g_exeBase+ic::kFunction,ic::kHead,sizeof(ic::kHead)) ||
        std::memcmp(g_exeBase+ic::kSite,ic::kContext,sizeof(ic::kContext))) {
        Logf("industrial_construction refused: construction instructions differ"); return 1;
    }
    g_railValid=RailContextMatches();
    g_asphaltValid=AsphaltContextMatches();
    g_terraformValid=TerraformContextMatches();
    Logf("industrial_construction validated WRSR 1.1.1.9; storages(type5, any name) + farms(type9)"
         " + electric buildings(type17/18/19/31/32/35/60) + power lines(road type 8/9) + pipes(road type 7, oil and steam)"
         " + construction/demolition offices(type12/106) + roads(road type 0/3/4) + road buildings(type0/14/34/38, repair stations 108) + fire stations(type26)"
         " + ships(subtype19, ship docks type42) + rail buildings(type15/28/52, subtype26, train stations 1802/1804)"
         " + cargo(type43/44/45, subtype32 cableway, road type 5/12) + conveyors(road type 6, conveyor and bulk; engines type11)"
         " + decorations(monuments type33, accessories type80 but pedestrian/metro, fences road type 15)"
         " + industries(fields type8, factories type6, mines type7, production lines type40, scrapyards type109, any name)"
         " + airports(type46/47/48, road type 13) + border(customs type20, foreign pipeline type107)"
         " + waste(technical services type49, trash containers type105) + water(type90/91/92/93/94/95/97/98, road type 11/16); virtual workers <=%d; renovation %s; rail construction %s; asphalt delivery %s; terraforming %s",
         g_workers,g_renovation?"enabled":"disabled",!g_railWanted?"disabled":g_railValid?"ready":"instructions differ",
         g_asphaltValid?"ready":"instructions differ",!g_terraformWanted?"disabled":g_terraformValid?"ready":"instructions differ");
    return 0;
}
extern "C" __declspec(dllexport) int TsmPluginStart() {
    BYTE* site=g_exeBase+ic::kSite;
    if (std::memcmp(site,ic::kContext,sizeof(ic::kContext))) {
        Logf("industrial_construction refused: patch site changed during initialization"); return 1;
    }
    BYTE* cave=AllocNear(site,ic::kCapacity);
    if (!cave) { Logf("industrial_construction refused: no nearby code memory"); return 1; }
    ic::Stub stub;
    BYTE patch[sizeof(ic::kOriginal)]={0x90,0x90,0x90,0x90,0x90,0x90,0x90};
    if (!ic::BuildStub(stub,reinterpret_cast<std::uintptr_t>(cave),
        reinterpret_cast<std::uintptr_t>(site+sizeof(patch)),reinterpret_cast<std::uintptr_t>(&g_counts[0]),reinterpret_cast<std::uintptr_t>(&g_diag[0]),g_workers,g_renovation) ||
        !industrial::RelativeJump(patch,reinterpret_cast<std::uintptr_t>(site),reinterpret_cast<std::uintptr_t>(cave))) {
        VirtualFree(cave,0,MEM_RELEASE);
        Logf("industrial_construction refused: stub does not fit or jump is out of range"); return 1;
    }
    std::memcpy(cave,stub.bytes,stub.size);
    FlushInstructionCache(GetCurrentProcess(),cave,stub.size);
    if (!WriteCode(site,patch,sizeof(patch))) {
        VirtualFree(cave,0,MEM_RELEASE);
        Logf("industrial_construction refused: unable to write runtime patch"); return 1;
    }
    InstallRailConstruction();
    InstallAsphaltDelivery();
    InstallTerraforming();
    HANDLE reporter=CreateThread(nullptr,0,ReportActivity,nullptr,0,nullptr);
    if (reporter) CloseHandle(reporter);
    Logf("industrial_construction INSTALLED v" CONSTRUCTION_VERSION " storages+small-farm+electricity+offices+pipes+roads+road buildings+fire stations+ships+rail buildings+cargo+conveyors+decorations+industries+airports+border+waste+water%s%s%s%s (stub %u/%u bytes); renovation, water treatment and terraforming gameplay validation pending",
         g_renovation?" + renovation":"",g_railInstalled?" + rail construction":"",g_asphaltInstalled?" + asphalt delivery":"",g_terraformInstalled?" + terraforming":"",
         static_cast<unsigned>(stub.size),static_cast<unsigned>(ic::kCapacity));
    return 0;
}
BOOL WINAPI DllMain(HINSTANCE,DWORD reason,LPVOID) {
    if (reason==DLL_PROCESS_DETACH) InterlockedExchange(&g_stopping,1);
    return TRUE;
}
