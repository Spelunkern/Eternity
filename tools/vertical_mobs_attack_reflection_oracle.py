#!/usr/bin/env python3
"""Trace the original mob hit reflection check through the exclusion loop."""
import random,struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game

c,digest=load_game()
user,mob,info,skill,stack=(0x30000000,0x31000000,0x32000000,0x33000000,0x34000000)
for a in (user,mob,info,skill,stack):c.mem_map(a,0x10000)
def put(a,v):c.mem_write(a,struct.pack('<I',v&0xffffffff))
def word(a,v):c.mem_write(a,struct.pack('<H',v))
def get(a):return struct.unpack('<I',c.mem_read(a,4))[0]
state={}
def hook(machine,address,size,context):
    sp=machine.reg_read(UC_X86_REG_ESP)
    if address==0x402770:
        if machine.reg_read(UC_X86_REG_EAX)!=state['skill'] or machine.reg_read(UC_X86_REG_ESI)!=state['level']:raise RuntimeError('wrong skill lookup')
        state['lookups']+=1;machine.reg_write(UC_X86_REG_EAX,skill)
    else:
        state['draws']+=1;machine.reg_write(UC_X86_REG_EAX,state['roll'])
    machine.reg_write(UC_X86_REG_EIP,get(sp));machine.reg_write(UC_X86_REG_ESP,sp+4)
for a in (0x402770,0x504697):c.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
rng=random.Random(0x44e080)
print('# ps_game.exe SHA256',digest,'CMob::Attack 004a20f1..004a2140 and CUser helper 0044e080; skill lookup and CRT random intercepted')
print('# kind skill level melee mode parameter intelligence wisdom roll excluded[6] | reflected lookups draws')
for i in range(1024):
    kind=i%3;sid=rng.choice((0,1,42,100,255,256));level=rng.randrange(1,4);melee=sid if kind==0 else rng.choice((0,1,42,100,255,256));mode=rng.randrange(5);parameter=rng.choice((1,2,10,50,99,100,255))
    intelligence=rng.choice((-2147483648,-100,-1,0,1,10,100,32767,2147483647));wisdom=rng.choice((-2147483648,-100,-1,0,1,10,100,32767,2147483647));roll=rng.randrange(32768)
    excluded=[rng.choice((0,1,42,100,255)) for _ in range(6)]
    c.mem_write(user,bytes(0x6000));c.mem_write(mob,bytes(0x1000));c.mem_write(info,bytes(0x1000));c.mem_write(skill,bytes(0x1000))
    put(mob+0xa8,info);put(mob+0x130,0);word(info+0x80,kind);c.mem_write(info+0x55,bytes(excluded));put(user+0x13b0,melee);put(user+0x13b0+44*kind,sid);put(user+0x13b4+44*kind,level);put(user+0x122c,intelligence);put(user+0x1234,wisdom);c.mem_write(skill+0x54,bytes([mode,parameter]))
    state.update(skill=sid,level=level,roll=roll,lookups=0,draws=0)
    c.reg_write(UC_X86_REG_ESP,stack+0x8000);c.reg_write(UC_X86_REG_ESI,user);c.reg_write(UC_X86_REG_EDI,mob);c.reg_write(UC_X86_REG_EBX,0)
    c.emu_start(0x4a20f1,0x4a2140,count=10000)
    result=c.mem_read(stack+0x8012,1)[0]
    print(kind,sid,level,melee,mode,parameter,intelligence,wisdom,roll,*excluded,result,state['lookups'],state['draws'],sep='\t')
