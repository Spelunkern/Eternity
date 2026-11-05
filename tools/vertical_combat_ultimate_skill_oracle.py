#!/usr/bin/env python3
"""Trace KillSoul's ultimate-mode rebirth-benefit path."""
import itertools
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EIP, UC_X86_REG_ESP
from original_oracle import load_game


def main():
    cpu,digest=load_game();user,zone,stack,stop=(0x30000000,0x31000000,0x20000000,0x10000000)
    for base in (user,zone,stack,stop):cpu.mem_map(base,0x10000)
    def put(at,v):cpu.mem_write(at,struct.pack('<I',v&0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def signed(v):return struct.unpack('<i',struct.pack('<I',v))[0]
    events=[];state={}
    def hook(machine,address,size,_):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x45f740:events.append('I');machine.emu_stop()
        elif address==0x4295a0:
            events.append('B');out=get(sp+12);map_out=get(sp+16);cpu.mem_write(out,struct.pack('<3f',1,2,3));put(map_out,9)
            if state['mutate']:put(user+0x1238,77)
        elif address==0x459ec0:
            if machine.reg_read(UC_X86_REG_ECX)!=user or [get(sp+i) for i in (8,12,16)]!=[4,9,0]:raise RuntimeError('Wrong ultimate Rebirth arguments')
            events.append('R')
    cpu.hook_add(UC_HOOK_CODE,hook)
    for address,cleanup in ((0x4295a0,16),(0x459ec0,16)):cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup))
    print(f'# ps_game.exe SHA256 {digest}; CUser::KillSoul 004599e0 ultimate benefit path 00459a2b; GetBindPos/Rebirth intercepted')
    print('# maximum HP SP MP; benefit; bind mutates HP; final HP SP MP; events')
    limits=(0,1,2,3,-1,-3,-2147483648,2147483647,65537,99999,-65537,123456789)
    for index,benefit,mutate in itertools.product(range(len(limits)),(0,1,0xffffffff),(0,1)):
        maximum=(limits[index],limits[(index*5+1)%len(limits)],limits[(index*7+2)%len(limits)])
        cpu.mem_write(user,bytes(0x7000));cpu.mem_write(zone,bytes(0x1000));put(user+0xe0,zone);put(zone+0x24,zone+0x1000);put(user+0x142c,1);put(user+0x1430,0);put(user+0x145c,17);put(user+0x58d8,benefit);cpu.mem_write(user+0x12f,b'\x03')
        for offset,value in zip((0x17c,0x184,0x180),maximum):put(user+offset,value)
        for offset,value in zip((0x1238,0x1240,0x123c),(10,20,30)):put(user+offset,value)
        put(stack+0xf000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_EAX,user);state['mutate']=mutate;events.clear();cpu.emu_start(0x4599e0,stop,count=1000)
        expected=0x45f740 if benefit==0 else stop
        if cpu.reg_read(UC_X86_REG_EIP)!=expected:raise RuntimeError('Original ultimate branch did not reach boundary')
        current=[signed(get(user+offset)) for offset in (0x1238,0x1240,0x123c)]
        print(*maximum,benefit,mutate,*current,','.join(events),sep='\t')

if __name__=='__main__':main()
