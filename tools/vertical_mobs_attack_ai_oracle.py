#!/usr/bin/env python3
"""Trace AttackAI target eligibility before its unrecovered attack selection."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
from original_oracle import load_game
cpu,digest=load_game();stack,mob,user,definition,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
for address in (stack,mob,user,definition,stop):cpu.mem_map(address,0x10000)
def put(a,*v):cpu.mem_write(a,struct.pack('<'+'I'*len(v),*v))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
state={}
def hook(machine,address,size,context):
    if address==0x41d760:machine.reg_write(UC_X86_REG_EAX,get(mob+0x124))
    elif address==0x4a1580:state['trace']+='V'
    elif address==0x4a1b12:state['trace']+='C';machine.reg_write(UC_X86_REG_EIP,0x4a1fdb)
for address,code in ((0x41d760,b'\xc3'),(0x4a1580,b'\xc2\x04\x00')):
    cpu.mem_write(address,code);cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
cpu.hook_add(UC_HOOK_CODE,hook,begin=0x4a1b12,end=0x4a1b12)
print('# ps_game.exe SHA256 '+digest+'; CMob::AttackAI 004a1ab0 eligibility; skill cleanup and continuation intercepted')
print('# mob_country user_country life immune attackable visible status | trace status target_kind in_battle')
for case in ((2,1,0,0,1,1,1),(1,1,0,0,1,1,1),(2,1,1,0,1,1,1),(2,1,0,1,1,1,1),(2,1,0,0,0,1,1),(2,1,0,0,1,0,1),(2,1,1,0,1,1,2)):
    country,user_country,life,immune,attackable,visible,status=case
    cpu.mem_write(mob,bytes(0x1000));cpu.mem_write(user,bytes(0x6000));cpu.mem_write(definition,bytes(0x1000))
    put(mob+0xa8,definition);put(definition+0x78,country);put(mob+0x124,status,7,42);put(user+0x142c,life)
    for offset,value in ((0x12d,user_country),(0x1366,immune),(0x5775,attackable),(0x5774,visible)):cpu.mem_write(user+offset,bytes([value]))
    state['trace']='';put(stack+0x8000,stop,123,user);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,mob);cpu.emu_start(0x4a1ab0,stop,count=10000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('AttackAI did not return')
    print(*case,state['trace'],get(mob+0x124),get(mob+0x128),cpu.mem_read(mob+0x252,1)[0],sep='\t')
