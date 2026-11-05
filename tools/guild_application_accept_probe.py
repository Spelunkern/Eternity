#!/usr/bin/env python3
"""Execute original guild JoinOK with isolated map, world and transport owners."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,guild,record,peer,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
for a in (stack,guild,record,peer,stop):cpu.mem_map(a,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
events=[];online=False;present=False
def hook(m,a,size,ctx):
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x434460:events.append(('remove',get(get(sp+4))));m.reg_write(UC_X86_REG_EAX,record if present else 0)
 elif a==0x4343d0:events.append(('add',m.reg_read(UC_X86_REG_ESI),get(get(sp+4)),get(sp+8)))
 elif a==0x40d640:events.append(('lookup',get(sp+4)));m.reg_write(UC_X86_REG_EAX,peer if online else 0)
 elif a==0x4d4f10:events.append(('db',bytes(m.mem_read(get(sp+4),get(sp+8)))))
 elif a==0x4d4d20:events.append(('client',bytes(m.mem_read(get(sp+4),get(sp+8)))))
 elif a in (0x42c8f0,0x42c050):events.append(('admin' if a==0x42c8f0 else 'broadcast',bytes(m.mem_read(m.reg_read(UC_X86_REG_ECX),m.reg_read(UC_X86_REG_EAX)))))
 elif a in (0x42c980,0x42f250,0x42f0e0):events.append(({0x42c980:'members',0x42f250:'bank',0x42f0e0:'npcs'}[a],))
 elif a==0x4d4e50:events.append(('log',bytes(m.mem_read(get(sp+4),get(sp+8)))))
 elif a in (0x4cb8e0,stop+0x300):m.reg_write(UC_X86_REG_EAX,123)
for a,n in ((0x434460,4),(0x4343d0,8),(0x40d640,4),(0x4d4f10,8),(0x4d4d20,8),(0x42c8f0,4),(0x42c050,4),(0x42c980,8),(0x42f250,0),(0x42f0e0,4),(0x45b0d0,0),(0x4cb8e0,0),(0x4d4e50,8),(0x505316,0),(stop+0x100,4),(stop+0x200,4),(stop+0x300,0)):
 cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
for a,v in ((0x5210cc,stop+0x100),(0x5210d0,stop+0x200),(0x5210e4,stop+0x300),(0x546f44,stop+0x400),(0x546f4c,stop+0x500),(0x546f38,stop+0x600)):put(a,v)
def run(found,is_online):
 global present,online
 present=found;online=is_online;events.clear()
 for a in (stack,guild,record,peer):cpu.mem_write(a,bytes(0x10000))
 put(guild+8,77);cpu.mem_write(guild+12,b'GuildName\0');put(record+8,43);cpu.mem_write(record+12,b'Applicant\0');put(record+0x24,3);put(record+0x28,25);put(record+0x2c,0);put(peer+0x1750,77);cpu.mem_write(peer+0x136,struct.pack('<H',41))
 put(stack+0xf000,stop);put(stack+0xf004,43);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_ECX,guild);
 try:cpu.emu_start(0x42e330,stop,count=100000)
 except Exception:print("EIP",hex(cpu.reg_read(UC_X86_REG_EIP)),"events",events);raise
 assert cpu.reg_read(UC_X86_REG_EIP)==stop
 return list(events),get(record+0x2c),get(peer+0x1748),get(peer+0x174c),get(peer+0x1750),get(peer+0x1758),get(peer+0x54c0)
assert run(False,False)[0]==[('remove',43)]
for online in (False,True):
 e,rank,gid,level,req,member,deadline=run(True,online)
 assert [x[0] for x in e]==(['remove','lookup','add','db','admin','broadcast','client','members','bank','npcs','log'] if online else ['remove','lookup','add','db','admin','broadcast','log']),(online,e)
 assert e[2][1:]==(guild+(0x4f4 if online else 0x520),43,record) and e[3]==('db',struct.pack('<HIIB',0xb09,77,43,9)) and e[4]==('admin',struct.pack('<HI',0xd18,43))
 broadcast=e[5][1];assert len(broadcast)==32 and broadcast[:11]==struct.pack('<HBI BHB',0xd14,int(online),43,9,41 if online else 25,3) and broadcast[11:21]==b'Applicant\0'
 if online:assert (rank,gid,level,req,member,deadline)==(9,77,9,0,guild,0xf73147b) and e[6][1][:8]==struct.pack('<HBIB',0xd08,1,77,9) and len(e[6][1])==33
 else:assert rank==9 and gid==level==member==deadline==0 and req==77
 assert e[-1][1][:2]==b'\x04\x0b' and len(e[-1][1])==110
print('3 original guild application acceptance cases passed; ps_game.exe SHA256',digest)
