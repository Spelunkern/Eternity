#!/usr/bin/env python3
"""Execute original instance portal queries and mutations; requires Unicorn."""
import random
import struct
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EDX,UC_X86_REG_EBX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,zone,map_object,stop=0x20000000,0x30000000,0x31000000,0x10000000
    for at in (stack,zone,map_object,stop):cpu.mem_map(at,0x10000)
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def call(at):
        put(stack+0x8000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ESI,zone);cpu.emu_start(at,stop,count=10000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original portal function did not return')
        return cpu.reg_read(UC_X86_REG_EAX)
    print(f'# ps_game.exe SHA256 {digest}; GetInsZonePortalCountry 004179e0, UpdateInsZonePortalCountry 00417a10, no ports.')
    rng=random.Random(0x417a10)
    for scenario in range(128):
        create_type=(0,1,3,6,-1)[scenario%5];put(zone+0x24,map_object);put(map_object+0x1a4,create_type)
        initial=[]
        for slot in range(10):initial.extend((0 if scenario%3==0 or slot==scenario%10 else 1001+slot%4,rng.getrandbits(32)))
        put(zone+0x218,*initial);print('S',create_type,*initial,sep='\t')
        for step in range(64):
            portal=(-2147483648,-1,0,1,999,1000,1001,1002,1003,1004,1005,1010,2147483647)[step%13];country=rng.getrandbits(32)
            cpu.reg_write(UC_X86_REG_EDI,portal&0xffffffff);cpu.reg_write(UC_X86_REG_EBX,country);call(0x417a10)
            cpu.reg_write(UC_X86_REG_EDX,portal&0xffffffff);result=call(0x4179e0)
            print('U',portal,country,result,bytes(cpu.mem_read(zone+0x218,80)).hex(),sep='\t')


if __name__=='__main__':main()
