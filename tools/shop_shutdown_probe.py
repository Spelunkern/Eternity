#!/usr/bin/env python3
"""CUserShop::Ended 00478520 with customer traversal as a port."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP
from original_oracle import load_game
cpu,digest=load_game();stop,stack,user,zone=(0x10000000,0x20000000,0x30000000,0x31000000)
for p in (stop,stack,user,zone):cpu.mem_map(p,0x10000)
def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
def get(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
for p,n in ((0x478720,4),(0x4d4d20,8),(0x4208c0,12),(0x505316,0)):cpu.mem_write(p,b'\xc2'+struct.pack('<H',n))
events=[]
def hook(m,p,size,data):
 sp=m.reg_read(UC_X86_REG_ESP)
 if p==0x478720:events.append(('customers',get(user+0x15e0),get(user+0x16f4),bytes(m.mem_read(user+0x15e4,48))))
 elif p==0x4d4d20:events.append(('self',m.reg_read(UC_X86_REG_ECX),bytes(m.mem_read(get(sp+4),get(sp+8)))))
 elif p==0x4208c0:events.append(('view',m.reg_read(UC_X86_REG_ECX),m.reg_read(UC_X86_REG_EAX),bytes(m.mem_read(get(sp+8),get(sp+12)))))
cpu.hook_add(UC_HOOK_CODE,hook)
count=0
for mode in (0,1,2,3,0xffffffff):
 for present in (False,True):
  for customers in (0,2):
   cpu.mem_write(user,bytes(0x7000));events.clear();put(user+0xdc,42);put(user+0xe0,zone if present else 0);put(user+0xe4,1,2);put(user+0x15dc,user,mode);cpu.mem_write(user+0x15e4,b'\xa5'*48);put(user+0x16f4,customers,41,43);put(stack+0x8000,stop)
   cpu.reg_write(UC_X86_REG_ECX,user+0x15dc);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.emu_start(0x478520,stop,count=10000)
   expected=[]
   if mode in (1,2):
    expected=[('customers',0,customers,b'\xa5'*48),('self',user,b'\x02\x23')]
    if present:expected.append(('view',1,2,b'\x23\x05\x2a\0\0\0\0'))
   assert events==expected,(mode,present,customers,events,expected)
   assert get(user+0x15e0)==(0 if mode in (1,2) else mode)
   assert get(user+0x16f4)==(0 if mode in (1,2) else customers)
   assert bytes(cpu.mem_read(user+0x15e4,48))==(bytes(48) if mode in (1,2) else b'\xa5'*48)
   count+=1
print(count,'original shop shutdown cases passed; ps_game.exe SHA256',digest)
