#!/usr/bin/env python3
"""Execute original DBGuild::CreateChatacter SQL formatting with an isolated DB port."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_original
cpu,digest=load_original('ps_dbAgent');stack,obj,stop=0x20000000,0x30000000,0x10000000
for a in (stack,obj,stop):cpu.mem_map(a,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
def string(a):
 out=bytearray()
 while (b:=cpu.mem_read(a+len(out),1))!=b'\0':out.extend(b)
 return bytes(out)
queries=[]
def hook(m,a,size,ctx):
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x43ccc0:m.reg_write(UC_X86_REG_EAX,obj)
 elif a==0x44447f:
  dest,fmt,guild,character,rank=(get(sp+i) for i in (4,8,12,16,20));signed=lambda n:n if n<0x80000000 else n-0x100000000
  query=string(fmt).decode()%(signed(guild),signed(character),rank);m.mem_write(dest,query.encode()+b'\0');m.reg_write(UC_X86_REG_EAX,len(query))
 elif a==0x43f350:queries.append(string(get(sp+4)));m.reg_write(UC_X86_REG_EAX,0)
for a,n in ((0x43ccc0,0),(0x43cd80,4),(0x44447f,0),(0x43f350,4),(0x444dc3,0),(0x432d80,0)):
 cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
for guild,character,rank in ((10,123,0),(0xffffffff,0x80000000,255)):
 queries.clear();cpu.mem_write(stack,bytes(0x10000));put(stack+0xf000,stop);put(stack+0xf004,guild);put(stack+0xf008,character);put(stack+0xf00c,rank);put(stack+0xf010,obj+0x100);put(obj+0x100,0);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.emu_start(0x428400,stop,count=100000)
 assert cpu.reg_read(UC_X86_REG_EIP)==stop and cpu.reg_read(UC_X86_REG_EAX)==1
 signed=lambda n:n if n<0x80000000 else n-0x100000000
 assert queries==[f'EXEC usp_Create_GuildChar_E {signed(guild)}, {signed(character)}, {rank}'.encode()],queries
print('2 original guild member create SQL cases passed; ps_dbAgent.exe SHA256',digest)
