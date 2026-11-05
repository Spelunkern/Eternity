#!/usr/bin/env python3
"""Probe original guild-house purchase dispatch with real guild lock."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,guild,stop=0x20000000,0x30000000,0x31000000,0x10000000
for at in (stack,user,guild,stop):cpu.mem_map(at,0x10000)
def put(at,v):cpu.mem_write(at,struct.pack('<I',v&0xffffffff))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
events=[]
def hook(m,a,size,ctx):
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x4d4d20:events.append(('client',bytes(m.mem_read(get(sp+4),get(sp+8)))))
 elif a==0x46c700:events.append(('money',get(user+0x140)))
 elif a==0x4d4f10:events.append(('db',bytes(m.mem_read(get(sp+4),get(sp+8)))))
 elif a==0x4d4e50:events.append(('log',get(sp+8)))
for a,n in ((stop+0x100,4),(stop+0x200,4),(0x4d4d20,8),(0x46c700,0),(0x4d4f10,8),(0x4d4e50,8),(0x45b0d0,0)):
 cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
put(0x5210cc,stop+0x100);put(0x5210d0,stop+0x200);put(0x541b10,1000);put(0x541b14,200);put(0x546f44,stop+0x300);put(0x546f4c,stop+0x400)
def run(rank,money,buy=0,member=True):
 events.clear()
 for a in (stack,user,guild):cpu.mem_write(a,bytes(0x10000))
 put(user+0x1758,guild if member else 0);put(user+0x174c,rank);put(user+0x140,money);put(guild+8,77);put(guild+0x50,123);cpu.mem_write(guild+0x55,bytes([buy]));cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_EBP,user)
 cpu.emu_start(0x477211,0x47768c,count=100000)
 assert cpu.reg_read(UC_X86_REG_EIP)==0x47768c
 return list(events),get(user+0x140),cpu.mem_read(guild+0x55,1)[0],get(guild+0x50)
for rank,money,buy,member,status in ((1,5000,0,False,3),(2,5000,0,True,3),(1,5000,1,True,1),(1,999,0,True,2)):
 e,m,b,k=run(rank,money,buy,member);assert e==[('client',struct.pack('<HBI',0xd34,status,money))] and (m,b,k)==(money,buy,123)
e,m,b,k=run(1,1000);assert e==[('client',struct.pack('<HBI',0xd34,0,0)),('money',0),('db',struct.pack('<HII',0xb10,77,200)),('log',110)] and (m,b,k)==(0,1,200),e
print('5 original guild-house purchase cases passed; ps_game.exe SHA256',digest)
