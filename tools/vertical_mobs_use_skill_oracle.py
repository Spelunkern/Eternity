#!/usr/bin/env python3
"""Trace original CMob::UseSkill gates, effect dispatch, and deadline."""
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
    state['trace']+={0x4a4990:'A',0x4a54e0:'H',0x4a5090:'D'}[a]
    if state['mutate']!=999:put(mob+0x240,state['mutate'])
for a in (0x4a4990,0x4a54e0,0x4a5090):c.mem_write(a,b'\xc3');c.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
print('# ps_game.exe SHA256 '+digest+'; CMob::UseSkill 004a48e0; SkillAttack/Heal/Debuff intercepted')
print('# now sleep stun kind disabled effect speed mutate | trace deadline final_speed')
cases=list(itertools.product((100,0xfffffffe),(0,1),(0,1),(0,1,2),(0,1),(0,1,2,3,4),(-1,3,255),(999,6)))
for now,sleep,stun,kind,disabled,effect,speed,mutate in cases:
    c.mem_write(mob,bytes(0x1000));c.mem_write(skill,bytes(0x100));put(mob+0x24c,sleep);put(mob+0x248,stun);put(mob+0x240,speed);put(mob+0x134+kind*28,disabled);put(mob+0xcb4,12345);c.mem_write(skill+0x30,bytes((kind,effect)));put(stack+0x8000,stop,now)
    state.update(trace='',mutate=mutate);c.reg_write(UC_X86_REG_ESP,stack+0x8000);c.reg_write(UC_X86_REG_EAX,skill);c.reg_write(UC_X86_REG_EDI,mob);c.reg_write(UC_X86_REG_EDX,user)
    c.emu_start(0x4a48e0,stop,count=10000);assert c.reg_read(UC_X86_REG_EIP)==stop
    print(now,sleep,stun,kind,disabled,effect,speed,mutate,state['trace'] or '-',get(mob+0xcb4),get(mob+0x240),sep='\t')
