#!/usr/bin/env python3
"""Execute original Damage/DamageParty; observe required external service ports."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game

def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x20000)
    mob=base;values=base+0x2000;definition=base+0x3000;stack=base+0x10000;stop=base+0x1f000
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    events=[];credit=[];status=0
    def hook(c,at,size,data):
        nonlocal events,credit
        pop=0;result=None
        if at==0x41d760: result=status
        elif at==0x4925f0: events.append('E');pop=4
        elif at==stop+16: events.append('T');result=0xfffffff0
        elif at==0x4a1810: events.append('L');pop=12
        elif at==0x4b8dd0:
            esp=c.reg_read(UC_X86_REG_ESP);credit=[c.reg_read(UC_X86_REG_EBX)]+[get(esp+i) for i in (4,8,12,16)];events.append('A');pop=16
        elif at in (0x4b7490,0x490ab0): events.append('R' if at==0x490ab0 else 'X')
        else:return
        esp=c.reg_read(UC_X86_REG_ESP);ret=get(esp);c.reg_write(UC_X86_REG_ESP,esp+4+pop)
        if result is not None:c.reg_write(UC_X86_REG_EAX,result)
        c.reg_write(UC_X86_REG_EIP,ret)
    cpu.hook_add(UC_HOOK_CODE,hook);put(0x5210e4,stop+16)
    rng=random.Random(0x490cf0)
    print(f'# ps_game.exe SHA256 {digest}; Damage 00490cf0/00490ee0; DamageParty 00490e40/00490e90; original bodies, status/world/Add observed ports')
    print('# mode status hp mp sp flags damage mp_damage sp_damage bonus base result hp mp sp hits state secondary tick events credit(type,id,damage,aggro,party)')
    for case in range(768):
        mode=case%4;status=rng.randrange(4);hp=rng.choice((0,1,100,0x80000000,0xffffffff));mp=rng.randrange(65536);sp=rng.randrange(65536);flags=rng.randrange(64)
        ds=[rng.choice((0,1,99,100,65535,0x7fffffff,0x80000000,0xffffffff)) for _ in range(3)]
        bonus=rng.choice((0,1,0xffffffff,0x7fffffff));aggro=rng.choice((0,3,0xffffffff))
        cpu.mem_write(mob,b'\0'*0x1000);put(mob+0x224,hp);cpu.mem_write(mob+0x228,struct.pack('<HH',mp,sp));put(mob+0x124,7,8)
        put(mob+0x25c,0xffffffff);cpu.mem_write(mob+0x258,bytes([flags&1]));put(mob+0x44,flags&2);put(mob+0xa8,definition);put(definition+0x6c,flags&4);put(mob+0x284,flags&8)
        put(mob+0xc84,definition if flags&16 else 0);cpu.mem_write(definition+0x1c,bytes([bool(flags&32)]));put(values,*ds)
        events=[];credit=[]
        if mode<2:
            put(stack,stop,mob,1,42,bonus,aggro,77);cpu.reg_write(UC_X86_REG_EAX,ds[0] if mode==0 else values);address=(0x490cf0,0x490ee0)[mode]
        else:
            put(stack,stop,42,values if mode==2 else ds[0]);cpu.reg_write(UC_X86_REG_EAX,mob);address=(0x490e40,0x490e90)[mode-2]
        cpu.reg_write(UC_X86_REG_ESP,stack);cpu.emu_start(address,stop,count=10000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('did not return')
        outmp,outsp=struct.unpack('<HH',cpu.mem_read(mob+0x228,4))
        print(*([mode,status,hp,mp,sp,flags,*ds,bonus,aggro,cpu.reg_read(UC_X86_REG_EAX)&255,get(mob+0x224),outmp,outsp,get(mob+0x25c),get(mob+0x124),get(mob+0x128),get(mob+0xcb0),''.join(events) or '-']+credit),sep='\t')
if __name__=='__main__':main()
