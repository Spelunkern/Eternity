#!/usr/bin/env python3
"""Execute the original LeaveGuild game-log packet construction."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EBP,UC_X86_REG_EBX,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,guild=0x20000000,0x30000000,0x31000000
for a in (stack,user,guild):cpu.mem_map(a,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
events=[]
def hook(m,a,size,ctx):
 if a==0x4d4e50:
  sp=m.reg_read(UC_X86_REG_ESP);events.append(bytes(m.mem_read(get(sp+4),get(sp+8))))
for a,n in ((0x45b0d0,0),(0x4d4e50,8)):
 cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
put(0x546f4c,0x33000000)
for name in (b'GuildName',b'ABCDEFGHIJKLMNOPQRSTUVWX'):
 events.clear()
 for a in (stack,user,guild):cpu.mem_write(a,bytes(0x10000))
 put(guild+8,0x12345678);cpu.mem_write(guild+12,name+b'\0');cpu.reg_write(UC_X86_REG_EBP,guild);cpu.reg_write(UC_X86_REG_EDI,user);cpu.reg_write(UC_X86_REG_EBX,0);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000)
 cpu.emu_start(0x42dbef,0x42dc2f,count=100000)
 assert cpu.reg_read(UC_X86_REG_EIP)==0x42dc2f and len(events)==1
 p=events[0];assert len(p)==110 and p[:2]==b'\x05\x0b' and p[74:78]==struct.pack('<I',0x12345678) and p[78:78+len(name)+1]==name+b'\0'
print('2 original guild leave log cases passed; ps_game.exe SHA256',digest)
