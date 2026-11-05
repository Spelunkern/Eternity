#!/usr/bin/env python3
"""Execute AttackMob skill prefix up to AttackDamage; intercept its services."""
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
 if at==0x44d010:
  if cpu.reg_read(UC_X86_REG_ECX)!=m or cpu.reg_read(UC_X86_REG_EAX)!=s or get(sp+4)!=1:raise RuntimeError('success ABI')
  state['events'].append('C');cpu.reg_write(UC_X86_REG_EAX,state['success']);ret(cpu,4)
 elif at==0x4a16c0:
  if get(sp+4)!=m:raise RuntimeError('sleep ABI')
  state['events'].append('S');ret(cpu,4)
 elif at==0x44d540:
  state['events'].append('D');cpu.emu_stop()
 elif at==0x421220:
  if get(sp+4)!=base+0x30000 or get(sp+24)!=42 or get(sp+32)!=5:raise RuntimeError('send ABI')
  n=get(sp+12);p=get(sp+8);state['events'].append('P'+bytes(c.mem_read(p,n)).hex());ret(cpu,32)
 elif at==0x49fad0:
  if cpu.reg_read(UC_X86_REG_ECX)!=m or [get(sp+4),get(sp+8)]!=[a,1]:raise RuntimeError('target ABI')
  state['events'].append('T');put(m+0x254,state['after_target']);ret(cpu,8)
c.hook_add(UC_HOOK_CODE,hook)
print(f'# ps_game.exe SHA256 {digest}; CUser::AttackMob 0044fd90 prefix through AttackDamage boundary; external services intercepted')
print('# first success detail skill zone battle after_target speed_bits events flags result_speed_bits')
for first in (0,1):
 for success in (0,1):
  for detail in (21,27):
   for skill in (55,56):
    for zone in (0,1):
     for battle in (0,1):
      for after in (0,1):
       for speed in (0x41200000,0xc0b00000):
        c.mem_write(a,bytes(0x6000));c.mem_write(m,bytes(0x6000));c.mem_write(s,bytes(160))
        put(a+0xdc,42);put(a+0xe0,base+0x30000 if zone else 0);put(a+0x54b8,speed);put(m+0x5c,77);put(m+0x254,battle)
        c.mem_write(s,struct.pack('<HB',skill,3));c.mem_write(s+0x30,b'\x01');c.mem_write(s+0x32,struct.pack('<H',detail));c.mem_write(s+0x4c,b'\x07')
        state.update(events=[],success=success,after_target=after)
        put(stack,stop,m,s,first,1);c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_ECX,a)
        c.emu_start(0x44fd90,stop,count=100000)
        if not success and (c.reg_read(UC_X86_REG_EIP)!=stop or c.reg_read(UC_X86_REG_ESP)!=stack+20):raise RuntimeError('unbalanced original')
        flags=bytes(c.mem_read(m+0x251,3)).hex()
        print(first,success,detail,skill,zone,battle,after,speed,','.join(state['events']),flags,get(a+0x54b8),sep='\t')
