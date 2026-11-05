#!/usr/bin/env python3
"""Trace CMob::SkillAttack, including its original selected-target dispatcher."""
import itertools,struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
c,digest=load_game();stack,mob,user,skill,stop=(0x20000000,0x30000000,0x31000000,0x32000000,0x33000000)
for a in (stack,mob,user,skill,stop):c.mem_map(a,0x10000)
def put(a,*v):c.mem_write(a,struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v)))
def get(a):return struct.unpack('<I',c.mem_read(a,4))[0]
state={}
def hook(m,a,size,ctx):
    sp=m.reg_read(UC_X86_REG_ESP)
    if a==0x4a3f50:state['trace']+='S'
    elif a==0x4a3fe0:
        assert m.reg_read(UC_X86_REG_ECX)==mob and m.reg_read(UC_X86_REG_EDX)==user and get(sp+4)==skill
        state['trace']+='A' if get(sp+8) else 'a';state['hits']+=1
        if state['killat'] and state['hits']==state['killat']:put(user+0x142c,1)
        if state['mutate'] and state['hits']==1:c.mem_write(skill+0x58,b'\x01')
    elif a==0x41d760:m.reg_write(UC_X86_REG_EAX,get(mob+0x124))
    elif a==0x490cf0:
        assert m.reg_read(UC_X86_REG_EAX)==100 and [get(sp+i) for i in (4,8,12,16,20,24)]==[mob,2,77,0,0,0]
        state['trace']+='X';m.reg_write(UC_X86_REG_EAX,state['dead'])
    elif a==0x4910f0:
        assert m.reg_read(UC_X86_REG_ECX)==mob and [get(sp+i) for i in (4,8)]==[2,77];state['trace']+='K'
    elif a==0x4a4aa0:state['trace']+='R'
    elif a==0x4a4d90:state['trace']+='Q'
for a,ret in ((0x4a3f50,b'\xc2\x04\x00'),(0x4a3fe0,b'\xc2\x08\x00'),(0x41d760,b'\xc3'),(0x490cf0,b'\xc2\x18\x00'),(0x4910f0,b'\xc2\x08\x00'),(0x4a4aa0,b'\xc3'),(0x4a4d90,b'\xc2\x04\x00')):
    c.mem_write(a,ret);c.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
print('# ps_game.exe SHA256 '+digest+'; CMob::SkillAttack 004a4990 and SkillAttackSelect 004a49d0; effect services intercepted')
print('# target detail multi killat status dead mutate | trace final_mode final_multi')
for target,detail,multi,killat,status,dead,mutate in itertools.product((0,3,6,7),(20,21,22,23,24,25,26,27,28),(0,3),(0,1),(1,2),(0,1),(0,1)):
    for a in (mob,skill):c.mem_write(a,bytes(0x1000))
    c.mem_write(user,bytes(0x2000))
    put(mob+0x5c,77);put(mob+0x124,status);put(mob+0x224,100);put(user+0xdc,42);c.mem_write(skill+0x32,struct.pack('<H',detail));c.mem_write(skill+0x56,bytes([target]));c.mem_write(skill+0x58,bytes([multi]));put(stack+0x8000,stop)
    state.update(trace='',hits=0,killat=killat,dead=dead,mutate=mutate);c.reg_write(UC_X86_REG_ESP,stack+0x8000);c.reg_write(UC_X86_REG_EAX,skill);c.reg_write(UC_X86_REG_ECX,mob);c.reg_write(UC_X86_REG_EDX,user)
    c.emu_start(0x4a4990,stop,count=10000);assert c.reg_read(UC_X86_REG_EIP)==stop
    print(target,detail,multi,killat,status,dead,mutate,state['trace'] or '-',get(user+0x142c),c.mem_read(skill+0x58,1)[0],sep='\t')
