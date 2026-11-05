#!/usr/bin/env python3
"""Probe original guild application deadline branch with controlled tick."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,stop=0x20000000,0x30000000,0x10000000
for at in (stack,user,stop):cpu.mem_map(at,0x10000)
def put(at,value):cpu.mem_write(at,struct.pack('<I',value&0xffffffff))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
events=[];tick=0
def hook(m,at,size,ctx):
 if at==stop+0x100:events.append(('tick',));m.reg_write(UC_X86_REG_EAX,tick)
 elif at==0x4d4d20:
  sp=m.reg_read(UC_X86_REG_ESP);events.append(('client',bytes(m.mem_read(get(sp+4),get(sp+8)))))
for at,n in ((stop+0x100,0),(0x4d4d20,8)):
 cpu.mem_write(at,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
put(0x5210e4,stop+0x100)
for deadline,tick,blocked in ((0,1,False),(2,1,True),(1,1,False),(1,2,False),(0xfffffff0,1,True),(1,0xfffffff0,False)):
 events.clear();cpu.mem_write(user,bytes(0x10000));cpu.mem_write(stack,bytes(0x10000));put(user+0x54c0,deadline);cpu.reg_write(UC_X86_REG_EBP,user);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000)
 cpu.emu_start(0x476dfc,0x47768c if blocked else 0x476e32,count=100000)
 assert cpu.reg_read(UC_X86_REG_EIP)==(0x47768c if blocked else 0x476e32)
 expected=([('tick',)] if deadline else [])+([('client',b'\x07\x0d\x02')] if blocked else [])
 assert events==expected and get(user+0x54c0)==deadline,(deadline,tick,events)
print('6 original guild application deadline cases passed; ps_game.exe SHA256',digest)
