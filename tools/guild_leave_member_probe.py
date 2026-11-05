#!/usr/bin/env python3
"""Execute LeaveGuild with isolated map, duel, database and transport owners."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,guild,record,node,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x10000000
for a in (stack,user,guild,record,node,stop):cpu.mem_map(a,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
events=[]
def hook(m,a,size,ctx):
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x434460:events.append(('take',get(get(sp+4))));m.reg_write(UC_X86_REG_EAX,record)
 elif a==0x435b10:events.append(('directory_find',));put(node,node+0x100);m.reg_write(UC_X86_REG_EAX,node)
 elif a==0x435860:events.append(('directory_erase',))
 elif a==0x47bd00:events.append(('duel',get(user+0x1748),get(user+0x174c),get(user+0x1758)))
 elif a in (0x4d4f10,0x4d4d20,0x4d4e50):events.append(({0x4d4f10:'db',0x4d4d20:'client',0x4d4e50:'log'}[a],bytes(m.mem_read(get(sp+4),get(sp+8)))))
 elif a==0x42c050:events.append(('guild',bytes(m.mem_read(m.reg_read(UC_X86_REG_ECX),m.reg_read(UC_X86_REG_EAX)))))
for a,n in ((0x434460,4),(0x435b10,0),(0x435860,12),(0x47bd00,4),(0x4d4f10,8),(0x4d4d20,8),(0x4d4e50,8),(0x42c050,4),(0x45b0d0,0),(0x505316,0),(stop+0x100,4),(stop+0x200,4)):
 cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
for a,v in ((0x5210cc,stop+0x100),(0x5210d0,stop+0x200),(0x546f44,stop+0x300),(0x546f4c,stop+0x400)):put(a,v)
for rank in (1,2,3):
 events.clear()
 for a in (stack,user,guild,record,node):cpu.mem_write(a,bytes(0x10000))
 put(0x3ed3310,0);put(0x3ed3318,0);put(0x547784,node+0x200)
 put(user+0xdc,43);put(user+0x1748,77);put(user+0x174c,rank);put(user+0x1758,guild);put(guild+8,77);cpu.mem_write(guild+12,b'GuildName\0');put(guild+0x3c,5);put(guild+0x514,1);put(guild+0x540,1 if rank>1 else 0);put(guild+0x4d8,record);put(record+8,43)
 put(stack+0xf000,stop);put(stack+0xf004,guild);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_ECX,user)
 cpu.emu_start(0x42da90,stop,count=100000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
 assert [e[0] for e in events]==['take','directory_find','directory_erase','duel','db','client','guild','log'],events
 assert events[3]==('duel',0,0,guild) and events[4][1]==struct.pack('<HII',0xb08,77,43) and events[5][1]==b'\x09\x0d\x01' and events[6][1]==struct.pack('<HBI',0xd0c,0x66,43) and events[7][1][:2]==b'\x05\x0b' and len(events[7][1])==110
 assert get(user+0x1748)==0 and get(user+0x174c)==0 and get(user+0x1758)==0 and get(guild+0x3c)==(4 if rank<=2 else 5) and get(guild+0x4d8)==(0 if rank<=2 else record)
print('3 original guild member leave cases passed; ps_game.exe SHA256',digest)
