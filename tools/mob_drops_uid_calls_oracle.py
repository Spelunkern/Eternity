#!/usr/bin/env python3
"""Audit original direct generator calls and absolute global-state references."""
import bisect
import re
import subprocess
import struct
from pathlib import Path
from original_oracle import ROOT, load_game


def main():
    cpu, sha = load_game()
    # Symbol/address catalog is immutable exported evidence, not a new analysis
    # project. Override explicitly if the original exports live elsewhere.
    import argparse
    parser=argparse.ArgumentParser()
    parser.add_argument('functions',type=Path)
    args=parser.parse_args()
    symbols=[]
    for line in args.functions.read_text().splitlines():
        fields=line.split('\t')
        try: symbols.append((int(fields[0],16),fields[1]))
        except ValueError: pass
    symbols.sort();addresses=[row[0] for row in symbols]
    disassembly=subprocess.check_output(['objdump','-d','-Mintel',str(ROOT/'originals/ps_game/ps_game.exe')],text=True)
    print(f'# ps_game.exe SHA256 {sha}; direct code references only; indirect aliasing is not proven absent. GenIDU/SetStartID have no direct call sites in this image. QuestionMgr uses its own generator, not global item state.')
    print('# instruction\towner entry\towner\tinstruction text')
    seen=set()
    for line in disassembly.splitlines():
        if not re.search(r'0x547c(?:e[048c]|f[04])\b|call\s+0x(?:4cbe60|4cbec0|457a50|4494d0)\b',line): continue
        at=int(line.split(':')[0],16);owner=symbols[bisect.bisect_right(addresses,at)-1]
        instruction=line.split('\t',2)[-1].strip()
        seen.add(at)
        print(f'{at:08x}\t{owner[0]:08x}\t{owner[1]}\t{instruction}')

    # Linear objdump can lose instruction alignment after embedded switch data.
    # Independently scan every E8 displacement in .text, then decode each missing
    # direct reference at its actual address. This recovers ByServer::GenID's
    # 00457a51 call, which linear decoding consumes as bytes of a prior table.
    code=bytes(cpu.mem_read(0x401000,0x120000))
    for offset in range(len(code)-4):
        if code[offset]!=0xe8: continue
        at=0x401000+offset
        target=(at+5+struct.unpack_from('<i',code,offset+1)[0])&0xffffffff
        if target not in (0x4cbe60,0x4cbec0,0x457a50,0x4494d0) or at in seen: continue
        owner=symbols[bisect.bisect_right(addresses,at)-1]
        print(f'{at:08x}\t{owner[0]:08x}\t{owner[1]}\tcall 0x{target:08x} (E8 displacement scan; verify function-entry decode)')

if __name__=='__main__':main()

