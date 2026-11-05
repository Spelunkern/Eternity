#!/usr/bin/env python3
"""Trace AttackDamage's conditional SendLogBothMob call and arguments."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game

c, digest = load_game()
mob, user, respawn, damage, stack = (0x30000000,0x31000000,0x32000000,0x33000000,0x34000000)
for address in (mob,user,respawn,damage,stack): c.mem_map(address,0x10000)
def put(a,v): c.mem_write(a,struct.pack('<I',v & 0xffffffff))
def get(a): return struct.unpack('<I',c.mem_read(a,4))[0]
state={}
def hook(machine,address,size,context):
    sp=machine.reg_read(UC_X86_REG_ESP)
    state.update(calls=state['calls']+1, subject=int(machine.reg_read(UC_X86_REG_EDX)==mob),
                 user=int(machine.reg_read(UC_X86_REG_EDI)==user+0x188),
                 event=get(sp+4), detail=get(sp+8), amount=get(sp+12))
    machine.reg_write(UC_X86_REG_EIP,0x4a3054)
c.hook_add(UC_HOOK_CODE,hook,begin=0x4a1810,end=0x4a1810)
print('# ps_game.exe SHA256',digest,'CMob::AttackDamage 004a3026..004a3054; SendLogBothMob intercepted')
print('# lua respawn both damage | calls subject user event detail amount')
for lua in (0,1):
 for present in (0,1):
  for both in (0,1):
   put(mob+0x284,lua);put(mob+0xc84,respawn if present else 0);c.mem_write(respawn+0x1c,bytes([both]));put(damage,23456)
   c.reg_write(UC_X86_REG_ESI,mob);c.reg_write(UC_X86_REG_EBX,user);c.reg_write(UC_X86_REG_EBP,damage);c.reg_write(UC_X86_REG_ESP,stack+0x8000)
   state.update(calls=0,subject=0,user=0,event=0,detail=0,amount=0)
   c.emu_start(0x4a3026,0x4a3054,count=1000)
   print(lua,present,both,23456,*(state[key] for key in ('calls','subject','user','event','detail','amount')),sep='\t')
