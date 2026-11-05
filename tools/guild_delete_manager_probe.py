#!/usr/bin/env python3
"""Probe original CGuildMgr::DeleteGuild manager ordering and packets."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,guild,manager,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
for a in (stack,user,guild,manager,stop):cpu.mem_map(a,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
events=[]
def hook(m,a,size,ctx):
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x42d5a0:events.append(('release',m.reg_read(UC_X86_REG_ECX),m.reg_read(UC_X86_REG_EDX)))
 elif a==0x4347e0:events.append(('erase_id',get(get(sp+4))))
 elif a==0x434a40:
  addr=m.reg_read(UC_X86_REG_EAX);buf=bytearray()
  while (b:=m.mem_read(addr+len(buf),1))!=b'\0':buf.extend(b)
  events.append(('erase_name',bytes(buf)))
 elif a==0x4d4f10:events.append(('db',bytes(m.mem_read(get(sp+4),get(sp+8)))))
 elif a==0x412f50:events.append(('country',get(sp+4)&255,bytes(m.mem_read(m.reg_read(UC_X86_REG_ECX),m.reg_read(UC_X86_REG_EAX)))))
for a,n in ((0x42d5a0,0),(0x4347e0,4),(0x434a40,0),(0x4d4f10,8),(0x412f50,4),(stop+0x100,4),(stop+0x200,4)):
 cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
for a,v in ((0x5210cc,stop+0x100),(0x5210d0,stop+0x200),(0x546f44,stop+0x300),(0x546f38,stop+0x400)):put(a,v)
for country in (0,1,255):
 events.clear()
 for a in (stack,user,guild,manager):cpu.mem_write(a,bytes(0x10000))
 put(guild+8,77);cpu.mem_write(guild+12,b'GuildName\0');cpu.mem_write(guild+0x40,bytes([country]));put(stack+0xf000,stop);put(stack+0xf004,user)
 cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_EBX,guild);cpu.reg_write(UC_X86_REG_EDI,manager);cpu.emu_start(0x4325e0,stop,count=100000)
 assert cpu.reg_read(UC_X86_REG_EIP)==stop
 assert events==[('release',guild,user),('erase_id',77),('erase_name',b'GuildName'),('db',struct.pack('<HI',0xb05,77)),('country',country,struct.pack('<HI',0xd11,77))],events
print('3 original guild manager deletion cases passed; ps_game.exe SHA256',digest)
