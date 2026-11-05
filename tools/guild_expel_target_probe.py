#!/usr/bin/env python3
"""Probe original JoinExpel target lookup and rank rejection."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,guild,record,node,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x10000000
for a in (stack,user,guild,record,node,stop):cpu.mem_map(a,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
events=[];online=offline=False
def hook(m,a,size,ctx):
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x435670:
  which='online' if m.reg_read(UC_X86_REG_ECX)==guild+0x50c else 'offline';events.append(('find',which));out=m.reg_read(UC_X86_REG_EAX);present=online if which=='online' else offline;put(out,node if present else node+0x100);put(node+0x10,record)
 elif a==0x4d4d20:events.append(('client',m.reg_read(UC_X86_REG_ECX),bytes(m.mem_read(get(sp+4),get(sp+8)))))
for a,n in ((0x435670,0),(0x4d4d20,8),(0x505316,0),(stop+0x100,4),(stop+0x200,4)):
 cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
for a,v in ((0x5210cc,stop+0x100),(0x5210d0,stop+0x200)):put(a,v)
for online,offline,target_rank,requester_rank in ((False,False,0,1),(True,False,1,1),(False,True,2,2),(True,True,0xffffffff,2),(True,False,3,2),(False,True,9,1)):
 events.clear()
 for a in (stack,user,guild,record,node):cpu.mem_write(a,bytes(0x10000))
 put(guild+0x510,node+0x100);put(guild+0x53c,node+0x100);put(user+0x174c,requester_rank);put(record+0x2c,target_rank);put(stack+0xf000,stop);put(stack+0xf004,guild);put(stack+0xf008,43);put(stack+0xf00c,user)
 cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);allowed=(online or offline) and (target_rank if target_rank<0x80000000 else target_rank-0x100000000)>requester_rank
 cpu.emu_start(0x42dcb0,0x42dd90 if allowed else stop,count=100000)
 expect=[('find','online')]+([] if online else [('find','offline')]);
 if (online or offline) and not allowed:expect.append(('client',user,struct.pack('<HBI',0xd0a,0,43)))
 assert cpu.reg_read(UC_X86_REG_EIP)==(0x42dd90 if allowed else stop) and events==expect,(online,offline,target_rank,requester_rank,events,expect)
print('6 original guild expulsion target cases passed; ps_game.exe SHA256',digest)
