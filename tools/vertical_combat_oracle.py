#!/usr/bin/env python3
"""Original complete AttackMob caller, with unrecovered callees observed as ports."""
import itertools, struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
c,digest=load_game(); base=0x30000000;c.mem_map(base,0x20000)
u,m,item,info,stack,end=(base+x for x in (0,0x3000,0x5000,0x6000,0x10000,0x18000))
def put(a,v):c.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',c.mem_read(a,4))[0]
def hook(c,a,size,_):
    global events
    sp=c.reg_read(UC_X86_REG_ESP); pop=0; result=None
    if a==0x4a16c0:events.append('S');pop=4
    elif a==0x44d010:events.append('H'+str(get(sp+4)));result=success;pop=4
    elif a==0x44d540:
        events.append('C'+str(get(sp+4)));result=critical;pop=8
        ptr=c.reg_read(UC_X86_REG_ESI)
        for i,v in enumerate((0x12345,0x23456,0x34567)):put(ptr+4*i,v)
    elif a==0x45b260:events.append('W')
    elif a==0x490ee0:events.append('D');result=killed;pop=24
    elif a==0x489ed0:events.append('X')
    elif a==0x4910f0:events.append('K');pop=8
    elif a==0x421220:
        events.append('P'+bytes(c.mem_read(get(sp+8),get(sp+12))).hex());pop=32
    elif a==0x49fad0:events.append('T');pop=8
    elif a==0x44c720:events.append('I');pop=4
    elif a==0x505316:pass
    else:return
    c.reg_write(UC_X86_REG_ESP,sp+4+pop)
    if result is not None:c.reg_write(UC_X86_REG_EAX,result)
    c.reg_write(UC_X86_REG_EIP,get(sp))
c.hook_add(UC_HOOK_CODE,hook)
print('# ps_game.exe SHA256',digest,'CUser::AttackMob 0044ce10 complete caller; external callees observed ports')
print('# type blocked success critical killed dead zone events flags')
for typ,blocked,success,critical,killed,dead,zone in itertools.product((1,11,13,14),range(2),range(2),range(2),range(2),range(2),range(2)):
    c.mem_write(u,b'\0'*0x2000);c.mem_write(m,b'\0'*0x1000);c.mem_write(stack,b'\0'*0x1000)
    put(u+0x1d8,item);put(item+0x30,info);put(info+0xa0,typ);put(u+0xdc,42);put(m+0x5c,77);put(u+0xe0,base+0x7000 if zone else 0);put(u+0x142c,1 if dead else 0)
    kind=int(typ in (11,13,14));put(u+0x13a4+kind*44,blocked)
    sp=stack+0x800;put(sp,end);put(sp+4,m);c.reg_write(UC_X86_REG_ESP,sp);c.reg_write(UC_X86_REG_ECX,u);events=[]
    c.emu_start(0x44ce10,end,count=10000)
    flags=bytes(c.mem_read(m+0x251,3)).hex()
    print(typ,blocked,success,critical,killed,dead,zone,'/'.join(events) or '-',flags,sep='\t')
