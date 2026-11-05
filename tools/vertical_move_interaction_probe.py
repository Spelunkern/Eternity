#!/usr/bin/env python3
"""Original PacketPC movement interaction cleanup; send, PK, CRT sqrt are ports."""
import math,struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_game
cpu,digest=load_game();stack,user,peer,npc,stop=(0x20000000,0x30000000,0x31000000,0x32000000,0x10000000)
for p in (stack,user,peer,npc,stop):cpu.mem_map(p,0x10000)
def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
def get(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
cpu.mem_write(0x5048a4,b'\xdd\x44\x24\x04\xd9\xfa\xc3');cpu.mem_write(0x4d4d20,b'\xc2\x08\x00');cpu.mem_write(0x47bef0,b'\xc2\x04\x00');cpu.reg_write(UC_X86_REG_FPCW,0x27f)
events=[]
def hook(m,p,size,data):
 if p==0x4d4d20:
  sp=m.reg_read(UC_X86_REG_ESP);ptr,n=struct.unpack('<2I',m.mem_read(sp+4,8));events.append((m.reg_read(UC_X86_REG_ECX),bytes(m.mem_read(ptr,n))))
 elif p==0x47bef0:events.append(('pk',))
cpu.hook_add(UC_HOOK_CODE,hook)
for distance in (0.,9.9,10.,10.1,float('nan')):
 for present in (False,True):
  for trading in (False,True):
   cpu.mem_write(user,bytes(0x7000));cpu.mem_write(peer,bytes(0x7000));cpu.mem_write(npc,bytes(0x100));events.clear()
   cpu.mem_write(user+0xd0,struct.pack('<3f',0.,0.,0.));cpu.mem_write(peer+0xd0,struct.pack('<3f',distance,2.,0.));cpu.mem_write(npc+8,struct.pack('<3f',distance,2.,0.));put(npc+0x40,2)
   if present:put(user+0x1564,npc)
   if trading:
    cpu.mem_write(user+0x1568,bytes([0xab])*37);cpu.mem_write(peer+0x1568,bytes([0xab])*37);put(user+0x1568,3,peer)
   cpu.reg_write(UC_X86_REG_EDI,user);cpu.reg_write(UC_X86_REG_EBX,0xffffffff);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000)
   cpu.emu_start(0x46a59f,0x46a64d,count=100000);assert cpu.reg_read(UC_X86_REG_EIP)==0x46a64d
   apart=not math.isnan(distance) and distance>10
   assert get(user+0x1564)==(0 if present and apart else npc if present else 0)
   assert get(npc+0x40)==(1 if present and apart else 2)
   if trading and apart:
    assert bytes(cpu.mem_read(user+0x1568,37))==bytes(37) and bytes(cpu.mem_read(peer+0x1568,37))==bytes(37)
    assert events==[(user,b'\x04\x0a\x02'),(peer,b'\x04\x0a\x03'),('pk',)]
   else:assert events==[('pk',)],events
print('20 original movement interaction cases passed; ps_game.exe SHA256',digest)
