#!/usr/bin/env python3
"""Execute CMob::GetCurrentPos; intercept GetTickCount and GetStatus."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
c,digest=load_game();base=0x30000000;c.mem_map(base,0x80000)
mob,info,out,stack,stop=base,base+0x10000,base+0x20000,base+0x60000,base+0x7f000
state={}
def put(p,*v):c.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
def get(p):return struct.unpack('<I',c.mem_read(p,4))[0]
def fs(p,*v):c.mem_write(p,struct.pack('<'+'f'*len(v),*v))
def ret(cpu):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
def hook(cpu,at,size,data):
 if at==stop+0x100:state['events']+='T';cpu.reg_write(UC_X86_REG_EAX,state['now']);ret(cpu)
 elif at==0x41d760:state['events']+='S';cpu.reg_write(UC_X86_REG_EAX,state['status']);ret(cpu)
c.hook_add(UC_HOOK_CODE,hook);put(0x5210e4,stop+0x100)
print('# ps_game.exe SHA256',digest,'CMob::GetCurrentPos 004a6b90; clock and GetStatus intercepted')
print('# secondary status start now walk run range x y z events result_x result_y result_z')
for secondary in (0,1):
 for status in (0,3):
  for start in (0,100,0xfffffffe):
   for now in (0,1,100,101,109,110,111,120,0xffffffff):
    for move_range in (0.0,1.5,11.0):
     walk,run=10,20;c.mem_write(mob,bytes(0x1000));c.mem_write(info,bytes(0x100));put(mob+0xa8,info);put(mob+0x128,secondary);put(info+0x5c,walk);put(info+0x64,run)
     fs(mob+0x50,100,200,300);fs(mob+0xcd4,1,2,3);fs(mob+0xce0,0.5,-0.25,2.0);put(mob+0xcec,start);fs(mob+0xcf0,move_range)
     state.update(now=now,status=status,events='');put(stack,stop);c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_ESI,mob);c.reg_write(UC_X86_REG_EDI,out);c.reg_write(UC_X86_REG_FPCW,0x27f);c.emu_start(0x4a6b90,stop,count=10000)
     if c.reg_read(UC_X86_REG_EIP)!=stop or c.reg_read(UC_X86_REG_ESP)!=stack+4:raise RuntimeError('unbalanced original')
     xyz=struct.unpack('<III',c.mem_read(out,12));print(secondary,status,start,now,walk,run,move_range,100,200,300,state['events'],*xyz,sep='\t')
