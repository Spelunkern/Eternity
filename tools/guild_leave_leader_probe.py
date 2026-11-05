#!/usr/bin/env python3
"""Execute the original leader leave-count guard and failure reply."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,guild,stop=0x20000000,0x30000000,0x31000000,0x10000000
for a in (stack,user,guild,stop):cpu.mem_map(a,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
events=[]
def hook(m,a,size,ctx):
 if a==0x4d4d20:
  sp=m.reg_read(UC_X86_REG_ESP);events.append(bytes(m.mem_read(get(sp+4),get(sp+8))))
for a,n in ((0x4d4d20,8),(0x505316,0)):
 cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
for rank,online,offline,blocked in ((0,0,0,True),(1,1,1,True),(1,1,0,False),(2,3,2,False),(0xffffffff,0,0,True),(1,0xffffffff,2,False)):
 events.clear()
 for a in (stack,user,guild):cpu.mem_write(a,bytes(0x10000))
 put(user+0x174c,rank);put(guild+0x514,online);put(guild+0x540,offline);put(stack+0xf000,stop);put(stack+0xf004,guild)
 cpu.reg_write(UC_X86_REG_ECX,user);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000)
 cpu.emu_start(0x42da90,stop if blocked else 0x42dace,count=100000)
 assert cpu.reg_read(UC_X86_REG_EIP)==(stop if blocked else 0x42dace) and events==([b'\x09\x0d\x00'] if blocked else []),(rank,online,offline,events)
 if blocked:assert cpu.reg_read(UC_X86_REG_EAX)&255==0
print('6 original guild leader leave cases passed; ps_game.exe SHA256',digest)
