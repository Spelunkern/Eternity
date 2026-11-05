#!/usr/bin/env python3
"""Probe ps_game guild etin donation without eligible inventory items."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,guild,item,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
for at in (stack,user,guild,item,stop):cpu.mem_map(at,0x10000)
def put(at,value):cpu.mem_write(at,struct.pack('<I',value&0xffffffff))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
events=[]
def hook(m,at,size,ctx):
 sp=m.reg_read(UC_X86_REG_ESP)
 if at==0x4d4d20:events.append(bytes(m.mem_read(get(sp+4),get(sp+8))))
for at,n in ((stop+0x100,4),(stop+0x200,4),(0x4d4d20,8)):
 cpu.mem_write(at,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
put(0x5210cc,stop+0x100);put(0x5210d0,stop+0x200)
def run(member,bags,noneligible):
 events.clear()
 for at in (stack,user,guild,item):cpu.mem_write(at,bytes(0x10000))
 put(user+0x1758,guild if member else 0);put(user+0x1304,bags)
 if noneligible:put(user+0x224,item);cpu.mem_write(item+0x40,b'U')
 cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_EBP,user);cpu.emu_start(0x47734d,0x47768c,count=200000)
 assert cpu.reg_read(UC_X86_REG_EIP)==0x47768c
 return list(events)
assert run(False,2,False)==[]
for bags,noneligible in ((0,False),(1,False),(2,False),(1,True),(2,True)):
 assert run(True,bags,noneligible)==[b'\x35\x0d\x00']
print('6 original guild donation empty/no-eligible cases passed; ps_game.exe SHA256',digest)
