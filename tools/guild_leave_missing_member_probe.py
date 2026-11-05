#!/usr/bin/env python3
"""Probe the original LeaveGuild online member removal failure."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,guild,record,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
for a in (stack,user,guild,record,stop):cpu.mem_map(a,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
events=[];found=False
def hook(m,a,size,ctx):
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x434460:events.append(('take',get(get(sp+4))));m.reg_write(UC_X86_REG_EAX,record if found else 0)
 elif a==0x4d4d20:events.append(('client',bytes(m.mem_read(get(sp+4),get(sp+8)))))
for a,n in ((0x434460,4),(0x4d4d20,8),(0x505316,0)):
 cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
for rank,found in ((1,False),(2,False),(1,True),(2,True)):
 events.clear()
 for a in (stack,user,guild,record):cpu.mem_write(a,bytes(0x10000))
 put(user+0x174c,rank);put(user+0xdc,43);put(guild+0x514,1);put(stack+0xf000,stop);put(stack+0xf004,guild)
 cpu.reg_write(UC_X86_REG_ECX,user);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000)
 cpu.emu_start(0x42da90,0x42daec if found else stop,count=100000)
 assert cpu.reg_read(UC_X86_REG_EIP)==(0x42daec if found else stop) and events==([('take',43)] if found else [('take',43),('client',b'\x09\x0d\x00')]),(rank,found,events)
 if not found:assert cpu.reg_read(UC_X86_REG_EAX)&255==0
print('4 original missing guild member leave cases passed; ps_game.exe SHA256',digest)
