#!/usr/bin/env python3
"""Execute original Rebirth raw stores and EnterZone presentation reset boundary."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
from original_oracle import load_game

def main():
    cpu,sha=load_game();base=0x30000000;cpu.mem_map(base,0x10000)
    def put(at,value):cpu.mem_write(base+at,struct.pack('<I',value&0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(base+at,4))[0]
    print(f'# ps_game.exe SHA256 {sha}; Rebirth raw resource copy 00459ef6..00459f1a and presentation clear instruction 0045a148..0045a14e. Exact original instruction slices; no action ports or full-Rebirth gameplay claim. Original resource offsets current1238/123c/1240, maxima17c/180/184; life142c,presentation1434,attack1448.')
    values=[0,1,65535,65536,0x7fffffff,0x80000000,0xffffffff,123456789]
    for case in range(512):
        hp,mp,sp=[values[(case//(8**i))%8] for i in range(3)];life=(0,1,2,0xffffffff)[case%4];attack=case%256;motion=(case*7)%256
        cpu.mem_write(base,bytes(0x2000));put(0x17c,hp);put(0x180,mp);put(0x184,sp)
        put(0x1238,0xaabbccdd);put(0x123c,0x11223344);put(0x1240,0x55667788);put(0x142c,life)
        cpu.mem_write(base+0x1434,bytes([motion]));cpu.mem_write(base+0x1448,bytes([attack]));cpu.reg_write(UC_X86_REG_EDI,base)
        cpu.emu_start(0x459ef6,0x459f1a,count=100)
        if cpu.reg_read(UC_X86_REG_EIP)!=0x459f1a:raise RuntimeError('Incomplete original raw stores')
        # Isolate only the presentation store; recovery/life tail belongs to the
        # existing worker08 coordinator and must not be duplicated by Actor APIs.
        from unicorn.x86_const import UC_X86_REG_ECX
        cpu.reg_write(UC_X86_REG_ECX,0);cpu.emu_start(0x45a148,0x45a14e,count=2)
        print(case,hp,mp,sp,life,attack,motion,get(0x1238),get(0x123c),get(0x1240),get(0x142c),cpu.mem_read(base+0x1434,1)[0],cpu.mem_read(base+0x1448,1)[0],sep='\t')

if __name__=='__main__':main()
