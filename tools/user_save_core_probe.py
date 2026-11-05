#!/usr/bin/env python3
"""SendDBAgentCharGetInfo 0046c980 core and equipment prefix."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_ESP
from original_oracle import load_game
cpu,digest=load_game();stop,stack,user,items=(0x10000000,0x20000000,0x30000000,0x31000000)
for p in (stop,stack,user,items):cpu.mem_map(p,0x10000)
def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
def get(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
cpu.mem_write(0x505bb0,b'\x5a\x29\xc4\xff\xe2');cpu.mem_write(0x4d4f10,b'\xc2\x08\x00');events=[]
def hook(m,p,size,data):
 if p==0x4d4f10:
  sp=m.reg_read(UC_X86_REG_ESP);events.append(bytes(m.mem_read(get(sp+4),get(sp+8))))
cpu.hook_add(UC_HOOK_CODE,hook)
for mask in (0,1,0x555,0x1fff):
 for changed in (False,True):
  cpu.mem_write(user,bytes(0x7000));cpu.mem_write(items,bytes(0x10000));events.clear();put(user+0x578c,0x12345678);put(user+0x1238,0x10001);put(user+0x123c,0x20002);put(user+0x1240,0x30003)
  cpu.mem_write(user+0x164,struct.pack('<HH',321,654));cpu.mem_write(user+0xd0,struct.pack('<fff',1.25,-2.5,3.75))
  for i in range(13):
   cpu.mem_write(user+0x1aa+2*i,struct.pack('<H',100+i))
   if mask & (1<<i):put(user+0x1c4+4*i,items+0x100*i);cpu.mem_write(items+0x100*i+0x44,struct.pack('<H',100+i+int(changed)))
  put(stack+0x8000,stop);cpu.reg_write(UC_X86_REG_ECX,user);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.emu_start(0x46c980,0x46cab9,count=10000)
  expected=[struct.pack('<HIHHH',0x606,0x12345678,1,2,3),struct.pack('<HIHHfff',0x607,0x12345678,321,654,1.25,-2.5,3.75)]
  if changed and mask:
   rows=b''.join(struct.pack('<BH',i,101+i) for i in range(13) if mask & (1<<i));expected.append(struct.pack('<HIB',0x70d,0x12345678,mask.bit_count())+rows)
  assert events==expected,(mask,changed,events,expected)
print('8 original save core/equipment cases passed; ps_game.exe SHA256',digest)
