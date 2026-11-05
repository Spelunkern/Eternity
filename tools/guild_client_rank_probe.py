#!/usr/bin/env python3
"""Execute ps_game CGuild::ChangeGuildUserLevel with retained record maps."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,guild,record,node,target,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x10000000
for a in (stack,guild,record,node,target,stop):cpu.mem_map(a,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
events=[];presence='online'
def capture(m,a,size,ctx):
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x435670:
  tree=m.reg_read(UC_X86_REG_ECX);out=m.reg_read(UC_X86_REG_EAX);live=(tree==guild+0x50c);found=(presence=='online' and live) or (presence=='offline' and not live);put(out,node if found else 0);put(node+0x10,record)
 elif a==0x4d4f10:events.append(('db',bytes(m.mem_read(get(sp+4),get(sp+8)))))
 elif a==0x42c050:events.append(('guild',bytes(m.mem_read(m.reg_read(UC_X86_REG_ECX),m.reg_read(UC_X86_REG_EAX)))))
for a,cleanup in ((0x435670,0),(0x4d4f10,8),(0x42c050,4),(stop+0x100,4),(stop+0x200,4)):
 cpu.mem_write(a,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,capture,begin=a,end=a)
put(0x5210cc,stop+0x100);put(0x5210d0,stop+0x200);put(0x546f44,stop+0x300)
def run(action,rank,old,officers,where):
 global presence
 presence=where;events.clear()
 for a in (guild,record,node,target,stack):cpu.mem_write(a,bytes(0x10000))
 put(guild+8,77);put(guild+0x3c,officers);put(record+8,43);put(record+0x2c,old);put(record+0x30,target if where=='online' else 0)
 if old==2 and where=='online':put(guild+0x4d8,record)
 put(stack+0xf000,stop);put(stack+0xf004,43);put(stack+0xf008,rank)
 cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_EDI,guild);cpu.reg_write(UC_X86_REG_EAX,action)
 cpu.emu_start(0x42e9f0,stop,count=100000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
 return cpu.reg_read(UC_X86_REG_EAX)&255,get(record+0x2c),get(guild+0x3c),get(target+0x174c),get(guild+0x4d8),list(events)
for where in ('online','offline'):
 for action,old,new,off,after in ((0,3,2,1,2),(1,2,3,2,1),(0,9,8,1,1)):
  ok,r,n,live,leader,e=run(action,1,old,off,where);assert (ok,r,n)==(1,new,after) and (where!='online' or live==new)
  assert [x[0] for x in e]==['db','guild'] and e[0][1]==struct.pack('<HIIB',0xb09,77,43,new) and e[1][1]==struct.pack('<HBI',0xd0c,(new-0x38)&255,43),(action,where,e)
for args in ((2,1,3,1,'online'),(0,2,3,1,'online'),(0,1,3,7,'online'),(1,1,9,1,'online'),(0,1,3,1,'missing')):
 assert run(*args)[0]==0 and not events,args
print('11 original guild rank cases passed; ps_game.exe SHA256',digest)
