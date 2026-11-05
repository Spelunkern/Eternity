#!/usr/bin/env python3
"""Trace AttackAI's no-ready branch and intercept the first ready slot."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EIP,UC_X86_REG_ESP
from original_oracle import load_game
cpu,digest=load_game();stack,mob,user,definition,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
for address in (stack,mob,user,definition,stop):cpu.mem_map(address,0x10000)
def put(a,*v):cpu.mem_write(a,struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v)))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
state={}
def hook(machine,address,size,context):state['slot']=machine.reg_read(UC_X86_REG_EBX);machine.reg_write(UC_X86_REG_EIP,0x4a1fdb)
cpu.hook_add(UC_HOOK_CODE,hook,begin=0x4a1b49,end=0x4a1b49)
print('# ps_game.exe SHA256 '+digest+'; CMob::AttackAI 004a1ab0; first ready slot intercepted')
print('# now speed enabled0 enabled1 enabled2 time0 time1 time2 | slot next_attack')
cases=[(100,s,0,0,0,0,0,0) for s in (-1,0,1,2,3,4,5,6,7)]+[(100,3,1,1,1,101,101,101),(100,3,1,1,1,100,100,100),(100,3,1,1,1,100,99,101),(100,3,0,1,0,0,100,0),(0xfffffffe,0,0,0,0,0,0,0),(0,3,1,0,0,0xffffffff,0,0)]
for now,speed,e0,e1,e2,t0,t1,t2 in cases:
    cpu.mem_write(mob,bytes(0x1000));cpu.mem_write(user,bytes(0x6000));cpu.mem_write(definition,bytes(0x1000));put(mob+0xa8,definition);put(definition+0x78,2);cpu.mem_write(user+0x12d,b'\x01');cpu.mem_write(user+0x5774,b'\x01\x01');put(mob+0x240,speed);put(mob+0x1b8,t0,t1,t2);put(mob+0xcb4,12345)
    for i,v in enumerate((e0,e1,e2)):cpu.mem_write(definition+0x82+i*20,bytes([v]))
    state['slot']=0xffffffff;put(stack+0x8000,stop,now,user);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,mob);cpu.emu_start(0x4a1ab0,stop,count=10000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('AttackAI did not return')
    print(now,speed,e0,e1,e2,t0,t1,t2,state['slot'],get(mob+0xcb4),sep='\t')
