#!/usr/bin/env python3
"""Original PvP movement guild branch with real captured-guild lock helper."""
import math,struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBP,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_game
cpu,digest=load_game();stack,user,guild,stop=(0x20000000,0x30000000,0x31000000,0x10000000)
for p in (stack,user,guild,stop):cpu.mem_map(p,0x10000)
def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
for ptr,target in ((0x5210cc,stop+0x100),(0x5210d0,stop+0x200)):put(ptr,target);cpu.mem_write(target,b'\xc2\x04\x00')
cpu.mem_write(0x5048a4,b'\xdd\x44\x24\x04\xd9\xfa\xc3');cpu.mem_write(0x438160,b'\xc2\x08\x00');cpu.reg_write(UC_X86_REG_FPCW,0x27f)
events=[];disappear=False
def hook(m,p,size,data):
 global disappear
 if p in (stop+0x100,stop+0x200):
  sp=m.reg_read(UC_X86_REG_ESP);addr=struct.unpack('<I',m.mem_read(sp+4,4))[0];events.append(('lock' if p==stop+0x100 else 'unlock',addr))
  if p==stop+0x100 and disappear:put(user+0x1758,0);disappear=False
 elif p==0x438160:
  sp=m.reg_read(UC_X86_REG_ESP);args=struct.unpack('<2I',m.mem_read(sp+4,8));events.append(('leave',m.reg_read(UC_X86_REG_EDI),*args))
cpu.hook_add(UC_HOOK_CODE,hook)
for enemy in (0,123):
 for mode in (-1,0):
  for distance in (0.,40.,40.1,float('nan')):
   for guild_status in (0,3,4,5):
    for variant in ('absent','present','disappear'):
     cpu.mem_write(user,bytes(0x7000));cpu.mem_write(guild,bytes(0x1000));events.clear();disappear=variant=='disappear'
     put(user+0x54e4,enemy);put(user+0x54f4,mode);put(user+0x1758,0 if variant=='absent' else guild);put(guild+0x4b4,guild_status)
     cpu.mem_write(user+0xd0,struct.pack('<3f',distance,0.,0.));cpu.mem_write(user+0x54e8,struct.pack('<3f',0.,0.,0.))
     cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EBP,user);cpu.emu_start(0x47c161,0x47c1dd,count=100000)
     assert cpu.reg_read(UC_X86_REG_EIP)==0x47c1dd
     triggered=enemy!=0 and mode==-1 and not math.isnan(distance) and distance>40
     expected=[]
     if triggered and variant!='absent':
      expected=[('lock',guild+0x578)]
      if variant=='disappear':expected += [('unlock',guild+0x578)]
      else:
       if guild_status in (3,4):expected += [('leave',guild,user,1)]
       expected += [('unlock',guild+0x578)]
     assert events==expected,(enemy,mode,distance,guild_status,variant,events,expected)
print('192 original guild-duel movement cases passed; ps_game.exe SHA256',digest)
