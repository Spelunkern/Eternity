#!/usr/bin/env python3
"""Probe ps_game CGuild::NpcUpgrade against executable tables and live slots."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,guild,stop=0x20000000,0x30000000,0x31000000,0x10000000
for a in (stack,user,guild,stop):cpu.mem_map(a,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
events=[]
def hook(m,a,size,ctx):
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x4d4d20:events.append(('self',bytes(m.mem_read(get(sp+4),get(sp+8)))))
 elif a==0x42c050:events.append(('guild',bytes(m.mem_read(m.reg_read(UC_X86_REG_ECX),m.reg_read(UC_X86_REG_EAX)))))
 elif a==0x4d4f10:events.append(('db',bytes(m.mem_read(get(sp+4),get(sp+8)))))
 elif a==0x4d4e50:events.append(('log',get(sp+8)))
 elif a==stop+0x300:
  ptr,delta=get(sp+4),get(sp+8);old=get(ptr);put(ptr,old+delta);m.reg_write(UC_X86_REG_EAX,old)
for a,n in ((0x4d4d20,8),(0x42c050,4),(0x4d4f10,8),(0x4d4e50,8),(0x45b0d0,0),(0x505316,0),(stop+0x100,4),(stop+0x200,4),(stop+0x300,8)):
 cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
put(0x5210cc,stop+0x100);put(0x5210d0,stop+0x200);put(0x52111c,stop+0x300);put(0x546f44,stop+0x400);put(0x546f4c,stop+0x500);put(0x541b14,2000)
def run(rank=1,use=1,buy=1,kind=2,level=1,etin=10000,keep=2000,slots=()):
 events.clear();cpu.mem_write(guild,bytes(0x10000));cpu.mem_write(user,bytes(0x10000));cpu.mem_write(stack,bytes(0x10000));put(guild+8,77);cpu.mem_write(guild+0x48,bytes([rank]));put(guild+0x4c,etin);put(guild+0x50,keep);cpu.mem_write(guild+0x54,bytes([use,buy]));
 for i,(t,l) in enumerate(slots):cpu.mem_write(guild+0x47c+i*4,struct.pack('<HH',t,l))
 put(stack+0xf000,stop);put(stack+0xf004,guild);put(stack+0xf008,kind);put(stack+0xf00c,level);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_ECX,user);cpu.emu_start(0x430640,stop,count=100000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
 return list(events),get(guild+0x4c),get(guild+0x50),bytes(cpu.mem_read(guild+0x47c,32))
def failure(status,**kwargs):
 e,etin,keep,slots=run(**kwargs);assert len(e)==1 and e[0][0]=='self' and e[0][1][:3]==bytes([0x25,0x0d,status]),(kwargs,e)
for kwargs in ({'rank':31},{'use':0},{'buy':0},{'level':0},{'level':4},{'kind':999}):failure(3,**kwargs)
for kwargs in ({'kind':3,'rank':15},{'kind':0x401,'rank':7}):failure(4,**kwargs)
for kwargs in ({'etin':2000},{'etin':2500}):failure(1,**kwargs)
failure(2,slots=((2,2),),level=1)
failure(3,slots=((3,1),),level=2)
e,etin,keep,slots=run();assert [v[0] for v in e]==['guild','self','db','log'] and e[0][1]==struct.pack('<HBHBB',0xd25,0,2,1,0) and e[1][1]==struct.pack('<HI',0xd23,9000) and e[2][1]==struct.pack('<HIHBBII',0xb0f,77,2,1,0,9000,2200) and e[3]==('log',121) and (etin,keep)==(9000,2200) and slots[:4]==struct.pack('<HH',2,1)
e,etin,keep,slots=run(level=2,slots=((2,1),));assert (etin,keep)==(8500,2500) and e[2][1]==struct.pack('<HIHBBII',0xb0f,77,2,2,0,8500,2500)
e,etin,keep,slots=run(slots=((6,1),)*8);assert e[0][1]==struct.pack('<HBHBB',0xd25,0,2,1,8) and e[2][1]==struct.pack('<HIHBBII',0xb0f,77,2,1,8,9000,2800) and slots==struct.pack('<HH',6,1)*8
cpu.mem_write(0x541b7a,struct.pack('<H',1250));cpu.mem_write(0x541b1a,struct.pack('<H',321));put(0x541b14,3000)
e,etin,keep,slots=run();assert (etin,keep)==(8750,3321) and e[2][1]==struct.pack('<HIHBBII',0xb0f,77,2,1,0,8750,3321)
packet=0x32000000;cpu.mem_map(packet,0x10000);branch=[]
def capture_branch(m,a,size,ctx):
 sp=m.reg_read(UC_X86_REG_ESP);branch.append((m.reg_read(UC_X86_REG_ECX),get(sp+4),get(sp+8)&0xffff,get(sp+12)&255));m.emu_stop()
cpu.hook_add(UC_HOOK_CODE,capture_branch,begin=0x430640,end=0x430640)
for rank,has_guild in ((0,True),(1,True),(2,True),(3,True),(1,False)):
 branch.clear();cpu.mem_write(user,bytes(0x10000));cpu.mem_write(guild,bytes(0x10000));cpu.mem_write(stack,bytes(0x10000));cpu.mem_write(packet,struct.pack('<HHB',0xd25,0x401,3));put(user+0x174c,rank);put(user+0x1758,guild if has_guild else 0)
 cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_EBP,user);cpu.reg_write(UC_X86_REG_EBX,packet);cpu.emu_start(0x4771ca,0x47768c,count=100000)
 assert branch==([(user,guild,0x401,3)] if has_guild and rank<3 else []),(rank,has_guild,branch)
print('21 original guild NPC upgrade cases passed; ps_game.exe SHA256',digest)
