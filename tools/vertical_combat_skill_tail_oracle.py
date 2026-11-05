#!/usr/bin/env python3
"""Execute original skill hit secondary damage and target/effect tail."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
c,digest=load_game();base=0x30000000;c.mem_map(base,0x80000)
a,m,s,stack,stop=base,base+0x10000,base+0x20000,base+0x60000,base+0x7f000
state={}
def put(p,*v):c.mem_write(p,struct.pack('<'+'I'*len(v),*[v&0xffffffff for v in v]))
def get(p):return struct.unpack('<I',c.mem_read(p,4))[0]
def ret(cpu,n):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+n)
def hook(cpu,at,size,data):
 sp=cpu.reg_read(UC_X86_REG_ESP)
 if at==0x44d010:cpu.reg_write(UC_X86_REG_EAX,1);ret(cpu,4)
 elif at==0x4a16c0:ret(cpu,4)
 elif at==0x44d540:put(cpu.reg_read(UC_X86_REG_ESI),100,200,300);cpu.reg_write(UC_X86_REG_EAX,0);ret(cpu,8)
 elif at==0x490ee0:
  state['hits']+=1;p=cpu.reg_read(UC_X86_REG_EAX)
  if state['hits']==1:cpu.reg_write(UC_X86_REG_EAX,state['primary_killed'])
  else:
   state['events'].append('D'+','.join(str(x) for x in struct.unpack('<III',c.mem_read(p,12))))
   put(p,99,88,77);cpu.reg_write(UC_X86_REG_EAX,state['secondary_killed'])
  ret(cpu,24)
 elif at==0x489ed0:
  if state['started']:state['events'].append('S')
  ret(cpu,0)
 elif at==0x4910f0:
  if state['started']:state['events'].append('X')
  ret(cpu,8)
 elif at==0x4505a0:ret(cpu,0)
 elif at==0x421220:
  if state['started']:
   p,n=get(sp+8),get(sp+12);state['events'].append('P'+bytes(c.mem_read(p,n)).hex())
  ret(cpu,32)
 elif at==0x4bd8d0:
  if cpu.reg_read(UC_X86_REG_EAX)!=a or cpu.reg_read(UC_X86_REG_EDX)!=5:raise RuntimeError('combat send ABI')
  p,n=get(sp+4),get(sp+8);state['events'].append('C'+bytes(c.mem_read(p,n)).hex());ret(cpu,12)
 elif at==0x49fad0:
  state['events'].append('T');c.mem_write(s+0x31,bytes([state['target_effect']]));ret(cpu,8)
 elif at==0x4a0aa0:
  if cpu.reg_read(UC_X86_REG_EAX)!=s or [get(sp+i) for i in (4,8,12)]!=[m,1,42]:raise RuntimeError('effect ABI')
  state['events'].append('E');ret(cpu,12)
 elif at==0x44c720:state['events'].append('I');ret(cpu,4)
 elif at in (0x45017e,0x450437) or (at==0x45053f and not state['started']):
  if state['started']:raise RuntimeError('duplicate start')
  state['started']=True;state['events'].append('B')
c.hook_add(UC_HOOK_CODE,hook)
print(f'# ps_game.exe SHA256 {digest}; CUser::AttackMob 0044fd90 secondary and target/effect tail; services intercepted')
print('# first drain primary_killed secondary_killed extra zone second target_effect events flags')
for first in (0,1):
 for drain in (0,1):
  for primary in (0,1):
   for secondary in (0,1):
    for extra in (0,1):
     for zone in (0,1):
      for second in (0,1):
       for effect in (1,3):
        c.mem_write(a,bytes(0x6000));c.mem_write(m,bytes(0x6000));c.mem_write(s,bytes(160));put(a+0xdc,42);put(a+0xe0,base+0x30000 if zone else 0);put(m+0x5c,77)
        c.mem_write(s,struct.pack('<HB',11,3));c.mem_write(s+0x30,b'\x01');c.mem_write(s+0x31,b'\x01');c.mem_write(s+0x32,struct.pack('<H',27 if drain else 21));c.mem_write(s+0x76,struct.pack('<HHH',*( (11,22,33) if extra else (0,0,0) )))
        state.update(events=[],hits=0,started=False,primary_killed=primary,secondary_killed=secondary,target_effect=effect)
        put(stack,stop,m,s,first,second);c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_ECX,a);c.emu_start(0x44fd90,stop,count=100000)
        if c.reg_read(UC_X86_REG_EIP)!=stop or c.reg_read(UC_X86_REG_ESP)!=stack+20 or not state['started']:raise RuntimeError('original did not return')
        print(first,drain,primary,secondary,extra,zone,second,effect,';'.join(state['events']),bytes(c.mem_read(m+0x251,3)).hex(),sep='\t')
