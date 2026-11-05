#!/usr/bin/env python3
"""Trace UpdateIdle's original movement body with no zone attached."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_FPCW
from original_oracle import load_game
cpu, digest = load_game()
stack, mob, definition, stop = 0x20000000, 0x30000000, 0x31000000, 0x10000000
for address in (stack, mob, definition, stop): cpu.mem_map(address, 0x10000)
def put(address, *values): cpu.mem_write(address, struct.pack('<'+'I'*len(values), *[x & 0xffffffff for x in values]))
def get(address): return struct.unpack('<I',cpu.mem_read(address,4))[0]
state = {}
def hook(machine,address,size,context):
    if address == 0x504697:
        machine.reg_write(UC_X86_REG_EAX,state['values'][state['index']]);state['index']+=1
cpu.mem_write(0x504697,b'\xc3');cpu.hook_add(UC_HOOK_CODE,hook,begin=0x504697,end=0x504697)
cpu.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x1000)+b'\xc3')
print('# ps_game.exe SHA256 '+digest+'; CMob::UpdateIdle 004a7e60 movement body, GetMoveSpeed 004908c0/00490900; random observed, zone absent')
print('# now x chase ai def_speed step normal stop sleep stun hold speed | status next_move random_count')
cases=[(50,0,0,4,1,2,100,0,0,0,0,2),(50,11,0,1,1,2,100,0,0,0,0,2),(50,1,1,1,1,2,100,0,0,0,0,2),(50,0,0,1,1,2,100,1,0,0,0,2),(50,0,0,1,1,2,100,0,0,0,1,2),(0xfffffff0,0,0,1,1,2,0xffffffff,0,0,0,0,2),(50,0,0,1,0,2,100,0,0,0,0,2)]
for now,x,chase,ai,speed,step,normal,stopped,sleep,stun,hold,current_speed in cases:
    cpu.mem_write(mob,bytes(0x1000));cpu.mem_write(definition,bytes(0x1000))
    put(mob+0xa8,definition);put(mob+0xa4,chase);put(mob+0xcc8,0xffffffff);put(mob+0xcbc,0)
    put(mob+0x23c,current_speed);put(mob+0x244,stopped);put(mob+0x24c,sleep);put(mob+0x248,stun);put(mob+0x294,hold)
    cpu.mem_write(mob+0x50,struct.pack('<3f',x,0,0))
    cpu.mem_write(definition+0x26,bytes([ai]));cpu.mem_write(definition+0x60,bytes([step]));cpu.mem_write(definition+0x68,bytes([speed]));put(definition+0x5c,normal)
    state.update(values=[1234,567,891],index=0);put(stack+0x8000,stop,mob,now)
    cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_FPCW,0x27f)
    cpu.emu_start(0x4a7e60,stop,count=20000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop: raise RuntimeError('UpdateIdle did not return')
    print(now,x,chase,ai,speed,step,normal,stopped,sleep,stun,hold,current_speed,get(mob+0x124),get(mob+0xcbc),state['index'],sep='\t')
