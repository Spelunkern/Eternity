#!/usr/bin/env python3
"""Trace CMob::UpdateAttack with real gates and intercepted world callbacks."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_ESP
from original_oracle import load_game
cpu,digest=load_game()
stack,mob,user,zone,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
for address in (stack,mob,user,zone,stop): cpu.mem_map(address,0x10000)
def put(a,*v): cpu.mem_write(a,struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v)))
def get(a): return struct.unpack('<I',cpu.mem_read(a,4))[0]
state={}
def hook(machine,address,size,context):
    if address==0x41d760: machine.reg_write(UC_X86_REG_EAX,get(mob+0x124))
    elif address==0x415f30: state['trace']+='F';machine.reg_write(UC_X86_REG_EAX,user if state['found'] else 0)
    elif address==0x4a1580:
        state['trace']+='V'
        if state['mutate']: put(mob+0x124,2)
    elif address==0x4a1ab0: state['trace']+='A'
    elif address==0x4a7670: state['trace']+='R'
for addr,ret in ((0x41d760,b'\xc3'),(0x415f30,b'\xc2\x04\x00'),(0x4a1580,b'\xc2\x04\x00'),(0x4a1ab0,b'\xc2\x08\x00'),(0x4a7670,b'\xc2\x08\x00')):
    cpu.mem_write(addr,ret);cpu.hook_add(UC_HOOK_CODE,hook,begin=addr,end=addr)
print('# ps_game.exe SHA256 '+digest+'; CMob::UpdateAttack 004a7db0; status and world callbacks intercepted')
print('# status stack now deadline sleep stun found life mutate | trace status target_kind')
cases=[(100,0,50,50,0,0,1,0,0),(1,1,50,50,0,0,1,0,0),(2,0,50,50,0,0,1,0,0),(1,0,49,50,0,0,1,0,0),(1,0,50,50,1,0,1,0,0),(1,0,50,50,0,1,1,0,0),(1,0,50,50,0,0,1,0,0),(1,0,50,50,0,0,0,0,0),(1,0,50,50,0,0,1,1,0),(1,0,50,50,0,0,0,0,1),(3,0,50,50,0,0,0,0,0),(0,0,50,50,0,0,0,0,0)]
for status,active,now,deadline,sleep,stun,found,life,mutate in cases:
    cpu.mem_write(mob,bytes(0x1000));cpu.mem_write(user,bytes(0x2000));put(mob+0x124,status,7,42);put(mob+0x44,active);put(mob+0x60,zone);put(mob+0xcb4,deadline);put(mob+0x24c,sleep);put(mob+0x248,stun);put(user+0x142c,life)
    state.update(trace='',found=found,mutate=mutate);put(stack+0x8000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ESI,mob);cpu.reg_write(UC_X86_REG_EDI,now)
    cpu.emu_start(0x4a7db0,stop,count=10000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('UpdateAttack did not return')
    print(status,active,now,deadline,sleep,stun,found,life,mutate,state['trace'] or '-',get(mob+0x124),get(mob+0x128),sep='\t')
