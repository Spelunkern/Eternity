#!/usr/bin/env python3
"""Original CUser::PSendView 004bd950 cadence, zone broadcasts ported."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();user,zone,stack,stop,packet=(0x30000000,0x31000000,0x20000000,0x10000000,0x32000000)
for p in (user,zone,stack,stop,packet):cpu.mem_map(p,0x10000)
def put(p,*values):cpu.mem_write(p,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
def get(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
for addr,cleanup in ((0x421220,32),(0x4213d0,36)):cpu.mem_write(addr,b'\xc2'+struct.pack('<H',cleanup))
events=[]
def hook(m,addr,size,data):
 if addr in (0x421220,0x4213d0):
  sp=m.reg_read(UC_X86_REG_ESP);n=8 if addr==0x421220 else 9;events.append((addr,struct.unpack('<'+'I'*n,m.mem_read(sp+4,n*4))))
cpu.hook_add(UC_HOOK_CODE,hook)
for present in (False,True):
 for counter in (0,1,2,3,4,0x7fffffff,0xffffffff):
  cpu.mem_write(user,bytes(0x1000));put(user+0xe0,zone if present else 0);put(user+0xdc,42);put(user+0xf8,counter);events.clear()
  put(stack+0x8000,stop,packet,21,0);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ESI,user);cpu.reg_write(UC_X86_REG_EBX,7)
  cpu.emu_start(0x4bd950,stop,count=1000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
  increment=(counter+1)&0xffffffff;signed=increment if increment<0x80000000 else increment-0x100000000
  wide=present and signed>=4
  assert get(user+0xf8)==(0 if wide else increment if present else counter)
  assert (cpu.reg_read(UC_X86_REG_EAX)&255)==int(wide)
  assert [e[0] for e in events]==([0x421220]+([0x4213d0] if wide else []) if present else [])
  if present:
   first=events[0][1];assert first==(zone,packet,21,user+0xd0,0x42a00000,42,0,7),first
   if wide:
    second=events[1][1];assert second==(zone,packet,21,user+0xd0,0x42a00000,0x42f00000,42,0,7),second
print('14 original prioritized-view cadence cases passed; ps_game.exe SHA256',digest)
