#!/usr/bin/env python3
"""Trace ReturnHome using an existing A* cursor and real CursorNext/ResetPath."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_FPCW
from original_oracle import load_game
cpu,digest=load_game();stack,mob,definition,zone,node,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x10000000
for at in (stack,mob,definition,zone,node,stop):cpu.mem_map(at,0x10000)
def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
def ret(cleanup=0,result=None):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+cleanup)
 if result is not None:cpu.reg_write(UC_X86_REG_EAX,result)
state={}
def hook(machine,at,size,context):
 sp=machine.reg_read(UC_X86_REG_ESP)
 if at==0x42ab80:raise RuntimeError('Retained path unexpectedly checked the line')
 if at==0x4169d0:
  assert machine.reg_read(UC_X86_REG_EAX)==mob and get(sp+4)==zone and get(sp+20)==1
  state['moves']+=1;put(mob+0x50,get(sp+8),get(sp+12),get(sp+16));put(definition+0x64,state['after'])
  if state['mutate']:cpu.mem_write(mob+0x50,struct.pack('<3f',33,22,33))
  ret(20,1)
for at in (0x42ab80,0x4169d0):cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
cpu.mem_write(stop+16,b'\xc2\x04\x00');put(0x5210cc,stop+16);put(0x5210d0,stop+16)
cpu.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x1000)+b'\xc3')
print('# ps_game.exe SHA256 '+digest+'; CMob::ReturnHome 004a7670, AStar::CursorNext 004a6ae0 and ResetPath 0048ec70; MoveMob/locks intercepted')
print('# now x z home_x home_z path_x path_z speed step size stop before after mutate | moves x_bits y_bits z_bits next_chase found cursor')
cases=((100,35,20,10,20,20,20,2,2,0,0,100,500,0),(100,35,20,10,20,20,20,2,20,0,0,100,500,0),(100,35,20,10,20,20,20,2,20,0,0,100,500,1),(100,35,20,10,20,20,20,2,2,0,1,100,500,0),(0xfffffff0,35,20,10,20,20,20,2,20,0,0,100,0xffffffff,0))
for now,x,z,hx,hz,px,pz,speed,step,size,stopped,before,after,mutate in cases:
 for at in (mob,definition,zone,node):cpu.mem_write(at,bytes(0x10000))
 put(0x3ed34b8,0,0,0);put(mob+0xa8,definition);put(mob+0x60,zone);put(zone+0x24,zone+0x1000);put(mob+0x23c,speed);put(mob+0x244,stopped);put(definition+0x64,before)
 put(mob+0x84,node,node,node);cpu.mem_write(mob+0xa0,b'\x01');put(node+4,px,pz);put(node+0x1c,0)
 cpu.mem_write(definition+0x68,bytes([step]));cpu.mem_write(definition+0x3f,bytes([size]));cpu.mem_write(mob+0x50,struct.pack('<3f',x,0,z));cpu.mem_write(mob+0xc88,struct.pack('<3f',hx,0,hz))
 state.update(moves=0,after=after,mutate=mutate);put(stack+0x8000,stop,mob,now);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,mob+0xc88);cpu.reg_write(UC_X86_REG_FPCW,0x27f);cpu.emu_start(0x4a7670,stop,count=100000)
 if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('ReturnHome retained path failed')
 print(now,x,z,hx,hz,px,pz,speed,step,size,stopped,before,after,mutate,state['moves'],get(mob+0x50),get(mob+0x54),get(mob+0x58),get(mob+0xcb8),int(cpu.mem_read(mob+0xa0,1)[0]!=0),int(get(mob+0x8c)!=0),sep='\t')
