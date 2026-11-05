#!/usr/bin/env python3
"""Execute skill hit through AttackDamage and critical wear; stop at credit."""
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
 if at==0x44d010:state['events'].append('C');cpu.reg_write(UC_X86_REG_EAX,1);ret(cpu,4)
 elif at==0x4a16c0:state['events'].append('S');ret(cpu,4)
 elif at==0x44d540:
  if cpu.reg_read(UC_X86_REG_ECX)!=a or cpu.reg_read(UC_X86_REG_EAX)!=m or get(sp+4)!=state['kind'] or get(sp+8)!=s:raise RuntimeError('damage ABI')
  state['ptr']=cpu.reg_read(UC_X86_REG_ESI);put(state['ptr'],*state['values']);state['events'].append('D');cpu.reg_write(UC_X86_REG_EAX,state['critical']);ret(cpu,8)
 elif at==0x45b260:
  if cpu.reg_read(UC_X86_REG_EAX)!=a:raise RuntimeError('wear ABI')
  state['events'].append('W');put(state['ptr'],*state['changed']);ret(cpu,0)
 elif at in (0x44ff71,0x450359):
  state['events'].append('B');c.emu_stop()
c.hook_add(UC_HOOK_CODE,hook)
print(f'# ps_game.exe SHA256 {digest}; CUser::AttackMob 0044fd90 successful prefix to credit boundary; calculations intercepted')
print('# first critical kind detail damage0 damage1 damage2 changed0 changed1 changed2 events out0 out1 out2')
for first in (0,1):
 for critical in (0,1):
  for kind in (0,1,2):
   for detail in (21,27):
    for variant in range(4):
     values=(100+variant,200+variant,300+variant);changed=(400+variant,500+variant,600+variant)
     c.mem_write(a,bytes(0x6000));c.mem_write(m,bytes(0x6000));c.mem_write(s,bytes(160));put(a+0xdc,42);put(m+0x5c,77)
     c.mem_write(s,struct.pack('<HB',11,3));c.mem_write(s+0x30,bytes([kind]));c.mem_write(s+0x32,struct.pack('<H',detail))
     state.update(events=[],critical=critical,kind=kind,values=values,changed=changed,ptr=0)
     put(stack,stop,m,s,first,1);c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_ECX,a);c.emu_start(0x44fd90,stop,count=100000)
     out=struct.unpack('<III',c.mem_read(state['ptr'],12))
     print(first,critical,kind,detail,*values,*changed,','.join(state['events']),*out,sep='\t')
