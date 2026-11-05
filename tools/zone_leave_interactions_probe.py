#!/usr/bin/env python3
"""CUser::LeaveZone prefix 0044aee0..0044b04c; shop shutdown is ported."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_ESP
from original_oracle import load_game
cpu,digest=load_game();stop,stack,user,peer,npc,zone=(0x10000000,0x20000000,0x30000000,0x31000000,0x32000000,0x33000000)
for p in (stop,stack,user,peer,npc,zone):cpu.mem_map(p,0x10000)
def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
def get(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
for p,n in ((0x415f30,4),(0x478520,0),(0x4d4d20,8)):cpu.mem_write(p,b'\xc2'+struct.pack('<H',n))
events=[];found=False
def hook(m,p,size,data):
 if p==0x415f30:events.append('find');m.reg_write(UC_X86_REG_EAX,peer if found else 0)
 elif p==0x478520:events.append(('shop',get(user+0x1564),get(user+0x1738),get(user+0x16f4),get(user+0x16f8),get(npc+0x40)))
 elif p==0x4d4d20:
  sp=m.reg_read(UC_X86_REG_ESP);events.append(('send',bytes(m.mem_read(get(sp+4),get(sp+8)))))
cpu.hook_add(UC_HOOK_CODE,hook)
count=0
for has_npc in (False,True):
 for partner in (0,99):
  for found in (False,True):
   for mode in (0,3):
    cpu.mem_write(user,bytes(0x7000));cpu.mem_write(peer,bytes(0x7000));cpu.mem_write(npc,bytes(0x1000));events.clear()
    put(user+0xdc,42);put(user+0xe0,zone);put(user+0x1564,npc if has_npc else 0);put(npc+0x40,0);put(user+0x15dc,user);put(user+0x1738,partner);put(user+0x16f4,2,42,99);put(user+0x1568,mode,peer if mode else 0);put(peer+0x1568,3,user)
    cpu.reg_write(UC_X86_REG_ESI,user);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.emu_start(0x44aee0,0x44b04c,count=1000)
    expected=[]
    if partner:expected.append('find')
    removed=bool(partner and found)
    expected.append(('shop',0,0 if removed else partner,1 if removed else 2,99 if removed else 42,0xffffffff if has_npc else 0))
    if mode:expected.extend((('send',b'\x04\x0a\x02'),('send',b'\x04\x0a\x03')))
    assert events==expected,(has_npc,partner,found,mode,events,expected);count+=1
print(count,'original leave-zone interaction prefix cases passed; ps_game.exe SHA256',digest)
