#!/usr/bin/env python3
"""Execute original guild JoinRequest with controlled map, allocator and transports."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,guild,user,record,node,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x10000000
for a in (stack,guild,user,record,node,stop):cpu.mem_map(a,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
events=[];exists=False;allocate=True
def hook(m,a,size,ctx):
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x435670:
  events.append(('find',));out=m.reg_read(UC_X86_REG_EAX);put(out,node if exists else 0);put(node+0x10,record)
 elif a==0x434570:events.append(('allocate',));m.reg_write(UC_X86_REG_EAX,record if allocate else 0)
 elif a==0x4343d0:events.append(('application',get(get(sp+4)),get(sp+8)))
 elif a==0x432ab0:events.append(('directory',get(sp+4),get(sp+8)))
 elif a in (0x4d4f10,0x4d4d20,0x4d4e50):events.append(({0x4d4f10:'db',0x4d4d20:'client',0x4d4e50:'log'}[a],bytes(m.mem_read(get(sp+4),get(sp+8)))))
 elif a==0x42c8f0:events.append(('admin',bytes(m.mem_read(m.reg_read(UC_X86_REG_ECX),m.reg_read(UC_X86_REG_EAX)))))
for a,n in ((0x435670,0),(0x434570,0),(0x4343d0,8),(0x432ab0,8),(0x4d4f10,8),(0x4d4d20,8),(0x4d4e50,8),(0x42c8f0,4),(0x45b0d0,0),(0x505316,0),(stop+0x100,4),(stop+0x200,4)):
 cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
for a,v in ((0x5210cc,stop+0x100),(0x5210d0,stop+0x200),(0x546f44,stop+0x300),(0x546f4c,stop+0x400)):put(a,v)
def run(is_existing,allocation):
 global exists,allocate
 exists=is_existing;allocate=allocation;events.clear()
 for a in (stack,guild,user,record,node):cpu.mem_write(a,bytes(0x10000))
 put(guild+8,77);cpu.mem_write(guild+12,b'GuildName\0');put(user+0xdc,43);put(user+0x1750,11);cpu.mem_write(user+0x134,b'\x03');cpu.mem_write(user+0x136,struct.pack('<H',25));cpu.mem_write(user+0x188,b'Applicant\0')
 put(stack+0xf000,stop);put(stack+0xf004,guild);put(stack+0xf008,user);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000)
 cpu.emu_start(0x42e120,stop,count=100000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
 return list(events),get(user+0x1750),cpu.reg_read(UC_X86_REG_EAX)&255
for existing,allocate in ((True,True),(False,False)):
 e,request,ok=run(existing,allocate);assert ok==0 and request==11 and e[-1]==('client',b'\x07\x0d\x00'),e
 ekeys=[x[0] for x in e];assert ekeys==(['find','client'] if existing else ['find','allocate','client'])
e,request,ok=run(False,True);assert ok==1 and request==77 and [x[0] for x in e]==['find','allocate','application','directory','db','admin','client','log'],e
assert e[2]==('application',43,record) and e[3]==('directory',43,guild) and e[4]==('db',struct.pack('<HIIB',0xb07,77,43,0))
assert e[5][1][:9]==struct.pack('<HIHB',0xd17,43,25,3) and len(e[5][1])==30 and e[5][1][9:19]==b'Applicant\0'
assert e[6]==('client',b'\x07\x0d\x01') and e[7][1][:2]==b'\x03\x0b' and len(e[7][1])==110
assert get(record+8)==43 and get(record+0x24)==3 and get(record+0x28)==25 and get(record+0x2c)==0 and get(record+0x30)==0
print('3 original guild application creation cases passed; ps_game.exe SHA256',digest)
