#!/usr/bin/env python3
"""Trace actual CMob::SendUseSkill packet and zone routing."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
c,digest=load_game();stack,mob,skill,zone,stop=(0x20000000,0x30000000,0x31000000,0x32000000,0x33000000)
for a in (stack,mob,skill,zone,stop):c.mem_map(a,0x10000)
def put(a,*v):c.mem_write(a,struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v)))
def get(a):return struct.unpack('<I',c.mem_read(a,4))[0]
state={}
def hook(m,a,size,ctx):
    sp=m.reg_read(UC_X86_REG_ESP)
    if a==0x421220:
        args=[get(sp+i) for i in range(4,36,4)]
        assert args[0]==zone and args[2:]==[21,mob+0x50,0x42700000,0,state['target'],6]
        state['packet']=bytes(c.mem_read(args[1],21)).hex()
for a,ret in ((0x421220,b'\xc2\x20\x00'),(0x505316,b'\xc3')):
    c.mem_write(a,ret);c.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
print('# ps_game.exe SHA256 '+digest+'; CMob::SendUseSkill 004a3f50; PSendRange intercepted')
print('# mob target slot skill level zone | packet')
for mid,target,slot,sid,level,has_zone in ((77,42,0,1,1,1),(0xffffffff,0x12345678,-1,65535,255,1),(55,42,257,257,17,1),(77,42,2,1,1,0)):
    c.mem_write(mob,bytes(0x1000));c.mem_write(skill,bytes(0x100));put(mob+0x5c,mid);put(mob+0x130,slot);put(mob+0x60,zone if has_zone else 0);c.mem_write(skill,struct.pack('<HB',sid,level));put(stack+0x8000,stop,mob);state.update(packet='-',target=target)
    c.reg_write(UC_X86_REG_ESP,stack+0x8000);c.reg_write(UC_X86_REG_ECX,skill);c.reg_write(UC_X86_REG_ESI,target);c.emu_start(0x4a3f50,stop,count=10000);assert c.reg_read(UC_X86_REG_EIP)==stop
    print(mid,target,slot,sid,level,has_zone,state['packet'],sep='\t')
