#!/usr/bin/env python3
"""Execute original EnableAttackMob and IsBothmob with retained lookup intercepted."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
c,digest=load_game();base=0x30000000;c.mem_map(base,0x80000)
user,mob,info,respawn,zone,stack,stop=[base+x for x in (0,0x10000,0x20000,0x30000,0x40000,0x60000,0x7f000)]
state={}
def put(p,*v):c.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
def get(p):return struct.unpack('<I',c.mem_read(p,4))[0]
def ret(cpu,n=0):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+n)
def hook(cpu,at,size,data):
 if at==0x416800:
  state['events']+='F';cpu.reg_write(UC_X86_REG_EAX,mob if state['found'] else 0);ret(cpu,4)
c.hook_add(UC_HOOK_CODE,hook)
print('# ps_game.exe SHA256',digest,'CUser::EnableAttackMob 0044cd80 and CMob::IsBothmob 0044c610; zone FindMob intercepted')
print('# zoned found status user_country mob_country lua respawn accepted both events')
for zoned in (0,1):
 for found in (0,1):
  for status in (0,1,2):
   for user_country in (0,1,2):
    for mob_country in (0,1,2):
     for lua in (0,1):
      for spawn in (0,1,2):
       c.mem_write(user,bytes(0x2000));c.mem_write(mob,bytes(0x1000));c.mem_write(info,bytes(0x100));c.mem_write(respawn,bytes(0x100));put(user+0xe0,zone if zoned else 0);c.mem_write(user+0x12d,bytes([user_country]));put(mob+0xa8,info);put(mob+0x124,status);put(mob+0x284,lua);put(mob+0xc84,respawn if spawn else 0);c.mem_write(respawn+0x1c,bytes([spawn==2]));put(info+0x78,mob_country);state.update(found=found,events='')
       put(stack,stop);c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_EDI,user);c.reg_write(UC_X86_REG_ECX,77);c.emu_start(0x44cd80,stop,count=10000)
       accepted=int(c.reg_read(UC_X86_REG_EAX)==mob)
       if c.reg_read(UC_X86_REG_EIP)!=stop or c.reg_read(UC_X86_REG_ESP)!=stack+4:raise RuntimeError('attack target ABI')
       put(stack,stop);c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_EAX,mob);c.emu_start(0x44c610,stop,count=10000)
       both=int((c.reg_read(UC_X86_REG_EAX)&255)!=0)
       if c.reg_read(UC_X86_REG_EIP)!=stop or c.reg_read(UC_X86_REG_ESP)!=stack+4:raise RuntimeError('both-mob ABI')
       print(zoned,found,status,user_country,mob_country,lua,spawn,accepted,both,state['events'] or '-',sep='\t')
