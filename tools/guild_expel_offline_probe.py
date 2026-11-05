#!/usr/bin/env python3
"""Execute original JoinExpel with a nonnull offline target record."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,guild,record,node,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x10000000
for a in (stack,user,guild,record,node,stop):cpu.mem_map(a,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
events=[];in_online=False
def hook(m,a,size,ctx):
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x435670:
  which='online' if m.reg_read(UC_X86_REG_ECX)==guild+0x50c else 'offline';events.append(('find',which));out=m.reg_read(UC_X86_REG_EAX);present=in_online if which=='online' else not in_online;put(out,node if present else node+0x100);put(node+0x10,record)
 elif a==0x434460:
  which='online' if m.reg_read(UC_X86_REG_ESI)==guild+0x4f4 else 'offline';events.append(('take',which,get(get(sp+4))));m.reg_write(UC_X86_REG_EAX,record if (in_online if which=='online' else not in_online) else 0)
 elif a==0x4347e0:events.append(('directory',get(get(sp+4))))
 elif a==0x4cb8e0:m.reg_write(UC_X86_REG_EAX,0x12345678)
 elif a in (0x4d4f10,0x4d4d20,0x4d4e50):events.append(({0x4d4f10:'db',0x4d4d20:'client',0x4d4e50:'log'}[a],bytes(m.mem_read(get(sp+4),get(sp+8)))))
 elif a==0x42c050:events.append(('guild',bytes(m.mem_read(m.reg_read(UC_X86_REG_ECX),m.reg_read(UC_X86_REG_EAX)))))
for a,n in ((0x435670,0),(0x434460,4),(0x4347e0,4),(0x4cb8e0,0),(0x4d4f10,8),(0x4d4d20,8),(0x4d4e50,8),(0x42c050,4),(0x505316,0),(stop+0x100,4),(stop+0x200,4)):
 cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
for a,v in ((0x5210cc,stop+0x100),(0x5210d0,stop+0x200),(0x546f44,stop+0x300),(0x546f4c,stop+0x400)):put(a,v)
for in_online,rank in ((False,2),(False,9),(True,9)):
 events.clear()
 for a in (stack,user,guild,record,node):cpu.mem_write(a,bytes(0x10000))
 put(0x3ed3310,0);put(0x3ed3318,0);put(guild+0x510,node+0x100);put(guild+0x53c,node+0x100);put(guild+8,77);cpu.mem_write(guild+12,b'GuildName\0');put(guild+0x3c,5);put(user+0x174c,1);cpu.mem_write(user+0x188,b'Requester\0');put(record+8,43);put(record+0x2c,rank);cpu.mem_write(record+12,b'Target\0')
 put(stack+0xf000,stop);put(stack+0xf004,guild);put(stack+0xf008,43);put(stack+0xf00c,user);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.emu_start(0x42dcb0,stop,count=100000)
 expect=['find']+([] if in_online else ['find'])+['take']+([] if in_online else ['take'])+['directory','db','log','client','guild']
 assert cpu.reg_read(UC_X86_REG_EIP)==stop and [e[0] for e in events]==expect,events
 assert events[-5]==('directory',43) and events[-4]==('db',struct.pack('<HII',0xb08,77,43)) and events[-3][1][:2]==b'\x06\x0b' and events[-2]==('client',struct.pack('<HBI',0xd0a,1,43)) and events[-1]==('guild',struct.pack('<HBI',0xd0c,0x67,43)) and get(guild+0x3c)==(4 if rank<=2 else 5)
print('3 original offline guild expulsion cases passed; ps_game.exe SHA256',digest)
