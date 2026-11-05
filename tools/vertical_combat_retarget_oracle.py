#!/usr/bin/env python3
"""Original ordinary/stacked aggro retarget with clock, ledger and lookup ports intercepted."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
c,digest=load_game();base=0x30000000;c.mem_map(base,0x80000)
mob,user,info,zone,stack,stop=base,base+0x10000,base+0x20000,base+0x30000,base+0x60000,base+0x7f000
state={}
def put(p,*v):c.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
def get(p):return struct.unpack('<I',c.mem_read(p,4))[0]
def fl(p,v):c.mem_write(p,struct.pack('<f',v))
def ret(cpu,n=0):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+n)
def hook(cpu,at,size,data):
 if at==stop+0x100:state['events']+='T';cpu.reg_write(UC_X86_REG_EAX,1000);ret(cpu)
 elif at==0x4b9000:
  state['events']+='A';out=cpu.reg_read(UC_X86_REG_EBX);c.mem_write(out+8,bytes([state['kind']]));put(out+12,43);cpu.reg_write(UC_X86_REG_EAX,state['present']);ret(cpu)
 elif at==0x415f30:
  state['events']+='F';cpu.reg_write(UC_X86_REG_EAX,user if state['found'] else 0);ret(cpu,4)
 elif at==0x4b74f0:
  state['events']+='X';put(mob+0x288,700);cpu.reg_write(UC_X86_REG_EAX,state['exid']);ret(cpu)
c.hook_add(UC_HOOK_CODE,hook);put(0x5210e4,stop+0x100);c.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x1000)+b'\xc3')
print('# ps_game.exe SHA256',digest,'CMob::ReTargetByAggro 00490ab0 and ReTargetByAggroEx 004b7490; clock, ledger, user lookup and GetMaxAggroEx intercepted')
print('# stacked next fixed scenario distance found kind present exid events status target_kind target_id next_move next_target count interval')
scenarios=[(0,1,1,1,0,0,1,1),(1,1,1,1,0,0,1,1),(0,0,1,1,0,0,1,1),(0,1,0,1,0,0,1,1),(0,1,1,0,0,0,1,1),(0,1,1,1,1,0,1,1),(0,1,1,1,0,1,1,1),(0,1,1,1,0,0,2,1),(0,1,1,1,0,0,1,0),(0,1,1,1,0,0,1,1)]
for stacked in (0,1):
 for next_tick in (999,1000,1001,0xffffffff):
  for fixed in (0,1):
   for scenario,(same,attackable,visible,found,dead,immune,kind,present) in enumerate(scenarios):
    for distance in (44.0,45.0,46.0):
     c.mem_write(mob,bytes(0x1000));c.mem_write(user,bytes(0x6000));c.mem_write(info,bytes(0x100));put(mob+0xa8,info);put(mob+0x60,zone);put(mob+0x124,2 if scenario==9 else 0);put(mob+0x128,7);put(mob+0x12c,44);put(mob+0x2b8,fixed);put(mob+0x288,300);put(mob+0x28c,9);put(mob+0xcbc,77);put(mob+0xcc4,next_tick);put(info+0x78,1);c.mem_write(user+0x12d,bytes([1 if same else 2]));c.mem_write(user+0x5775,bytes([attackable]));c.mem_write(user+0x5774,bytes([visible]));c.mem_write(user+0x1366,bytes([immune]));put(user+0x142c,dead);put(user+0xdc,43);fl(user+0xd0,distance)
     state.update(events='',kind=kind,present=present,found=found,exid=43 if present else 0);put(stack,stop);c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_EDI,mob);c.reg_write(UC_X86_REG_EAX,mob)
     c.reg_write(UC_X86_REG_FPCW,0x27f);c.emu_start(0x4b7490 if stacked else 0x490ab0,stop,count=50000)
     if c.reg_read(UC_X86_REG_EIP)!=stop or c.reg_read(UC_X86_REG_ESP)!=stack+4:raise RuntimeError('original return')
     print(stacked,next_tick,fixed,scenario,distance,found,kind,present,state['exid'],state['events'] or '-',get(mob+0x124),get(mob+0x128),get(mob+0x12c),get(mob+0xcbc),get(mob+0xcc4),get(mob+0x28c),get(mob+0x288),sep='\t')
