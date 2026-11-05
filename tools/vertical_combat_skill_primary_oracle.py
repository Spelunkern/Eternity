#!/usr/bin/env python3
"""Execute skill hit through primary credit, drain heal and result packet."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
c,digest=load_game();base=0x30000000;c.mem_map(base,0x80000)
a,m,s,stack,stop=base,base+0x10000,base+0x20000,base+0x60000,base+0x7f000
state={}
def put(p,*v):c.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
def get(p):return struct.unpack('<I',c.mem_read(p,4))[0]
def ret(cpu,n):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+n)
def hook(cpu,at,size,data):
 sp=cpu.reg_read(UC_X86_REG_ESP)
 if at==0x44d010:cpu.reg_write(UC_X86_REG_EAX,1);ret(cpu,4)
 elif at==0x4a16c0:ret(cpu,4)
 elif at==0x44d540:
  p=cpu.reg_read(UC_X86_REG_ESI);put(p,*state['initial']);cpu.reg_write(UC_X86_REG_EAX,state['critical']);ret(cpu,8)
 elif at==0x45b260:ret(cpu,0)
 elif at==0x490ee0:
  p=cpu.reg_read(UC_X86_REG_EAX)
  if cpu.reg_read(UC_X86_REG_ESP)==0 or get(sp+4)!=m:raise RuntimeError('damage ABI')
  if struct.unpack('<III',c.mem_read(p,12))!=state['initial']:raise RuntimeError('primary values')
  put(p,*state['changed']);state['events'].append('D');cpu.reg_write(UC_X86_REG_EAX,state['killed']);ret(cpu,24)
 elif at==0x489ed0:state['events'].append('S');ret(cpu,0)
 elif at==0x4910f0:state['events'].append('X');ret(cpu,8)
 elif at==0x482360:state['events'].append('H');ret(cpu,0)
 elif at==0x421220:
  n=get(sp+12);p=get(sp+8);state['events'].append('P'+bytes(c.mem_read(p,n)).hex());ret(cpu,32)
 elif at in (0x45017e,0x450437,0x45053f):state['events'].append('B');c.emu_stop()
c.hook_add(UC_HOOK_CODE,hook)
print(f'# ps_game.exe SHA256 {digest}; CUser::AttackMob 0044fd90 primary hit to next boundary; damage/death/send ports intercepted; real Heal')
print('# first detail critical killed zone initial0 initial1 initial2 changed0 changed1 changed2 events hp sp mp')
for first in (0,1):
 for detail in (21,27):
  for critical in (0,1):
   for killed in (0,1):
    for zone in (0,1):
     for variant in range(4):
      initial=(100+variant,200+variant,300+variant);changed=(400+variant,500+variant,600+variant)
      c.mem_write(a,bytes(0x6000));c.mem_write(m,bytes(0x6000));c.mem_write(s,bytes(160));put(a+0xdc,42);put(a+0xe0,base+0x30000 if zone else 0);put(m+0x5c,77)
      put(a+0x1238,10,30,20);put(a+0x17c,1000,1000,1000)
      c.mem_write(s,struct.pack('<HB',11,3));c.mem_write(s+0x30,b'\x01');c.mem_write(s+0x32,struct.pack('<H',detail))
      state.update(events=[],initial=initial,changed=changed,critical=critical,killed=killed)
      put(stack,stop,m,s,first,1);c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_ECX,a);c.emu_start(0x44fd90,stop,count=100000)
      print(first,detail,critical,killed,zone,*initial,*changed,','.join(state['events']),get(a+0x1238),get(a+0x1240),get(a+0x123c),sep='\t')
