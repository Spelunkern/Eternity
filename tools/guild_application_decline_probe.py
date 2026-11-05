#!/usr/bin/env python3
"""Execute original guild JoinNO with isolated lookup and transport owners."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,guild,record,peer,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
for a in (stack,guild,record,peer,stop):cpu.mem_map(a,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
events=[];online=False;present=False
def hook(m,a,size,ctx):
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x434460:events.append(('remove_application',get(get(sp+4))));m.reg_write(UC_X86_REG_EAX,record if present else 0)
 elif a==0x4347e0:events.append(('remove_index',get(get(sp+4))));m.reg_write(UC_X86_REG_EAX,guild)
 elif a==0x40d640:events.append(('lookup',get(sp+4)));m.reg_write(UC_X86_REG_EAX,peer if online else 0)
 elif a==0x4d4f10:events.append(('db',bytes(m.mem_read(get(sp+4),get(sp+8)))))
 elif a==0x4d4d20:events.append(('client',bytes(m.mem_read(get(sp+4),get(sp+8)))))
 elif a==0x42c8f0:events.append(('admin',bytes(m.mem_read(m.reg_read(UC_X86_REG_ECX),m.reg_read(UC_X86_REG_EAX)))))
for a,n in ((0x434460,4),(0x4347e0,4),(0x40d640,4),(0x4d4f10,8),(0x4d4d20,8),(0x42c8f0,4),(0x505316,0),(stop+0x100,4),(stop+0x200,4)):
 cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
put(0x5210cc,stop+0x100);put(0x5210d0,stop+0x200);put(0x546f44,stop+0x300);put(0x546f38,stop+0x400)
def run(is_present,is_online):
 global present,online
 present=is_present;online=is_online;events.clear()
 for a in (stack,guild,record,peer):cpu.mem_write(a,bytes(0x10000))
 put(guild+8,77);cpu.mem_write(guild+12,b'GuildName\0');put(record+8,43);put(peer+0x1750,77)
 put(stack+0xf000,stop);put(stack+0xf004,guild);put(stack+0xf008,43);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000)
 cpu.emu_start(0x42e630,stop,count=100000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
 return list(events),get(peer+0x1750),cpu.reg_read(UC_X86_REG_EAX)&255
assert run(False,False)==([('remove_application',43)],77,0)
for online in (False,True):
 e,request,ok=run(True,online);assert ok==1 and e[:4]==[('remove_application',43),('remove_index',43),('lookup',43),('db',struct.pack('<HII',0xb08,77,43))]
 if online:assert request==0 and e[4][0]=='client' and e[4][1][:8]==struct.pack('<HBI B',0xd08,0,77,9) and len(e[4][1])==33 and e[4][1][8:18]==b'GuildName\0' and e[5]==('admin',struct.pack('<HI',0xd18,43))
 else:assert request==77 and e[4]==('admin',struct.pack('<HI',0xd18,43))
print('3 original guild application decline cases passed; ps_game.exe SHA256',digest)
