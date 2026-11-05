#!/usr/bin/env python3
"""Compare original attack-range overloads with intercepted live mob position and clock."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
c,digest=load_game();base=0x30000000;c.mem_map(base,0x80000)
a,target,mob,info,stack,stop=base,base+0x10000,base+0x20000,base+0x30000,base+0x60000,base+0x7f000
state={}
def put(p,*v):c.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
def get(p):return struct.unpack('<I',c.mem_read(p,4))[0]
def fl(p,v):c.mem_write(p,struct.pack('<f',v))
def ret(cpu,n=0):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+n)
def hook(cpu,at,size,data):
 if at==0x4a6b90:
  if cpu.reg_read(UC_X86_REG_ESI)!=mob:raise RuntimeError('mob ABI')
  c.mem_write(cpu.reg_read(UC_X86_REG_EDI),struct.pack('<fff',state['distance'],0,0));state['events']+='P';ret(cpu)
 elif at==stop+0x100:
  state['events']+='T';cpu.reg_write(UC_X86_REG_EAX,state['now']);ret(cpu)
c.hook_add(UC_HOOK_CODE,hook);put(0x5210e4,stop+0x100)
c.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x1000)+b'\xc3')
print('# ps_game.exe SHA256',digest,'CUser::EnableAttackRange 0044bf90/0044c030; mob current position and clock intercepted')
print('# mob speed weapon extra size now last distance result events')
for ismob in (0,1):
 for speed in (-1,0,2,4,5):
  for elapsed in (0,1,999,1000,1001,0xffffffff):
   for distance in (0.0,4.3,5.3,7.3,12.3,20.0):
    weapon=3;extra=1;size=2;now=1234;last=(now-elapsed)&0xffffffff
    c.mem_write(a,bytes(0x6000));c.mem_write(target,bytes(0x1000));c.mem_write(mob,bytes(0x1000));c.mem_write(info,bytes(0x100));
    for i,v in enumerate((1.,2.,3.,4.,5.)):fl(0x542508+4*i,v)
    put(a+0x12f0,speed);put(a+0x12e8,weapon);put(a+0x57cc,last);put(mob+0xa8,info);c.mem_write(info+0x3f,bytes([size]));fl(target+0xd0,distance)
    state.update(distance=distance,now=now,events='');put(stack,stop,*((mob,extra) if ismob else (extra,)))
    c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_EBX,a);c.reg_write(UC_X86_REG_ECX,a);c.reg_write(UC_X86_REG_EAX,target);c.reg_write(UC_X86_REG_FPCW,0x27f)
    c.emu_start(0x44c030 if ismob else 0x44bf90,stop,count=100000)
    if c.reg_read(UC_X86_REG_EIP)!=stop or c.reg_read(UC_X86_REG_ESP)!=stack+(12 if ismob else 8):raise RuntimeError('original range did not return')
    print(ismob,speed,weapon,extra,size,now,last,distance,c.reg_read(UC_X86_REG_EAX)&255,state['events'],sep='\t')
