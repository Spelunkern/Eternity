#!/usr/bin/env python3
"""Probe CGuild::Release game-log construction before member iteration."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,guild,stop=0x20000000,0x30000000,0x31000000,0x10000000
for a in (stack,user,guild,stop):cpu.mem_map(a,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
events=[]
def hook(m,a,size,ctx):
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x42c050:events.append(('broadcast',bytes(m.mem_read(m.reg_read(UC_X86_REG_ECX),m.reg_read(UC_X86_REG_EAX)))))
 elif a==0x45b0d0:events.append(('header',m.reg_read(UC_X86_REG_EDI)))
 elif a==0x4cb8e0:events.append(('clock',));m.reg_write(UC_X86_REG_EAX,0x12345678)
 elif a==0x4d4e50:events.append(('log',bytes(m.mem_read(get(sp+4),get(sp+8)))))
for a,n in ((0x42c050,4),(0x45b0d0,0),(0x4cb8e0,0),(0x4d4e50,8)):
 cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
put(0x546f4c,stop+0x300)
for online in (False,True):
 events.clear()
 for a in (stack,user,guild):cpu.mem_write(a,bytes(0x10000))
 put(guild+8,77);cpu.mem_write(guild+12,b'GuildName\0');cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_ECX,guild);cpu.reg_write(UC_X86_REG_EDX,user if online else 0)
 cpu.emu_start(0x42d5a0,0x42d65e,count=100000);assert cpu.reg_read(UC_X86_REG_EIP)==0x42d65e
 assert [e[0] for e in events]==(['broadcast','header','clock','log'] if online else ['broadcast','clock','log']),events
 assert events[0][1]==b'\x0c\x0d\x65\0\0\0\0';p=events[-1][1];assert len(p)==110 and p[:2]==b'\x02\x0b' and p[70:74]==struct.pack('<I',0x12345678) and p[74:78]==struct.pack('<I',77) and p[78:88]==b'GuildName\0'
print('2 original guild release log cases passed; ps_game.exe SHA256',digest)
