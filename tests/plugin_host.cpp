// SPDX-License-Identifier: GPL-3.0-or-later
// Loads the actual DLL into a synthetic host. Never opens a game process.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <vector>
#include <cstring>
#include "../vendor/TesmioLoader/src/tesmio_api.h"
#include "../src/labor_stub.h"
#include "../src/construction_stub.h"
#include "../src/fire_stub.h"
#include "../src/rail_stub.h"
#include "../src/work_stub.h"
#include "../src/repair_stub.h"
#include "../src/plant_sources.h"

extern "C" void RunLaborStub(void*,void*,void*,std::uint64_t*);
extern "C" void RunConstructionStub(void*,void*,void*,std::uint64_t*);
extern "C" void RunFireStub(void*,void*,void*,std::uint64_t*);
extern "C" void RunTrackStub(void*,void*,void*,std::uint64_t*);
extern "C" void RunWorkStub(void*,void*,void*,std::uint64_t*);
extern "C" void RunWorkStub2(void*,void*,void*,std::uint64_t*);
extern "C" void RunRepairStub(void*,void*,void*,std::uint64_t*);
static BYTE* image;
static constexpr size_t imageSize=0x280000; // reaches the fire dispatch site 0x25373E
static bool enabled=true,allocationFails=false,invalidWorkers=false,fireDisabled=false,railDisabled=false,repairDisabled=false,plantsDisabled=false;
static const char* workDisabledKey="";
static unsigned allocations=0;
static void Log(const char* fmt,...) {
    va_list ap; va_start(ap,fmt); vprintf(fmt,ap); va_end(ap); puts("");
}
static int Readable(const void* p,size_t n) {
    auto a=reinterpret_cast<std::uintptr_t>(p);
    auto b=reinterpret_cast<std::uintptr_t>(image);
    return a>=b && n<=imageSize && a-b<=imageSize-n;
}
static BYTE* Allocate(BYTE*,size_t n) {
    // Both stubs fit in one 4 KiB slot; the real loader reserves a whole region.
    return !allocationFails && n<=0x1000?image+0x190000+0x1000*allocations++:nullptr;
}
static int Config(const char*,const char*,const char* key,int fallback) {
    if (!std::strcmp(key,"virtual_workers")) return invalidWorkers?0:fallback;
    if (!std::strcmp(key,"fire_service")) return fireDisabled?0:1;
    if (!std::strcmp(key,"virtual_firefighters")) return fallback;
    if (!std::strcmp(key,"repair_station")) return repairDisabled?0:1;
    if (!std::strcmp(key,"plant_sources")) return plantsDisabled?0:1;
    if (!std::strcmp(key,"rail_construction")) return railDisabled?0:1;
    for (const auto& s:industrial::work::kSites) if (!std::strcmp(key,s.key)) return std::strcmp(key,workDisabledKey)?1:0;
    if (!std::strcmp(key,"virtual_track_crew")) return fallback;
    return enabled?1:0;
}

