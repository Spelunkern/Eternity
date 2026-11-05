#!/usr/bin/env python3
"""Original mob hit reflection plus damage, stopping before application."""
import random,struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game

c,digest=load_game()
user,mob,info,skill,stack=(0x30000000,0x31000000,0x32000000,0x33000000,0x34000000)
for a in (user,mob,info,skill,stack):c.mem_map(a,0x10000)
def put(a,v):c.mem_write(a,struct.pack('<I',v&0xffffffff))
def word(a,v):c.mem_write(a,struct.pack('<H',v))
def real(a,v):c.mem_write(a,struct.pack('<f',v))
def get(a):return struct.unpack('<I',c.mem_read(a,4))[0]
state={}
def hook(machine,address,size,context):
    sp=machine.reg_read(UC_X86_REG_ESP)
    if address==0x402770:
        state['lookups']+=1;machine.reg_write(UC_X86_REG_EAX,skill)
    else:
        machine.reg_write(UC_X86_REG_EAX,state['rolls'][state['draws']]);state['draws']+=1
    machine.reg_write(UC_X86_REG_EIP,get(sp));machine.reg_write(UC_X86_REG_ESP,sp+4)
for a in (0x402770,0x504697):c.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
rng=random.Random(0x4a2140)
print('# ps_game.exe SHA256',digest,'CMob::Attack 004a20f1..004a216d and AttackDamage 004a2920; skill lookup and CRT random intercepted')
print('# kind mirror mode parameter base variance mobwis targetwis rolls[6] | reflected critical damage lookups draws')
for i in range(256):
    kind=i%3;mirror=rng.choice((0,7));mode=rng.randrange(1,4);parameter=rng.choice((1,10,50,100));raw=rng.choice((0,10,100,1000));variance=rng.choice((0,1,25));mw=rng.choice((0,1,100));uw=rng.choice((0,1,50,100));rolls=[rng.randrange(32768) for _ in range(6)]
    for a,n in ((user,0x6000),(mob,0x1000),(info,0x1000),(skill,0x1000),(stack,0x10000)):c.mem_write(a,bytes(n))
    put(mob+0xa8,info);put(mob+0x130,0);word(info+0x80,kind);word(info+0x8a,raw);word(info+0x8c,variance);word(info+0x22,10);word(info+0x3c,mw);word(user+0x136,10);put(user+0x1234,uw);put(user+0x122c,20)
    put(user+0x13b0,7);put(user+0x13b0+44*kind,mirror);put(user+0x13b4+44*kind,1);c.mem_write(info+0x55,bytes([9]*6));c.mem_write(skill+0x54,bytes([mode,parameter]));real(mob+0x268,1.0)
    state.update(rolls=rolls,draws=0,lookups=0);sp=stack+0x8000
    c.reg_write(UC_X86_REG_ESP,sp);c.reg_write(UC_X86_REG_ESI,user);c.reg_write(UC_X86_REG_EDI,mob);c.reg_write(UC_X86_REG_EBX,0)
    c.reg_write(UC_X86_REG_FPCW,0x27f);c.reg_write(UC_X86_REG_FPSW,0);c.reg_write(UC_X86_REG_FPTAG,0xffff)
    c.emu_start(0x4a20f1,0x4a216d,count=10000)
    reflected=c.mem_read(sp+0x12,1)[0];critical=c.mem_read(sp+0x14,1)[0];damage=struct.unpack('<i',c.mem_read(sp+0x1c,4))[0]
    print(kind,mirror,mode,parameter,raw,variance,mw,uw,*rolls,reflected,critical,damage,state['lookups'],state['draws'],sep='\t')
