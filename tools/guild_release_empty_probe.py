#!/usr/bin/env python3
"""Probe original CGuild::Release with no members and optional application."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,guild,record,node,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x10000000
for a in (stack,user,guild,record,node,stop):cpu.mem_map(a,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
events=[]
def hook(m,a,size,ctx):
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x42c050:events.append(('broadcast',bytes(m.mem_read(m.reg_read(UC_X86_REG_ECX),m.reg_read(UC_X86_REG_EAX)))))
 elif a==0x4d4e50:events.append(('log',bytes(m.mem_read(get(sp+4),get(sp+8)))))
 elif a==0x4cb8e0:m.reg_write(UC_X86_REG_EAX,123)
 elif a==0x4353c0:events.append(('map_erase',))
 elif a==0x435b10:events.append(('directory_find',get(m.reg_read(UC_X86_REG_EBX))));put(node+0x300,node+0x400);m.reg_write(UC_X86_REG_EAX,node+0x300)
 elif a==0x435860:events.append(('directory_erase',))
for a,n in ((0x42c050,4),(0x45b0d0,0),(0x4cb8e0,0),(0x4d4e50,8),(0x505316,0),(0x4353c0,12),(0x435b10,0),(0x435860,12),(stop+0x100,4),(stop+0x200,4)):
 cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
for a,v in ((0x5210cc,stop+0x100),(0x5210d0,stop+0x200),(0x546f4c,stop+0x300)):put(a,v)
for mode in ('empty','application','null'):
 events.clear()
 for a in (stack,user,guild,record,node):cpu.mem_write(a,bytes(0x10000))
 put(0x3ed3310,0);put(0x3ed3318,0);put(0x547784,node+0x500);put(guild+8,77);cpu.mem_write(guild+12,b'GuildName\0');put(guild+0x3c,5)
 for i,off in enumerate((0x4f4,0x520,0x54c)):
  sentinel=node+0x1000+i*0x100;put(guild+off+0x1c,sentinel);put(sentinel,sentinel);put(sentinel+4,sentinel);put(sentinel+8,sentinel);cpu.mem_write(sentinel+0x15,b'\x01')
  if off==0x54c and mode!='empty':
   entry=node+0x2000;put(sentinel,entry);put(entry,sentinel);put(entry+4,sentinel);put(entry+8,sentinel);put(entry+0x10,record if mode=='application' else 0)
 for i in range(7):put(guild+0x4d8+i*4,record)
 put(record+8,43);put(stack+0xf000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_ECX,guild);cpu.reg_write(UC_X86_REG_EDX,user)
 cpu.emu_start(0x42d5a0,stop,count=100000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
 expect=['broadcast','log']+(['map_erase','directory_find','directory_erase'] if mode=='application' else [])
 assert [e[0] for e in events]==expect,(mode,events)
 assert events[0][1]==b'\x0c\x0d\x65\0\0\0\0' and events[1][1][:2]==b'\x02\x0b' and get(guild+0x3c)==0 and all(get(guild+0x4d8+i*4)==0 for i in range(7))
 if mode=='application':assert events[3]==('directory_find',43)
print('3 original empty guild release cases passed; ps_game.exe SHA256',digest)
