#!/usr/bin/env python3
"""Original GetMaxAggroEx with real list traversal, intercepted locks and user-map lookup."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
c,digest=load_game();base=0x30000000;c.mem_map(base,0xa0000)
mob,definition,zone,head,n1,n2,u1,u2,m1,m2,missing,stack,stop=[base+x for x in (0,0x10000,0x20000,0x30000,0x31000,0x32000,0x40000,0x50000,0x60000,0x61000,0x62000,0x80000,0x9f000)]
state={}
def put(p,*v):c.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
def get(p):return struct.unpack('<I',c.mem_read(p,4))[0]
def fl(p,v):c.mem_write(p,struct.pack('<f',v))
def ret(cpu,n=0):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+n)
def hook(cpu,at,size,data):
 if at in (stop+0x100,stop+0x200):ret(cpu,4)
 elif at==0x410320:
  ident=get(cpu.reg_read(UC_X86_REG_EBX));out=cpu.reg_read(UC_X86_REG_EAX);node={43:m1,44:m2}.get(ident,missing)
  if ident==44 and state['missing']:node=missing
  put(out,node);cpu.reg_write(UC_X86_REG_EAX,out);ret(cpu)
c.hook_add(UC_HOOK_CODE,hook);put(0x5210cc,stop+0x100);put(0x5210d0,stop+0x200);c.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x1000)+b'\xc3')
print('# ps_game.exe SHA256',digest,'CMob::GetMaxAggroEx 004b74f0; real list traversal, user lookup and locks intercepted')
print('# kind1 kind2 aggro1 aggro2 scenario distance result')
for kind1,kind2 in ((1,1),(2,1)):
 for aggro1 in (-5,0,1,5):
  for aggro2 in (-5,0,1,5):
   for scenario in range(7):
    for distance in (44.0,45.0,46.0):
     c.mem_write(mob,bytes(0x1000));c.mem_write(definition,bytes(0x100));c.mem_write(zone,bytes(0x1000));c.mem_write(head,bytes(0x100));c.mem_write(n1,bytes(0x100));c.mem_write(n2,bytes(0x100));c.mem_write(u1,bytes(0x6000));c.mem_write(u2,bytes(0x6000));
     put(mob+0xa8,definition);put(mob+0x60,zone);put(mob+0x1f4,head);put(head+4,n1);put(n1+4,n2);put(n2+4,head);c.mem_write(n1+8,bytes([kind1]));c.mem_write(n2+8,bytes([kind2]));put(n1+0xc,43);put(n2+0xc,44);put(n1+0x14,aggro1);put(n2+0x14,aggro2);put(definition+0x78,1);put(zone+0x7c,missing);put(m1+0x10,u1);put(m2+0x10,u2)
     for u in (u1,u2):c.mem_write(u+0x12d,b'\x02');c.mem_write(u+0x5774,b'\x01\x01')
     fl(u1+0xd0,44);fl(u2+0xd0,distance)
     if scenario==1:c.mem_write(u2+0x12d,b'\x01')
     if scenario==2:put(u2+0x142c,1)
     if scenario==3:c.mem_write(u2+0x1366,b'\x01')
     if scenario==4:c.mem_write(u2+0x5774,b'\x00')
     if scenario==6:c.mem_write(u2+0x5775,b'\x00')
     state['missing']=scenario==5;put(stack,stop);c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_EDI,mob);c.reg_write(UC_X86_REG_FPCW,0x27f)
     c.emu_start(0x4b74f0,stop,count=100000)
     if c.reg_read(UC_X86_REG_EIP)!=stop or c.reg_read(UC_X86_REG_ESP)!=stack+4:raise RuntimeError('original return')
     print(kind1,kind2,aggro1,aggro2,scenario,distance,c.reg_read(UC_X86_REG_EAX),sep='\t')
