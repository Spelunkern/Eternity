#!/usr/bin/env python3
"""Original zone priority-range and ring cell traversal; allocator/cell sender ports."""
import struct,math
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_game
cpu,digest=load_game();stop,stack,zone,grid,pos,packet,buffer=(0x10000000,0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000)
for p in (stop,stack,zone,grid,pos,packet,buffer):cpu.mem_map(p,0x10000)
def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
def bits(f):return struct.unpack('<I',struct.pack('<f',f))[0]
put(zone+0x28,7,5,grid);put(grid,*(grid+0x1000+z*7*292 for z in range(5)))
for p,n in ((0x4df9f0,0),(0x424780,20),(0x424880,24),(stop+0x100,4)):cpu.mem_write(p,b'\xc2'+struct.pack('<H',n))
put(0x5211f0,stop+0x100);cpu.reg_write(UC_X86_REG_FPCW,0x27f);events=[]
def hook(m,p,size,data):
 if p==0x4df9f0:m.reg_write(UC_X86_REG_EAX,buffer)
 elif p==stop+0x100:m.reg_write(UC_X86_REG_EAX,1)
 elif p in (0x424780,0x424880):
  off=m.reg_read(UC_X86_REG_EBX)-grid-0x1000;z,rem=divmod(off,7*292);x,rem=divmod(rem,292);assert rem==0;events.append((x,z))
cpu.hook_add(UC_HOOK_CODE,hook)
for x,z in ((0.,0.),(39.9,40.),(80.,80.),(159.9,119.9),(279.,199.)):
 for outer in (80.,120.):
  x=struct.unpack('<f',struct.pack('<f',x))[0];z=struct.unpack('<f',struct.pack('<f',z))[0]
  cpu.mem_write(pos,struct.pack('<3f',x,0,z));events.clear()
  args=(zone,packet,21,pos,bits(outer),42,0,7) if outer==80 else (zone,packet,21,pos,bits(80.),bits(outer),42,0,7)
  put(stack+0x8000,stop,*args);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000)
  cpu.emu_start(0x421220 if outer==80 else 0x4213d0,stop,count=100000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
  lo_x=max(0,math.trunc((x-outer)/40));hi_x=min(6,math.trunc((x+outer)/40));lo_z=max(0,math.trunc((z-outer)/40));hi_z=min(4,math.trunc((z+outer)/40))
  expected=[(xx,zz) for zz in range(lo_z,hi_z+1) for xx in range(lo_x,hi_x+1)]
  assert events==expected,(x,z,outer,events,expected)
print('10 original priority-area traversals passed; ps_game.exe SHA256',digest)
