#!/usr/bin/env python3
"""Trace original SConnection baseline through construction, open and reset."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_ESP
from original_oracle import load_game

cpu,digest=load_game();stack,user,agent,reply,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
for address in (stack,user,agent,reply,stop):cpu.mem_map(address,0x10000)
def put(address,*values):cpu.mem_write(address,struct.pack('<'+'I'*len(values),*[value&0xffffffff for value in values]))
def get(address):return struct.unpack('<I',cpu.mem_read(address,4))[0]
# Critical-section init, getsockname, queue cleanup and socket close are ports.
put(0x5210c8,stop+0x100);cpu.mem_write(stop+0x100,b'\xc2\x08\x00')
put(0x5212ec,stop+0x120);cpu.mem_write(stop+0x120,b'\xc2\x0c\x00')
for address,code in ((0x4e06f0,b'\xc3'),(0x4e0930,b'\xc3'),(0x4d43a0,b'\xc2\x08\x00')):cpu.mem_write(address,code)
reasons=[]
def close_hook(machine,address,size,context):
    sp=machine.reg_read(UC_X86_REG_ESP);reasons.append((get(sp+4),get(sp+8)))
cpu.hook_add(UC_HOOK_CODE,close_hook,begin=0x4d43a0,end=0x4d43a0)
def call(address,this,*args):
    put(stack+0x8000,stop,*args);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ECX,this);cpu.reg_write(UC_X86_REG_EAX,1);cpu.emu_start(address,stop,count=10000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original did not return')
print(f'# ps_game.exe SHA256 {digest}; SConnection::SConnection 004d4c50, Open 004d4430, Reset 004d4340, SReply::CheckTime 004dd670; OS and queue calls are ports')
cpu.mem_write(user,bytes([0xa5])*0x100);call(0x4d4c50,user);baseline=get(user+0x40)
if baseline!=0:raise RuntimeError('Constructor baseline not zero')
call(0x4d4430,user,agent);opened=get(user+0x40);call(0x4d4340,user);reset=get(user+0x40);call(0x4d4430,user,agent);reopened=get(user+0x40)
if (opened,reset,reopened)!=(0,0,0):raise RuntimeError('Zero baseline changed during open/reset')
print('fresh',baseline,opened,reset,reopened)
put(user+0x40,0x12345678);call(0x4d4340,user);preserved=get(user+0x40);call(0x4d4430,user,agent);reopened=get(user+0x40)
if (preserved,reopened)!=(0x12345678,0x12345678):raise RuntimeError('Reset/open unexpectedly changed injected baseline')
print('injected',preserved,reopened)
put(user+0x40,0);put(user+0x4c,0);put(agent+0x30,300000,300000,600000)
for now in (0,900000,900001,0xffffffff):
    reasons.clear();call(0x4dd670,reply,user,now)
    print('scan',now,reasons[0][0] if reasons else 0)
