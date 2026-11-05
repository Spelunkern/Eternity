#!/usr/bin/env python3
"""CUser::PvPChkLeaveByZone 0047b920 individual-duel branch."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESP
from original_oracle import load_game
cpu,digest=load_game();stop,stack,user,peer,zone=(0x10000000,0x20000000,0x30000000,0x31000000,0x32000000)
cpu.mem_map(0,0x1000)
for p in (stop,stack,user,peer,zone):cpu.mem_map(p,0x10000)
def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
def get(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
for p,n in ((0x415f30,4),(0x47b200,0),(0x47c990,4),(0x45b0d0,0),(0x4d4d20,8),(0x4d4e50,8)):cpu.mem_write(p,b'\xc2'+struct.pack('<H',n))
events=[];found=False
def hook(m,p,size,data):
 sp=m.reg_read(UC_X86_REG_ESP)
 if p==0x415f30:events.append(('find',get(sp+4)));m.reg_write(UC_X86_REG_EAX,peer if found else 0)
 elif p==0x47b200:events.append(('end',m.reg_read(UC_X86_REG_EDI)))
 elif p==0x47c990:events.append(('drop',m.reg_read(UC_X86_REG_EAX)))
 elif p==0x4d4d20:events.append(('send',m.reg_read(UC_X86_REG_ECX),bytes(m.mem_read(get(sp+4),get(sp+8)))))
 elif p==0x4d4e50:
  data=bytes(m.mem_read(get(sp+4),get(sp+8)));events.append(('log',data[0:2],data[99]))
cpu.hook_add(UC_HOOK_CODE,hook)
for state in (0,1,2,3,4,5,6):
 for found in (False,True):
  cpu.mem_write(user,bytes(0x7000));cpu.mem_write(peer,bytes(0x7000));events.clear();put(user+0xdc,42);put(peer+0xdc,99);put(user+0xe0,zone);put(user+0x54cc,state,0,99);put(stack+0x8000,stop,user)
  cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.emu_start(0x47b920,0x47bafb,count=10000)
  expected=[]
  if state:expected.append(('find',99))
  if state in (1,2,3,4,5) and found:
   if state in (3,4):expected.extend((('drop',peer),('drop',user)))
   expected.extend((('end',peer),('end',user)))
   if state==5:expected.extend((('send',user,b'\x08\x24\x02'),('send',peer,b'\x08\x24\x03')))
   else:
    identity=99 if state==1 else 42;packet=bytes([6 if state in (3,4) else 2,0x24,0 if state in (3,4) else 3])+struct.pack('<I',identity)
    expected.append(('send',peer,packet))
    if state in (3,4):expected.extend((('log',b'\x03\x0e',0),('log',b'\x03\x0e',0)))
    else:expected.append(('send',user,packet))
  assert events==expected,(state,found,events,expected)
print('14 original zone-leave individual duel cases passed; ps_game.exe SHA256',digest)
