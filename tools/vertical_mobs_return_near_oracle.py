#!/usr/bin/env python3
"""Trace ReturnHome's deadline and near-home reset ordering."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_FPCW
from original_oracle import load_game
cpu,digest=load_game();stack,mob,stop=0x20000000,0x30000000,0x10000000
for at in (stack,mob,stop):cpu.mem_map(at,0x10000)
def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*v))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
state={}
def ret(cleanup=0):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+cleanup)
def hook(machine,at,size,context):
 if at==0x4a775e:state['events'].append('X');machine.reg_write(UC_X86_REG_EIP,0x4a7b92);return
 tag={0x492310:'F',0x4905c0:'A',0x48ec70:'P',0x48f600:'T',0x4b8d10:'D'}[at];state['events'].append(tag)
 if tag=='F':put(mob+0x124,100)
 if tag=='T':put(mob+0xa4,0)
 ret(4 if tag=='P' else 0)
for at in (0x492310,0x4905c0,0x48ec70,0x48f600,0x4b8d10,0x4a775e):cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
cpu.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x1000)+b'\xc3')
print('# ps_game.exe SHA256 '+digest+'; CMob::ReturnHome 004a7670; FSM/attack/path/trail/damage intercepted; far path intercepted')
print('# now deadline x trail pais status | result status trace')
for now,deadline,x,trail,pais,status in ((50,51,0,0,1,3),(50,50,0,0,1,3),(50,50,0.5,1,1,3),(50,50,9.5,0,1,3),(50,50,10,1,1,3),(50,50,0,0,0,2),(50,50,0,0,0,3)):
 cpu.mem_write(mob,bytes(0x1000));put(mob+0xcb8,deadline);put(mob+0xa4,stop+0x100 if trail else 0);put(mob+0x124,status);cpu.mem_write(mob+0x250,bytes([pais]));cpu.mem_write(mob+0x50,struct.pack('<3f',x,0,0));state['events']=[]
 put(stack+0x8000,stop,mob,now);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,mob+0xc88);cpu.reg_write(UC_X86_REG_FPCW,0x27f)
 cpu.emu_start(0x4a7670,stop,count=20000)
 if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('ReturnHome did not return')
 print(now,deadline,x,trail,pais,status,cpu.reg_read(UC_X86_REG_EAX)&255,get(mob+0x124),''.join(state['events']) or '-',sep='\t')
