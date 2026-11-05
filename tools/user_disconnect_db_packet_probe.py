#!/usr/bin/env python3
"""Capture ps_game.exe SendDBAgentUserLeave 0046c660."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP
from original_oracle import load_game
cpu,digest=load_game();stop,stack,user=(0x10000000,0x20000000,0x30000000)
for p in (stop,stack,user):cpu.mem_map(p,0x10000)
def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*v))
def get(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
cpu.mem_write(0x4d4f10,b'\xc2\x08\x00');captured=[]
def hook(m,p,size,data):
 if p==0x4d4f10:
  sp=m.reg_read(UC_X86_REG_ESP);addr=get(sp+4);length=get(sp+8);captured.append(bytes(m.mem_read(addr,length)))
cpu.hook_add(UC_HOOK_CODE,hook)
for uid in (0,1,11,0x12345678,0xffffffff):
 put(user+0x578c,uid);put(stack+0x8000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,user);captured.clear();cpu.emu_start(0x46c660,stop,count=1000)
 assert captured==[b'\x02\x03'+struct.pack('<I',uid)],(uid,captured)
print('5 original DB disconnect packets passed; ps_game.exe SHA256',digest)
