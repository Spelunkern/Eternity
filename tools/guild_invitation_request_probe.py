#!/usr/bin/env python3
"""Probe the original PacketGuild 0d05 invitation request prefix."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,peer,packet=0x20000000,0x30000000,0x31000000,0x32000000
for a in (stack,user,peer,packet):cpu.mem_map(a,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
events=[];found=False
def hook(m,a,size,ctx):
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x415f30:events.append(('find',get(sp+4)));m.reg_write(UC_X86_REG_EAX,peer if found else 0)
 elif a==0x4d4d20:events.append(('send',m.reg_read(UC_X86_REG_ECX),bytes(m.mem_read(get(sp+4),get(sp+8)))))
for a,n in ((0x415f30,4),(0x4d4d20,8)):
 cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
for rank,found,same,country in ((3,True,False,1),(2,False,False,1),(2,True,True,1),(2,True,False,2),(2,True,False,1),(0xffffffff,True,False,1)):
 events.clear()
 for a in (stack,user,peer,packet):cpu.mem_write(a,bytes(0x10000))
 put(user+0xdc,9);put(user+0x174c,rank);put(peer+0xdc,9 if same else 43);put(packet+2,43)
 cpu.mem_write(user+0x12d,b'\x01');cpu.mem_write(peer+0x12d,bytes([country]));cpu.reg_write(UC_X86_REG_EBP,user);cpu.reg_write(UC_X86_REG_EBX,packet);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000)
 eligible=(rank<=2 or rank>=0x80000000) and found and not same and country==1
 cpu.emu_start(0x476c20,0x476c6a if eligible else 0x47768c,count=100000)
 expect=([('find',43)] if rank<=2 or rank>=0x80000000 else [])+([] if eligible else [('send',user,struct.pack('<HBI',0xd06,0,43))])
 assert events==expect and cpu.reg_read(UC_X86_REG_EIP)==(0x476c6a if eligible else 0x47768c),(rank,found,same,country,events,expect)
print('6 original invitation request guard cases passed; ps_game.exe SHA256',digest)
# CUser temporary lock guard returns an empty guard object in these fixtures.
from unicorn.x86_const import UC_X86_REG_ESI
cpu.mem_write(0x42b800,b'\xc3')
def empty_guard(m,a,size,ctx):put(m.reg_read(UC_X86_REG_ESI),0)
cpu.hook_add(UC_HOOK_CODE,empty_guard,begin=0x42b800,end=0x42b800)
for offset in (0x1758,0x175c,0x1754,0x1750):
 events.clear();found=True
 for a in (stack,user,peer,packet):cpu.mem_write(a,bytes(0x10000))
 put(user+0xdc,9);put(user+0x174c,2);put(peer+0xdc,43);put(packet+2,43);put(peer+offset,0x4567)
 cpu.mem_write(user+0x12d,b'\x01');cpu.mem_write(peer+0x12d,b'\x01');cpu.reg_write(UC_X86_REG_EBP,user);cpu.reg_write(UC_X86_REG_EBX,packet);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000)
 cpu.emu_start(0x476c20,0x47768c,count=100000)
 assert events==[('find',43),('send',user,struct.pack('<HBI',0xd06,0,43))],(hex(offset),events)
 assert get(peer+0x1754)==(0x4567 if offset==0x1754 else 0)
print('4 original blocked invitation target cases passed')
# The target-notice header is read from stack scratch at original ESP+0x1c.
stop=0x33000000;cpu.mem_map(stop,0x10000);put(0x5210e4,stop+0x100);cpu.mem_write(stop+0x100,b'\xc3')
def tick_hook(m,a,size,ctx):
 events.append(('tick',get(peer+0x1754),get(peer+0x5838)))
 m.reg_write(UC_X86_REG_EAX,0xfffffff0)
cpu.hook_add(UC_HOOK_CODE,tick_hook,begin=stop+0x100,end=stop+0x100)
for header in (b'\x05\x0d',b'\xa5\x5a'):
 events.clear();found=True
 for a in (stack,user,peer,packet):cpu.mem_write(a,bytes(0x10000))
 put(user+0xdc,9);put(user+0x174c,2);put(peer+0xdc,43);put(packet+2,43)
 cpu.mem_write(user+0x12d,b'\x01');cpu.mem_write(peer+0x12d,b'\x01')
 sp=stack+0xf000;cpu.mem_write(sp+0x1c,header);cpu.reg_write(UC_X86_REG_EBP,user);cpu.reg_write(UC_X86_REG_EBX,packet);cpu.reg_write(UC_X86_REG_ESP,sp)
 cpu.emu_start(0x476c20,0x477678,count=100000)
 assert events==[('find',43),('tick',9,0),('send',peer,header+struct.pack('<I',9))] and get(peer+0x1754)==9 and get(peer+0x5838)==0x7520,(header,events)
print('2 original successful invitation cases passed, including preserved scratch header')
