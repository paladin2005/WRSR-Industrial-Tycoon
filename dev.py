"""Build and verify the isolated Industrial Tycoon prototype (no game writes)."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8',errors='replace')

ROOT=Path(__file__).resolve().parent
BUILD=ROOT/'build'
CLANG=ROOT/'tools/llvm-mingw-20261006-ucrt-x86_64/bin/clang++.exe'
EXPECTED_SHA='296644a9f207d609031fc2ae73fed2dcb34619a1d55a35d1c7b51965ce6841b8'
VERSION='0.16.0-experimental'


def run(argv):
    print('RUN '+str(argv[0])+' '+ ' '.join(str(v) for v in argv[1:]),flush=True)
    p=subprocess.run([str(a) for a in argv],cwd=ROOT,capture_output=True,text=True,errors='replace')
    if p.stdout: print(p.stdout,flush=True)
    if p.stderr: print(p.stderr,flush=True)
    if p.returncode: raise SystemExit(p.returncode)
    return p.stdout


def verify():
    import capstone
    import pefile
    binary=ROOT/'research/SOVIET64.exe'
    data=binary.read_bytes()
    assert hashlib.sha256(data).hexdigest()==EXPECTED_SHA,'Different executable'
    pe=pefile.PE(data=data)
    assert pe.FILE_HEADER.Machine==0x8664
    assert pe.FILE_HEADER.TimeDateStamp==0x6A3EB6AD
    image=pe.get_memory_mapped_image()
    assert image[0x175CF7:0x175D03]==bytes.fromhex('f3 0f 10 7d 87 4c 8d 86 08 14 01 00')
    assert image[0x175A00:0x175A11]==bytes.fromhex('48 8b c4 48 89 58 18 48 89 70 20 55 57 41 54 41 56')
    assert image[0x1B392D:0x1B3932]==bytes.fromhex('e8 ce 20 fc ff'),'Mine work-factor call changed'
    assert image[0x1B39F2:0x1B39F7]==bytes.fromhex('e8 e9 02 00 00'),'Woodcutting dispatch changed'
    assert image[0x10E928:0x10E932]==bytes.fromhex('c7 85 10 1e 00 00 28 00 00 00'),'Production-line type changed'
    assert image[0x1CA151:0x1CA156]==bytes.fromhex('e8 aa b8 fa ff'),'Vehicle work-factor call changed'
    # Power plants: parser type 17 and the factory routine that branches on it before the call.
    assert image[0x10EF29:0x10EF33]==bytes.fromhex('c7 85 10 1e 00 00 11 00 00 00'),'Power plant type changed'
    assert image[0x1D2015:0x1D201C]==bytes.fromhex('83 b8 60 03 00 00 11'),'Factory power plant branch changed'
    assert image[0x1D213A:0x1D213F]==bytes.fromhex('e8 c1 38 fa ff'),'Factory work-factor call changed'
    # Fire service: station selection (0x253060, fire-station branch after the type 0x1A check at
    # 0x253418) and extinguishing (building fire update 0x13EC60, crew count from vehicle +0x4C8).
    assert image[0x253418:0x25341F]==bytes.fromhex('83 b8 60 03 00 00 1a'),'Fire station selection changed'
    assert image[0x253724:0x253743]==bytes.fromhex('45 85 c9 7e 17 48 8b 87 c8 0b 00 00 48 2b 87 c0 0b 00 00 48 c1 f8 03 48 85 c0 75 09 45 85 db'),'Fire dispatch people check changed'
    assert image[0x13F132:0x13F144]==bytes.fromhex('48 8b 99 d0 04 00 00 48 2b 99 c8 04 00 00 48 c1 fb 03'),'Fire truck crew count changed'
    assert image[0x13F15A:0x13F175]==bytes.fromhex('0f 28 f8 f3 0f 59 3d 13 ab 7c 00 66 0f 6e cb 0f 5b c9 f3 0f 59 f9 45 33 d2 85 db'),'Extinguishing rate code changed'
    assert image[0x13F17B:0x13F185]==bytes.fromhex('41 ff c4 44 89 a5 38 01 00 00'),'Fighting-truck counter changed'
    assert image[0x13F22E:0x13F232]==bytes.fromhex('41 83 cc ff'),'Fire-point reduction entry changed'
    assert image[0x13F114:0x13F11C]==bytes.fromhex('83 38 1f 74 09 83 38 20'),'Fire truck skill check changed'
    # Ship section: $SUBTYPE_SHIP writes 0x13 to descriptor+0x364. Production lines (0x28, drydock
    # included) all go to 0x1CA020; scrapyards (0x6D) go to 0x14B920, which calls the labor function.
    assert image[0x10FAE4:0x10FAEE]==bytes.fromhex('c7 85 14 1e 00 00 13 00 00 00'),'Ship subtype mapping changed'
    # Rail section: $SUBTYPE_RAIL writes 0x1A; train factories are production lines like the drydock.
    assert image[0x1187EB:0x1187F5]==bytes.fromhex('c7 85 14 1e 00 00 1a 00 00 00'),'Rail subtype mapping changed'
    assert image[0x10F4B2:0x10F4BC]==bytes.fromhex('c7 85 10 1e 00 00 6d 00 00 00'),'Scrapyard type mapping changed'
    # Container facilities: type 0x2C, tick 0x1C27F0 sums the work of the people present into XMM13;
    # both paths (empty list at 0x1C2933, end of loop) meet at 0x1C2A6A.
    assert image[0x10F310:0x10F31A]==bytes.fromhex('c7 85 10 1e 00 00 2c 00 00 00'),'Container facility type mapping changed'
    assert image[0x13E31A:0x13E32E]==bytes.fromhex('83 b8 60 03 00 00 2c 75 10 49 8b d0 48 8b ce e8 c2 44 08 00'),'Container facility dispatch changed'
    assert image[0x1C2A58:0x1C2A7B]==bytes.fromhex('f3 44 0f 10 15 17 80 74 00 44 0f 28 9c 24 a0 00 00 00 45 39 a6 bc 05 00 00 7e 13 48 8b d3 e8 05 78 fe ff'),'Container facility work site changed'
    assert image[0x1C2930:0x1C2939]==bytes.fromhex('48 85 c0 0f 84 31 01 00 00'),'Container facility empty list branch changed'
    # Fields: type 8, tick 0x16E3B0 sums the productivity of the people present into XMM6; the empty-list
    # branch (0x16E466) and the end of the loop meet at 0x16E4FD. Food section factories are type 6 and
    # go through the factory routine like the refinery.
    assert image[0x10E9C8:0x10E9D2]==bytes.fromhex('c7 85 10 1e 00 00 08 00 00 00'),'Field type mapping changed'
    assert image[0x13E0C6:0x13E0DE]==bytes.fromhex('83 b8 60 03 00 00 08 75 14 41 0f 28 d3 49 8b d0 48 8b ce e8 d2 02 03 00'),'Field dispatch changed'
    assert image[0x16E4F6:0x16E50B]==bytes.fromhex('8b cf 48 3b c8 72 83 48 8b 8e 68 0e 00 00 48 2b 8e 60 0e 00 00'),'Field work site changed'
    assert image[0x16E463:0x16E46C]==bytes.fromhex('48 85 c0 0f 84 91 00 00 00'),'Field empty list branch changed'
    # Control towers: type 0x30 (parser 0x10F278), tick 0x1C23C0 (dispatch 0x13E2A9) sums the engineers' work into
    # XMM8 and counts them in R14D; the empty-list branch (0x1C249B) and the loop end meet at 0x1C25B5.
    assert image[0x10F278:0x10F282]==bytes.fromhex('c7 85 10 1e 00 00 30 00 00 00'),'Control tower type mapping changed'
    assert image[0x13E29A:0x13E2AE]==bytes.fromhex('83 b8 60 03 00 00 30 75 10 49 8b d0 48 8b ce e8 12 41 08 00'),'Control tower dispatch changed'
    assert image[0x1C25B5:0x1C25E0]==bytes.fromhex('44 0f 28 ac 24 80 00 00 00 44 0f 28 9c 24 a0 00 00 00 48 8b b4 24 28 01 00 00 44 39 bd bc 05 00 00 7e 17 48 8b d3 e8 a0 7c fe ff'),'Control tower work site changed'
    assert image[0x1C249B:0x1C24A1]==bytes.fromhex('0f 84 14 01 00 00'),'Control tower empty list branch changed'
    # Water wells: type 0x5C (parser 0x10EB64), tick 0x189030 (dispatch 0x13C93B) calls 0x1B1290 at 0x1890A1, which
    # sums the workers' work into XMM15 and [rsp+0x74]; the empty-list branch (0x1B13E1) and the loop end meet at 0x1B14C6.
    assert image[0x10EB64:0x10EB6E]==bytes.fromhex('c7 85 10 1e 00 00 5c 00 00 00'),'Water well type mapping changed'
    assert image[0x1890A1:0x1890A6]==bytes.fromhex('e8 ea 81 02 00'),'Water well work call changed'
    assert image[0x1B14B6:0x1B14D7]==bytes.fromhex('48 3b c8 0f 82 33 ff ff ff f3 44 0f 11 7c 24 74 45 39 be bc 05 00 00 7e 1a 48 8b d6 e8 a9 8d ff ff'),'Water well work site changed'
    assert image[0x1B13E1:0x1B13E7]==bytes.fromhex('0f 84 df 00 00 00'),'Water well empty list branch changed'
    assert image[0x13DD69:0x13DD7D]==bytes.fromhex('83 b8 60 03 00 00 06 75 10 49 8b d0 48 8b ce e8 03 41 09 00'),'Factory dispatch changed'
    # Mines: every $TYPE_MINE_* writes type 7 and its extraction resource to descriptor+0x368 (stack 0x1E18);
    # gravel (0x10EAFF) and wood (0x10EB30) are admitted by name.
    for rva in (0x10EAFF,0x10EB30):
        assert image[rva:rva+10]==bytes.fromhex('c7 85 10 1e 00 00 07 00 00 00'),'Mine type mapping changed at %#x'%rva
    assert image[0x13DD89:0x13DD9D]==bytes.fromhex('83 b8 60 03 00 00 28 75 10 49 8b d0 48 8b ce e8 83 c2 08 00'),'Production line dispatch changed'
    assert image[0x13E1BA:0x13E1CE]==bytes.fromhex('83 b8 60 03 00 00 6d 75 10 49 8b d0 48 8b ce e8 52 d7 00 00'),'Scrapyard dispatch changed'
    assert image[0x14BAA4:0x14BAAC]==bytes.fromhex('e8 57 9f 02 00 0f 28 f0'),'Scrapyard work-factor call changed'
    # Water and sewage treatment (0x61/0x62, parser 0x10E8B6/0x10E8DC): the dispatcher picks both with
    # `sub ecx,0x61; cmp ecx,1; ja` and calls 0x1CE9A0, which calls the labor function at 0x1CEA80.
    assert image[0x13DD24:0x13DD3D]==bytes.fromhex('8b 88 60 03 00 00 83 e9 61 83 f9 01 77 10 49 8b d0 48 8b ce e8 63 0c 09 00'),'Treatment plant dispatch changed'
    assert image[0x1CEA74:0x1CEA88]==bytes.fromhex('00 00 41 0f 28 d0 48 8b d3 48 8b cf e8 7b 6f fa ff 0f 28 f0'),'Treatment plant work-factor call changed'
    for rva,value in ((0x10E8B6,0x61),(0x10E8DC,0x62)):
        assert image[rva:rva+10]==bytes.fromhex('c7 85 10 1e 00 00')+value.to_bytes(4,'little'),'Treatment plant type mapping changed at %#x'%rva
    # Repair stations: type 0x6C, tick 0x14C5B0 (dispatch 0x13E1A9). The repair rate 0x1AD790 (only caller
    # 0x14C992) sums the productivity of the people present and returns 0 without people (jge 0x1AD7DF);
    # with nobody present the job loop runs only if the station flag +0x11C1 is set (0x14C8A8).
    assert image[0x10EE6B:0x10EE75]==bytes.fromhex('c7 85 10 1e 00 00 6c 00 00 00'),'Repair station type mapping changed'
    assert image[0x13E19A:0x13E1AE]==bytes.fromhex('83 b8 60 03 00 00 6c 75 10 49 8b d0 48 8b ce e8 02 e4 00 00'),'Repair station dispatch changed'
    assert image[0x14C992:0x14C997]==bytes.fromhex('e8 f9 0d 06 00'),'Repair rate call changed'
    assert image[0x1AD7DD:0x1AD7E5]==bytes.fromhex('3b f8 0f 8d d4 00 00 00'),'Repair rate empty branch changed'
    assert image[0x1AD895:0x1AD8A8]==bytes.fromhex('76 22 83 bd bc 05 00 00 00 7e 19 48 8b d3 e8 d8 c9 ff ff'),'Repair condition factor changed'
    assert image[0x14C89F:0x14C8AE]==bytes.fromhex('75 0d 80 bb c1 11 00 00 01 0f 85 8a 02 00 00'),'Repair job loop check changed'
    # Plant sources: the construction office routine 0x1561C0 takes a concrete plant (source kind 3) or an
    # asphalt plant (kind 4) with its inputs as able to produce only when its people list is not empty.
    assert image[0x156E08:0x156E0E]==bytes.fromhex('42 83 7c e2 08 03'),'Concrete plant source kind changed'
    assert image[0x156FBF:0x156FC5]==bytes.fromhex('42 83 7c e2 08 04'),'Asphalt plant source kind changed'
    assert image[0x156F7C:0x156FA8]==bytes.fromhex('49 8b 87 78 0f 00 00 4a 8b 0c e0 48 8b 81 c8 0b 00 00 48 2b 81 c0 0b 00 00 48 c1 f8 03 48 8b 7d c0 48 85 c0 48 0f 45 f9 48 89 7d c0'),'Concrete plant people check changed'
    assert image[0x15712C:0x157158]==bytes.fromhex('49 8b 87 78 0f 00 00 4a 8b 0c e0 48 8b 81 c8 0b 00 00 48 2b 81 c0 0b 00 00 48 c1 f8 03 48 8b 55 c8 48 85 c0 48 0f 45 d1 48 89 55 c8'),'Asphalt plant people check changed'
    md3=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
    for patched,expected in (('48 8b f9 90',['mov rdi, rcx','nop']),('48 8b d1 90',['mov rdx, rcx','nop'])):
        assert [i.mnemonic+' '+i.op_str if i.op_str else i.mnemonic for i in md3.disasm(bytes.fromhex(patched),0)]==expected,'Plant source patch decodes differently'
    # Plant sources in the other office tick: 0x13E139 calls 0x146B10 when [game+0x14455] is set (else 0x145900);
    # 0x146B10 picks sources through 0x155960, which calls 0x151780 (concrete plant, kind 3) and 0x151910
    # (asphalt plant, kind 4); both return false for a plant that needs staff and has nobody present.
    assert image[0x13E139:0x13E14E]==bytes.fromhex('80 be 55 44 01 00 00 74 07 e8 c9 89 00 00 eb 05 e8 b2 77 00 00'),'Office tick selection changed'
    for rva in (0x151791,0x151921):
        assert image[rva:rva+55]==bytes.fromhex('4c 8b 82 18 03 00 00 45 8b 88 10 08 00 00 45 03 88 0c 08 00 00 45 85 c9 7e 1f 48 8b 92 c8 0b 00 00 49 2b 92 c0 0b 00 00 48 c1 fa 03 48 85 d2 75 08 32 c0 48 83 c4 60'),'Plant source check changed at %#x'%rva
    assert image[0x155AC9:0x155AD7]==bytes.fromhex('41 83 f8 04 75 28 48 8b d3 e8 39 be ff ff') and image[0x155AF7:0x155B05]==bytes.fromhex('41 83 f8 03 75 28 48 8b d3 e8 7b bc ff ff'),'Source selector calls changed'
    md2=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64); md2.detail=True
    for lo,hi,flo,fhi in ((0x13F165,0x13F16C,0x13EC60,0x140060),(0x25373E,0x253740,0x253060,0x255182),(0x1C2A6A,0x1C2A71,0x1C27F0,0x1C3CD5),(0x16E4FD,0x16E504,0x16E3B0,0x16FCCE),(0x1AD7DF,0x1AD7E5,0x1AD790,0x1AD8DC),(0x14C8A8,0x14C8AE,0x14C5B0,0x14CDBF),(0x156FA0,0x156FA4,0x1561C0,0x158C45),(0x157150,0x157154,0x1561C0,0x158C45),(0x1517C0,0x1517C2,0x151780,0x151907),(0x151950,0x151952,0x151910,0x151A97),(0x1C25CF,0x1C25D6,0x1C23C0,0x1C27ED),(0x1B14C6,0x1B14CD,0x1B1290,0x1B36FB)):
        for ins in md2.disasm(image[flo:fhi],flo):
            if (ins.mnemonic.startswith('j') or ins.mnemonic=='call') and ins.operands and ins.operands[0].type==capstone.x86.X86_OP_IMM:
                assert not lo<ins.operands[0].imm<hi,'Branch enters fire patch bytes at %#x'%ins.address
    md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
    stolen=list(md.disasm(image[0x175CF7:0x175CFC],0x175CF7))
    assert len(stolen)==1 and stolen[0].mnemonic=='movss' and stolen[0].size==5
    # Both branch paths initialize one normalized local; patch is after those paths
    # and before the game's water/weather/wear factors. No branch enters its middle.
    print('PASS executable identity, PE architecture and complete patch instruction')
    result={'status':'static-verified','sha256':EXPECTED_SHA,'patch_rva':'0x175cf7',
            'instruction':stolen[0].mnemonic+' '+stolen[0].op_str,
            'in_game_validation':False}
    BUILD.mkdir(exist_ok=True)
    (BUILD/'static_validation.json').write_text(json.dumps(result,indent=2),encoding='utf-8')


def build():
    verify()
    assert '#define INDUSTRIAL_VERSION "'+VERSION.split('-')[0]+'"' in (ROOT/'src/industrial_tycoon.cpp').read_text(encoding='utf-8'),'industrial_tycoon.cpp version differs from dev.py'
    common=[CLANG,'-std=c++17','-O2','-fms-extensions','-static','-Wall','-Wextra',
            '-Wno-unknown-pragmas','-Wno-unused-function','-Wno-unused-variable']
    run(common+[ROOT/'tests/stub_test.cpp',ROOT/'tests/stub_call.S','-o',BUILD/'stub_test.exe'])
    run(common+[ROOT/'tests/fire_test.cpp',ROOT/'tests/fire_call.S','-o',BUILD/'fire_test.exe'])
    run(common+[ROOT/'tests/work_test.cpp',ROOT/'tests/work_call.S','-o',BUILD/'work_test.exe'])
    run(common+[ROOT/'tests/repair_test.cpp',ROOT/'tests/repair_call.S','-o',BUILD/'repair_test.exe'])
    run(common+['-shared',ROOT/'src/industrial_tycoon.cpp','-o',BUILD/'industrial_tycoon.dll'])
    run(common+[ROOT/'tests/plugin_host.cpp',ROOT/'tests/stub_call.S',ROOT/'tests/construction_call.S',ROOT/'tests/fire_call.S',ROOT/'tests/rail_call.S',ROOT/'tests/work_call.S',ROOT/'tests/repair_call.S','-o',BUILD/'plugin_host.exe'])
    print('PASS prototype and native test compiled')


def test():
    verify()
    output=run([BUILD/'stub_test.exe'])
    output+=run([BUILD/'fire_test.exe'])
    output+=run([BUILD/'work_test.exe'])
    output+=run([BUILD/'repair_test.exe'])
    for mode in ('wrong-version','wrong-architecture','different-code','disabled','late-conflict','allocation-fails','valid','valid-wood','valid-car','valid-power',
                 'valid-ship','valid-train-factory','valid-fire','fire-different-code','fire-disabled',
                 'valid-container','container-different-code','container-disabled',
                 'valid-field','field-different-code','field-disabled','valid-tower','tower-different-code','tower-disabled',
                 'valid-well','well-different-code','well-disabled','valid-food-factory','valid-gravel-quarry','valid-workshop-mine',
                 'valid-repair','repair-different-code','repair-disabled','plants-different-code','plants-disabled'):
        output+=run([BUILD/'plugin_host.exe',mode])
    (BUILD/'native_test_results.txt').write_text(output,encoding='utf-8')
    import pefile
    plugin=pefile.PE(str(BUILD/'industrial_tycoon.dll'))
    exports={x.name.decode() for x in plugin.DIRECTORY_ENTRY_EXPORT.symbols if x.name}
    assert {'TsmPluginApiVersion','TsmPluginInit','TsmPluginStart'}<=exports
    assert plugin.FILE_HEADER.Machine==0x8664
    imports=[x.dll.decode() for x in plugin.DIRECTORY_ENTRY_IMPORT]
    assert not any('libc++' in x.lower() or 'libstdc++' in x.lower() for x in imports)
    print('PASS TesmioLoader exports, x64 binary and self-contained C++ runtime')
    print('IMPORTS '+', '.join(imports))
    validated={name:hashlib.sha256((BUILD/name).read_bytes()).hexdigest()
               for name in ('industrial_tycoon.dll','stub_test.exe','fire_test.exe','repair_test.exe','plugin_host.exe')}
    (BUILD/'tested_binaries.json').write_text(json.dumps(validated,indent=2),encoding='utf-8')


def loader():
    # Build only the loader/launcher. No upstream gameplay plugins are enabled.
    common=[CLANG,'-std=c++17','-O2','-fms-extensions','-static',
            '-Wno-unknown-pragmas','-Wno-deprecated-declarations']
    source=ROOT/'vendor/TesmioLoader/src'
    libs=['-lkernel32','-luser32','-lshell32','-lole32','-lcomctl32','-lbcrypt','-lpsapi','-lshlwapi','-ldbghelp','-luuid']
    run(common+['-shared',source/'tesmioloader.cpp','-o',BUILD/'tesmioloader.dll']+libs)
    run(common+['-municode','-mwindows',source/'tesmiolauncher.cpp','-o',BUILD/'tesmiolauncher.exe']+libs)
    print('PASS loader/launcher compiled from upstream source')


def construction_verify():
    verify()
    import pefile
    import capstone
    image=pefile.PE(str(ROOT/'research/SOVIET64.exe')).get_memory_mapped_image()
    assert image[0x159070:0x15907C]==bytes.fromhex('48 8b c4 44 88 48 20 f3 0f 11 50 18')
    assert image[0x15A39D:0x15A3AC]==bytes.fromhex('66 0f 6e f6 0f 5b f6 45 8b c7 44 89 7c 24 50')
    assert image[0x1389F9:0x1389FE]==bytes.fromhex('e8 72 06 02 00'),'Construction tick call changed'
    assert image[0x10E861:0x10E86C]==bytes.fromhex('b8 05 00 00 00 89 85 10 1e 00 00'),'Storage type mapping changed'
    assert image[0x10EC5D:0x10EC67]==bytes.fromhex('c7 85 10 1e 00 00 09 00 00 00'),'Farm type mapping changed'
    # Electric building types written by the parser into descriptor+0x360.
    for rva,value in ((0x10EF29,0x11),(0x10EF4F,0x12),(0x10EF75,0x13),(0x10F0F1,0x1F),(0x10F117,0x20),(0x10E9EE,0x3C)):
        assert image[rva:rva+10]==bytes.fromhex('c7 85 10 1e 00 00')+value.to_bytes(4,'little'),'Electric type mapping changed at %#x'%rva
    # Office types: construction 0x0C, demolition 0x6A and (since 0.9) rail construction office 0x1C.
    for rva,value in ((0x10EDF9,0x0C),(0x10EE45,0x6A),(0x10EE1F,0x1C)):
        assert image[rva:rva+10]==bytes.fromhex('c7 85 10 1e 00 00')+value.to_bytes(4,'little'),'Office type mapping changed at %#x'%rva
    # Traction transformers: $TYPE_RAIL_TRAFO 0x23 (rail, trolleybus, tram, metro).
    assert image[0x10F206:0x10F210]==bytes.fromhex('c7 85 10 1e 00 00 23 00 00 00'),'Rail transformer type changed'
    # Steam pipes: the pipe tool (owner of [game+0xF858], the type-7 road tool at 0x16BCD6) takes
    # variant 5 for steam and matches existing pipes by road sublevel +0x124.
    assert image[0x16BCD6:0x16BCDD]==bytes.fromhex('83 bb 2c 01 00 00 07'),'Pipe road type dispatch changed'
    assert image[0x16BCEE:0x16BCF5]==bytes.fromhex('48 8b 8e 58 f8 00 00'),'Pipe tool slot changed'
    assert image[0x3AF072:0x3AF079]==bytes.fromhex('48 8b 81 58 f8 00 00'),'Pipe tool entry changed'
    assert image[0x3AF541:0x3AF54E]==bytes.fromhex('41 83 fd 05 75 09 45 39 af 24 01 00 00'),'Steam sublevel match changed'
    assert image[0x2F2BDC:0x2F2BDF]==bytes.fromhex('8d 50 05'),'pipe_steam variant changed'
    # Road section buildings: road depot 0x0E, gas station 0x22, parking 0x26 and cargo station 0
    # (written from r13d, which the parser zeroes at 0x10E2C0 and does not change before 0x10EC8A).
    for rva,value in ((0x10EE91,0x0E),(0x10F1AF,0x22),(0x10EF03,0x26)):
        assert image[rva:rva+10]==bytes.fromhex('c7 85 10 1e 00 00')+value.to_bytes(4,'little'),'Road building type mapping changed at %#x'%rva
    assert image[0x10E2C0:0x10E2C3]==bytes.fromhex('45 33 ed'),'Parser zero register changed'
    assert image[0x10EC8A:0x10EC91]==bytes.fromhex('44 89 ad 10 1e 00 00'),'Cargo station type mapping changed'
    assert image[0x10EC37:0x10EC41]==bytes.fromhex('c7 85 10 1e 00 00 1a 00 00 00'),'Fire station type mapping changed'
    # Ship section: ship docks 0x2A, and every other ship building by $SUBTYPE_SHIP (0x13 at descriptor+0x364).
    assert image[0x10EEDD:0x10EEE7]==bytes.fromhex('c7 85 10 1e 00 00 2a 00 00 00'),'Ship dock type mapping changed'
    assert image[0x10FAE4:0x10FAEE]==bytes.fromhex('c7 85 14 1e 00 00 13 00 00 00'),'Ship subtype mapping changed'
    # Rail section: rail depot 0x0F, rail distribution office 0x34, $SUBTYPE_RAIL 0x1A, and passenger
    # stations (type 1, written from ebx) admitted by the train station names 1802/1804.
    for rva,value in ((0x10EEB7,0x0F),(0x10F2EA,0x34)):
        assert image[rva:rva+10]==bytes.fromhex('c7 85 10 1e 00 00')+value.to_bytes(4,'little'),'Rail building type mapping changed at %#x'%rva
    assert image[0x1187EB:0x1187F5]==bytes.fromhex('c7 85 14 1e 00 00 1a 00 00 00'),'Rail subtype mapping changed'
    assert image[0x10ECAD:0x10ECB3]==bytes.fromhex('89 9d 10 1e 00 00'),'Passenger station type mapping changed'
    # Cargo & storage: distribution office 0x2B, container facility 0x2C, forklift garage 0x2D and the
    # cableway subtype 0x20; storages keep the type 5 check above (any name since 0.17).
    for rva,value in ((0x10F2C4,0x2B),(0x10F310,0x2C),(0x10F22C,0x2D)):
        assert image[rva:rva+10]==bytes.fromhex('c7 85 10 1e 00 00')+value.to_bytes(4,'little'),'Cargo type mapping changed at %#x'%rva
    # 0.17: airports, border, waste management, water and sewage (parser values of descriptor+0x360).
    for rva,value in ((0x10F29E,0x2E),(0x10F252,0x2F),(0x10F278,0x30),(0x10F0CB,0x14),(0x10F4D8,0x6B),(0x10F336,0x31),(0x10F48C,0x69),
                      (0x10ED61,0x5A),(0x10ED87,0x5B),(0x10EB64,0x5C),(0x10EB94,0x5C),(0x10E8B6,0x61),(0x10EDD3,0x5D),(0x10EDAD,0x5E),
                      (0x10E902,0x5F),(0x10E8DC,0x62)):
        assert image[rva:rva+10]==bytes.fromhex('c7 85 10 1e 00 00')+value.to_bytes(4,'little'),'0.17 type mapping changed at %#x'%rva
    # Vehicle repair stations: $TYPE_REPAIR_OFFICE 0x6C, admitted with the road buildings.
    assert image[0x10EE6B:0x10EE75]==bytes.fromhex('c7 85 10 1e 00 00 6c 00 00 00'),'Repair station type mapping changed'
    assert image[0x10FC24:0x10FC2E]==bytes.fromhex('c7 85 14 1e 00 00 20 00 00 00'),'Cableway subtype mapping changed'
    # Conveyor section: $TYPE_ENGINE 0x0B; conveyor segments are type 6, owned by the conveyor tool
    # [game+0xF800] (0x16BCB2), which the 'bulk' tool calls with variant 5 (0x2F28F3) and 'conveyor' with 0.
    assert image[0x10ED3B:0x10ED45]==bytes.fromhex('c7 85 10 1e 00 00 0b 00 00 00'),'Engine type mapping changed'
    assert image[0x16BCB2:0x16BCD1]==bytes.fromhex('83 bb 2c 01 00 00 06 75 1b c6 44 24 20 00 41 83 c9 ff 45 33 c0 48 8b d3 48 8b 8e 00 f8 00 00'),'Conveyor road type dispatch changed'
    assert image[0x2F28F3:0x2F2903]==bytes.fromhex('44 8b c1 45 33 c9 48 8b cf 33 d2 e8 0d bd 0b 00'),'Bulk conveyor tool call changed'
    # Trees & accessories: $TYPE_MONUMENT 0x21, $TYPE_PEDESTRIAN_BRIDGE 0x50 (fence gates, pergolas, colonnades
    # share it with pedestrian bridges and metro entrances) and fence segments, type 15 (tool [game+0xF8B0]).
    for rva,value in ((0x10F13D,0x21),(0x10E9A2,0x50)):
        assert image[rva:rva+10]==bytes.fromhex('c7 85 10 1e 00 00')+value.to_bytes(4,'little'),'Decoration type mapping changed at %#x'%rva
    assert image[0x16BCFA:0x16BD01]==bytes.fromhex('83 bb 2c 01 00 00 0f'),'Fence road type dispatch changed'
    # Rail construction: track crew at 0x159657 (crew loop end, reached by je from 0x1595DF after the crew
    # refill 0x66BD60, which only fills vehicles of a 0x1C office; 0x66BD30 marks skill 27 as automatic),
    # and the office refuel gate at 0x142345 inside the rail office tick 0x1403F0.
    assert image[0x159650:0x15966B]==bytes.fromhex('41 b5 01 8b 5c 24 40 48 8b 86 08 17 00 00 48 8b 80 e0 85 00 00 f3 44 0f 59 48 0c'),'Track crew site changed'
    assert image[0x1595A0:0x1595A9]==bytes.fromhex('e8 8b 27 51 00 84 c0 74 19'),'Automatic rail machine check changed'
    assert image[0x1595C2:0x1595C7]==bytes.fromhex('e8 99 27 51 00'),'Track crew refill call changed'
    assert image[0x1595D8:0x1595E1]==bytes.fromhex('48 c1 f8 03 48 85 c0 74 76'),'Empty crew branch changed'
    assert image[0x66BDAC:0x66BDBA]==bytes.fromhex('48 8b 82 18 03 00 00 83 b8 60 03 00 00 1c'),'Crew refill office check changed'
    assert image[0x66BD4E:0x66BD58]==bytes.fromhex('48 8b 82 e0 85 00 00 83 38 1b'),'Automatic skill check changed'
    assert image[0x13E15A:0x13E16E]==bytes.fromhex('83 b8 60 03 00 00 1c 75 10 49 8b d0 48 8b ce e8 82 22 00 00'),'Rail office dispatch changed'
    assert image[0x142330:0x14234E]==bytes.fromhex('49 8b 87 c8 0b 00 00 49 2b 87 c0 0b 00 00 48 c1 f8 03 48 3b c1 77 2a 48 8b 8b 08 17 00 00'),'Rail office refuel gate changed'
    # Infrastructure sites: +0x5D0 is the road object and its +0x12C is SOVIET_ROADTYPE (script
    # handler 70001); the construction routine only diverts rails (type 1) before the worker path.
    assert image[0x5C64B0:0x5C64B7]==bytes.fromhex('48 8b 80 d0 05 00 00'),'Road-of-construction-building read changed'
    assert image[0x5C69FC:0x5C6A02]==bytes.fromhex('8b 80 2c 01 00 00'),'Road type field changed'
    assert image[0x1594AD:0x1594CD]==bytes.fromhex('4c 39 bf d0 05 00 00 0f 84 47 0b 00 00 4c 8b 8f d0 05 00 00 4c 89 4c 24 48 45 39 a9 2c 01 00 00'),'Infrastructure dispatch changed'
    md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
    md.detail=True
    # Renovation (0.18): maintenance mode [rbp+0x240]=1 is set at 0x1591A3 when the setting 0x9D54CC is on, the
    # building's +0x879 is set, +0x604 >= 1 and the first phase is kind 0x12/0x13; the routine then loads the
    # renovation progress from 0x1AA120 into XMM15 (0x159274), which nothing else writes before the hook.
    assert image[0x159141:0x159148]==bytes.fromhex('44 39 3d 84 c3 87 00'),'Renovation setting check changed'
    assert image[0x15914E:0x159155]==bytes.fromhex('44 38 ba 79 08 00 00'),'Renovation building flag changed'
    assert image[0x159194:0x1591AA]==bytes.fromhex('83 78 04 12 74 06 83 78 04 13 75 7c 41 b4 01 44 88 a5 40 02 00 00'),'Maintenance mode flag changed'
    assert image[0x159264:0x159278]==bytes.fromhex('45 84 e4 74 19 48 8b d7 49 8b ce e8 ac 0e 05 00 44 0f 28 f8'),'Renovation progress load changed'
    assert image[0x15A408:0x15A40F]==bytes.fromhex('44 38 bd 40 02 00 00'),'Maintenance mode read after the hook changed'
    for ins in md.disasm(image[0x159107:0x15A39D],0x159107):
        written=ins.op_str.split(',')[0].strip()=='xmm15' and ins.mnemonic not in ('comiss','ucomiss')
        assert not written or ins.address in (0x159107,0x159274),'XMM15 written before the hook at %#x'%ins.address
    stolen=list(md.disasm(image[0x15A39D:0x15A3A4],0x15A39D))
    assert [i.mnemonic for i in stolen]==['movd','cvtdq2ps'] and sum(i.size for i in stolen)==7
    for ins in md.disasm(image[0x159070:0x15D1D4],0x159070):
        if ins.mnemonic.startswith('j') and ins.operands[0].type==capstone.x86.X86_OP_IMM:
            assert not 0x15A39D<ins.operands[0].imm<0x15A3A4,'Branch enters stolen instructions'
            assert not 0x159657<ins.operands[0].imm<0x15965E,'Branch enters track crew instructions'
    for ins in md.disasm(image[0x1403F0:0x142425],0x1403F0):
        if ins.mnemonic.startswith('j') and ins.operands[0].type==capstone.x86.X86_OP_IMM:
            assert ins.operands[0].imm!=0x142346,'Branch enters refuel gate'
    (BUILD/'construction_static_validation.json').write_text(json.dumps({
        'sha256':EXPECTED_SHA,'patch_rva':'0x15a39d','stolen':['movd xmm6, esi','cvtdq2ps xmm6, xmm6'],
        'scope':['storage type5 6203','storage type5 6202','small farm type9 6115',
                 'electric buildings type17/18/19/31/32/60','power lines road type 8/9',
                 'construction offices type12','demolition offices type106',
                 'traction transformers type35','pipes road type 7 (oil sublevel 0, steam sublevel 5)',
                 'roads road type 0/3/4','road buildings type0/14/34/38/108','fire stations type26',
                 'ship buildings subtype19','ship docks type42',
                 'rail buildings type15/28/52','rail subtype26','train stations type1 1802/1804',
                 'rail track crew 0x159657','rail office refuel 0x142345',
                 'storages type5 any name (waste and landfill since 0.17)','cargo type43/44/45','cableway subtype32','road types 5/12',
                 'conveyors road type 6 (conveyor variant 0, bulk variant 5)','engines type11',
                 'monuments type33','accessories type80 except pedestrian and metro names','fences road type 15',
                 'farms type9','fields type8',                 'mines type7, production lines type40, scrapyards type109, factories type6 any name (waste chain since 0.17)',
                 'airports type46/47/48, road type 13','customs type20, foreign pipeline type107',
                 'technical services type49, trash containers type105','water and sewage type90/91/92/93/94/95/97/98, road types 11/16','renovation of admitted buildings in maintenance mode (XMM15 progress)'],'gameplay_validation':False},indent=2),encoding='utf-8')
    print('PASS construction executable context, complete instructions and branch boundaries')


def construction_build():
    construction_verify()
    assert '#define CONSTRUCTION_VERSION "'+CONSTRUCTION_VERSION.split('-')[0]+'"' in (ROOT/'src/industrial_construction.cpp').read_text(encoding='utf-8'),'industrial_construction.cpp version differs from dev.py'
    common=[CLANG,'-std=c++17','-O2','-fms-extensions','-static','-Wall','-Wextra',
            '-Wno-unknown-pragmas','-Wno-unused-function','-Wno-unused-variable']
    run(common+['-shared',ROOT/'src/industrial_construction.cpp','-o',BUILD/'industrial_construction.dll'])
    run(common+[ROOT/'tests/construction_test.cpp',ROOT/'tests/construction_call.S','-o',BUILD/'construction_test.exe'])
    run(common+[ROOT/'tests/rail_test.cpp',ROOT/'tests/rail_call.S','-o',BUILD/'rail_test.exe'])
    run(common+[ROOT/'tests/plugin_host.cpp',ROOT/'tests/stub_call.S',ROOT/'tests/construction_call.S',ROOT/'tests/fire_call.S',ROOT/'tests/rail_call.S',ROOT/'tests/work_call.S',ROOT/'tests/repair_call.S','-o',BUILD/'construction_host.exe'])


def construction_test():
    construction_verify()
    output=run([BUILD/'construction_test.exe'])
    output+=run([BUILD/'rail_test.exe'])
    for mode in ('wrong-version','wrong-architecture','different-code','disabled','late-conflict','allocation-fails','invalid-workers',
                 'valid','valid-farm','valid-substation','valid-powerline','valid-office','valid-steam','valid-trafo','valid-road','valid-depot','valid-firestation','valid-ship',
                 'valid-rail-office','valid-track','rail-different-code','rail-disabled','valid-cargo','valid-monument','valid-field','valid-quarry','coexist'):
        output+=run([BUILD/'construction_host.exe','construction-'+mode])
    # The same host still executes the industry targets, including power plants.
    for mode in ('valid','valid-wood','valid-car','valid-power','valid-ship','valid-train-factory','valid-food-factory','valid-gravel-quarry','valid-workshop-mine'):
        output+=run([BUILD/'construction_host.exe',mode])
    import pefile
    plugin=pefile.PE(str(BUILD/'industrial_construction.dll'))
    exports={x.name.decode() for x in plugin.DIRECTORY_ENTRY_EXPORT.symbols if x.name}
    assert {'TsmPluginApiVersion','TsmPluginInit','TsmPluginStart'}<=exports
    assert plugin.FILE_HEADER.Machine==0x8664
    assert not any('libc++' in x.dll.decode().lower() or 'libstdc++' in x.dll.decode().lower() for x in plugin.DIRECTORY_ENTRY_IMPORT)
    (BUILD/'construction_native_test_results.txt').write_text(output,encoding='utf-8')
    hashes={name:hashlib.sha256((BUILD/name).read_bytes()).hexdigest()
            for name in ('industrial_construction.dll','construction_test.exe','construction_host.exe')}
    (BUILD/'construction_tested_binaries.json').write_text(json.dumps(hashes,indent=2),encoding='utf-8')
    print('PASS construction DLL x64, plugin API and runtime dependencies')


CONSTRUCTION_VERSION='0.18.0-experimental'
PREVIOUS_CONSTRUCTION=('0.17.0-experimental','0.16.0-experimental','0.15.0-experimental','0.14.0-experimental','0.13.0-experimental','0.12.0-experimental','0.11.0-experimental','0.10.0-experimental','0.9.0-experimental','0.8.0-experimental','0.7.0-experimental','0.6.0-experimental','0.5.0-experimental','0.4.0-experimental','0.3.0-experimental','0.2.0-experimental','0.1.0-experimental')


def construction_package():
    tested=json.loads((BUILD/'construction_tested_binaries.json').read_text(encoding='utf-8'))
    for name,digest in tested.items():
        assert hashlib.sha256((BUILD/name).read_bytes()).hexdigest()==digest,'Construction binary changed since testing'
    dist=ROOT/('dist/industrial-construction-'+CONSTRUCTION_VERSION)
    plugins=dist/'tesmioloader/build/plugins'
    plugins.mkdir(parents=True,exist_ok=True)
    shutil.copy2(BUILD/'industrial_construction.dll',plugins/'industrial_construction.dll')
    shutil.copy2(ROOT/'config/industrial_construction.ini',plugins/'industrial_construction.ini')
    for name in ('README.md','guide.md'):
        shutil.copy2(ROOT/name,dist/name)
    shutil.copy2(ROOT/'vendor/TesmioLoader/LICENSE',dist/'COPYING.txt')
    for subdir in ('src','tests','config'):
        shutil.copytree(ROOT/subdir,dist/'source'/subdir,dirs_exist_ok=True)
    shutil.copytree(ROOT/'vendor/TesmioLoader',dist/'source/vendor/TesmioLoader',dirs_exist_ok=True,ignore=shutil.ignore_patterns('.git'))
    for name in ('dev.py','requirements.txt','README.md','guide.md'):
        shutil.copy2(ROOT/name,dist/'source'/name)
    for name in ('construction_native_test_results.txt','construction_static_validation.json'):
        shutil.copy2(BUILD/name,dist/name)
    relative='tesmioloader/build/plugins/industrial_construction.dll'
    (dist/'SHA256.json').write_text(json.dumps({relative:hashlib.sha256((dist/relative).read_bytes()).hexdigest()},indent=2),encoding='utf-8')
    print('PACKAGE '+shutil.make_archive(str(dist),'zip',dist))


def construction_install():
    game=Path(r'F:\SteamLibrary\steamapps\common\SovietRepublic')
    assert hashlib.sha256((game/'SOVIET64.exe').read_bytes()).hexdigest()==EXPECTED_SHA,'Game build changed'
    for name in ('SOVIET64.exe','SOVIET.exe'):
        p=subprocess.run(['tasklist','/FI','IMAGENAME eq '+name,'/FO','CSV','/NH'],capture_output=True,creationflags=subprocess.CREATE_NO_WINDOW)
        if ('"'+name+'"').lower().encode() in p.stdout.lower():
            raise SystemExit('CONSTRUCTION INSTALL PENDING: close the game; package is ready.')
    plugins=game/'tesmioloader/build/plugins'
    assert (plugins/'industrial_tycoon.dll').exists(),'Existing industry mod required'
    source=ROOT/('dist/industrial-construction-'+CONSTRUCTION_VERSION)
    relative='tesmioloader/build/plugins/industrial_construction.dll'
    hashes=json.loads((source/'SHA256.json').read_text(encoding='utf-8'))
    digest=hashlib.sha256((source/relative).read_bytes()).hexdigest()
    assert digest==hashes[relative]
    destination=plugins/'industrial_construction.dll'
    if destination.exists():
        installed=hashlib.sha256(destination.read_bytes()).hexdigest()
        if installed!=digest:
            previous=None
            for version in PREVIOUS_CONSTRUCTION:
                manifest=ROOT/('dist/industrial-construction-'+version)/'SHA256.json'
                if manifest.exists() and installed==json.loads(manifest.read_text(encoding='utf-8')).get(relative):
                    previous=version; break
            assert previous,'Unknown construction DLL; not overwritten'
            backup=destination.with_suffix('.dll.v'+previous+'.backup')
            if backup.exists():
                assert hashlib.sha256(backup.read_bytes()).hexdigest()==installed,'Unknown construction backup; not overwritten'
            else:
                shutil.copy2(destination,backup)
            shutil.copy2(source/relative,destination)
    else:
        shutil.copy2(source/relative,destination)
    ini=plugins/'industrial_construction.ini'
    if not ini.exists(): shutil.copy2(source/'tesmioloader/build/plugins/industrial_construction.ini',ini)
    assert hashlib.sha256(destination.read_bytes()).hexdigest()==digest
    print('CONSTRUCTION INSTALLED '+CONSTRUCTION_VERSION+' '+str(destination)+'; INI kept; industry DLL untouched')


def probe():
    # This upstream flag identifies the game and exits. It does not launch it.
    run([BUILD/'tesmiolauncher.exe','--find','--game',
         r'F:\SteamLibrary\steamapps\common\SovietRepublic\SOVIET64.exe'])


def package():
    tested=json.loads((BUILD/'tested_binaries.json').read_text(encoding='utf-8'))
    for name,digest in tested.items():
        assert hashlib.sha256((BUILD/name).read_bytes()).hexdigest()==digest,'Binary changed since testing'
    dist=ROOT/('dist/industrial-tycoon-'+VERSION)
    plugins=dist/'tesmioloader/build/plugins'
    plugins.mkdir(parents=True,exist_ok=True)
    for name in ('tesmioloader.dll','tesmiolauncher.exe'):
        shutil.copy2(BUILD/name,plugins.parent/name)
    shutil.copy2(BUILD/'industrial_tycoon.dll',plugins/'industrial_tycoon.dll')
    shutil.copy2(ROOT/'config/industrial_tycoon.ini',plugins/'industrial_tycoon.ini')
    shutil.copy2(ROOT/'config/tesmioloader.ini',plugins.parent/'tesmioloader.ini')
    for name in ('README.md','guide.md'):
        shutil.copy2(ROOT/name,dist/name)
    shutil.copy2(ROOT/'vendor/TesmioLoader/LICENSE',dist/'COPYING-TesmioLoader.txt')
    shutil.copy2(ROOT/'vendor/TesmioLoader/LICENSE',dist/'COPYING.txt')
    source=dist/'source'
    source.mkdir(exist_ok=True)
    for subdir in ('src','tests','config'):
        shutil.copytree(ROOT/subdir,source/subdir,dirs_exist_ok=True)
    for name in ('dev.py','requirements.txt','README.md','guide.md'):
        shutil.copy2(ROOT/name,source/name)
    # Source archive is included for the rebuilt GPL loader and ABI headers.
    upstream=source/'vendor/TesmioLoader'
    shutil.copytree(ROOT/'vendor/TesmioLoader',upstream,dirs_exist_ok=True,ignore=shutil.ignore_patterns('.git'))
    shutil.copy2(BUILD/'native_test_results.txt',dist/'native_test_results.txt')
    shutil.copy2(BUILD/'static_validation.json',dist/'static_validation.json')
    hashes={str(p.relative_to(dist)):hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(dist.rglob('*')) if p.is_file() and p.suffix in {'.dll','.exe'}}
    (dist/'SHA256.json').write_text(json.dumps(hashes,indent=2),encoding='utf-8')
    archive=shutil.make_archive(str(dist),'zip',dist)
    print('PACKAGE '+archive)


RELEASE='0.1.0'


def release():
    # Player archive: only the tesmioloader folder, with the binaries of the two tested packages.
    industry=ROOT/('dist/industrial-tycoon-'+VERSION)
    construction=ROOT/('dist/industrial-construction-'+CONSTRUCTION_VERSION)
    tested={}
    for source in (industry,construction):
        for relative,digest in json.loads((source/'SHA256.json').read_text(encoding='utf-8')).items():
            relative=relative.replace('\\','/')
            assert hashlib.sha256((source/relative).read_bytes()).hexdigest()==digest,'Package hash mismatch: '+relative
            if relative.startswith('tesmioloader/'):
                tested[relative]=digest
    dist=ROOT/('dist/wrsr-industrial-tycoon-v'+RELEASE)
    if dist.exists():
        shutil.rmtree(dist)
    loader=dist/'tesmioloader/build'
    (loader/'plugins').mkdir(parents=True)
    for name in ('tesmioloader.dll','tesmiolauncher.exe'):
        shutil.copy2(industry/'tesmioloader/build'/name,loader/name)
    shutil.copy2(ROOT/'config/tesmioloader.ini',loader/'tesmioloader.ini')
    for plugin,source in (('industrial_tycoon',industry),('industrial_construction',construction)):
        shutil.copy2(source/'tesmioloader/build/plugins'/(plugin+'.dll'),loader/'plugins'/(plugin+'.dll'))
        shutil.copy2(ROOT/'config'/(plugin+'.ini'),loader/'plugins'/(plugin+'.ini'))
    # The GPL travels with the binaries; their source is this repository and upstream TesmioLoader.
    shutil.copy2(ROOT/'vendor/TesmioLoader/LICENSE',dist/'tesmioloader/LICENSE.txt')
    hashes={str(p.relative_to(dist)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(loader.rglob('*')) if p.suffix in {'.dll','.exe'}}
    assert hashes==tested,'Release binaries differ from the tested packages'
    print('RELEASE '+shutil.make_archive(str(dist),'zip',dist))


def install():
    game=Path(r'F:\SteamLibrary\steamapps\common\SovietRepublic')
    assert hashlib.sha256((game/'SOVIET64.exe').read_bytes()).hexdigest()==EXPECTED_SHA,'Game build changed'
    destination=game/'tesmioloader'
    if destination.exists():
        raise SystemExit('A loader folder already exists; no files overwritten: '+str(destination))
    source=ROOT/('dist/industrial-tycoon-'+VERSION)
    hashes=json.loads((source/'SHA256.json').read_text(encoding='utf-8'))
    for relative,digest in hashes.items():
        assert hashlib.sha256((source/relative).read_bytes()).hexdigest()==digest,'Package hash mismatch'
    shutil.copytree(source/'tesmioloader',destination)
    (destination/'industrial-tycoon-installation.txt').write_text(
        'Industrial Tycoon '+VERSION+'\nSource project: '+str(ROOT)+'\nRepair stations and plant sources gameplay validation pending.\n',encoding='utf-8')
    run([destination/'build/tesmiolauncher.exe','--find','--game',game/'SOVIET64.exe'])
    print('INSTALLED '+str(destination))


def update():
    game=Path(r'F:\SteamLibrary\steamapps\common\SovietRepublic')
    assert hashlib.sha256((game/'SOVIET64.exe').read_bytes()).hexdigest()==EXPECTED_SHA,'Game build changed'
    for name in ('SOVIET64.exe','SOVIET.exe'):
        processes=subprocess.run(['tasklist','/FI','IMAGENAME eq '+name,'/FO','CSV','/NH'],
                                 capture_output=True,creationflags=subprocess.CREATE_NO_WINDOW)
        if ('"'+name+'"').lower().encode() in processes.stdout.lower():
            raise SystemExit('UPDATE PENDING: game is running; close it and run dev.py update. Package is ready.')
    source=ROOT/('dist/industrial-tycoon-'+VERSION)
    relative='tesmioloader/build/plugins/industrial_tycoon.dll'
    packageHashes={key.replace('\\','/'):value for key,value in
                   json.loads((source/'SHA256.json').read_text(encoding='utf-8')).items()}
    assert hashlib.sha256((source/relative).read_bytes()).hexdigest()==packageHashes[relative]
    destination=game/relative
    current=hashlib.sha256(destination.read_bytes()).hexdigest()
    if current==packageHashes[relative]:
        print('UPDATE already installed'); return
    previous=None
    for version in ('0.15.0-experimental','0.14.0-experimental','0.13.2-experimental','0.13.1-experimental','0.13.0-experimental','0.12.0-experimental','0.11.0-experimental','0.10.0-experimental','0.9.0-experimental','0.8.0-experimental','0.7.0-experimental','0.6.0-experimental','0.5.0-experimental','0.4.0-experimental','0.3.0-experimental','0.2.0-experimental','0.1.0-experimental'):
        manifest=ROOT/('dist/industrial-tycoon-'+version)/'SHA256.json'
        if not manifest.exists():
            continue
        oldHashes={key.replace('\\','/'):value for key,value in
                   json.loads(manifest.read_text(encoding='utf-8')).items()}
        if current==oldHashes.get(relative):
            previous=version
            break
    assert previous,'An unrecognized plugin is installed; no files overwritten'
    backup=destination.with_suffix('.dll.v'+previous+'.backup')
    if backup.exists():
        assert hashlib.sha256(backup.read_bytes()).hexdigest()==current,'Backup differs; not overwritten'
    else:
        shutil.copy2(destination,backup)
    shutil.copy2(source/relative,destination)
    assert hashlib.sha256(destination.read_bytes()).hexdigest()==packageHashes[relative]
    (game/'tesmioloader/industrial-tycoon-installation.txt').write_text(
        'Industrial Tycoon '+VERSION+'\nSource project: '+str(ROOT)+'\nRepair stations and plant sources gameplay validation pending.\n',encoding='utf-8')
    print('UPDATED '+str(destination)+'; previous version backed up')


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('action',choices=['verify','build','test','loader','probe','package','release','install','update',
        'construction-build','construction-test','construction-package','construction-install'])
    args=parser.parse_args()
    {'verify':verify,'build':build,'test':test,'loader':loader,'probe':probe,'package':package,'release':release,'install':install,'update':update,
     'construction-build':construction_build,'construction-test':construction_test,
     'construction-package':construction_package,'construction-install':construction_install}[args.action]()


if __name__=='__main__': main()
