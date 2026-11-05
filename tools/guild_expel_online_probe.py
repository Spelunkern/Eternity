#!/usr/bin/env python3
"""Execute original JoinExpel with a retained online target fixture."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,target,guild,record,node,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000,0x10000000
for a in (stack,user,target,guild,record,node,stop):cpu.mem_map(a,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
events=[]
def hook(m,a,size,ctx):
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x435670:events.append(('find',));put(m.reg_read(UC_X86_REG_EAX),node);put(node+0x10,record)
 elif a==0x434460:events.append(('take',get(get(sp+4))));m.reg_write(UC_X86_REG_EAX,record)
 elif a==0x4347e0:events.append(('directory',get(get(sp+4))))
 elif a==0x47bd00:events.append(('duel',get(target+0x1748),get(target+0x174c),get(target+0x1758)))
 elif a in (0x4d4f10,0x4d4d20,0x4d4e50):events.append(({0x4d4f10:'db',0x4d4d20:'client',0x4d4e50:'log'}[a],m.reg_read(UC_X86_REG_ECX),bytes(m.mem_read(get(sp+4),get(sp+8)))))
 elif a==0x42c050:events.append(('guild',bytes(m.mem_read(m.reg_read(UC_X86_REG_ECX),m.reg_read(UC_X86_REG_EAX)))))
for a,n in ((0x435670,0),(0x434460,4),(0x4347e0,4),(0x47bd00,4),(0x45b0d0,0),(0x4d4f10,8),(0x4d4d20,8),(0x4d4e50,8),(0x42c050,4),(0x505316,0),(stop+0x100,4),(stop+0x200,4)):
 cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
for a,v in ((0x5210cc,stop+0x100),(0x5210d0,stop+0x200),(0x546f44,stop+0x300),(0x546f4c,stop+0x400)):put(a,v)
for rank in (2,9):
 events.clear()
 for a in (stack,user,target,guild,record,node):cpu.mem_write(a,bytes(0x10000))
 put(0x3ed3310,0);put(0x3ed3318,0);put(guild+0x510,node+0x100);put(guild+8,77);cpu.mem_write(guild+12,b'GuildName\0');put(guild+0x3c,5);put(guild+0x4d8,record);put(user+0x174c,1);cpu.mem_write(user+0x188,b'Requester\0');put(record+8,43);put(record+0x2c,rank);put(record+0x30,target);put(target+0x1748,77);put(target+0x174c,rank);put(target+0x1758,guild)
 put(stack+0xf000,stop);put(stack+0xf004,guild);put(stack+0xf008,43);put(stack+0xf00c,user);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.emu_start(0x42dcb0,stop,count=100000)
 assert cpu.reg_read(UC_X86_REG_EIP)==stop and [e[0] for e in events]==['find','take','directory','client','duel','db','log','client','guild'],events
 assert events[3]==('client',target,struct.pack('<HBI',0xd0a,1,43)) and events[4]==('duel',0,0,guild) and events[5][2]==struct.pack('<HII',0xb08,77,43) and events[6][2][:2]==b'\x06\x0b' and events[7]==('client',user,struct.pack('<HBI',0xd0a,1,43)) and events[8][1]==struct.pack('<HBI',0xd0c,0x67,43)
 assert get(target+0x1748)==0 and get(target+0x174c)==0 and get(target+0x1758)==0 and get(guild+0x3c)==(4 if rank<=2 else 5) and get(guild+0x4d8)==(0 if rank<=2 else record)
print('2 original online guild expulsion cases passed; ps_game.exe SHA256',digest)
