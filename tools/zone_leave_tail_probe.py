#!/usr/bin/env python3
"""CUser::LeaveZone tail 0044b1eb..0044b225, duel call as port."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_ESP
from original_oracle import load_game
cpu,digest=load_game();stop,stack,user=(0x10000000,0x20000000,0x30000000)
for p in (stop,stack,user):cpu.mem_map(p,0x10000)
def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
def get(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
cpu.mem_write(0x47b920,b'\xc2\x04\x00');events=[]
def hook(m,p,size,data):
 if p==0x47b920:events.append((get(user+0x1460),get(user+0x1464),get(user+0x147c),get(user+0x1480)))
cpu.hook_add(UC_HOOK_CODE,hook)
for mode in (0,1,2,3,0xffffffff):
 for pending in (0,123):
  cpu.mem_write(user,bytes(0x7000));events.clear();put(user+0x1460,mode,55);put(user+0x147c,pending,66);put(stack+0x8000,0)
  cpu.reg_write(UC_X86_REG_ESI,user);cpu.reg_write(UC_X86_REG_EBX,0);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.emu_start(0x44b1eb,0x44b220,count=1000)
  expected=(0 if mode==1 else mode,0 if mode==1 else 55,0,0 if pending else 66)
  assert events==[expected],(mode,pending,events,expected)
print('10 original zone-leave tail cases passed; ps_game.exe SHA256',digest)
