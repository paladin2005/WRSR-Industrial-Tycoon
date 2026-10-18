"""Read-only PE analysis of the project's local executable copy.

Writes disassembly and research notes inside this project only.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import sys

sys.stdout.reconfigure(encoding='utf-8',errors='replace')

import capstone
import pefile

ROOT = Path(__file__).resolve().parent.parent
BINARY = ROOT / 'research' / 'SOVIET64.exe'
OUTPUT = ROOT / 'research'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('action', choices=['summary', 'function', 'field', 'source', 'excerpt', 'xref', 'callers', 'strings'])
    parser.add_argument('value', nargs='?', default='')
    parser.add_argument('--start', type=lambda x: int(x, 0), default=0)
    parser.add_argument('--limit', type=int, default=180)
    parser.add_argument('--at', type=lambda x: int(x, 0), default=None)
    args = parser.parse_args()
    if args.action in {'source', 'excerpt'}:
        if args.action == 'excerpt':
            path = (ROOT / args.value).resolve()
            if not path.is_relative_to(ROOT):
                raise SystemExit('Only project files can be read')
            lines = path.read_text(encoding='utf-8', errors='replace').splitlines()
            for number, line in enumerate(lines[args.start:args.start+args.limit], args.start+1):
                print(f'{number}: {line}')
            return
        rx = re.compile(args.value, re.IGNORECASE)
        count = 0
        for path in sorted((ROOT / 'vendor' / 'TesmioLoader').rglob('*')):
            if path.suffix not in {'.cpp', '.h', '.md'}:
                continue
            for number, line in enumerate(path.read_text(encoding='utf-8', errors='replace').splitlines(), 1):
                if rx.search(line):
                    print(f'{path.relative_to(ROOT)}:{number}: {line}')
                    count += 1
                    if count >= args.limit:
                        return
        return
    pe = pefile.PE(str(BINARY))
    data = BINARY.read_bytes()
    image = pe.get_memory_mapped_image()
    funcs = [(x.struct.BeginAddress, x.struct.EndAddress) for x in pe.DIRECTORY_ENTRY_EXCEPTION]
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    md.detail = True
    if args.action == 'strings':
        rx = re.compile(args.value, re.IGNORECASE)
        matches = [(m.start(), m.group().decode('ascii')) for m in re.finditer(rb'[\x20-\x7e]{5,}', data)
                   if rx.search(m.group().decode('ascii'))]
        for offset, value in matches[args.start:args.start+args.limit]:
            print(f'{pe.get_rva_from_offset(offset):#x}: {value}')
        return
    if args.action == 'callers':
        target = int(args.value, 0)
        import struct
        for begin, end in funcs:
            code = image[begin:end]
            candidates = [i for i, value in enumerate(code[:-4]) if value == 0xe8
                          and begin+i+5+struct.unpack_from('<i', code, i+1)[0] == target]
            if not candidates:
                continue
            for ins in md.disasm(code, begin):
                if ins.mnemonic == 'call' and ins.operands[0].type == capstone.x86.X86_OP_IMM and ins.operands[0].imm == target:
                    print(f'function={begin:#x} reference={ins.address:#x}: {ins.mnemonic} {ins.op_str}')
        return
    if args.action == 'xref':
        token = args.value.encode() + b'\0'
        offsets = [m.start() for m in re.finditer(re.escape(token), data)]
        if not offsets:
            raise SystemExit('Token not found')
        targets = {pe.get_rva_from_offset(offset) for offset in offsets}
        print('Token at RVA ' + ', '.join(hex(t) for t in sorted(targets)))
        import struct
        candidate_functions = set()
        for match in re.finditer(rb'[\x48\x4c]\x8d.', image):
            at = match.start()
            if image[at+2] & 0xc7 != 0x05 or at+7 > len(image):
                continue
            if at+7+struct.unpack_from('<i',image,at+3)[0] in targets:
                span = next(((b,e) for b,e in funcs if b <= at < e), None)
                if span:
                    candidate_functions.add(span)
        for begin, end in sorted(candidate_functions):
            for ins in md.disasm(image[begin:end], begin):
                if any(op.type == capstone.x86.X86_OP_MEM and op.mem.base == capstone.x86.X86_REG_RIP
                       and ins.address + ins.size + op.mem.disp in targets for op in ins.operands):
                    print(f'function={begin:#x} reference={ins.address:#x}: {ins.mnemonic} {ins.op_str}')
        return
    if args.action == 'summary':
        result = {'sha256': hashlib.sha256(data).hexdigest(),
                  'timestamp': hex(pe.FILE_HEADER.TimeDateStamp),
                  'image_base': hex(pe.OPTIONAL_HEADER.ImageBase),
                  'runtime_functions': len(funcs),
                  'sections': [{'name': s.Name.rstrip(b'\0').decode(), 'rva': hex(s.VirtualAddress),
                                'size': s.Misc_VirtualSize} for s in pe.sections]}
        (OUTPUT / 'binary_identity.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
        print(json.dumps(result, indent=2))
    elif args.action == 'function':
        rva = int(args.value, 0)
        span = next(((b, e) for b, e in funcs if b <= rva < e), None)
        if span is None:
            raise SystemExit(f'No RUNTIME_FUNCTION at {rva:#x}')
        begin, end = span
        lines = []
        for ins in md.disasm(image[begin:end], begin):
            annotation = ''
            for op in ins.operands:
                if op.type == capstone.x86.X86_OP_MEM and op.mem.base == capstone.x86.X86_REG_RIP:
                    target = ins.address + ins.size + op.mem.disp
                    annotation += f' [RVA {target:#x}]'
                    sample = image[target:target+100].split(b'\0', 1)[0]
                    if sample and all(32 <= c < 127 for c in sample):
                        annotation += f' {sample.decode()!r}'
            lines.append(f'{ins.address:08x}  {ins.bytes.hex(" "):32} {ins.mnemonic:9} {ins.op_str}{annotation}')
        path = OUTPUT / f'function_{begin:08x}.asm'
        path.write_text('\n'.join(lines) + '\n', encoding='utf-8')
        print(f'{begin:#x}..{end:#x}, {len(lines)} instructions -> {path}')
        start = args.start
        if args.at is not None:
            start = next((i for i, line in enumerate(lines) if int(line[:8], 16) >= args.at), len(lines))
        print('\n'.join(lines[start:start+args.limit]))
    elif args.action == 'field':
        disp = int(args.value, 0)
        found = []
        for begin, end in funcs:
            code = image[begin:end]
            if disp.to_bytes(4, 'little', signed=True) not in code:
                continue
            for ins in md.disasm(code, begin):
                if any(op.type == capstone.x86.X86_OP_MEM and op.mem.disp == disp for op in ins.operands):
                    found.append(f'function={begin:#x} at={ins.address:#x}: {ins.mnemonic} {ins.op_str}')
        (OUTPUT / f'field_{disp:x}.txt').write_text('\n'.join(found) + '\n', encoding='utf-8')
        print('\n'.join(found[args.start:args.start+args.limit]))


if __name__ == '__main__':
    main()
