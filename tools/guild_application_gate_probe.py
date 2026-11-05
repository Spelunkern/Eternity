#!/usr/bin/env python3
"""Probe original guild application preconditions after the deadline gate."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EBP,UC_X86_REG_EBX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,guild,packet,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
for at in (stack,user,guild,packet,stop):cpu.mem_map(at,0x10000)
def put(at,value):cpu.mem_write(at,struct.pack('<I',value&0xffffffff))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
events=[]
def hook(m,at,size,ctx):
 if at==0x4d4d20:
  sp=m.reg_read(UC_X86_REG_ESP);events.append(bytes(m.mem_read(get(sp+4),get(sp+8))))
for at,n in ((stop+0x100,4),(stop+0x200,4),(0x4d4d20,8)):
 cpu.mem_write(at,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
put(0x5210cc,stop+0x100);put(0x5210d0,stop+0x200)
for grow,member,creation,invite,blocked in ((0,0,0,0,True),(1,1,0,0,True),(1,0,1,0,True),(1,0,0,1,True),(1,0,0,0,False),(255,0,0,0,False)):
 events.clear()
 for at in (stack,user,guild,packet):cpu.mem_write(at,bytes(0x10000))
 cpu.mem_write(user+0x12f,bytes([grow]));put(user+0x1758,guild if member else 0);put(user+0x175c,creation);put(user+0x1754,invite);cpu.reg_write(UC_X86_REG_EBP,user);cpu.reg_write(UC_X86_REG_EBX,packet);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000)
 cpu.emu_start(0x476e32,0x47768c if blocked else 0x476e82,count=100000)
 assert cpu.reg_read(UC_X86_REG_EIP)==(0x47768c if blocked else 0x476e82) and events==([b'\x07\x0d\x00'] if blocked else []),(grow,member,creation,invite,events)
print('6 original guild application precondition cases passed; ps_game.exe SHA256',digest)
