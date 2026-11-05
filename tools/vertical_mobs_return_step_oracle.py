#!/usr/bin/env python3
"""Trace ReturnHome's stopped and direct-line movement branches."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_FPCW
from original_oracle import load_game
cpu,digest=load_game();stack,mob,definition,zone,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
for at in (stack,mob,definition,zone,stop):cpu.mem_map(at,0x10000)
def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
def ret(cleanup=0,result=None):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+cleanup)
 if result is not None:cpu.reg_write(UC_X86_REG_EAX,result)
state={}
def hook(machine,at,size,context):
 sp=machine.reg_read(UC_X86_REG_ESP)
 if at==0x42ab80:state['line']+=1;ret(20,1)
 elif at==0x4169d0:
  assert machine.reg_read(UC_X86_REG_EAX)==mob and get(sp+4)==zone and get(sp+20)==1
  state['moves']+=1;state['x']=get(sp+8);state['z']=get(sp+16)
  put(mob+0x50,state['x'],get(sp+12),state['z']);put(definition+0x64,state['after']);ret(20,1)
for at in (0x42ab80,0x4169d0):cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
cpu.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x1000)+b'\xc3')
print('# ps_game.exe SHA256 '+digest+'; CMob::ReturnHome 004a7670 direct step; EnableLine and MoveMob intercepted')
print('# now x z home_x home_z speed step size stop initial_time callback_time | line moves result_x_bits result_z_bits next_chase')
cases=((100,40,40,10,10,2,2,0,0,100,500),(100,50,40,10,40,4,10,1,0,100,0xffffffff),(0xfffffff0,40,40,10,40,2,2,0,0,100,500),(100,40,40,10,40,-1,2,0,0,100,200),(100,40,40,10,40,2,0,0,0,100,300),(100,40,40,10,40,2,2,0,1,100,500))
for now,x,z,hx,hz,speed,step,size,stopped,before,after in cases:
 cpu.mem_write(mob,bytes(0x1000));cpu.mem_write(definition,bytes(0x1000));cpu.mem_write(zone,bytes(0x10000));put(mob+0xa8,definition);put(mob+0x60,zone);put(zone+0x24,zone+0x1000);put(mob+0x23c,speed);put(mob+0x244,stopped);put(definition+0x64,before)
 cpu.mem_write(definition+0x68,bytes([step]));cpu.mem_write(definition+0x3f,bytes([size]));cpu.mem_write(mob+0x50,struct.pack('<3f',x,0,z));cpu.mem_write(mob+0xc88,struct.pack('<3f',hx,0,hz))
 state.update(line=0,moves=0,x=get(mob+0x50),z=get(mob+0x58),after=after);put(stack+0x8000,stop,mob,now);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,mob+0xc88);cpu.reg_write(UC_X86_REG_FPCW,0x27f);cpu.emu_start(0x4a7670,stop,count=100000)
 if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('ReturnHome direct step failed')
 print(now,x,z,hx,hz,speed,step,size,stopped,before,after,state['line'],state['moves'],get(mob+0x50),get(mob+0x58),get(mob+0xcb8),sep='\t')
