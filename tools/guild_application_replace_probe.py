#!/usr/bin/env python3
"""Probe PacketGuild's previous-request cancellation and new request order."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,target,old,packet,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x10000000
for a in (stack,user,target,old,packet,stop):cpu.mem_map(a,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
events=[];old_exists=False
def hook(m,a,size,ctx):
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x4328c0:
  key=get(sp+4);events.append(('lookup',key));m.reg_write(UC_X86_REG_EAX,target if key==77 else (old if old_exists else 0))
 elif a==0x42e040:events.append(('cancel',m.reg_read(UC_X86_REG_EBX),get(sp+4)))
 elif a==0x42e120:events.append(('create',get(sp+4),get(sp+8)))
for a,n in ((0x4328c0,4),(0x42e040,4),(0x42e120,8),(stop+0x100,4),(stop+0x200,4)):
 cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
put(0x5210cc,stop+0x100);put(0x5210d0,stop+0x200)
for pending,old_exists in ((0,False),(11,False),(11,True)):
 events.clear()
 for a in (stack,user,target,old,packet):cpu.mem_write(a,bytes(0x10000))
 cpu.mem_write(user+0x12d,b'\x01');put(user+0x1750,pending);put(target+0x40,1);put(packet+2,77)
 cpu.reg_write(UC_X86_REG_EBP,user);cpu.reg_write(UC_X86_REG_EBX,packet);cpu.reg_write(UC_X86_REG_ESI,stop+0x200);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000)
 cpu.emu_start(0x476e82,0x476ef6,count=100000);assert cpu.reg_read(UC_X86_REG_EIP)==0x476ef6
 expect=[('lookup',77)]+([('lookup',11)] if pending else [])+([('cancel',old,user)] if old_exists else [])+[('create',target,user)]
 assert events==expect,(events,expect)
print('3 original guild application replacement cases passed; ps_game.exe SHA256',digest)
