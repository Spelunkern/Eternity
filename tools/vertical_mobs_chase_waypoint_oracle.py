#!/usr/bin/env python3
"""Trace Chase intermediate path waypoint using original line and A* owners."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_FPCW
from original_oracle import load_game
cpu,digest=load_game();stack,mob,target,definition,zone,mapobj,tiles,heap,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000,0x35000000,0x42000000,0x10000000
for address in (stack,mob,target,definition,zone,mapobj,tiles,stop):cpu.mem_map(address,0x10000)
cpu.mem_map(heap,0x1000000)
def put(a,*v):cpu.mem_write(a,struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v)))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
def bits(f):return struct.unpack('<I',struct.pack('<f',f))[0]
def ret(machine,cleanup=0,result=None):
 sp=machine.reg_read(UC_X86_REG_ESP);machine.reg_write(UC_X86_REG_EIP,get(sp));machine.reg_write(UC_X86_REG_ESP,sp+4+cleanup)
 if result is not None:machine.reg_write(UC_X86_REG_EAX,result)
state={}
def hook(machine,address,size,context):
    if address==0x503a3f:
        amount=get(machine.reg_read(UC_X86_REG_ESP)+4);p=state['alloc'];state['alloc']+=(amount+15)&~15;cpu.mem_write(p,bytes(amount));ret(machine,result=p)
    elif address==0x48f660:
        state['trails']+=1;ret(machine,4)
    elif address==0x4169d0:
        sp=machine.reg_read(UC_X86_REG_ESP);state['moves']+=1
        state['mx']=get(sp+8);state['my']=get(sp+12);state['mz']=get(sp+16);ret(machine,20,state['accepted'])
for address in (0x503a3f,0x48f660,0x4169d0):cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
cpu.mem_write(stop+16,b'\xc2\x04\x00');put(0x5210cc,stop+16);put(0x5210d0,stop+16);cpu.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x1000)+b'\xc3')
print('# ps_game.exe SHA256 '+digest+'; CMob::Chase 004a6de0 intermediate waypoint; real AStar, vector and path advance; allocation/locks/trail/MoveMob intercepted')
print('# step accepted | x_bits y_bits z_bits next_chase cursor_x cursor_z trails moves')
for step,accepted in ((2,1),(6,1),(6,0),(5,1)):
 for address in (mob,target,definition,zone,mapobj,tiles):cpu.mem_write(address,bytes(0x10000))
 put(0x3ed34b8,0,0,0);put(0x3ed34dc,0,0,0);put(mob+0xa8,definition);put(mob+0x60,zone);put(zone+0x24,mapobj);put(mapobj+4,32,tiles);put(mob+0x50,bits(10),bits(3),bits(10));put(mob+0xc88,bits(10),0,bits(10));put(target,bits(20),0,bits(20));put(mob+0x124,1,7,42);put(mob+0x23c,2);put(definition+0x64,500);cpu.mem_write(definition+0x68,bytes([step]))
 for z in range(32):
  if z!=14:
   tile=z*32+15;cpu.mem_write(tiles+tile//8,bytes([cpu.mem_read(tiles+tile//8,1)[0]|(1<<(tile%8))]))
 state.update(alloc=heap,trails=0,moves=0,accepted=accepted,mx=0,my=0,mz=0);put(stack+0x8000,stop,mob,50,target,0);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_FPCW,0x27f);cpu.emu_start(0x4a6de0,stop,count=100000000)
 if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Chase waypoint did not return')
 cursor=get(mob+0x8c);cx=get(cursor+4) if cursor else 0xffffffff;cz=get(cursor+8) if cursor else 0xffffffff
 print(step,accepted,state['mx'],state['my'],state['mz'],get(mob+0xcb8),cx,cz,state['trails'],state['moves'],sep='\t')
