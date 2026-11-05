#!/usr/bin/env python3
"""Trace the original mob hit application routing and 0605 packet."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game

c,digest=load_game()
user,mob,stack=(0x30000000,0x31000000,0x32000000)
for a in (user,mob,stack):c.mem_map(a,0x10000)
def put(a,v):c.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',c.mem_read(a,4))[0]
state={}
def hook(machine,address,size,context):
    sp=machine.reg_read(UC_X86_REG_ESP)
    if address==0x44c1e0:
        if machine.reg_read(UC_X86_REG_EAX)!=user or get(sp+4)!=2:raise RuntimeError('wrong user damage call')
        ptr=get(sp+8);put(ptr,get(ptr)+4);put(mob+0x5c,101);state['trace']+='A'
        machine.reg_write(UC_X86_REG_EAX,state['killed']);machine.reg_write(UC_X86_REG_EIP,get(sp));machine.reg_write(UC_X86_REG_ESP,sp+12)
    elif address==0x4a2188:
        state['trace']+='D';put(mob+0x5c,102);machine.reg_write(UC_X86_REG_EIP,0x4a21d0)
    else:
        if get(sp+12)!=17 or get(sp+20)!=0x42700000 or get(sp+24)!=0 or get(sp+28)!=77 or get(sp+32)!=6:raise RuntimeError('wrong view arguments')
        state['trace']+='P';state['packet']=bytes(machine.mem_read(get(sp+8),17)).hex();machine.reg_write(UC_X86_REG_EIP,0x4a223f)
for a in (0x44c1e0,0x4a2188,0x421220):c.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
print('# ps_game.exe SHA256',digest,'CMob::Attack 004a2164..004a223f; Damage, death and PSendView intercepted')
print('# reflected critical killed zone | trace mobid hp sp mp packet')
for reflected in (0,1):
 for critical in (0,1):
  for killed in (0,1):
   for zone in (0,1):
    c.mem_write(stack,bytes(0x10000));c.mem_write(user,bytes(0x10000));c.mem_write(mob,bytes(0x10000));sp=stack+0x8000
    c.mem_write(sp+0x12,bytes([reflected,0]));put(sp+0x1c,70000);put(sp+0x20,-2);put(sp+0x24,10);word=b'\x05\x06';c.mem_write(sp+0x28,word)
    put(mob+0x5c,100);put(mob+0x60,0x35000000 if zone else 0);put(user+0xdc,77)
    state.update(trace='',packet='-',killed=killed)
    c.reg_write(UC_X86_REG_ESP,sp);c.reg_write(UC_X86_REG_ESI,user);c.reg_write(UC_X86_REG_EDI,mob);c.reg_write(UC_X86_REG_EAX,critical)
    c.emu_start(0x4a2164,0x4a223f,count=10000)
    damage=struct.unpack('<iii',c.mem_read(sp+0x1c,12))
    print(reflected,critical,killed,zone,state['trace']+'T',get(mob+0x5c),*damage,state['packet'],sep='\t')
