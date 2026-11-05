#!/usr/bin/env python3
"""Probe the original 0d09 guild-leave guards before member removal."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBP,UC_X86_REG_EBX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,guild,stop=0x20000000,0x30000000,0x31000000,0x10000000
for a in (stack,user,guild,stop):cpu.mem_map(a,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v&0xffffffff))
events=[]
def hook(m,a,size,ctx):
 if a==0x42da90:events.append('leave_member');m.reg_write(UC_X86_REG_EAX,0)
cpu.mem_write(0x42da90,b'\xc2\x04\x00');cpu.hook_add(UC_HOOK_CODE,hook,begin=0x42da90,end=0x42da90)
for member,creation in ((False,False),(False,True),(True,True),(True,False)):
 events.clear()
 for a in (stack,user,guild):cpu.mem_write(a,bytes(0x10000))
 put(user+0x1758,guild if member else 0);put(user+0x175c,0x1234 if creation else 0)
 cpu.reg_write(UC_X86_REG_EBP,user);cpu.reg_write(UC_X86_REG_EBX,0);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000)
 cpu.emu_start(0x476fbc,0x477678,count=100000)
 assert cpu.reg_read(UC_X86_REG_EIP)==0x477678 and events==(['leave_member'] if member and not creation else []),(member,creation,events)
print('4 original guild leave guard cases passed; ps_game.exe SHA256',digest)
