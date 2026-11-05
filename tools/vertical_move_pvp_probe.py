#!/usr/bin/env python3
"""Original PvPChkLeaveByMove 0047bef0, state-five branch with external ports."""
import math,struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_game
cpu,digest=load_game();stop,stack,user,peer,zone=(0x10000000,0x20000000,0x30000000,0x31000000,0x32000000)
for p in (0,stop,stack,user,peer,zone):cpu.mem_map(p,0x10000)
def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
for p,n in ((0x5048a4,0),(0x415f30,4),(0x47b200,0),(0x4d4d20,8),(0x505316,0),(0x47c990,4),(0x47c610,4),(0x4b8aa0,0),(0x45b0d0,0),(0x4d4e50,8)):cpu.mem_write(p,b'\xc2'+struct.pack('<H',n))
cpu.mem_write(0x5048a4,b'\xdd\x44\x24\x04\xd9\xfa\xc3');cpu.reg_write(UC_X86_REG_FPCW,0x27f);events=[];found=False
def hook(m,p,size,data):
 if p==0x415f30:events.append(('find',));m.reg_write(UC_X86_REG_EAX,peer if found else 0)
 elif p==0x47b200:events.append(('end',m.reg_read(UC_X86_REG_EDI)))
 elif p in (0x47c990,0x47c610):
  sp=m.reg_read(UC_X86_REG_ESP);owner=struct.unpack('<I',m.mem_read(sp+4,4))[0];events.append(('drop' if p==0x47c990 else 'clear',owner))
 elif p==0x4b8aa0:events.append(('reset',m.reg_read(UC_X86_REG_ESI)-0x1538))
 elif p==0x4d4e50:
  sp=m.reg_read(UC_X86_REG_ESP);ptr,n=struct.unpack('<2I',m.mem_read(sp+4,8));raw=bytes(m.mem_read(ptr,n));events.append(('log',n,int.from_bytes(raw[74:78],'little'),raw[99]))
 elif p==0x4d4d20:
  sp=m.reg_read(UC_X86_REG_ESP);ptr,n=struct.unpack('<2I',m.mem_read(sp+4,8));events.append(('send',m.reg_read(UC_X86_REG_ECX),bytes(m.mem_read(ptr,n))))
cpu.hook_add(UC_HOOK_CODE,hook)
for status in (0,1,2,5,6):
 for distance in (0.,39.9,40.,40.1,float('nan')):
  for found in (False,True):
   cpu.mem_write(user,bytes(0x7000));events.clear();put(user+0x54cc,status);put(user+0x54d4,43);put(user+0xe0,zone)
   cpu.mem_write(user+0xd0,struct.pack('<3f',distance,0.,0.));cpu.mem_write(user+0x54d8,struct.pack('<3f',0.,0.,0.))
   put(stack+0x8000,stop,user);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.emu_start(0x47bef0,stop,count=100000)
   assert cpu.reg_read(UC_X86_REG_EIP)==stop
   expected=[]
   if status==5 and not math.isnan(distance) and distance>40:
    expected=[('find',)]+([('end',peer),('send',peer,b'\x08\x24\x03')] if found else [])+[('end',user),('send',user,b'\x08\x24\x02')]
   assert events==expected,(status,distance,found,events,expected)
print('50 original movement PvP state-five cases passed; ps_game.exe SHA256',digest)

for status in (3,4):
 for distance in (0.,40.,40.1,float('nan')):
  for found in (False,True):
   cpu.mem_write(user,bytes(0x7000));cpu.mem_write(peer,bytes(0x7000));events.clear();put(user+0x54cc,status);put(user+0x54d4,43);put(user+0xdc,42);put(peer+0xdc,43);put(user+0x128,420);put(peer+0x128,430);put(user+0xe0,zone)
   cpu.mem_write(user+0xd0,struct.pack('<3f',distance,0.,0.));cpu.mem_write(user+0x54d8,struct.pack('<3f',0.,0.,0.))
   put(stack+0x8000,stop,user);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.emu_start(0x47bef0,stop,count=100000)
   assert cpu.reg_read(UC_X86_REG_EIP)==stop
   expected=[]
   if not math.isnan(distance) and distance>40:
    expected=[('find',)]+([('drop',peer),('clear',peer),('reset',peer),('send',peer,b'\x06\x24\x01\x2a\x00\x00\x00')] if found else [])
    expected += [('drop',user),('clear',user),('reset',user),('send',user,b'\x06\x24\x01\x2a\x00\x00\x00'),('log',100,430 if found else 0,1)]
    if found:expected += [('log',100,420,1)]
   assert events==expected,(status,distance,found,events,expected)
print('16 original active-duel movement cases passed')
