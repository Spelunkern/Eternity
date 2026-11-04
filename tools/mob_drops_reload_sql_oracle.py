#!/usr/bin/env python3
"""Execute original reload row writes over read-only restored selective SQL results."""
import argparse
import json
import struct
from pathlib import Path
from original_oracle import load_game
from unicorn.x86_const import UC_X86_REG_EBP,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('snapshot',type=Path);args=parser.parse_args();cases=json.loads(args.snapshot.read_text())
    cpu,sha=load_game();base=0x30000000;cpu.mem_map(base,0x1000000);table,pointer,counter,stack=base+0x100000,base+0x10000,base+0x11000,base+0xf00000
    cpu.mem_write(0x4d9510,b'\xc3')
    def put(at,value):cpu.mem_write(at,struct.pack('<I',value&0xffffffff))
    print(f'# ps_game.exe SHA256 {sha}; actual ReLoadConstMobItem comparison/write/counter block 004c3043..004c3105 over restored usp_Read_MobItems_R2 projections. Full query/GetData/failure coordinator has separate synthetic fixtures. Hash covers every live slot and cumulative DWORD change counter.')
    for case in cases:
        count=case['count'];put(pointer,count);put(pointer+4,table);put(counter,case['counter'])
        for m in range(count):
            for i in range(10):
                grade,rate=case['initial'][(m*10+i)*2:(m*10+i)*2+2];cpu.mem_write(table+m*0x10c+0xbc+i*8,struct.pack('<H2xI',grade,rate))
        for mob,order,grade,rate in case['rows']:
            cpu.mem_write(stack+0xc2c,struct.pack('<H',mob));cpu.mem_write(stack+0xf,bytes([order]));cpu.mem_write(stack+0x10,struct.pack('<H',grade));put(stack+0x18,rate)
            cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_EBP,pointer-0xa81804);cpu.reg_write(UC_X86_REG_EDI,counter);cpu.emu_start(0x4c3043,0x4c3105,count=1000)
            if cpu.reg_read(UC_X86_REG_EIP)!=0x4c3105:raise RuntimeError('Original SQL reload row did not finish')
        result=14695981039346656037
        for m in range(count):
            for i in range(10):
                data=bytes(cpu.mem_read(table+m*0x10c+0xbc+i*8,2))+bytes(cpu.mem_read(table+m*0x10c+0xc0+i*8,4))
                for byte in data:result=((result^byte)*1099511628211)&0xffffffffffffffff
        value=struct.unpack('<I',cpu.mem_read(counter,4))[0]
        for byte in struct.pack('<I',value):result=((result^byte)*1099511628211)&0xffffffffffffffff
        print('A',case['request'],value,result,len(case['rows']),sep='\t')

if __name__=='__main__':main()
