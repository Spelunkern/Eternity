#!/usr/bin/env python3
"""Trace the actor's original normal attack block and finite charge behavior."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,stop=0x20000000,0x30000000,0x10000000
for at in (stack,user,stop):cpu.mem_map(at,0x10000)
def put(a,*v):cpu.mem_write(a,struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v)))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
def ret(value=None):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
 if value is not None:cpu.reg_write(UC_X86_REG_EAX,value)
state={}
def hook(machine,at,size,ctx):
 if at==0x487b20:state['removes']+=1;put(user+0x13a8+44*state['kind'],0);ret()
 elif at==0x504697:state['draws']+=1;ret(state['roll'])
for at in (0x487b20,0x504697):cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
print('# ps_game.exe SHA256 '+digest+'; CUser::IsAttackBlock 0044c5a0; remove detail-85 and CRT random intercepted')
print('# kind all mode value roll | result mode value removes draws')
cases=((0,1,1,1,99),(0,0,0,0,0),(0,0,1,2,0),(1,0,1,1,0),(2,0,1,0,0),(0,0,1,-2147483648,0),(0,0,2,100,99),(1,0,2,0,0),(1,0,2,0,1),(2,0,2,50,50),(2,0,2,50,51),(0,0,2,-1,0),(0,0,3,1,0))
for kind,all_,mode,value,roll in cases:
 cpu.mem_write(user,bytes(0x6000));put(user+0x1360,all_);put(user+0x13a8+44*kind,mode,value);state.update(kind=kind,roll=roll,removes=0,draws=0)
 put(stack+0x8000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,user);cpu.reg_write(UC_X86_REG_ECX,kind);cpu.emu_start(0x44c5a0,stop,count=10000)
 if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('AttackBlock did not return')
 print(kind,all_,mode,value,roll,cpu.reg_read(UC_X86_REG_EAX)&255,get(user+0x13a8+44*kind),get(user+0x13ac+44*kind),state['removes'],state['draws'],sep='\t')
