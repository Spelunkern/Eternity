#!/usr/bin/env python3
"""Trace original control settings and a precision-sensitive AttackSuccess."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
c,digest=load_game();base=0x30000000;c.mem_map(base,0x20000)
u,m,definition,stack,end=(base+x for x in (0,0x3000,0x5000,0x10000,0x18000))
def put(a,v):c.mem_write(a,struct.pack('<I',v&0xffffffff))
def word(a,v):c.mem_write(a,struct.pack('<H',v))
def get(a):return struct.unpack('<I',c.mem_read(a,4))[0]
def hook(c,a,size,_):
 global draws
 if a!=0x504697:return
 draws+=1;sp=c.reg_read(UC_X86_REG_ESP);c.reg_write(UC_X86_REG_EAX,roll);c.reg_write(UC_X86_REG_EIP,get(sp));c.reg_write(UC_X86_REG_ESP,sp+4)
c.hook_add(UC_HOOK_CODE,hook)
print('# ps_game.exe SHA256',digest)
for control in (0x37f,0x77f):
 c.reg_write(UC_X86_REG_FPCW,control);sp=stack+0x800;put(sp,end);c.reg_write(UC_X86_REG_ESP,sp);c.emu_start(0x51136e,end,count=5000)
 print('_setdefaultprecision',hex(control),hex(c.reg_read(UC_X86_REG_FPCW)))
roll=69
for control in (0x27f,0x37f):
 c.mem_write(u,b'\0'*0x2000);c.mem_write(m,b'\0'*0x1000);put(m+0xa8,definition);word(u+0x136,30);word(definition+0x22,60)
 put(u+0x13b8,100);put(u+0x13c0,50);put(m+0x138,50);put(m+0x140,50)
 sp=stack+0x800;put(sp,end);put(sp+4,0);c.reg_write(UC_X86_REG_ESP,sp);c.reg_write(UC_X86_REG_ECX,m);c.reg_write(UC_X86_REG_EDI,u);c.reg_write(UC_X86_REG_EAX,0);c.reg_write(UC_X86_REG_FPCW,control);c.reg_write(UC_X86_REG_FPSW,0);c.reg_write(UC_X86_REG_FPTAG,0xffff);draws=0
 c.emu_start(0x44d010,end,count=5000)
 print('AttackSuccess',hex(control),c.reg_read(UC_X86_REG_EAX)&255,draws,hex(c.reg_read(UC_X86_REG_FPCW)))
# Same roll, but precision stays 53-bit and rounding changes.
for control in (0x27f,0x67f):
 c.mem_write(u,b'\0'*0x2000);c.mem_write(m,b'\0'*0x1000);put(m+0xa8,definition);word(u+0x136,65535);word(definition+0x22,0)
 put(u+0x13e4,0);put(u+0x13ec,100);put(m+0x154,0x80000000);put(m+0x15c,10)
 sp=stack+0x800;put(sp,end);put(sp+4,1);c.reg_write(UC_X86_REG_ESP,sp);c.reg_write(UC_X86_REG_ECX,m);c.reg_write(UC_X86_REG_EDI,u);c.reg_write(UC_X86_REG_EAX,0);c.reg_write(UC_X86_REG_FPCW,control);c.reg_write(UC_X86_REG_FPSW,0);c.reg_write(UC_X86_REG_FPTAG,0xffff);draws=0
 # Hook reads a mutable global roll.
 roll=59
 c.emu_start(0x44d010,end,count=5000)
 print('AttackSuccess rounding',hex(control),c.reg_read(UC_X86_REG_EAX)&255,draws,hex(c.reg_read(UC_X86_REG_FPCW)))
