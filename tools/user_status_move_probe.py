#!/usr/bin/env python3
"""Original CUser::StatusMove 0048a090, clock port only."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,stop=0x20000000,0x30000000,0x10000000
for p in (stack,user,stop):cpu.mem_map(p,0x10000)
def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
def get(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
put(0x5210e4,stop+0x100);cpu.mem_write(stop+0x100,b'\xc3');calls=0
def hook(m,address,size,data):
 global calls
 if address==stop+0x100:calls+=1;m.reg_write(UC_X86_REG_EAX,0x12345678)
cpu.hook_add(UC_HOOK_CODE,hook)
for mode in (0,1,2,3,4):
 for previous_tick in (0,99):
  for target in (0,1,2):
   for self_target in (False,True):
    cpu.mem_write(user,bytes(0x7000));calls=0;put(user+0xdc,42);put(user+0x142c,mode);put(user+0x143c,previous_tick);put(user+0x1440,77);put(user+0x1450,88)
    put(user+0x1528,target,42 if self_target else 43,2,99);put(stack+0x8000,stop)
    cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ESI,user);cpu.emu_start(0x48a090,stop,count=1000)
    assert cpu.reg_read(UC_X86_REG_EIP)==stop
    actual=(get(user+0x142c),get(user+0x143c),get(user+0x1440),get(user+0x1450),get(user+0x1528),get(user+0x152c),calls)
    t=(2,99) if mode==3 and target==1 and self_target else (target,42 if self_target else 43)
    if mode!=3:expected=(mode,previous_tick,77,88,*t,0)
    elif previous_tick:expected=(2,previous_tick,1,0x12345678,*t,1)
    else:expected=(0,0,0,88,0,t[1],0)
    assert actual==expected,(mode,previous_tick,target,self_target,actual,expected)
print('60 original StatusMove cases passed; ps_game.exe SHA256',digest)
