#!/usr/bin/env python3
"""Trace the user-killed tail of ps_game.exe CMob::Attack."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game()
stack,mob,user=0x20000000,0x30000000,0x31000000
for address in (stack,mob,user): cpu.mem_map(address,0x10000)
def put(a,*v): cpu.mem_write(a,struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v)))
def get(a): return struct.unpack('<I',cpu.mem_read(a,4))[0]
state={}
def hook(machine,address,size,context):
    sp=machine.reg_read(UC_X86_REG_ESP)
    if address==0x4a1580: state['trace']+='V'
    elif address==0x41d760: machine.reg_write(UC_X86_REG_EAX,get(mob+0x124))
    elif address==0x49fd20:
        state['trace']+='T'
        if state['found']: put(mob+0x128,1,501)
        machine.reg_write(UC_X86_REG_EAX,user if state['found'] else 0)
    elif address==0x492310: state['trace']+='R';put(mob+0x124,100,0,0)
    elif address==0x4586a0:
        assert machine.reg_read(UC_X86_REG_ECX)==user and [get(sp+i) for i in (4,8,12)]==[2,77,0]
        state['trace']+='D';machine.reg_write(UC_X86_REG_EIP,0x4a21d0)
for address,ret in ((0x4a1580,b'\xc2\x04\x00'),(0x41d760,b'\xc3'),(0x49fd20,b'\xc3'),(0x492310,b'\xc3'),(0x4586a0,b'\xc3')):
    cpu.mem_write(address,ret);cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
print('# ps_game.exe SHA256 '+digest+'; CMob::Attack 004a2188; callbacks intercepted; FindTarget returns none except status 100 which selects target 501')
print('# status pais trace final_status target_kind target_id')
for status,pais in ((1,0),(1,1),(2,0),(2,1),(100,1)):
    cpu.mem_write(mob,bytes(0x1000));put(mob+0x5c,77);put(mob+0x124,status,7,42);cpu.mem_write(mob+0x250,bytes([pais]));state.update(trace='',found=status==100)
    cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EDI,mob);cpu.reg_write(UC_X86_REG_ESI,user)
    cpu.emu_start(0x4a2188,0x4a21d0,count=10000)
    assert cpu.reg_read(UC_X86_REG_EIP)==0x4a21d0
    print(status,pais,state['trace'],get(mob+0x124),get(mob+0x128),get(mob+0x12c),sep='\t')
