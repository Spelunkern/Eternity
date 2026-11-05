#!/usr/bin/env python3
"""Probe original CUser::JoinGuild with retained map and transport hooks."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,guild,record,user,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
for a in (stack,guild,record,user,stop):cpu.mem_map(a,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
events=[]
def hook(m,a,size,ctx):
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x434570:events.append(('allocate',));m.reg_write(UC_X86_REG_EAX,record)
 elif a in (0x435300,0x4357a0):events.append(('member' if a==0x435300 else 'directory',get(user+0x1748),get(user+0x174c),get(user+0x1758)))
 elif a in (0x4d4f10,0x4d4d20,0x4d4e50):events.append(({0x4d4f10:'db',0x4d4d20:'client',0x4d4e50:'log'}[a],bytes(m.mem_read(get(sp+4),get(sp+8)))))
 elif a==0x42c050:events.append(('broadcast',bytes(m.mem_read(m.reg_read(UC_X86_REG_ECX),m.reg_read(UC_X86_REG_EAX)))))
 elif a in (0x42c980,0x42f250,0x42f0e0):events.append(({0x42c980:'members',0x42f250:'bank',0x42f0e0:'npcs'}[a],))
 elif a==stop+0x300:events.append(('tick',));m.reg_write(UC_X86_REG_EAX,123)
for a,n in ((0x434570,0),(0x435300,4),(0x4357a0,4),(0x4d4f10,8),(0x4d4d20,8),(0x42c050,4),(0x42c980,8),(0x42f250,0),(0x42f0e0,4),(0x45b0d0,0),(0x4d4e50,8),(0x505316,0),(stop+0x100,4),(stop+0x200,4),(stop+0x300,0)):
 cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
for a,v in ((0x5210cc,stop+0x100),(0x5210d0,stop+0x200),(0x5210e4,stop+0x300),(0x546f44,stop+0x400),(0x546f4c,stop+0x500)):put(a,v)
put(guild+8,77);cpu.mem_write(guild+12,b'GuildName\0');put(user+0xdc,43);cpu.mem_write(user+0x188,b'Applicant\0');cpu.mem_write(user+0x134,b'\x03');cpu.mem_write(user+0x136,struct.pack('<H',41));put(stack+0xf000,stop);put(stack+0xf004,guild);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_ECX,user)
cpu.emu_start(0x42e7a0,stop,count=100000)
assert cpu.reg_read(UC_X86_REG_EIP)==stop
assert [e[0] for e in events]==['allocate','member','directory','db','client','broadcast','members','log','bank','npcs','tick'],events
assert events[1][1:]==(77,9,guild) and events[2][1:]==(77,9,guild)
assert events[3]==('db',struct.pack('<HIIB',0xb07,77,43,9))
assert events[4][1][:8]==struct.pack('<HBIB',0xd08,1,77,9) and len(events[4][1])==33
assert events[5][1][:11]==struct.pack('<HBIBHB',0xd14,1,43,9,41,3) and len(events[5][1])==32
assert get(user+0x54c0)==0xf73147b and get(record+8)==43 and get(record+0x30)==user and get(record+0x34)==0
print('original JoinGuild owner and transport ordering passed; ps_game.exe SHA256',digest)
