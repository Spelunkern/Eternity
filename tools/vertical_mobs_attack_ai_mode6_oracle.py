#!/usr/bin/env python3
"""Trace AttackAI mode 6 control flow with target providers intercepted."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EIP,UC_X86_REG_ESP
from original_oracle import load_game
cpu,digest=load_game();stack,mob,user,definition,other,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x10000000
for address in (stack,mob,user,definition,other,stop):cpu.mem_map(address,0x10000)
def put(a,*v):cpu.mem_write(a,struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v)))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
state={}
def hook(machine,address,size,context):
    if address==0x4a0810:state['trace']+='F';machine.reg_write(UC_X86_REG_EAX,other if state['found'] else 0)
    elif address==0x49fad0:state['trace']+='S'
    elif address==0x490ab0:state['trace']+='A';state['slot']=machine.reg_read(UC_X86_REG_EBX);machine.reg_write(UC_X86_REG_EIP,0x4a1fdb)
    elif address==0x4a1be3:state['trace']+='C';state['slot']=machine.reg_read(UC_X86_REG_EBX);machine.reg_write(UC_X86_REG_EIP,0x4a1fdb)
for address,code in ((0x4a0810,b'\xc2\x04\x00'),(0x49fad0,b'\xc2\x08\x00')):
    cpu.mem_write(address,code);cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
for address in (0x490ab0,0x4a1be3):cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
print('# ps_game.exe SHA256 '+digest+'; CMob::AttackAI 004a1ab0 mode 6; find/set/retarget/continuation intercepted')
print('# fixed found lower | trace slot next_attack')
for fixed,found,lower in ((0,0,0),(0,0,1),(0,1,0),(0,1,1),(1,0,0),(1,1,1)):
    cpu.mem_write(mob,bytes(0x1000));cpu.mem_write(user,bytes(0x6000));cpu.mem_write(definition,bytes(0x1000));put(mob+0xa8,definition);put(definition+0x78,2);cpu.mem_write(user+0x12d,b'\x01');cpu.mem_write(user+0x5774,b'\x01\x01');put(mob+0x240,3);put(mob+0xcb4,12345);put(mob+0x2b8,fixed);cpu.mem_write(definition+0xaa,b'\x06');cpu.mem_write(definition+0x96,bytes([lower]));state.update(trace='',found=found,slot=0xffffffff)
    put(stack+0x8000,stop,100,user);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,mob);cpu.emu_start(0x4a1ab0,stop,count=10000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('AttackAI did not return')
    print(fixed,found,lower,state['trace'] or '-',state['slot'],get(mob+0xcb4),sep='\t')
