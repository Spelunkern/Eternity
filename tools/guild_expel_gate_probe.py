#!/usr/bin/env python3
"""Probe original PacketGuild expulsion permission and creation guards."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,guild,packet,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
for a in (stack,user,guild,packet,stop):cpu.mem_map(a,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
events=[]
def hook(m,a,size,ctx):
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x42dcb0:events.append(('expel',get(sp+4),get(sp+8),get(sp+12)))
cpu.mem_write(0x42dcb0,b'\xc2\x0c\x00');cpu.hook_add(UC_HOOK_CODE,hook,begin=0x42dcb0,end=0x42dcb0)
for rank,member,creation in ((3,True,False),(255,True,False),(2,False,False),(2,True,True),(2,True,False),(1,True,False),(0xffffffff,True,False)):
 events.clear()
 for a in (stack,user,guild,packet):cpu.mem_write(a,bytes(0x10000))
 put(user+0x174c,rank);put(user+0x1758,guild if member else 0);put(user+0x175c,123 if creation else 0);put(packet+2,43)
 cpu.reg_write(UC_X86_REG_EBP,user);cpu.reg_write(UC_X86_REG_EBX,packet);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.emu_start(0x47701f,0x477678,count=100000)
 permitted=(rank<=2 or rank>=0x80000000) and member and not creation
 assert cpu.reg_read(UC_X86_REG_EIP)==0x477678 and events==([('expel',guild,43,user)] if permitted else []),(rank,member,creation,events)
print('7 original guild expulsion guard cases passed; ps_game.exe SHA256',digest)
