#!/usr/bin/env python3
"""Trace CMob::Attack's casting/duel prefix with target callbacks intercepted."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game

c,digest=load_game()
user,mob,stack=(0x30000000,0x31000000,0x32000000)
for a in (user,mob,stack):c.mem_map(a,0x10000)
def put(a,v):c.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',c.mem_read(a,4))[0]
state={}
def hook(machine,address,size,context):
    sp=machine.reg_read(UC_X86_REG_ESP)
    if address==0x489f10:
        if machine.reg_read(UC_X86_REG_EAX)!=user:raise RuntimeError('bad casting target')
        state['trace']+='S';put(user+0x54cc,state['after'])
        machine.reg_write(UC_X86_REG_EIP,get(sp));machine.reg_write(UC_X86_REG_ESP,sp+4)
    elif address==0x47c750:
        if get(sp+4)!=user:raise RuntimeError('bad duel target')
        state['trace']+='D';machine.reg_write(UC_X86_REG_EIP,get(sp));machine.reg_write(UC_X86_REG_ESP,sp+8)
for a in (0x489f10,0x47c750):c.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
print('# ps_game.exe SHA256',digest,'CMob::Attack 004a20dc..004a20f1; casting and duel intercepted')
print('# initial_duel duel_after_casting | call_order')
for initial in (0,1):
 for after in (0,1):
    put(user+0x54cc,initial);state.update(trace='',after=after)
    c.reg_write(UC_X86_REG_ESP,stack+0x8000);c.reg_write(UC_X86_REG_ESI,user);c.reg_write(UC_X86_REG_EDI,mob);c.reg_write(UC_X86_REG_EBX,0)
    c.emu_start(0x4a20dc,0x4a20f1,count=1000)
    print(initial,after,state['trace']+'H',sep='\t')
