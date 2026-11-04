#!/usr/bin/env python3
"""Run original SetKeepObelisk, real portal lookup and signed 64-bit timer math."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP)
from original_oracle import load_game


def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x100000)
    world,maps,ob,links,portal= [base+x for x in (0,0x1000,0x3000,0x6000,0x8000)]
    stack,stop=base+0xe0000,base+0xf0000;state={}
    def put(at,*values): cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    def get(at): return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def event(*values): state['trace'].extend(v&0xffffffff for v in values)
    for at in (0x4cb8e0,0x4cb9d0): cpu.mem_write(at,b'\xc3')
    def hook(machine,address,size,data):
        if address==0x4cb8e0:
            now=(0xa1b2c3d4+state['scenario'])&0xffffffff;event(1,now)
            if state['scenario']%11==0:
                for mi,count in enumerate(state['counts']):
                    for row in range(count):
                        at=ob+mi*0x400+row*0x70;put(at+0x14,get(at+0x14)^0x00400000);put(at+4,-5)
            machine.reg_write(UC_X86_REG_EAX,now)
        elif address==0x4cb9d0:
            event(2,machine.reg_read(UC_X86_REG_EAX),machine.reg_read(UC_X86_REG_ECX))
            machine.reg_write(UC_X86_REG_EAX,state['seconds'])
    cpu.hook_add(UC_HOOK_CODE,hook)
    print(f'# ps_game.exe SHA256 {digest}; real CWorld::SetKeepObelisk 00413bd0, FindPortalLink 0042b500 and CRT _allmul 0050d3b0. Packed-calendar acquisition/subtraction are observed required ports. No SQL or transport stub is invoked.')
    print('# scenario followed by uint32 words: 1=calendar(now); 2=seconds_between(now,change); 99=ordinary(country,last,reset_time,change,base_id,current,deadline); 98=portal(backing country,open0,open1). Packed now callback mutates live record IDs/countries in scenarios divisible by 11.')
    for scenario in range(512):
        cpu.mem_write(base,bytes(0xff000));counts=[(scenario//3+mi)%4 for mi in range(3)]
        packed=((-1,0,1,2,3,100,-0x80000000,0x7fffffff)[scenario//3%8] if scenario%3==0 else (scenario*73)^0xdead0000)&0xffffffff
        requested=(0x10010203 if scenario%3==0 else 0x10203)+(scenario//7%3)
        change=0 if scenario%5==0 else 0xf2345678+scenario
        seconds=(0,1,86399,86400,86401,0x7fffffff,0x80000000,0xffffffff)[scenario//2%8]
        state.update(scenario=scenario,counts=counts,seconds=seconds,trace=[])
        put(world+8,3,maps);put(0x5471c8,3,links)
        for mi,count in enumerate(counts):
            put(maps+mi*0x1f0+0x1e8,count,ob+mi*0x400)
            for row in range(count):
                at=ob+mi*0x400+row*0x70
                put(at+4,70+mi*10+row,-30-mi*10-row,0x11110000+mi*16+row,0x22220000+mi*16+row,0x10203+row%2)
                put(at+0x48,9,3334)
        for i in range(3):
            at=links+i*0x84;put(at+4,0x10010203+i%2)
            put(at+0x14,portal+i*0x40 if (scenario//5+i)%3 else 0)
            cpu.mem_write(at+0x18,bytes([(scenario+i)%2,(scenario//2+i)%2]));put(portal+i*0x40+0x14,-7+i)
        put(stack,stop,requested,packed,change);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_EAX,world)
        cpu.emu_start(0x413bd0,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop: raise RuntimeError('Original instruction limit exceeded')
        for mi,count in enumerate(counts):
            for row in range(count):
                at=ob+mi*0x400+row*0x70;event(99,*[get(at+x) for x in (4,8,12,16,20,0x48,0x4c)])
        for i in range(3): event(98,get(portal+i*0x40+0x14),*cpu.mem_read(links+i*0x84+0x18,2))
        print(scenario,' '.join(map(str,state['trace'])),sep='\t')

if __name__=='__main__': main()
