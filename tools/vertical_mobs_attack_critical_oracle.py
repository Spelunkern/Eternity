#!/usr/bin/env python3
"""Trace mob critical chance and the original damage caller's roll."""
import random,struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
cpu,digest=load_game();stack,mob,user,stop=0x20000000,0x30000000,0x31000000,0x10000000
for at in (stack,mob,user,stop):cpu.mem_map(at,0x10000)
def put(a,*v):cpu.mem_write(a,struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v)))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
def signed(v):return struct.unpack('<i',struct.pack('<I',v))[0]
state={}
def hook(machine,at,size,ctx):
 if at==0x504697:
  state['draws']+=1;sp=machine.reg_read(UC_X86_REG_ESP);machine.reg_write(UC_X86_REG_EAX,state['roll']);machine.reg_write(UC_X86_REG_EIP,get(sp));machine.reg_write(UC_X86_REG_ESP,sp+4)
 elif at in (0x4a2f84,0x4a2fca):state['critical']=int(at==0x4a2f84);machine.reg_write(UC_X86_REG_EIP,stop)
for at in (0x504697,0x4a2f84,0x4a2fca):cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
rng=random.Random(0x4a28c0)
print('# ps_game.exe SHA256 '+digest+'; mob critical 004a28c0 and AttackDamage 004a2f5b..004a2f84; CRT random intercepted')
print('# kind critical_attack target_wisdom roll | raw result draws')
for i in range(1024):
 kind=i%3;crit=rng.choice((0,1,-1,5,99,100,32767,-32768,2147483647,-2147483648,rng.randrange(-10000,10001)));wis=rng.choice((0,1,-1,100,1000,32767,-32768,2147483647,-2147483648,rng.randrange(-10000,10001)));roll=rng.randrange(32768)
 cpu.mem_write(mob,bytes(0x1000));cpu.mem_write(user,bytes(0x2000));put(mob+0x148+28*kind,crit);put(user+0x1234,wis);put(stack+0x8000,stop,user)
 cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,kind);cpu.reg_write(UC_X86_REG_EDX,mob);cpu.reg_write(UC_X86_REG_FPCW,0x27f);cpu.reg_write(UC_X86_REG_FPSW,0);cpu.reg_write(UC_X86_REG_FPTAG,0xffff);cpu.emu_start(0x4a28c0,stop,count=10000)
 raw=cpu.reg_read(UC_X86_REG_EAX);state.update(roll=roll,draws=0,critical=-1);put(stack+0x8000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,raw);cpu.emu_start(0x4a2f5b,stop,count=10000)
 if state['critical']<0:raise RuntimeError('critical roll did not branch')
 print(kind,crit,wis,roll,signed(raw),state['critical'],state['draws'],sep='\t')