int main(int argc,char** argv) {
    if (argc!=2) return 2;
    const char* mode=argv[1];
    bool construction=!std::strncmp(mode,"construction-",13);
    if (construction) mode+=13;
    auto site=construction?industrial::construction::kSite:industrial::kLaborSite;
    auto patchSize=construction?sizeof(industrial::construction::kOriginal):sizeof(industrial::kOriginal);
    image=static_cast<BYTE*>(VirtualAlloc(nullptr,imageSize,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
    if (!image) return 3;
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(image);
    dos->e_magic=IMAGE_DOS_SIGNATURE; dos->e_lfanew=0x100;
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(image+dos->e_lfanew);
    nt->Signature=IMAGE_NT_SIGNATURE;
    nt->FileHeader.Machine=IMAGE_FILE_MACHINE_AMD64;
    nt->FileHeader.TimeDateStamp=industrial::kTimestamp;
    std::memcpy(image+industrial::kLaborFunction,industrial::kFunctionHead,sizeof(industrial::kFunctionHead));
    std::memcpy(image+industrial::kLaborSite,industrial::kContext,sizeof(industrial::kContext));
    std::memcpy(image+industrial::construction::kFunction,industrial::construction::kHead,sizeof(industrial::construction::kHead));
    std::memcpy(image+industrial::construction::kSite,industrial::construction::kContext,sizeof(industrial::construction::kContext));
    namespace fs=industrial::fire;
    std::memcpy(image+fs::kDispatchContextAt,fs::kDispatchContext,sizeof(fs::kDispatchContext));
    std::memcpy(image+fs::kExtinguishContextAt,fs::kExtinguishContext,sizeof(fs::kExtinguishContext));
    std::memcpy(image+fs::kExtinguishVirtual,fs::kVirtualContext,sizeof(fs::kVirtualContext));
    if (!std::strcmp(mode,"fire-different-code")) image[fs::kDispatchContextAt+3]^=1;
    fireDisabled=!std::strcmp(mode,"fire-disabled");
    namespace rp=industrial::repair;
    std::memcpy(image+rp::kRateContextAt,rp::kRateContext,sizeof(rp::kRateContext));
    std::memcpy(image+rp::kRateFactorContextAt,rp::kRateFactorContext,sizeof(rp::kRateFactorContext));
    std::memcpy(image+rp::kTickContextAt,rp::kTickContext,sizeof(rp::kTickContext));
    if (!std::strcmp(mode,"repair-different-code")) image[rp::kTickContextAt+3]^=1;
    repairDisabled=!std::strcmp(mode,"repair-disabled");
    namespace ps=industrial::plants;
    for (const auto& s:ps::kSites) std::memcpy(image+s.contextAt,s.context,s.contextSize);
    if (!std::strcmp(mode,"plants-different-code")) image[ps::kSites[1].contextAt+3]^=1;
    plantsDisabled=!std::strcmp(mode,"plants-disabled");
    namespace rs=industrial::rail;
    std::memcpy(image+rs::kTrackContextAt,rs::kTrackContext,sizeof(rs::kTrackContext));
    std::memcpy(image+rs::kRefuelContextAt,rs::kRefuelContext,sizeof(rs::kRefuelContext));
    if (!std::strcmp(mode,"rail-different-code")) image[rs::kRefuelContextAt+1]^=1;
    railDisabled=!std::strcmp(mode,"rail-disabled");
    namespace ws=industrial::work;
    // Work sites: modes "<prefix>-different-code" and "<prefix>-disabled", prefix container or field.
    const char* workPrefix[ws::kSiteCount]={"container","field","tower","well"};
    for (int i=0;i<ws::kSiteCount;++i) {
        const auto& s=ws::kSites[i];
        std::memcpy(image+s.contextAt,s.context,s.contextSize);
        char name[64];
        std::snprintf(name,sizeof(name),"%s-different-code",workPrefix[i]);
        if (!std::strcmp(mode,name)) image[s.site+2]^=1;
        std::snprintf(name,sizeof(name),"%s-disabled",workPrefix[i]);
        if (!std::strcmp(mode,name)) workDisabledKey=s.key;
    }
    if (!std::strcmp(mode,"wrong-version")) nt->FileHeader.TimeDateStamp^=1;
    if (!std::strcmp(mode,"wrong-architecture")) nt->FileHeader.Machine=IMAGE_FILE_MACHINE_I386;
    if (!std::strcmp(mode,"different-code")) image[site]^=1;
    enabled=std::strcmp(mode,"disabled")!=0;
    allocationFails=std::strcmp(mode,"allocation-fails")==0;
    invalidWorkers=!std::strcmp(mode,"invalid-workers");
    TsmHost host{};
    host.apiVersion=TSM_API_VERSION; host.structSize=sizeof(host);
    host.exeModule=image; host.exeBase=image; host.exeSize=imageSize;
    host.baseDir="."; host.pluginDir="plugins";
    host.log=Log; host.readablePtr=Readable; host.allocNear=Allocate; host.configInt=Config;
    HMODULE dll=LoadLibraryW(construction?L"build\\industrial_construction.dll":L"build\\industrial_tycoon.dll");
    if (!dll) { printf("DLL LOAD FAILED %lu\n",GetLastError()); return 4; }
    auto api=reinterpret_cast<unsigned(*)()>(GetProcAddress(dll,"TsmPluginApiVersion"));
    auto init=reinterpret_cast<int(*)(const TsmHost*,TsmPluginInfo*)>(GetProcAddress(dll,"TsmPluginInit"));
    auto start=reinterpret_cast<int(*)()>(GetProcAddress(dll,"TsmPluginStart"));
    if (!api || !init || !start || api()!=TSM_API_VERSION) return 5;
    TsmPluginInfo info{};
    int initialized=init(&host,&info);
    bool expectsRefusal=!std::strcmp(mode,"wrong-version") || !std::strcmp(mode,"different-code") ||
        !std::strcmp(mode,"disabled") || !std::strcmp(mode,"wrong-architecture") || invalidWorkers;
    if (expectsRefusal) {
        bool ok=initialized!=0;
        printf("%s DLL init refusal: %s\n",ok?"PASS":"FAIL",mode);
        return ok?0:1;
    }
    if (initialized) return 6;
    if (!std::strcmp(mode,"late-conflict")) image[site]^=1;
    int started=start();
    if (allocationFails || !std::strcmp(mode,"late-conflict")) {
        bool ok=started!=0 && image[site]!=0xE9;
        printf("%s DLL start refusal without patch: %s\n",ok?"PASS":"FAIL",mode);
        return ok?0:1;
    }
    if (started || image[site]!=0xE9) return 7;
    if (!construction) {
        bool fireExpected=std::strcmp(mode,"fire-different-code") && !fireDisabled;
        bool fireOk=fireExpected? image[fs::kExtinguishSite]==0xE9 && image[fs::kDispatchSite]==0xEB :
                                  image[fs::kExtinguishSite]==0x66 && image[fs::kDispatchSite]==0x75;
        printf("%s fire service %s (%s)\n",fireOk?"PASS":"FAIL",fireExpected?"installed":"left original",mode);
        if (!fireOk) return 11;
        bool repairExpected=std::strcmp(mode,"repair-different-code") && !repairDisabled;
        bool repairOk=repairExpected? image[rp::kRateSite]==0xE9 && !std::memcmp(image+rp::kTickSite,rp::kTickPatched,sizeof(rp::kTickPatched)) :
                                    !std::memcmp(image+rp::kRateSite,rp::kRateOriginal,sizeof(rp::kRateOriginal)) &&
                                    !std::memcmp(image+rp::kTickSite,rp::kTickOriginal,sizeof(rp::kTickOriginal));
        printf("%s repair station %s (%s)\n",repairOk?"PASS":"FAIL",repairExpected?"installed":"left original",mode);
        if (!repairOk) return 14;
        bool plantsExpected=std::strcmp(mode,"plants-different-code") && !plantsDisabled, plantsOk=true;
        for (const auto& s:ps::kSites) {
            std::uint32_t at=s.site-s.contextAt;
            bool consistent=s.size>=1 && s.size<=4 && at+s.size<=s.contextSize && !std::memcmp(s.context+at,s.original,s.size);
            plantsOk&=consistent && !std::memcmp(image+s.site,plantsExpected?s.patched:s.original,s.size);
        }
        printf("%s plant sources %s (%s)\n",plantsOk?"PASS":"FAIL",plantsExpected?"installed":"left original",mode);
        if (!plantsOk) return 15;
        for (int i=0;i<ws::kSiteCount;++i) {
            const auto& s=ws::kSites[i];
            char name[64];
            std::snprintf(name,sizeof(name),"%s-different-code",workPrefix[i]);
            bool expected=std::strcmp(mode,name) && std::strcmp(workDisabledKey,s.key);
            bool siteOk=expected? image[s.site]==0xE9 : image[s.site]==s.original[0];
            printf("%s %s %s (%s)\n",siteOk?"PASS":"FAIL",s.label,expected?"installed":"left original",mode);
            if (!siteOk) return 13;
        }
    }
    for (int i=0;!construction && i<ws::kSiteCount;++i) {
        char name[64]; std::snprintf(name,sizeof(name),"valid-%s",workPrefix[i]);
        if (std::strcmp(mode,name)) continue;
        // Run the installed hook from the game site; the continuation returns here.
        const auto& s=ws::kSites[i];
        const BYTE back[]={0x9C,0x41,0x5B,0xC3}; // pushfq; pop r11; ret
        std::memcpy(image+s.continuation(),back,sizeof(back));
        FlushInstructionCache(GetCurrentProcess(),image,imageSize);
        std::vector<BYTE> building(0x1000),descriptor(0x900),game(0x1000);
        void* q=descriptor.data(); std::memcpy(building.data()+0x318,&q,8);
        static const int staffBySite[]={60,150,20,7};
        int kind=s.buildingType,workers=staffBySite[i<4?i:0];
        std::memcpy(descriptor.data()+0x360,&kind,4); std::memcpy(descriptor.data()+s.staffOffset,&workers,4);
        std::uint64_t o[16]{};
        std::uint64_t observed;
        bool sideOk;
        if (s.workXmm==13 || s.workXmm==6) { // container facility, field
            RunWorkStub(image+s.site,building.data(),game.data(),o);
            observed=s.workXmm==13?o[1]:o[2]; sideOk=o[10]==0;
        } else {                             // control tower (xmm8, r14d), water well (xmm15, [rsp+0x74])
            RunWorkStub2(image+s.site,building.data(),game.data(),o);
            observed=s.workXmm==8?o[1]:o[2];
            float staff=static_cast<float>(workers); std::uint32_t staffBits; std::memcpy(&staffBits,&staff,4);
            sideOk=o[12]==0 && (s.countReg<0 || o[6]==static_cast<std::uint32_t>(workers)) && (s.spillRsp<0 || o[11]==staffBits);
        }
        float work; std::uint32_t w=static_cast<std::uint32_t>(observed); std::memcpy(&work,&w,4);
        bool ok=work==static_cast<float>(workers) && sideOk;
        printf("%s actual DLL %s work hook (%s); work=%.1f\n",ok?"PASS":"FAIL",s.label,mode,work);
        return ok?0:1;
    }
    if (construction) {
        bool railExpected=std::strcmp(mode,"rail-different-code") && !railDisabled;
        bool railOk=railExpected? image[rs::kTrackSite]==0xE9 && image[rs::kRefuelSite]==0xEB :
                                  image[rs::kTrackSite]==0x48 && image[rs::kRefuelSite]==0x77;
        printf("%s rail construction %s (%s)\n",railOk?"PASS":"FAIL",railExpected?"installed":"left original",mode);
        if (!railOk) return 12;
    }
    if (construction && !std::strcmp(mode,"valid-track")) {
        // Run the installed track crew hook from the game site; the continuation returns here.
        image[rs::kTrackContinuation]=0xC3;
        FlushInstructionCache(GetCurrentProcess(),image,imageSize);
        std::vector<BYTE> vehicle(0x2000),type(0x9000),skills(0x40),office(0x1000),descriptor(0x900),site(0x1000);
        auto ptr=[&](std::vector<BYTE>& v,size_t at,void* q) { std::memcpy(v.data()+at,&q,8); };
        auto put=[&](std::vector<BYTE>& v,size_t at,int x) { std::memcpy(v.data()+at,&x,4); };
        ptr(vehicle,0x4F8,office.data()); ptr(vehicle,0x1708,type.data()); ptr(type,0x85E0,skills.data());
        float capacity=70.0f,factor=1.0f; put(skills,0,22); std::memcpy(skills.data()+8,&capacity,4); std::memcpy(skills.data()+0xC,&factor,4);
        ptr(office,0x318,descriptor.data()); put(office,0xDEC,100); put(descriptor,0x360,0x1C); put(descriptor,0x80C,60);
        std::uint64_t o[16]{};
        RunTrackStub(image+rs::kTrackSite,vehicle.data(),site.data(),o);
        float crew; std::uint32_t c=static_cast<std::uint32_t>(o[1]); std::memcpy(&crew,&c,4);
        bool ok=crew==42.0f && o[2]==reinterpret_cast<std::uintptr_t>(type.data()) && o[10]==0;
        printf("%s actual construction DLL rail track crew hook (valid-track); crew=%.1f\n",ok?"PASS":"FAIL",crew);
        return ok?0:1;
    }
    if (!std::strcmp(mode,"valid-repair")) {
        // Run the installed rate hook from the game site; the three continuations return here.
        const BYTE loopRet[]={0x41,0xBB,0x01,0x00,0x00,0x00,0xC3},factorRet[]={0x41,0xBB,0x02,0x00,0x00,0x00,0xC3},
                   doneRet[]={0x41,0xBB,0x03,0x00,0x00,0x00,0xC3};
        std::memcpy(image+rp::kRateLoop,loopRet,sizeof(loopRet));
        std::memcpy(image+rp::kRateFactor,factorRet,sizeof(factorRet));
        std::memcpy(image+rp::kRateDone,doneRet,sizeof(doneRet));
        FlushInstructionCache(GetCurrentProcess(),image,imageSize);
        std::vector<BYTE> station(0x1000),descriptor(0x900),game(0x1000);
        void* q=descriptor.data(); std::memcpy(station.data()+0x318,&q,8);
        int kind=rp::kRepairType,workers=30;
        std::memcpy(descriptor.data()+0x360,&kind,4); std::memcpy(descriptor.data()+0x80C,&workers,4);
        std::uint64_t o[16]{}; o[0]=6; o[1]=0; o[2]=6; // second job of a station with nobody present
        RunRepairStub(image+rp::kRateSite,station.data(),game.data(),o);
        float rate; std::uint32_t r=static_cast<std::uint32_t>(o[4]); std::memcpy(&rate,&r,4);
        bool ok=o[3]==2 && rate==6.0f;
        printf("%s actual DLL repair rate hook (valid-repair); path=%d rate=%.1f\n",ok?"PASS":"FAIL",static_cast<int>(o[3]),rate);
        return ok?0:1;
    }
    if (!std::strcmp(mode,"valid-fire")) {
        // Run the installed hook from the game site; continuations return to the harness.
        const BYTE normalRet[]={0x41,0xBB,0x01,0x00,0x00,0x00,0xC3}, virtualRet[]={0x41,0xBB,0x02,0x00,0x00,0x00,0xC3};
        std::memcpy(image+fs::kExtinguishNormal,normalRet,sizeof(normalRet));
        std::memcpy(image+fs::kExtinguishVirtual,virtualRet,sizeof(virtualRet));
        FlushInstructionCache(GetCurrentProcess(),image,imageSize);
        std::vector<BYTE> building(0x1000),frame(0x400),type(0x9000),skill(0x40),vehicle(0x2000);
        int skillType=31; float value=9.0f;
        std::memcpy(skill.data(),&skillType,4); std::memcpy(skill.data()+4,&value,4);
        void* p=skill.data(); std::memcpy(type.data()+0x85E0,&p,8);
        p=type.data(); std::memcpy(vehicle.data()+0x1708,&p,8);
        void* list[1]={vehicle.data()}; p=list; std::memcpy(building.data()+0xC70,&p,8);
        std::uint64_t o[16]{}; o[0]=0; o[1]=0; o[2]=5; float rate=2.0f; std::memcpy(&o[3],&rate,4);
        RunFireStub(image+fs::kExtinguishSite,building.data(),frame.data()+0x100,o);
        float crew,out; std::uint32_t c=static_cast<std::uint32_t>(o[5]),r=static_cast<std::uint32_t>(o[6]);
        std::memcpy(&crew,&c,4); std::memcpy(&out,&r,4);
        bool ok=o[4]==2 && crew==9.0f && out==18.0f && o[8]==6;
        printf("%s actual DLL fire extinguishing hook (valid-fire); path=%s crew=%.1f rate=%.1f\n",ok?"PASS":"FAIL",o[4]==2?"virtual":"normal",crew,out);
        return ok?0:1;
    }
    // The synthetic continuation returns to the harness instead of running game code.
    image[site+patchSize]=0xC3;
    FlushInstructionCache(GetCurrentProcess(),image,imageSize);
    if (construction && !std::strcmp(mode,"coexist")) {
        HMODULE industry=LoadLibraryW(L"build\\industrial_tycoon.dll");
        if (!industry) return 8;
        auto otherInit=reinterpret_cast<int(*)(const TsmHost*,TsmPluginInfo*)>(GetProcAddress(industry,"TsmPluginInit"));
        auto otherStart=reinterpret_cast<int(*)()>(GetProcAddress(industry,"TsmPluginStart"));
        TsmPluginInfo otherInfo{};
        if (!otherInit || !otherStart || otherInit(&host,&otherInfo) || otherStart()) return 9;
        image[industrial::kLaborSite+5]=0xC3;
        FlushInstructionCache(GetCurrentProcess(),image,imageSize);
        std::vector<BYTE> td(0x900),bld(0x400),fr(0x300);
        int type=6,label=6053,workers=500;
        std::memcpy(td.data()+0x360,&type,4); std::memcpy(td.data()+0x200,&label,4);
        std::memcpy(td.data()+0x80C,&workers,4);
        void* p=td.data(); std::memcpy(bld.data()+0x318,&p,8);
        std::uint64_t seen[7]{};
        RunLaborStub(image+industrial::kLaborSite,bld.data(),fr.data()+0x100,seen);
        float labor; std::memcpy(&labor,fr.data()+0x16F,4);
        if (labor!=1.0f || image[site]!=0xE9) return 10;
        std::puts("PASS industry and construction DLLs coexist at distinct patch sites");
    }
    if (construction) {
        std::vector<BYTE> descriptor(0x900),building(0x1200),frame(0x600);
        auto set=[&](std::vector<BYTE>& v,size_t at,int x) { std::memcpy(v.data()+at,&x,4); };
        bool farm=!std::strcmp(mode,"valid-farm");
        bool substation=!std::strcmp(mode,"valid-substation");
        bool line=!std::strcmp(mode,"valid-powerline");
        bool office=!std::strcmp(mode,"valid-office");
        bool steam=!std::strcmp(mode,"valid-steam");
        bool trafo=!std::strcmp(mode,"valid-trafo");
        bool roadMode=!std::strcmp(mode,"valid-road");
        bool depot=!std::strcmp(mode,"valid-depot");
        bool fireStation=!std::strcmp(mode,"valid-firestation");
        bool ship=!std::strcmp(mode,"valid-ship"); // passenger harbor: type 1, subtype SHIP
        bool railOffice=!std::strcmp(mode,"valid-rail-office");
        bool cargo=!std::strcmp(mode,"valid-cargo"); // forklift station
        bool monument=!std::strcmp(mode,"valid-monument"); // DLC4 world monument
        bool field=!std::strcmp(mode,"valid-field"); // hand-worked field
        bool quarry=!std::strcmp(mode,"valid-quarry"); // gravel quarry
        bool infra=line||steam||roadMode;
        set(descriptor,0x360,farm?9:substation?18:infra?13:office?12:trafo?35:depot?14:fireStation?26:ship?1:railOffice?0x1C:cargo?0x2D:monument?0x21:field?8:quarry?7:5);
        set(descriptor,0x200,farm?6115:substation?6302:infra?0:office?6151:trafo?1609:depot?6130:fireStation?6259:ship?6373:railOffice?6152:cargo?6205:monument?90018:field?6110:quarry?6155:6203);
        set(descriptor,0x364,ship?industrial::kSubtypeShip:0);
        set(building,0x62C,farm?64:100);
        void* td=descriptor.data(); std::memcpy(building.data()+0x318,&td,8);
        std::vector<BYTE> road(0x400);
        if (infra) {
            int type=line?8:steam?7:0, sub=steam?5:roadMode?3:0;
            std::memcpy(road.data()+0x12C,&type,4); std::memcpy(road.data()+0x124,&sub,4);
            void* r=road.data(); std::memcpy(building.data()+0x5D0,&r,8);
        }
        float progress=farm?0.24f:0.25f; std::memcpy(building.data()+0x604,&progress,4);
        BYTE* rbp=frame.data()+0x100;
        auto before=building;
        std::uint64_t observed[16]{};
        RunConstructionStub(image+site,building.data(),rbp,observed);
        float labor; std::memcpy(&labor,reinterpret_cast<BYTE*>(observed)+88,4);
        bool ok=observed[8]==50 && labor==50.0f && observed[2]==observed[3] && building==before &&
            observed[13]==0x0FEDCBA987654321ULL;
        std::printf("%s actual construction DLL runtime patch (%s); virtual workers=%llu labor=%.2f memory=%s\n",
                    ok?"PASS":"FAIL",mode,observed[8],labor,building==before?"unchanged":"CHANGED");
        return ok?0:1;
    }
    std::vector<BYTE> descriptor(0x900),building(0x400),frame(0x300);
    auto set=[&](size_t offset,int value) { std::memcpy(descriptor.data()+offset,&value,4); };
    bool isWood=!std::strcmp(mode,"valid-wood");
    bool isCar=!std::strcmp(mode,"valid-car");
    bool isPower=!std::strcmp(mode,"valid-power");
    bool isShip=!std::strcmp(mode,"valid-ship"); // drydock: production line, subtype SHIP
    bool isTrain=!std::strcmp(mode,"valid-train-factory"); // production line, subtype RAIL
    bool isFood=!std::strcmp(mode,"valid-food-factory");   // food section factory by name
    bool isQuarry=!std::strcmp(mode,"valid-gravel-quarry"); // mine
    bool isWorkshopMine=!std::strcmp(mode,"valid-workshop-mine"); // Gravel Mine v2: $NAME_STR, no name id
    bool line=isCar||isShip||isTrain;
    set(0x360,isWood||isQuarry||isWorkshopMine?7:line?40:isPower?17:6); set(0x200,isWood?7001:isQuarry?6155:isWorkshopMine?0:isCar?2020:isPower?6319:isShip?6391:isTrain?2021:isFood?6101:6053);
    set(0x80C,isWood?10:line?400:isPower?80:500); set(0x810,line?100:isPower?40:0); set(0x368,isWood?4:0);
    set(0x364,isShip?industrial::kSubtypeShip:isTrain?industrial::kSubtypeRail:0);
    void* td=descriptor.data(); std::memcpy(building.data()+0x318,&td,8);
    BYTE* rbp=frame.data()+0x100;
    float original=12.5f; std::memcpy(rbp-0x79,&original,4);
    std::uint64_t observed[7]{};
    RunLaborStub(image+industrial::kLaborSite,building.data(),rbp,observed);
    float factor; std::memcpy(&factor,rbp+0x6F,4);
    bool ok=factor==1.0f && observed[0]==observed[1];
    printf("%s actual DLL runtime patch on synthetic image (%s); zero-worker factor=%.2f\n",ok?"PASS":"FAIL",mode,factor);
    // Live code references the DLL: deliberately keep it loaded until process exit.
    return ok?0:1;
}
