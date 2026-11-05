#!/usr/bin/env python3
"""Original quick-slot client update and DB snapshot (00467058/0046c900)."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP
from original_oracle import load_game
cpu,digest=load_game();stop,stack,user,packet=(0x10000000,0x20000000,0x30000000,0x31000000)
for p in (stop,stack,user,packet):cpu.mem_map(p,0x10000)
def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
def get(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
cpu.mem_write(0x4d4f10,b'\xc2\x08\x00');events=[]
def hook(m,p,size,data):
 if p==0x4d4f10:
  sp=m.reg_read(UC_X86_REG_ESP);events.append(bytes(m.mem_read(get(sp+4),get(sp+8))))
cpu.hook_add(UC_HOOK_CODE,hook)
count=0
for n in (0,1,4,127):
 for pattern in (0,1,2):
  cpu.mem_write(user,bytes(0x7000));cpu.mem_write(packet,bytes(0x10000));events.clear();put(user+0x578c,42);cpu.mem_write(packet,b'\x0b\x01'+bytes([n]));rows=[]
  for i in range(n):
   old=5 if i%2==0 else 20;cpu.mem_write(user+0xec8+6*i,bytes([old,8,9,0,0x34,0x12]));bag=255 if pattern==1 or (pattern==2 and i%2==0) else 3;row=bytes([i%256,4,bag])+struct.pack('<H',100+i);cpu.mem_write(packet+3+5*i,row)
   if bag!=255 or old>=10:rows.append(i)
  cpu.reg_write(UC_X86_REG_EBX,0);cpu.reg_write(UC_X86_REG_ECX,0x10b);cpu.reg_write(UC_X86_REG_EDI,user);cpu.reg_write(UC_X86_REG_ESI,packet);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.emu_start(0x467058,0x4670bd,count=10000)
  assert get(user+0xec4)==len(rows);put(stack+0x8000,stop);cpu.reg_write(UC_X86_REG_EAX,user);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.emu_start(0x46c900,stop,count=10000)
  expected=[]
  if rows:
   payload=b''.join(bytes(cpu.mem_read(user+0xec8+6*i,3))+bytes(cpu.mem_read(user+0xecc+6*i,2)) for i in range(len(rows)))
   expected=[struct.pack('<HIB',0x609,42,len(rows))+payload]
  assert events==expected,(n,pattern,events,expected);count+=1
print(count,'original quick-slot update/save cases passed; ps_game.exe SHA256',digest)
