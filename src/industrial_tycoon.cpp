// SPDX-License-Identifier: GPL-3.0-or-later
#include "../vendor/TesmioLoader/src/tesmio_plugin.h"
#include "labor_stub.h"
#include "fire_stub.h"
#include "work_stub.h"
#include "repair_stub.h"
#include "plant_sources.h"

// The single version string: plugin info and the startup log both use it.
#define INDUSTRIAL_VERSION "0.16.0"

alignas(8) static volatile LONG64 g_hits=0;
alignas(8) static volatile LONG64 g_fireTicks=0;
alignas(8) static volatile LONG64 g_repairTicks=0;
alignas(8) static volatile LONG64 g_workTicks[industrial::work::kSiteCount]{};
static volatile LONG g_stopping=0;
static bool g_fireWanted=true, g_fireValid=false, g_fireInstalled=false;
static bool g_repairWanted=true, g_repairValid=false, g_repairInstalled=false;
static bool g_plantsWanted=true, g_plantsValid=false, g_plantsInstalled=false;
static bool g_workWanted[industrial::work::kSiteCount], g_workValid[industrial::work::kSiteCount]{},
            g_workInstalled[industrial::work::kSiteCount]{};
static int g_fireCrew=4;

static DWORD WINAPI ReportActivity(void*) {
    LONG64 reported=0,reportedFire=0,reportedRepair=0,reportedWork[industrial::work::kSiteCount]{};
    while (InterlockedCompareExchange(&g_stopping,0,0)==0) {
        Sleep(5000);
        if (InterlockedCompareExchange(&g_stopping,0,0)) break;
        LONG64 hits=InterlockedCompareExchange64(&g_hits,0,0);
        if (hits!=reported) {
            Logf("industrial_tycoon factories/mines/production-lines/scrapyards/power-plants/water-treatment labor substitutions=%lld (citizens unchanged)",hits);
            reported=hits;
        }
        LONG64 fire=InterlockedCompareExchange64(&g_fireTicks,0,0);
        if (fire!=reportedFire) {
            Logf("industrial_tycoon fire service: extinguishing ticks with a virtual crew=%lld",fire);
            reportedFire=fire;
        }
        LONG64 repair=InterlockedCompareExchange64(&g_repairTicks,0,0);
        if (repair!=reportedRepair) {
            Logf("industrial_tycoon repair station: repair ticks with a virtual staff=%lld",repair);
            reportedRepair=repair;
        }
        for (int i=0;i<industrial::work::kSiteCount;++i) {
            LONG64 work=InterlockedCompareExchange64(&g_workTicks[i],0,0);
            if (work==reportedWork[i]) continue;
            Logf("industrial_tycoon %s: work ticks with a virtual staff=%lld",industrial::work::kSites[i].label,work);
            reportedWork[i]=work;
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
    if (!VirtualProtect(at,n,oldProtection,&unused))
        Logf("industrial_tycoon warning: could not restore code page protection");
    return true;
}

static bool FireContextMatches() {
    namespace f=industrial::fire;
    return g_exeSize>f::kDispatchContextAt+sizeof(f::kDispatchContext) &&
        !std::memcmp(g_exeBase+f::kDispatchContextAt,f::kDispatchContext,sizeof(f::kDispatchContext)) &&
        !std::memcmp(g_exeBase+f::kExtinguishContextAt,f::kExtinguishContext,sizeof(f::kExtinguishContext)) &&
        !std::memcmp(g_exeBase+f::kExtinguishVirtual,f::kVirtualContext,sizeof(f::kVirtualContext));
}

// Optional module: a failure here leaves the labor patch installed and the fire code original.
static void InstallFireService() {
    namespace f=industrial::fire;
    if (!g_fireWanted) { Logf("industrial_tycoon fire service disabled in INI"); return; }
    if (!g_fireValid || !FireContextMatches()) {
        Logf("industrial_tycoon fire service skipped: fire instructions differ"); return;
    }
    BYTE* site=g_exeBase+f::kExtinguishSite;
    BYTE* cave=AllocNear(site,f::kCapacity);
    if (!cave) { Logf("industrial_tycoon fire service skipped: no nearby code memory"); return; }
    f::Stub stub;
    BYTE patch[sizeof(f::kExtinguishOriginal)]={0x90,0x90,0x90,0x90,0x90,0x90,0x90};
    if (!f::BuildExtinguishStub(stub,reinterpret_cast<std::uintptr_t>(cave),
            reinterpret_cast<std::uintptr_t>(g_exeBase+f::kExtinguishNormal),
            reinterpret_cast<std::uintptr_t>(g_exeBase+f::kExtinguishVirtual),
            reinterpret_cast<std::uintptr_t>(&g_fireTicks),g_fireCrew) ||
        !industrial::RelativeJump(patch,reinterpret_cast<std::uintptr_t>(site),reinterpret_cast<std::uintptr_t>(cave))) {
        VirtualFree(cave,0,MEM_RELEASE);
        Logf("industrial_tycoon fire service skipped: stub does not fit or jump is out of range"); return;
    }
    std::memcpy(cave,stub.bytes,stub.size);
    FlushInstructionCache(GetCurrentProcess(),cave,stub.size);
    // Extinguishing first, so dispatched trucks can always work once they arrive.
    if (!WriteCode(site,patch,sizeof(patch))) {
        VirtualFree(cave,0,MEM_RELEASE);
        Logf("industrial_tycoon fire service skipped: unable to write extinguishing patch"); return;
    }
    if (!WriteCode(g_exeBase+f::kDispatchSite,f::kDispatchPatched,sizeof(f::kDispatchPatched))) {
        Logf("industrial_tycoon fire service partial: extinguishing installed, dispatch left original"); return;
    }
    g_fireInstalled=true;
    Logf("industrial_tycoon fire service INSTALLED: trucks dispatch without firefighters; virtual crew = truck fire skill (default %d)",g_fireCrew);
}

static bool RepairContextMatches() {
    namespace r=industrial::repair;
    return g_exeSize>r::kRateFactorContextAt+sizeof(r::kRateFactorContext) &&
        !std::memcmp(g_exeBase+r::kRateContextAt,r::kRateContext,sizeof(r::kRateContext)) &&
        !std::memcmp(g_exeBase+r::kRateFactorContextAt,r::kRateFactorContext,sizeof(r::kRateFactorContext)) &&
        !std::memcmp(g_exeBase+r::kTickContextAt,r::kTickContext,sizeof(r::kTickContext));
}

// Optional module: a failure here leaves the other patches installed and the repair code original.
static void InstallRepairStation() {
    namespace r=industrial::repair;
    if (!g_repairWanted) { Logf("industrial_tycoon repair station disabled in INI"); return; }
    if (!g_repairValid || !RepairContextMatches()) {
        Logf("industrial_tycoon repair station skipped: repair instructions differ"); return;
    }
    BYTE* site=g_exeBase+r::kRateSite;
    BYTE* cave=AllocNear(site,r::kCapacity);
    if (!cave) { Logf("industrial_tycoon repair station skipped: no nearby code memory"); return; }
    r::Stub stub;
    BYTE patch[sizeof(r::kRateOriginal)]={0x90,0x90,0x90,0x90,0x90,0x90};
    if (!r::BuildRateStub(stub,reinterpret_cast<std::uintptr_t>(cave),
            reinterpret_cast<std::uintptr_t>(g_exeBase+r::kRateLoop),
            reinterpret_cast<std::uintptr_t>(g_exeBase+r::kRateFactor),
            reinterpret_cast<std::uintptr_t>(g_exeBase+r::kRateDone),
            reinterpret_cast<std::uintptr_t>(&g_repairTicks)) ||
        !industrial::RelativeJump(patch,reinterpret_cast<std::uintptr_t>(site),reinterpret_cast<std::uintptr_t>(cave))) {
        VirtualFree(cave,0,MEM_RELEASE);
        Logf("industrial_tycoon repair station skipped: stub does not fit or jump is out of range"); return;
    }
    std::memcpy(cave,stub.bytes,stub.size);
    FlushInstructionCache(GetCurrentProcess(),cave,stub.size);
    // Rate first: with the default station flag the job loop already runs without people.
    if (!WriteCode(site,patch,sizeof(patch))) {
        VirtualFree(cave,0,MEM_RELEASE);
        Logf("industrial_tycoon repair station skipped: unable to write rate patch"); return;
    }
    if (!WriteCode(g_exeBase+r::kTickSite,r::kTickPatched,sizeof(r::kTickPatched))) {
        Logf("industrial_tycoon repair station partial: rate installed, job loop check left original"); return;
    }
    g_repairInstalled=true;
    Logf("industrial_tycoon repair station INSTALLED: repairs progress with a virtual staff = workers needed when nobody is there");
}

static bool PlantsContextMatches() {
    for (const auto& s:industrial::plants::kSites)
        if (g_exeSize<=s.contextAt+s.contextSize || std::memcmp(g_exeBase+s.contextAt,s.context,s.contextSize)) return false;
    return true;
}

// Optional module: a failure here leaves the other patches installed and the office code original.
static void InstallPlantSources() {
    namespace p=industrial::plants;
    if (!g_plantsWanted) { Logf("industrial_tycoon plant sources disabled in INI"); return; }
    if (!g_plantsValid || !PlantsContextMatches()) {
        Logf("industrial_tycoon plant sources skipped: construction office instructions differ"); return;
    }
    for (int i=0;i<p::kSiteCount;++i) {
        const auto& s=p::kSites[i];
        if (WriteCode(g_exeBase+s.site,s.patched,s.size)) continue;
        if (i==0) { Logf("industrial_tycoon plant sources skipped: unable to write %s patch",s.label); return; }
        Logf("industrial_tycoon plant sources partial: %s left original",s.label); return;
    }
    g_plantsInstalled=true;
    Logf("industrial_tycoon plant sources INSTALLED: concrete and asphalt plants with their inputs supply construction offices without staff (both office ticks)");
}

static bool WorkContextMatches(const industrial::work::Site& s) {
    return g_exeSize>s.contextAt+s.contextSize && !std::memcmp(g_exeBase+s.contextAt,s.context,s.contextSize);
}

// Optional modules, one per site: a failure leaves the other patches installed and that site original.
static void InstallWorkSites() {
    namespace w=industrial::work;
    for (int i=0;i<w::kSiteCount;++i) {
        const auto& s=w::kSites[i];
        if (!g_workWanted[i]) { Logf("industrial_tycoon %s disabled in INI",s.label); continue; }
        if (!g_workValid[i] || !WorkContextMatches(s)) { Logf("industrial_tycoon %s skipped: instructions differ",s.label); continue; }
        BYTE* site=g_exeBase+s.site;
        BYTE* cave=AllocNear(site,w::kCapacity);
        if (!cave) { Logf("industrial_tycoon %s skipped: no nearby code memory",s.label); continue; }
        w::Stub stub;
        BYTE patch[sizeof(s.original)]={0x90,0x90,0x90,0x90,0x90,0x90,0x90};
        if (!w::BuildWorkStub(stub,s,reinterpret_cast<std::uintptr_t>(cave),
                reinterpret_cast<std::uintptr_t>(g_exeBase+s.continuation()),reinterpret_cast<std::uintptr_t>(&g_workTicks[i])) ||
            !industrial::RelativeJump(patch,reinterpret_cast<std::uintptr_t>(site),reinterpret_cast<std::uintptr_t>(cave))) {
            VirtualFree(cave,0,MEM_RELEASE);
            Logf("industrial_tycoon %s skipped: stub does not fit or jump is out of range",s.label); continue;
        }
        std::memcpy(cave,stub.bytes,stub.size);
        FlushInstructionCache(GetCurrentProcess(),cave,stub.size);
        if (!WriteCode(site,patch,sizeof(patch))) {
            VirtualFree(cave,0,MEM_RELEASE);
            Logf("industrial_tycoon %s skipped: unable to write patch",s.label); continue;
        }
        g_workInstalled[i]=true;
        Logf("industrial_tycoon %s INSTALLED: works with its full staff (virtual) when nobody is there",s.label);
    }
}

extern "C" __declspec(dllexport) unsigned TsmPluginApiVersion() { return TSM_API_VERSION; }

extern "C" __declspec(dllexport) int TsmPluginInit(const TsmHost* host,TsmPluginInfo* info) {
    TsmBind(host);
    info->name="Industrial Tycoon - industries without staff (factories incl. waste chain, mines, production lines, scrapyards, power plants, water and sewage treatment, containers, fields, control towers, water wells), fire service, repair stations and concrete/asphalt plants as office sources";
    info->version=INDUSTRIAL_VERSION "-experimental";
    if (!H->configInt("plugins\\industrial_tycoon.ini","industrial_tycoon","enabled",1)) {
        Logf("industrial_tycoon disabled"); return 1;
    }
    g_fireWanted=H->configInt("plugins\\industrial_tycoon.ini","industrial_tycoon","fire_service",1)!=0;
    g_fireCrew=H->configInt("plugins\\industrial_tycoon.ini","industrial_tycoon","virtual_firefighters",4);
    if (g_fireCrew<1 || g_fireCrew>20) g_fireCrew=4;
    g_repairWanted=H->configInt("plugins\\industrial_tycoon.ini","industrial_tycoon","repair_station",1)!=0;
    g_plantsWanted=H->configInt("plugins\\industrial_tycoon.ini","industrial_tycoon","plant_sources",1)!=0;
    for (int i=0;i<industrial::work::kSiteCount;++i)
        g_workWanted[i]=H->configInt("plugins\\industrial_tycoon.ini","industrial_tycoon",industrial::work::kSites[i].key,1)!=0;
    if (!ReadablePtr(g_exeBase,0x1000)) return 1;
    auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(g_exeBase);
    if (dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<0 || dos->e_lfanew>0x800) return 1;
    auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(g_exeBase+dos->e_lfanew);
    if (nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64 ||
        nt->FileHeader.TimeDateStamp!=industrial::kTimestamp ||
        g_exeSize<industrial::kLaborSite+sizeof(industrial::kContext)) {
        Logf("industrial_tycoon refused: unsupported executable build"); return 1;
    }
    if (std::memcmp(g_exeBase+industrial::kLaborFunction,industrial::kFunctionHead,
                    sizeof(industrial::kFunctionHead)) ||
        std::memcmp(g_exeBase+industrial::kLaborSite,industrial::kContext,
                    sizeof(industrial::kContext))) {
        Logf("industrial_tycoon refused: labor instructions differ or another plugin patched them");
        return 1;
    }
    g_fireValid=FireContextMatches();
    g_repairValid=RepairContextMatches();
    g_plantsValid=PlantsContextMatches();
    for (int i=0;i<industrial::work::kSiteCount;++i) g_workValid[i]=WorkContextMatches(industrial::work::kSites[i]);
    Logf("industrial_tycoon validated WRSR 1.1.1.9; scope=factories(type6, waste chain included), mines(type7), production lines(type40), scrapyards(type109), power-plants(type17), water/sewage treatment(type97/98), ships(subtype19), rail(subtype26), any name; fire service %s; repair station %s; plant sources %s; container facility %s; fields %s; control tower %s; water well %s",
         !g_fireWanted?"disabled":g_fireValid?"ready":"instructions differ",
         !g_repairWanted?"disabled":g_repairValid?"ready":"instructions differ",
         !g_plantsWanted?"disabled":g_plantsValid?"ready":"instructions differ",
         !g_workWanted[0]?"disabled":g_workValid[0]?"ready":"instructions differ",
         !g_workWanted[1]?"disabled":g_workValid[1]?"ready":"instructions differ",
         !g_workWanted[2]?"disabled":g_workValid[2]?"ready":"instructions differ",
         !g_workWanted[3]?"disabled":g_workValid[3]?"ready":"instructions differ");
    return 0;
}

extern "C" __declspec(dllexport) int TsmPluginStart() {
    BYTE* site=g_exeBase+industrial::kLaborSite;
    if (std::memcmp(site,industrial::kContext,sizeof(industrial::kContext))) {
        Logf("industrial_tycoon refused: patch site changed during initialization"); return 1;
    }
    BYTE* cave=AllocNear(site,industrial::kStubCapacity);
    if (!cave) { Logf("industrial_tycoon refused: no nearby code memory"); return 1; }
    industrial::Stub stub;
    std::uint8_t jump[5];
    if (!industrial::BuildStub(stub,reinterpret_cast<std::uintptr_t>(cave),
                              reinterpret_cast<std::uintptr_t>(site+5),
                              reinterpret_cast<std::uintptr_t>(&g_hits)) ||
        !industrial::RelativeJump(jump,reinterpret_cast<std::uintptr_t>(site),
                                  reinterpret_cast<std::uintptr_t>(cave))) {
        VirtualFree(cave,0,MEM_RELEASE);
        Logf("industrial_tycoon refused: jump is out of range"); return 1;
    }
    std::memcpy(cave,stub.bytes,stub.size);
    FlushInstructionCache(GetCurrentProcess(),cave,stub.size);
    if (!WriteCode(site,jump,sizeof(jump))) {
        VirtualFree(cave,0,MEM_RELEASE);
        Logf("industrial_tycoon refused: unable to write runtime patch"); return 1;
    }
    InstallFireService();
    InstallRepairStation();
    InstallPlantSources();
    InstallWorkSites();
    // Once the code is patched this DLL must stay loaded, including if logging fails.
    HANDLE reporter=CreateThread(nullptr,0,ReportActivity,nullptr,0,nullptr);
    if (reporter) CloseHandle(reporter);
    else Logf("industrial_tycoon activity logging unavailable; patch remains installed");
    Logf("industrial_tycoon INSTALLED v" INDUSTRIAL_VERSION " factories+mines+production-lines+scrapyards+power-plants+water-treatment labor patch%s%s%s%s%s%s%s; waste chain, water management and control towers gameplay validation pending",
         g_fireInstalled?" + fire service":"",g_repairInstalled?" + repair station":"",g_plantsInstalled?" + plant sources":"",g_workInstalled[0]?" + container facility":"",g_workInstalled[1]?" + fields":"",
         g_workInstalled[2]?" + control tower":"",g_workInstalled[3]?" + water well":"");
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE,DWORD reason,LPVOID) {
    if (reason==DLL_PROCESS_DETACH) InterlockedExchange(&g_stopping,1);
    return TRUE;
}
