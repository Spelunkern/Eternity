#!/usr/bin/env python3
"""Original CWorld::LeaveUser 0040ca20, initial location-4 branch."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stop,stack,user,world,zone=(0x10000000,0x20000000,0x30000000,0x31000000,0x32000000)
for p in (stop,stack,user,world,zone):cpu.mem_map(p,0x10000)
def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
def get(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
for ptr,target in ((0x5210cc,stop+0x100),(0x5210d0,stop+0x200),(0x5210e4,stop+0x300)):put(ptr,target)
for p,n in ((stop+0x100,4),(stop+0x200,4),(stop+0x300,0),(0x459380,4),(0x46c980,0),(0x46c660,0),(0x40d4d0,0),(0x4161b0,0)):cpu.mem_write(p,b'\xc2'+struct.pack('<H',n))
events=[];at_tail=False
def hook(m,p,size,data):
 global at_tail
 if p==0x40cbd0:at_tail=True;m.emu_stop()
 elif p==stop+0x300:events.append('tick');m.reg_write(UC_X86_REG_EAX,1234)
 elif p in (0x459380,0x46c980,0x46c660,0x40d4d0,0x4161b0):events.append(hex(p))
cpu.hook_add(UC_HOOK_CODE,hook)
for dead in (False,True):
 for final in (False,True):
  for logout in (0,1,2):
   for in_zone in (False,True):
    cpu.mem_write(user,bytes(0x7000));events.clear();at_tail=False
    put(user+0x5758,4);put(user+0x142c,1 if dead else 0);put(user+0x57bc,int(final));put(user+0x57d8,logout);put(user+0xe0,zone if in_zone else 0)
    put(stack+0x8000,stop,world);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,user);cpu.emu_start(0x40ca20,stop,count=100000)
    assert at_tail or cpu.reg_read(UC_X86_REG_EIP)==stop
    expected=['0x459380'] if dead else []
    if final:expected+=['0x40d4d0']+(['0x4161b0'] if in_zone else [])
    elif logout==0:expected+=['tick']
    else:expected+=['0x46c980','0x46c660']
    assert events==expected,(dead,final,logout,in_zone,events,expected)
    assert at_tail==final
    if not final and logout==0:assert get(user+0x57d8)==2 and get(user+0x57dc)==11234
print('24 original location-4 disconnect cases passed; ps_game.exe SHA256',digest)
