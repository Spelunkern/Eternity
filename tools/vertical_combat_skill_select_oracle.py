#!/usr/bin/env python3
"""Execute selected mob skill coordinator with observed external service boundaries."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game

c,digest=load_game();base=0x30000000;c.mem_map(base,0x80000)
source,target,info,stack,stop=base,base+0x10000,base+0x20000,base+0x60000,base+0x7f000
state={}
def put(p,*v):c.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
def get(p):return struct.unpack('<I',c.mem_read(p,4))[0]
def ret(cpu,n):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+n)
def hook(cpu,at,size,data):
 sp=cpu.reg_read(UC_X86_REG_ESP)
 if at==0x44e900:
  if cpu.reg_read(UC_X86_REG_ECX)!=info or cpu.reg_read(UC_X86_REG_EDX)&255!=4 or [get(sp+i) for i in (4,8,12)]!=[source,77,1]:raise RuntimeError('announce ABI')
  state['trace'].append('A');c.mem_write(info+0x58,bytes([state['announced']]));ret(cpu,12)
 elif at==0x44fd90:
  if cpu.reg_read(UC_X86_REG_ECX)!=source or get(sp+4)!=target or get(sp+8)!=info:raise RuntimeError('hit ABI')
  state['trace'].append('H'+str(get(sp+12))+str(get(sp+16)));state['hits']+=1
  if state['hits']==1:
   c.mem_write(info+0x58,bytes([state['changed']]));put(source+0x142c,state['dead']);put(source+0x1238,0x80000011)
  ret(cpu,16)
 elif at==0x41d760:
  if cpu.reg_read(UC_X86_REG_EAX)!=target:raise RuntimeError('status ABI')
  state['trace'].append('T');cpu.reg_write(UC_X86_REG_EAX,state['status']);ret(cpu,0)
 elif at==0x44c120:
  if cpu.reg_read(UC_X86_REG_ECX)!=source or cpu.reg_read(UC_X86_REG_EAX)!=1 or cpu.reg_read(UC_X86_REG_EBX)!=42:raise RuntimeError('self damage ABI')
  state['trace'].append('D'+str(get(sp+4)));cpu.reg_write(UC_X86_REG_EAX,state['killed']);ret(cpu,4)
 elif at==0x490980:
  if cpu.reg_read(UC_X86_REG_EAX)!=target:raise RuntimeError('stop ABI')
  state['trace'].append('S');ret(cpu,0)
 elif at==0x4586a0:
  if cpu.reg_read(UC_X86_REG_ECX)!=source or [get(sp+i) for i in (4,8,12)]!=[1,42,0]:raise RuntimeError('death ABI')
  state['trace'].append('F');ret(cpu,12)
c.hook_add(UC_HOOK_CODE,hook)
print(f'# ps_game.exe SHA256 {digest}; CUser::SkillAttackSelect(CMob*) 00450e20; external calls intercepted')
print('# detail initial announced changed source_dead mob_status killed trace')
for detail in (20,21,22,23,24,25,26,27,28,65535):
 for variant in range(128):
  initial=variant%4;announced=(variant//2)%4;changed=(variant//4)%4;dead=(variant>>5)&1;status=2 if variant&64 else 1;killed=variant&1
  c.mem_write(source,bytes(0x6000));c.mem_write(target,bytes(0x6000));c.mem_write(info,bytes(160));put(source+0xdc,42);put(target+0x5c,77);put(source+0x1238,13)
  c.mem_write(info+0x32,struct.pack('<H',detail));c.mem_write(info+0x58,bytes([initial]))
  state.update(trace=[],hits=0,announced=announced,changed=changed,dead=dead,status=status,killed=killed)
  put(stack,stop,target,info);c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_ECX,source)
  c.emu_start(0x450e20,stop,count=100000)
  if c.reg_read(UC_X86_REG_EIP)!=stop or c.reg_read(UC_X86_REG_ESP)!=stack+12:raise RuntimeError('unbalanced original')
  print(detail,initial,announced,changed,dead,status,killed,','.join(state['trace']) or '-',sep='\t')
