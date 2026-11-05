#!/usr/bin/env python3
"""Execute CMob::StatusAttackStop with vision/target/FSM boundaries observed."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
from original_oracle import load_game
c,digest=load_game();base=0x30000000;c.mem_map(base,0x40000);mob,stack,stop=base,base+0x20000,base+0x3f000
state={}
def put(p,*v):c.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
def get(p):return struct.unpack('<I',c.mem_read(p,4))[0]
def hook(cpu,at,size,data):
 if at not in (0x4a1580,0x49fd20,0x492310):return
 sp=cpu.reg_read(UC_X86_REG_ESP)
 if at==0x4a1580:
  if get(sp+4)!=mob:raise RuntimeError('vision ABI')
  state['events']+='V';put(mob+0x124,state['after']);pop=4
 elif at==0x49fd20:
  state['events']+='F';cpu.reg_write(UC_X86_REG_EAX,state['found']);pop=0
 else:state['events']+='R';pop=0
 cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+pop)
c.hook_add(UC_HOOK_CODE,hook)
print(f'# ps_game.exe SHA256 {digest}; CMob::StatusAttackStop 00490980, vision/find/reset intercepted, actual GetStatus')
print('# initial after_vision pais found status target_kind target_id events')
for initial in (0,1,2):
 for after in (0,2):
  for pais in (0,1):
   for found in (0,1):
    c.mem_write(mob,bytes(0x1000));put(mob+0x124,initial,1,42);c.mem_write(mob+0x250,bytes([pais]));state.update(events='',after=after,found=found)
    put(stack,stop);c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_EAX,mob);c.emu_start(0x490980,stop,count=10000)
    if c.reg_read(UC_X86_REG_EIP)!=stop or c.reg_read(UC_X86_REG_ESP)!=stack+4:raise RuntimeError('unbalanced original')
    print(initial,after,pais,found,get(mob+0x124),get(mob+0x128),get(mob+0x12c),state['events'] or '-',sep='\t')
