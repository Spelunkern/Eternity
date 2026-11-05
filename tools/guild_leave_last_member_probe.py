#!/usr/bin/env python3
"""Probe PacketGuild's post-LeaveGuild empty-manager transition."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,guild,stop=0x20000000,0x30000000,0x31000000,0x10000000
for a in (stack,user,guild,stop):cpu.mem_map(a,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
events=[];leave_success=False;online=offline=0
def hook(m,a,size,ctx):
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x42da90:
  events.append(('leave',get(sp+4)));put(guild+0x514,online);put(guild+0x540,offline);m.reg_write(UC_X86_REG_EAX,int(leave_success))
 elif a==0x4325e0:events.append(('delete',m.reg_read(UC_X86_REG_EBX),m.reg_read(UC_X86_REG_EDI),get(sp+4)))
for a,n in ((0x42da90,4),(0x4325e0,4)):
 cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
for leave_success,online,offline in ((False,0,0),(True,1,0),(True,0,1),(True,0,0)):
 events.clear()
 for a in (stack,user,guild):cpu.mem_write(a,bytes(0x10000))
 put(user+0x1758,guild);cpu.reg_write(UC_X86_REG_EBP,user);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.emu_start(0x476fbc,0x477678,count=100000)
 expect=[('leave',guild)]+([('delete',guild,0x5476e0,user)] if leave_success and not (online or offline) else [])
 assert cpu.reg_read(UC_X86_REG_EIP)==0x477678 and events==expect,(leave_success,online,offline,events)
print('4 original last-member guild leave cases passed; ps_game.exe SHA256',digest)
