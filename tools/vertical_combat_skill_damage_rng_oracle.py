#!/usr/bin/env python3
"""Original AttackSuccess then AttackDamage with one MSVCRT-compatible stream."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
c,digest=load_game();base=0x30000000;c.mem_map(base,0x20000)
u,m,info,out,skill,stack,end=(base+x for x in (0,0x4000,0x6000,0x7000,0x9000,0x10000,0x18000))
def put(a,v):c.mem_write(a,struct.pack('<I',v&0xffffffff))
def word(a,v):c.mem_write(a,struct.pack('<H',v))
state=19;draws=[]
def hook(c,a,size,_):
 global state
 if a!=0x504697:return
 state=(state*214013+2531011)&0xffffffff;value=(state>>16)&32767;draws.append(value)
 sp=c.reg_read(UC_X86_REG_ESP);ret=struct.unpack('<I',c.mem_read(sp,4))[0];c.reg_write(UC_X86_REG_EAX,value);c.reg_write(UC_X86_REG_EIP,ret);c.reg_write(UC_X86_REG_ESP,sp+4)
c.hook_add(UC_HOOK_CODE,hook);put(m+0xa8,info);word(u+0x136,5);word(info+0x22,30);put(u+0x13b8,100);put(u+0x13c0,100);put(m+0x138,100);put(m+0x140,100);put(u+0x13bc,1000);put(u+0x12dc,-1);put(u+0x1234,1000);put(m+0x234,-100);put(u+0x1280,8)
word(skill,11);word(skill+0x32,21);c.mem_write(skill+0x54,bytes([1,100]));c.mem_write(skill+0x66,bytes([1]));word(skill+0x68,100);word(skill+0x6a,10);word(skill+0x6c,5)
sp=stack+0x800;put(sp,end);put(sp+4,0);c.reg_write(UC_X86_REG_ESP,sp);c.reg_write(UC_X86_REG_EAX,skill);c.reg_write(UC_X86_REG_ECX,m);c.reg_write(UC_X86_REG_EDI,u);c.reg_write(UC_X86_REG_FPCW,0x27f);c.emu_start(0x44d010,end,count=10000);success=c.reg_read(UC_X86_REG_EAX)&255
put(sp,end);put(sp+4,0);put(sp+8,skill);c.reg_write(UC_X86_REG_ESP,sp);c.reg_write(UC_X86_REG_EAX,m);c.reg_write(UC_X86_REG_ECX,u);c.reg_write(UC_X86_REG_ESI,out);c.reg_write(UC_X86_REG_FPCW,0x27f);c.emu_start(0x44d540,end,count=10000);critical=c.reg_read(UC_X86_REG_EAX)&255;damage=struct.unpack('<3i',c.mem_read(out,12))
print('# ps_game.exe SHA256',digest,'AttackSuccess 0044d010 and AttackDamage 0044d540 with CRT seed 19')
print('success',success,'damage',damage,'critical',critical,'draws',draws,'state',state)
if (success,damage,critical,draws,state)!=(1,(104,10,5),0,[100,15331,209],13737300):raise RuntimeError('original shared RNG trace mismatch')
