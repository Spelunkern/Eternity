#!/usr/bin/env python3
"""Trace the original Chase search-failure target reset."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
from original_oracle import load_game
cpu,digest=load_game();stack,mob,stop=0x20000000,0x30000000,0x10000000
for address in (stack,mob,stop):cpu.mem_map(address,0x10000)
def put(a,*v):cpu.mem_write(a,struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v)))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
def ret(cleanup=0,result=None):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+cleanup)
 if result is not None:cpu.reg_write(UC_X86_REG_EAX,result)
state={}
def hook(machine,address,size,context):
 if address==0x4a1580:
  state['trace']+='V';put(mob+0x2b8,0);ret(4)
 elif address==0x49fd20:
  state['trace']+='T';ret(result=state['found'])
 elif address==0x492310:
  state['trace']+='F';put(mob+0x124,100,0,0);ret()
for address in (0x4a1580,0x49fd20,0x492310):cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
print('# ps_game.exe SHA256 '+digest+'; target reset 00490980; vision cleanup, FindTarget and ResetFSM intercepted')
print('# status pais found | trace status target_kind target_id fixed')
for status,pais,found in ((2,0,0),(1,0,0),(1,1,0),(1,1,1),(2,1,1)):
 cpu.mem_write(mob,bytes(0x10000));put(mob+0x124,status,7,42);put(mob+0x2b8,99);cpu.mem_write(mob+0x250,bytes([pais]));state.update(trace='',found=found)
 put(stack+0x8000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,mob);cpu.emu_start(0x490980,stop,count=10000)
 if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('target reset did not return')
 print(status,pais,found,state['trace'] or '-',get(mob+0x124),get(mob+0x128),get(mob+0x12c),get(mob+0x2b8),sep='\t')
