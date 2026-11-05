#!/usr/bin/env python3
"""Probe PacketGuild's original target lookup and faction check."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBP,UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,guild,packet,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
for a in (stack,user,guild,packet,stop):cpu.mem_map(a,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
events=[];exists=False
def hook(m,a,size,ctx):
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x4328c0:events.append(('lookup',get(sp+4)));m.reg_write(UC_X86_REG_EAX,guild if exists else 0)
 if a==0x4d4d20:events.append(('client',bytes(m.mem_read(get(sp+4),get(sp+8)))))
for a,n in ((0x4328c0,4),(0x4d4d20,8),(stop+0x100,4),(stop+0x200,4)):
 cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
put(0x5210cc,stop+0x100);put(0x5210d0,stop+0x200)
for exists,country,target_country in ((False,1,1),(True,1,2),(True,1,1),(True,255,255)):
 events.clear()
 for a in (stack,user,guild,packet):cpu.mem_write(a,bytes(0x10000))
 cpu.mem_write(user+0x12d,bytes([country]));put(guild+0x40,target_country);put(packet+2,77)
 cpu.reg_write(UC_X86_REG_EBP,user);cpu.reg_write(UC_X86_REG_EBX,packet);cpu.reg_write(UC_X86_REG_ESI,stop+0x200);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000)
 blocked=not exists or country!=target_country
 cpu.emu_start(0x476e82,0x47768c if blocked else 0x476ead,count=100000)
 assert cpu.reg_read(UC_X86_REG_EIP)==(0x47768c if blocked else 0x476ead) and events==([('lookup',77),('client',b'\x07\x0d\x00')] if blocked else [('lookup',77)]),(exists,country,target_country,events)
print('4 original guild application target cases passed; ps_game.exe SHA256',digest)
