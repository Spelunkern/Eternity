#!/usr/bin/env python3
"""Trace original reflected mob damage, 051b view and final hit operation."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game

c,digest=load_game()
user,mob,respawn,stack=(0x30000000,0x31000000,0x32000000,0x33000000)
for a in (user,mob,respawn,stack):c.mem_map(a,0x10000)
def put(a,v):c.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',c.mem_read(a,4))[0]
state={}
def hook(machine,address,size,context):
    sp=machine.reg_read(UC_X86_REG_ESP)
    if address==0x490ee0:
        if get(sp+4)!=mob or get(sp+8)!=1 or get(sp+12)!=77 or any(get(sp+x) for x in (16,20,24)):raise RuntimeError('wrong mob damage credit')
        ptr=machine.reg_read(UC_X86_REG_EAX);put(ptr,get(ptr)+4);put(mob+0x5c,101);state['trace']+='D';state['name']=bytes(machine.mem_read(mob+0xcf4,7)).split(b'\0')[0].decode()
        machine.reg_write(UC_X86_REG_EAX,state['dead']);machine.reg_write(UC_X86_REG_EIP,get(sp));machine.reg_write(UC_X86_REG_ESP,sp+28)
    elif address==0x489ed0:
        state['trace']+='S';machine.reg_write(UC_X86_REG_EIP,get(sp));machine.reg_write(UC_X86_REG_ESP,sp+4)
    elif address==0x4910f0:
        state['trace']+='K';machine.reg_write(UC_X86_REG_EIP,get(sp));machine.reg_write(UC_X86_REG_ESP,sp+12)
    elif address==0x421220:
        if get(sp+12)!=16 or get(sp+20)!=0x42700000 or get(sp+24)!=0 or get(sp+28)!=77 or get(sp+32)!=6:raise RuntimeError('wrong reflection view')
        state['trace']+='P';state['packet']=bytes(machine.mem_read(get(sp+8),16)).hex();machine.reg_write(UC_X86_REG_EIP,0x4a2355)
    else:
        if machine.reg_read(UC_X86_REG_ECX)!=mob or get(sp+4)!=user:raise RuntimeError('wrong final operation')
        state['trace']+='F';machine.reg_write(UC_X86_REG_EIP,0x4a235d)
for a in (0x490ee0,0x489ed0,0x4910f0,0x421220,0x44cae0):c.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
print('# ps_game.exe SHA256',digest,'CMob::Attack 004a223f..004a235d; mob damage, stop, death, view and final intercepted')
print('# lua reflected user_dead both mob_dead zone | trace name mobid hp userbyte skillbyte packet')
for lua in (0,1):
 for reflected in (0,1):
  for user_dead in (0,1):
   for both in (0,1):
    for mob_dead in (0,1):
     for zone in (0,1):
      for a in (user,mob,respawn,stack):c.mem_write(a,bytes(0x10000))
      sp=stack+0x8000;put(mob+0x284,lua);put(mob+0xc84,respawn);c.mem_write(respawn+0x1c,bytes([both]));put(mob+0x5c,100);put(mob+0x60,0x35000000 if zone else 0);put(user+0xdc,77);c.mem_write(user+0x188,b'Victim\0');c.mem_write(mob+0xcf4,b'X');c.mem_write(mob+0xd14,b'Y');put(sp+0x1c,70000);put(sp+0x20,-2);put(sp+0x24,10);c.mem_write(sp+0x13,bytes([user_dead]))
      state.update(trace='',packet='-',name='-',dead=mob_dead)
      c.reg_write(UC_X86_REG_ESP,sp);c.reg_write(UC_X86_REG_ESI,user);c.reg_write(UC_X86_REG_EDI,mob);c.reg_write(UC_X86_REG_EBP,mob+0x50);c.reg_write(UC_X86_REG_EBX,reflected)
      c.emu_start(0x4a223f,0x4a235d,count=10000)
      print(lua,reflected,user_dead,both,mob_dead,zone,state['trace'],state['name'],get(mob+0x5c),get(sp+0x1c),c.mem_read(mob+0xcf4,1)[0],c.mem_read(mob+0xd14,1)[0],state['packet'],sep='\t')
