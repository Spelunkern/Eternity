#!/usr/bin/env python3
"""Probe original PacketGuild invitation reply decline and missing inviter."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,peer,packet,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
for a in (stack,user,peer,packet,stop):cpu.mem_map(a,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
events=[];found=False
def hook(m,a,size,ctx):
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x415f30:events.append(('find',get(sp+4)));m.reg_write(UC_X86_REG_EAX,peer if found else 0)
 elif a==0x4d4d20:events.append(('send',m.reg_read(UC_X86_REG_ECX),bytes(m.mem_read(get(sp+4),get(sp+8)))))
for a,n in ((0x415f30,4),(0x4d4d20,8)):
 cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
for pending,action,found in ((0,0,False),(0,1,True),(43,0,False),(43,1,False),(43,0,True),(43,2,True),(43,1,True)):
 events.clear()
 for a in (stack,user,peer,packet):cpu.mem_write(a,bytes(0x10000))
 put(user+0x1754,pending);put(user+0x5838,123);cpu.mem_write(packet+2,bytes([action]));cpu.reg_write(UC_X86_REG_EBP,user);cpu.reg_write(UC_X86_REG_EBX,packet);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000)
 continue_accept=bool(pending and action==1 and found);cpu.emu_start(0x476d21,0x476d4b if continue_accept else 0x47768c,count=100000)
 expect=([('find',pending)] if pending else [])+([('send',peer,struct.pack('<HBI',0xd06,0,pending))] if pending and found and action!=1 else [])
 assert cpu.reg_read(UC_X86_REG_EIP)==(0x476d4b if continue_accept else 0x47768c) and events==expect,(pending,action,found,events,expect)
 assert get(user+0x1754)==(pending if continue_accept else 0) and get(user+0x5838)==(123 if not pending or continue_accept else 0)
print('7 original guild invitation reply cases passed; ps_game.exe SHA256',digest)
# The small constructor owns an optional cleanup object; return an empty one.
from unicorn.x86_const import UC_X86_REG_ESI
cpu.mem_write(0x42b800,b'\xc3')
def empty_guard(m,a,size,ctx):
 put(m.reg_read(UC_X86_REG_ESI),0)
cpu.hook_add(UC_HOOK_CODE,empty_guard,begin=0x42b800,end=0x42b800)
for guild,rank in ((0,1),(0x33000000,3),(0x33000000,99)):
 events.clear();found=True
 for a in (stack,user,peer,packet):cpu.mem_write(a,bytes(0x10000))
 put(user+0x1754,43);put(user+0x5838,123);put(peer+0x1758,guild);put(peer+0x174c,rank)
 cpu.mem_write(packet+2,b'\x01');cpu.reg_write(UC_X86_REG_EBP,user);cpu.reg_write(UC_X86_REG_EBX,packet);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000)
 cpu.emu_start(0x476d21,0x47768c,count=100000)
 assert events==[('find',43)] and get(user+0x1754)==0 and get(user+0x5838)==0,(guild,rank,events)
print('3 original ineligible inviter cases passed')
for rank in (0,1,2,0xffffffff):
 events.clear();found=True
 for a in (stack,user,peer,packet):cpu.mem_write(a,bytes(0x10000))
 put(user+0x1754,43);put(user+0x5838,123);put(peer+0x1758,0x33000000);put(peer+0x174c,rank)
 cpu.mem_write(packet+2,b'\x01');cpu.reg_write(UC_X86_REG_EBP,user);cpu.reg_write(UC_X86_REG_EBX,packet);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000)
 cpu.emu_start(0x476d21,0x476d72,count=100000)
 assert cpu.reg_read(UC_X86_REG_EIP)==0x476d72 and events==[('find',43)] and get(user+0x1754)==43
print('4 original signed rank boundary cases reached guild join')
# Verify successful join is followed by the inviter notice using current state.
cpu.mem_write(0x42e7a0,b'\xc2\x04\x00')
def join_hook(m,a,size,ctx):
 events.append(('join',m.reg_read(UC_X86_REG_ECX),get(m.reg_read(UC_X86_REG_ESP)+4)))
 put(user+0x1754,88)
cpu.hook_add(UC_HOOK_CODE,join_hook,begin=0x42e7a0,end=0x42e7a0)
events.clear();found=True
for a in (stack,user,peer,packet):cpu.mem_write(a,bytes(0x10000))
put(user+0x1754,43);put(user+0x5838,123);put(peer+0x1758,0x33000000);put(peer+0x174c,2)
cpu.mem_write(packet+2,b'\x01');cpu.reg_write(UC_X86_REG_EBP,user);cpu.reg_write(UC_X86_REG_EBX,packet);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000)
cpu.emu_start(0x476d21,0x47768c,count=100000)
assert events==[('find',43),('join',user,0x33000000),('send',peer,struct.pack('<HBI',0xd06,1,88))] and get(user+0x1754)==get(user+0x5838)==0,events
print('original accepted reply join/notice ordering passed')
