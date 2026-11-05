#!/usr/bin/env python3
"""Trace UpdateIdle re-reading normal time after the MoveMob callback."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_FPCW
from original_oracle import load_game
cpu,digest=load_game();stack,mob,definition,zone,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
for address in (stack,mob,definition,zone,stop):cpu.mem_map(address,0x10000)
def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*v))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
def ret(cleanup=0,result=None):
    sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+cleanup)
    if result is not None:cpu.reg_write(UC_X86_REG_EAX,result)
state={}
def hook(machine,address,size,context):
    if address==0x504697:
        value=state['random'][state['index']];state['index']+=1;ret(result=value)
    elif address==0x42ab80:ret(20,1)
    elif address==0x4169d0:
        state['moves']+=1;put(definition+0x5c,state['after']);ret(20,1)
for address in (0x504697,0x42ab80,0x4169d0):cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
cpu.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x1000)+b'\xc3')
print('# ps_game.exe SHA256 '+digest+'; CMob::UpdateIdle 004a7e60, CZone::MoveMob 004169d0; random, EnableLine and MoveMob intercepted')
print('# initial_normal callback_normal random1 random2 random3 | moves resulting_next_move')
for before,after in ((100,500),(100,0xffffffff)):
    cpu.mem_write(mob,bytes(0x1000));cpu.mem_write(definition,bytes(0x1000));cpu.mem_write(zone,bytes(0x1000))
    put(mob+0xa8,definition);put(mob+0x60,zone);put(zone+0x24,zone+0x1000);put(mob+0xcc8,100);put(mob+0x23c,2);put(definition+0x5c,before)
    cpu.mem_write(definition+0x60,b'\x02');cpu.mem_write(definition+0x68,b'\x01')
    cpu.mem_write(mob+0x50,struct.pack('<3f',10,0,10));cpu.mem_write(mob+0xc88,struct.pack('<3f',10,0,10))
    state.update(after=after,random=[41,18467,6334],index=0,moves=0);put(stack+0x8000,stop,mob,50)
    cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_FPCW,0x27f);cpu.emu_start(0x4a7e60,stop,count=100000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop or state['index']!=3:raise RuntimeError('Original movement trace failed')
    print(before,after,*state['random'],state['moves'],get(mob+0xcbc),sep='\t')
