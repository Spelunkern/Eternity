#!/usr/bin/env python3
"""Trace mob Attack stand, hit/miss routing and real deadline."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_game
cpu,digest=load_game();stack,mob,user,definition,zone,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x10000000
for at in (stack,mob,user,definition,zone,stop):cpu.mem_map(at,0x10000)
def put(a,*v):cpu.mem_write(a,struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v)))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
def ret(cleanup=0,result=None):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+cleanup)
 if result is not None:cpu.reg_write(UC_X86_REG_EAX,result)
state={}
def hook(machine,at,size,ctx):
 sp=machine.reg_read(UC_X86_REG_ESP)
 if at==0x48a100:
  state['trace']+='S'
  if state['mutate']:put(mob+0x130,2);put(definition+0x84,1000);put(mob+0x240,6)
  ret()
 elif at==0x4a23f0:state['trace']+='R';state['type']=get(sp+4);ret(4,state['hit'])
 elif at==0x421220:
  state['trace']+='M';assert get(sp+4)==zone and get(sp+12)==17 and get(sp+20)==0x42700000 and get(sp+24)==0 and get(sp+28)==42 and get(sp+32)==6
  state['packet']=bytes(machine.mem_read(get(sp+8),17)).hex();ret(32)
 elif at==0x4a20dc:state['trace']+='H';machine.reg_write(UC_X86_REG_EIP,0x4a235f)
for at in (0x48a100,0x4a23f0,0x421220,0x4a20dc):cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
cpu.mem_write(0x505316,b'\xc3')
print('# ps_game.exe SHA256 '+digest+'; CMob::Attack 004a2000; stand/hit test/view/hit continuation intercepted, real deadline')
print('# sleep disabled slot hit zone mutate | trace type packet next')
for sleep,disabled,slot,hit,has_zone,mutate in ((0,0,0,0,1,0),(0,0,1,1,1,0),(0,0,1,0,0,0),(1,0,0,0,1,0),(0,1,0,0,1,0),(0,0,0,0,1,1),(0,0,1,1,1,1)):
 for at in (mob,user,definition,zone):cpu.mem_write(at,bytes(0x10000))
 put(mob+0xa8,definition);put(mob+0x60,zone if has_zone else 0);put(mob+0x5c,123);put(mob+0x130,slot);put(mob+0x134,disabled);put(mob+0x24c,sleep);put(mob+0x240,3);put(mob+0x26c,0x3f800000);put(user+0xdc,42);put(definition+0x84,2000)
 for i in range(3):cpu.mem_write(definition+0x80+20*i,struct.pack('<H',11+i))
 state.update(trace='',type=0xffffffff,packet='-',hit=hit,mutate=mutate);put(stack+0x8000,stop,100);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ECX,mob);cpu.reg_write(UC_X86_REG_EDX,user);cpu.reg_write(UC_X86_REG_FPCW,0x27f);cpu.emu_start(0x4a2000,stop,count=100000)
 if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Mob Attack target routing did not return')
 print(sleep,disabled,slot,hit,has_zone,mutate,state['trace'] or '-',state['type'],state['packet'],get(mob+0xcb4),sep='\t')
