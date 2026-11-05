#!/usr/bin/env python3
"""Execute ps_game 050d guild rejoin deadline branch with controlled tick."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,packet,stop=0x20000000,0x30000000,0x31000000,0x10000000
for a in (stack,user,packet,stop):cpu.mem_map(a,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
calls=[];tick=0
def hook(m,a,size,ctx):calls.append('tick');m.reg_write(UC_X86_REG_EAX,tick)
cpu.mem_write(stop+0x100,b'\xc3');cpu.hook_add(UC_HOOK_CODE,hook,begin=stop+0x100,end=stop+0x100)
for hours,tick in ((0,123),(1,123),(72,0xfffffff0),(0xffffffff,100),(0x7fffffff,0x80000000)):
 calls.clear();cpu.mem_write(user,bytes(0x10000));cpu.mem_write(stack,bytes(0x10000));cpu.mem_write(packet,struct.pack('<HII',0x50d,71,hours));put(user+0x54c0,0xdeadbeef)
 cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_EBX,packet);cpu.reg_write(UC_X86_REG_EBP,user);cpu.reg_write(UC_X86_REG_ESI,stop+0x100)
 cpu.emu_start(0x46e668,0x46e708,count=100000);assert cpu.reg_read(UC_X86_REG_EIP)==0x46e708
 assert get(user+0x54c0)==(0 if not hours else (tick+hours*3600000)&0xffffffff) and len(calls)==int(hours!=0),(hours,tick,calls,get(user+0x54c0))
print('5 original guild rejoin deadline cases passed; ps_game.exe SHA256',digest)
