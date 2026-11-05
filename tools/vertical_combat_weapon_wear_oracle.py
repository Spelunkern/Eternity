#!/usr/bin/env python3
"""Execute QualityByAttack and QualityDec, intercept modifier rebuilds."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
c,digest=load_game();base=0x30000000;c.mem_map(base,0x80000)
a,item,info,stack,stop=base,base+0x10000,base+0x20000,base+0x60000,base+0x7f000
state={}
def put(p,*v):c.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
def get(p):return struct.unpack('<I',c.mem_read(p,4))[0]
def ret(cpu,n):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+n)
def hook(cpu,at,size,data):
 sp=cpu.reg_read(UC_X86_REG_ESP)
 if at==0x45b350:
  if cpu.reg_read(UC_X86_REG_EAX)!=a or cpu.reg_read(UC_X86_REG_ESI)!=5 or get(sp+4)!=1:raise RuntimeError('quality ABI')
  state['events'].append('D')
 elif at==0x4566d0:state['events'].append('R');ret(cpu,4)
 elif at==0x456620:state['events'].append('A');ret(cpu,4)
 elif at==0x454280:state['events'].append('T');ret(cpu,0)
c.hook_add(UC_HOOK_CODE,hook)
print(f'# ps_game.exe SHA256 {digest}; CUser::QualityByAttack 0045b260 and QualityDec 0045b350; modifier/rebuild ports intercepted')
print('# protection shape present quality cached events final_quality final_cached')
for protection in (0,1):
 for shape in (0,1):
  for present in (0,1):
   for quality in (0,1,100,101,500,501,1000):
    for cached in (0,1,2):
     c.mem_write(a,bytes(0x6000));c.mem_write(item,bytes(0x100));c.mem_write(info,bytes(0x100));put(a+0x58b8,protection);c.mem_write(a+0x1354,bytes([shape]));put(a+0x1d8,item if present else 0);c.mem_write(a+0x1a2,bytes([cached]));put(item+0x30,info);c.mem_write(item+0x44,struct.pack('<H',quality));c.mem_write(info+0x4e,struct.pack('<HH',500,100))
     state['events']=[];put(stack,stop);c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_EAX,a);c.emu_start(0x45b260,stop,count=10000)
     if c.reg_read(UC_X86_REG_EIP)!=stop or c.reg_read(UC_X86_REG_ESP)!=stack+4:raise RuntimeError('unbalanced original')
     final=struct.unpack('<H',c.mem_read(item+0x44,2))[0];tier=c.mem_read(a+0x1a2,1)[0]
     print(protection,shape,present,quality,cached,','.join(state['events']) or '-',final,tier,sep='\t')
