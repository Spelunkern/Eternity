#!/usr/bin/env python3
"""Original CGuild::PvPLeaveGuild 00438160 with world/send/cleanup ports."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,guild,opponent,stop=(0x20000000,0x30000000,0x31000000,0x32000000,0x10000000)
for p in (stack,user,guild,opponent,stop):cpu.mem_map(p,0x10000)
def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
def get(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
for ptr,target in ((0x5210cc,stop+0x100),(0x5210d0,stop+0x200)):put(ptr,target);cpu.mem_write(target,b'\xc2\x04\x00')
for p,n in ((0x4328c0,4),(0x4383c0,0),(0x438320,0),(0x438070,4),(0x47c610,4),(0x4b8aa0,0)):cpu.mem_write(p,b'\xc2'+struct.pack('<H',n))
events=[];other=False
def hook(m,p,size,data):
 sp=m.reg_read(UC_X86_REG_ESP)
 if p==0x4328c0:events.append(('find',));m.reg_write(UC_X86_REG_EAX,opponent if other else 0)
 elif p in (0x4383c0,0x438320):
  length=m.reg_read(UC_X86_REG_EAX);address=m.reg_read(UC_X86_REG_ECX);events.append(('all' if p==0x4383c0 else 'members',m.reg_read(UC_X86_REG_EDX),bytes(m.mem_read(address,length))))
 elif p==0x438070:events.append(('reset_all',struct.unpack('<I',m.mem_read(sp+4,4))[0]))
 elif p==0x47c610:events.append(('clear',struct.unpack('<I',m.mem_read(sp+4,4))[0]))
 elif p==0x4b8aa0:events.append(('damage',m.reg_read(UC_X86_REG_ESI)-0x1538))
 elif p in (stop+0x100,stop+0x200):events.append(('lock' if p==stop+0x100 else 'unlock',struct.unpack('<I',m.mem_read(sp+4,4))[0]))
cpu.hook_add(UC_HOOK_CODE,hook)
for count in (0,1,2,3):
 for leader in (False,True):
  for mode in (1,2):
   for other in (False,True):
    for enemy_id in (0,99):
     cpu.mem_write(user,bytes(0x7000));cpu.mem_write(guild,bytes(0x1000));cpu.mem_write(opponent,bytes(0x1000));events.clear()
     put(user+0xdc,42);put(user+0x54e4,123);put(user+0x54f4,8);put(user+0x54fc,9);put(user+0x1528,4)
     put(guild+0x4b4,3,11,enemy_id,42 if leader else 43);put(guild+0x4d4,count);put(opponent+0x4b4,4,22,33,44)
     put(stack+0x8000,stop,user,mode);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EDI,guild);cpu.emu_start(0x438160,stop,count=10000)
     assert cpu.reg_read(UC_X86_REG_EIP)==stop
     actual_other=other and enemy_id!=0;terminal=(count-1<=0) or (leader and mode==1)
     leave=bytes((6,0x25,mode,42,0,0,0));expected=([('find',)] if enemy_id else [])
     if actual_other:expected += [('lock',opponent+0x578),('all',opponent,leave),('unlock',opponent+0x578)]
     expected += [('all',guild,leave)]
     if terminal:
      if actual_other:expected += [('lock',opponent+0x578),('members',opponent,b'\x05\x25\x01'),('reset_all',opponent),('unlock',opponent+0x578)]
      expected += [('members',guild,b'\x05\x25\x02'),('reset_all',guild)]
     else:expected += [('clear',user),('damage',user)]
     assert events==expected,(count,leader,mode,other,enemy_id,events,expected)
     assert get(guild+0x4d4)==(count-1)&0xffffffff
     assert get(guild+0x4b4)==(0 if terminal else 3)
     if not terminal:assert get(user+0x54e4)==0 and get(user+0x54f4)==0xffffffff and get(user+0x54fc)==0 and get(user+0x1528)==0
print('64 original guild duel leave cases passed; ps_game.exe SHA256',digest)
