#!/usr/bin/env python3
"""CUser leave-zone trade branch 0044af3c..0044b04c."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_ESI
from original_oracle import load_game
cpu,digest=load_game();stop,stack,user,peer=(0x10000000,0x20000000,0x30000000,0x31000000)
for p in (stop,stack,user,peer):cpu.mem_map(p,0x10000)
def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
def get(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
cpu.mem_write(0x4d4d20,b'\xc2\x08\x00');events=[]
def hook(m,p,size,data):
 if p==0x4d4d20:
  sp=m.reg_read(UC_X86_REG_ESP);events.append((m.reg_read(UC_X86_REG_ECX),bytes(m.mem_read(get(sp+4),get(sp+8)))))
cpu.hook_add(UC_HOOK_CODE,hook)
count=0
for mode in (0,1,2,3,7):
 for has_peer in (False,True):
  for other_mode in (0,1,2,3):
   cpu.mem_write(user,bytes(0x7000));cpu.mem_write(peer,bytes(0x7000));events.clear()
   put(user+0xdc,0x12345678);put(user+0x1568,mode,peer if has_peer else 0,999);put(peer+0x1568,other_mode,user,888);put(user+0x5830,77);put(peer+0x5830,88)
   cpu.reg_write(UC_X86_REG_EBX,0);cpu.reg_write(UC_X86_REG_ESI,user);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.emu_start(0x44af3c,0x44b04c,count=1000)
   local_reset=mode in (1,2) or (mode!=0 and mode not in (1,2) and has_peer)
   peer_reset=has_peer and ((mode in (1,2) and other_mode==1) or (mode not in (0,1,2)))
   expected=[]
   if mode==2 and has_peer and other_mode==1:expected=[(peer,b'\x03\x0a\x78\x56\x34\x12\x01')]
   elif mode not in (0,1,2) and has_peer:expected=[(user,b'\x04\x0a\x02'),(peer,b'\x04\x0a\x03')]
   assert events==expected,(mode,has_peer,other_mode,events)
   assert get(user+0x1568)==(0 if local_reset else mode)
   assert get(peer+0x1568)==(0 if peer_reset else other_mode)
   assert get(user+0x5830)==(0 if mode==2 else 77)
   assert get(peer+0x5830)==(0 if mode==1 and peer_reset else 88)
   count+=1
print(count,'original leave-zone trade cases passed; ps_game.exe SHA256',digest)
