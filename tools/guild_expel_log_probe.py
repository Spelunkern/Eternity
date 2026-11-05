#!/usr/bin/env python3
"""Probe online/offline JoinExpel log fields and requester-name boundaries."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,requester,target,guild,record,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x10000000
for a in (stack,requester,target,guild,record,stop):cpu.mem_map(a,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
events=[]
def hook(m,a,size,ctx):
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x45b0d0:
  p=m.reg_read(UC_X86_REG_ESI);put(p+2,71);put(p+25,0x11223344);m.mem_write(p+29,b'Target\0');put(p+70,0x01020304)
 elif a==0x4cb8e0:m.reg_write(UC_X86_REG_EAX,0x01020304)
 elif a==0x4d4e50:events.append(bytes(m.mem_read(get(sp+4),get(sp+8))))
for a,n in ((0x45b0d0,0),(0x4cb8e0,0),(0x4d4e50,8)):
 cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
put(0x546f4c,stop+0x300)
for online in (False,True):
 for n in (4,18,19,20):
  events.clear()
  for a in (stack,requester,target,guild,record):cpu.mem_write(a,bytes(0x10000))
  name=b'R'*n;cpu.mem_write(requester+0x188,name+b'\0');put(record+8,0x11223344);cpu.mem_write(record+12,b'Target\0');put(record+0x30,target);put(guild+8,77);cpu.mem_write(guild+12,b'GuildName\0');put(stack+0xf010,requester)
  cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_EBX,record);cpu.reg_write(UC_X86_REG_EBP,guild);cpu.reg_write(UC_X86_REG_EDI,0)
  cpu.emu_start(0x42de83 if online else 0x42def1,0x42dedf if online else 0x42df84,count=100000)
  assert cpu.reg_read(UC_X86_REG_EIP)==(0x42dedf if online else 0x42df84) and len(events)==1
  p=events[0];assert len(p)==110 and p[:2]==b'\x06\x0b' and p[6:6+min(n,19)]==name[:19] and p[29:36]==b'Target\0' and p[74:78]==struct.pack('<I',77)
  expected=bytearray(struct.pack('<I',0x11223344))
  if online and n>=19:
   expected[0]=0 if n==19 else ord('R')
   if n==20:expected[1]=0
  assert p[25:29]==expected,(online,n,p[25:29].hex(),expected.hex())
print('8 original guild expulsion log cases passed; ps_game.exe SHA256',digest)
