#!/usr/bin/env python3
"""Trace ps_game's mob Attack gates before the target-side body."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,mob,user,stop=0x20000000,0x30000000,0x31000000,0x10000000
for address in (stack,mob,user,stop):cpu.mem_map(address,0x10000)
def put(a,*v):cpu.mem_write(a,struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v)))
state={}
def hook(machine,address,size,context):
 if address==0x4a2056:state['calls']+=1;machine.reg_write(UC_X86_REG_EIP,0x4a23d4)
cpu.hook_add(UC_HOOK_CODE,hook,begin=0x4a2056,end=0x4a2056);cpu.mem_write(0x505316,b'\xc3')
print('# ps_game.exe SHA256 '+digest+'; CMob::Attack 004a2000; target-side continuation intercepted')
print('# sleep stun speed disable0 disable1 disable2 | calls')
for sleep,stun,speed,d0,d1,d2 in ((0,0,3,0,0,0),(1,0,3,0,0,0),(0,1,3,0,0,0),(0,0,3,1,0,0),(0,0,3,0,1,1),(0,0,255,0,0,0),(0,0,-1,0,0,0),(0,0,3,-1,0,0)):
 cpu.mem_write(mob,bytes(0x1000));put(mob+0x248,stun,sleep);put(mob+0x240,speed);put(mob+0x134,d0,d1,d2);state['calls']=0
 put(stack+0x8000,stop,100);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ECX,mob);cpu.reg_write(UC_X86_REG_EDX,user);cpu.emu_start(0x4a2000,stop,count=10000)
 if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Mob Attack guard did not return')
 print(sleep,stun,speed,d0,d1,d2,state['calls'],sep='\t')
