#!/usr/bin/env python3
"""ps_game.exe DB-agent 0302/0303 handler 00408d30."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_ECX
from original_oracle import load_game
cpu,digest=load_game();stop,stack,user,packet,world=(0x10000000,0x20000000,0x30000000,0x31000000,0x32000000)
for p in (stop,stack,user,packet,world):cpu.mem_map(p,0x10000)
def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
def get(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
for ptr,target in ((0x5210cc,stop+0x100),(0x5210d0,stop+0x200),(0x5210f8,stop+0x300)):put(ptr,target)
put(0x546f38,world);put(0x546f40,world)
for p,n in ((stop+0x100,4),(stop+0x200,4),(stop+0x300,4),(0x40d570,4),(0x4d43a0,8),(0x40ca20,4),(0x4d4f10,8)):cpu.mem_write(p,b'\xc2'+struct.pack('<H',n))
events=[];lookups=[]
def hook(m,p,size,data):
 sp=m.reg_read(UC_X86_REG_ESP)
 if p==0x40d570:events.append('lookup');m.reg_write(UC_X86_REG_EAX,lookups.pop(0))
 elif p==stop+0x300:
  addr=get(sp+4);value=(get(addr)+1)&0xffffffff;put(addr,value);m.reg_write(UC_X86_REG_EAX,value)
 elif p==0x4d43a0:events.append(('close',get(sp+4),get(sp+8)))
 elif p==0x40ca20:events.append('leave')
 elif p==0x4d4f10:events.append(('send',bytes(m.mem_read(get(sp+4),get(sp+8)))))
cpu.hook_add(UC_HOOK_CODE,hook)
count=0
for first,second in ((0,0),(user,0),(user,user)):
 for counter in (0,1,0xffffffff):
  for socket in (0xffffffff,3):
   for transfer in (0,1,2):
    cpu.mem_write(user,bytes(0x7000));events.clear();lookups[:]=[first,second];put(packet,0x0302 | (11<<16),0);put(user+0x57bc,counter);put(user+0x68,socket);put(user+0x57e0,transfer);put(stack+0x8000,stop)
    cpu.reg_write(UC_X86_REG_EAX,packet);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.emu_start(0x408d30,stop,count=1000)
    expected=['lookup']
    if first:
     expected+=['lookup']
     if second and ((counter+1)&0xffffffff)==1:
      if socket==0xffffffff:expected+=['leave']
      elif transfer!=1:expected += [('close',104,0)]
    assert events==expected,(first,second,counter,socket,transfer,events,expected)
    count+=1
print(count,'original DB 0302 acknowledgment cases passed; ps_game.exe SHA256',digest)
