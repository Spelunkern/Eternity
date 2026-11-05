#!/usr/bin/env python3
"""CZone::LeaveUser 004161b0 with lifecycle and list operations as ports."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_ESP
from original_oracle import load_game
cpu,digest=load_game();stop,stack,user,zone,rows,cells=(0x10000000,0x20000000,0x30000000,0x31000000,0x32000000,0x33000000)
for p in (stop,stack,user,zone,rows,cells):cpu.mem_map(p,0x10000)
def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
def get(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
for ptr,target in ((0x5210cc,stop+0x100),(0x5210d0,stop+0x200)):put(ptr,target)
for p,n in ((stop+0x100,4),(stop+0x200,4),(0x415ec0,0),(0x40f200,0),(0x44aee0,0),(0x4208c0,12)):cpu.mem_write(p,b'\xc2'+struct.pack('<H',n))
events=[]
def hook(m,p,size,data):
 if p in (0x415ec0,0x40f200,0x44aee0):events.append((hex(p),get(user+0xe0)))
 elif p==0x4208c0:
  sp=m.reg_read(UC_X86_REG_ESP);assert get(sp+4)==zone;data=bytes(m.mem_read(get(sp+8),get(sp+12)))
  events.append(('broadcast',m.reg_read(UC_X86_REG_ECX),m.reg_read(UC_X86_REG_EAX),data,get(user+0xe0)))
cpu.hook_add(UC_HOOK_CODE,hook)
for present in (False,True):
 for x,z in ((0,0),(1,0),(0,1),(1,1)):
  cpu.mem_write(user,bytes(0x7000));events.clear();put(zone+0x30,rows);put(rows,cells,cells+0x1000);put(user+0xdc,0x12345678);put(user+0xe0,zone if present else 0);put(user+0xe4,x);put(user+0xe8,z);put(stack+0x8000,stop)
  cpu.reg_write(UC_X86_REG_EAX,user);cpu.reg_write(UC_X86_REG_EBX,zone);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.emu_start(0x4161b0,stop,count=1000)
  expected=[('0x415ec0',zone),('0x40f200',zone),('0x44aee0',zone),('broadcast',x,z,b'\x02\x02\x78\x56\x34\x12',zone)] if present else []
  assert events==expected,(present,x,z,events);assert cpu.reg_read(UC_X86_REG_EAX)&255==int(present)
  if present:assert (get(user+0xe0),get(user+0xe4),get(user+0xe8))==(0,0,0)
print('8 original zone-leave cases passed; ps_game.exe SHA256',digest)
