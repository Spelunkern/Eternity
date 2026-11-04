#!/usr/bin/env python3
"""Execute real CreateCraftExpansion over the isolated SQL item projection."""
import argparse
import json
import struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('items',type=Path);args=parser.parse_args()
    rows=json.loads(args.items.read_text());cpu,sha=load_game();base=0x30000000;cpu.mem_map(base,0x100000)
    item,info,stack,stop=base,base+0x2000,base+0xf0000,base+0xff000;state={}
    for at in (0x504697,0x505316):cpu.mem_write(at,b'\xc3')
    def rand(machine,at,size,_):
        state['seed']=(state['seed']*214013+2531011)&0xffffffff;state['calls']+=1;machine.reg_write(UC_X86_REG_EAX,(state['seed']>>16)&32767)
    cpu.hook_add(UC_HOOK_CODE,rand,begin=0x504697,end=0x504697)
    result=14695981039346656037;draws=0;expanded=0
    for i,row in enumerate(rows):
        type,type_id,server,wis,real=row
        cpu.mem_write(item+0x30,struct.pack('<I',info));cpu.mem_write(info+36,bytes([type,type_id]));cpu.mem_write(info+74,bytes([server]));cpu.mem_write(info+62,struct.pack('<H',wis));cpu.mem_write(info+160,struct.pack('<I',real))
        cpu.mem_write(item+0x4c,b'abcdefghijklmnopqrst\0');cpu.mem_write(item+0x84,struct.pack('<11H',*[100+j for j in range(11)]))
        state.update(seed=(i*7919)&0xffffffff,calls=0);cpu.mem_write(stack,struct.pack('<I',stop));cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ESI,item)
        cpu.emu_start(0x4ba2e0,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('SQL craft expansion did not return')
        payload=bytes(cpu.mem_read(item+0x4c,21))+bytes(cpu.mem_read(item+0x84,22))+struct.pack('<II',state['seed'],state['calls'])
        for byte in payload:result=((result^byte)*1099511628211)&0xffffffffffffffff
        draws+=state['calls'];expanded+=int(server!=0 and 0<wis<100)
    print(f'# ps_game.exe SHA256 {sha}; actual complete CreateCraftExpansion 004ba2e0 and IsAccessory 004b9ee0 over normalized original SQL Items fields. Only explicit CRT and cookie ports. Hash covers 21 craft bytes, eleven retained addition WORDs, final CRT state and draw count per definition.')
    print('A',len(rows),result,draws,expanded,sep='\t')

if __name__=='__main__':main()
