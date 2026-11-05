#!/usr/bin/env python3
"""CZone::SendView 004208c0 rectangle, buffer allocation and release."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESP
from original_oracle import load_game
cpu,digest=load_game();stop,stack,zone,rows,cells,buf,payload=(0x10000000,0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000)
for p in (stop,stack,zone,rows,cells,buf,payload):cpu.mem_map(p,0x10000)
def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
def get(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
put(0x5211f0,stop+0x100)
for p,n in ((stop+0x100,4),(0x4df9f0,0),(0x4243d0,4),(0x4df070,0)):cpu.mem_write(p,b'\xc2'+struct.pack('<H',n))
events=[];alloc=True
def hook(m,p,size,data):
 sp=m.reg_read(UC_X86_REG_ESP)
 if p==0x4df9f0:events.append(('alloc',bytes(m.mem_read(get(sp+4),get(sp+8)))));m.reg_write(UC_X86_REG_EAX,buf if alloc else 0)
 elif p==0x4243d0:
  addr=m.reg_read(UC_X86_REG_EDI);z=(addr-cells)//0x1000;x=((addr-cells)%0x1000)//0x124;events.append(('cell',x,z))
 elif p==stop+0x100:events.append(('release',get(sp+4)));m.reg_write(UC_X86_REG_EAX,0)
 elif p==0x4df070:events.append(('free',get(sp+4)))
cpu.hook_add(UC_HOOK_CODE,hook)
count=0
for width,height in ((1,1),(4,3),(7,6)):
 for x,z in ((0,0),(width-1,height-1),(width//2,height//2)):
  for alloc in (False,True):
   events.clear();put(zone+0x28,width,height,rows);put(rows,*[cells+i*0x1000 for i in range(height)]);cpu.mem_write(payload,b'\x02\x02\x2a\0\0\0');put(stack+0x8000,stop,zone,payload,6)
   cpu.reg_write(UC_X86_REG_ECX,x);cpu.reg_write(UC_X86_REG_EAX,z);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.emu_start(0x4208c0,stop,count=10000)
   expected=[('alloc',b'\x02\x02\x2a\0\0\0')]
   if alloc:expected += [('cell',ix,iz) for iz in range(max(z-2,0),min(z+2,height-1)+1) for ix in range(max(x-2,0),min(x+2,width-1)+1)]+[('release',buf+8),('free',buf)]
   assert events==expected,(width,height,x,z,alloc,events,expected);count+=1
print(count,'original zone send-view cases passed; ps_game.exe SHA256',digest)
