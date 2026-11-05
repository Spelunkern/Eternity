#!/usr/bin/env python3
"""Capture CParty::SendMove's payload at the original SendSameMap boundary."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,party,user,stop=0x20000000,0x30000000,0x31000000,0x10000000
for base in (stack,party,user,stop):cpu.mem_map(base,0x10000)
def put(at,v):cpu.mem_write(at,struct.pack('<I',v))
cpu.mem_write(0x4457f0,b'\xc2\x04\x00'); captured=[]
def hook(machine,address,size,_):
    if address==0x4457f0:
        if machine.reg_read(UC_X86_REG_EAX)!=20:raise RuntimeError('bad payload length')
        captured.append(bytes(cpu.mem_read(machine.reg_read(UC_X86_REG_ECX),20)).hex())
cpu.hook_add(UC_HOOK_CODE,hook)
print(f'# ps_game.exe SHA256 {digest}; CParty::SendMove 00445880; SendSameMap intercepted')
print('# kind packet')
for kind in (0,1,2):
    cpu.mem_write(stack,bytes([0xa5])*0x10000);cpu.mem_write(party,bytes(0x200));cpu.mem_write(user,bytes(0x300));cpu.mem_write(party+0x114,bytes([kind]));put(user+0xdc,71);cpu.mem_write(user+0x164,struct.pack('<H',7));cpu.mem_write(user+0xd0,struct.pack('<3f',35,6,3));put(stack+0x8000,stop)
    cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,user);cpu.reg_write(UC_X86_REG_ECX,party);captured.clear();cpu.emu_start(0x445880,stop,count=1000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop or len(captured)!=1:raise RuntimeError('SendMove did not return')
    print(kind,captured[0],sep='\t')
