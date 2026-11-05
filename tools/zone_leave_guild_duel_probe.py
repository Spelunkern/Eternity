#!/usr/bin/env python3
"""CUser::PvPChkLeaveByZone guild branch 0047bafb..0047bcad."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBP,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP
from original_oracle import load_game
cpu,digest=load_game();stop,stack,user,peer,guild,opponent,zone=(0x10000000,0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000)
for p in (stop,stack,user,peer,guild,opponent,zone):cpu.mem_map(p,0x10000)
def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
def get(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
for ptr,target in ((0x5210cc,stop+0x100),(0x5210d0,stop+0x200)):put(ptr,target)
for p,n in ((stop+0x100,4),(stop+0x200,4),(0x415f30,4),(0x42b800,0),(0x47b230,0),(0x437eb0,0),(0x4d4d20,8),(0x438160,8)):cpu.mem_write(p,b'\xc2'+struct.pack('<H',n))
events=[];found=False
def hook(m,p,size,data):
 sp=m.reg_read(UC_X86_REG_ESP)
 if p in (stop+0x100,stop+0x200):events.append(('lock' if p==stop+0x100 else 'unlock',get(sp+4)))
 elif p==0x415f30:events.append(('find',get(sp+4)));m.reg_write(UC_X86_REG_EAX,peer if found else 0)
 elif p==0x42b800:events.append(('peer-lock',m.reg_read(UC_X86_REG_EDI)));put(m.reg_read(UC_X86_REG_ESI),0)
 elif p==0x47b230:events.append(('reset-user',m.reg_read(UC_X86_REG_EDI)))
 elif p==0x437eb0:events.append(('reset-guild',m.reg_read(UC_X86_REG_EAX)))
 elif p==0x4d4d20:events.append(('send',m.reg_read(UC_X86_REG_ECX),bytes(m.mem_read(get(sp+4),get(sp+8)))))
 elif p==0x438160:events.append(('leave',m.reg_read(UC_X86_REG_EDI),get(sp+4),get(sp+8)))
cpu.hook_add(UC_HOOK_CODE,hook)
count=0
for enemy in (0,123):
 for has_guild in (False,True):
  for found in (False,True):
   for status in (0,1,2,3,4,5):
    for leader in (42,77):
     cpu.mem_write(user,bytes(0x7000));cpu.mem_write(peer,bytes(0x7000));cpu.mem_write(guild,bytes(0x1000));events.clear();put(user+0xdc,42);put(peer+0xdc,99);put(user+0xe0,zone);put(user+0x54e4,enemy);put(user+0x1758,guild if has_guild else 0);put(peer+0x1758,opponent);put(guild+0x4b4,status,99,123,leader);put(stack+0x8018,0)
     cpu.reg_write(UC_X86_REG_EBP,user);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.emu_start(0x47bafb,0x47bcad,count=10000)
     expected=[]
     if enemy and has_guild:
      expected.extend((('lock',guild+0x578),('find',99)))
      if found and status in (1,2) and leader==42:
       expected.extend((('peer-lock',peer),('reset-user',user),('reset-user',peer),('reset-guild',guild),('reset-guild',opponent)))
       ident=99 if status==1 else 42;packet=b'\x02\x25\x03'+struct.pack('<I',ident);expected.extend((('send',peer,packet),('send',user,packet)))
      elif found and status in (3,4):expected.append(('leave',guild,user,1))
      expected.append(('unlock',guild+0x578))
     assert events==expected,(enemy,has_guild,found,status,leader,events,expected);count+=1
print(count,'original zone-leave guild duel cases passed; ps_game.exe SHA256',digest)
