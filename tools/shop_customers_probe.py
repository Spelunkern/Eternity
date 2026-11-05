#!/usr/bin/env python3
"""Original shop customer notification loop 00478720 with map lookup ported."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_ESP
from original_oracle import load_game
cpu,digest=load_game();stop,stack,user,customer,zone,holder,node,sentinel=(0x10000000,0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000,0x35000000)
for p in (stop,stack,user,customer,zone,holder,node,sentinel):cpu.mem_map(p,0x10000)
def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
def get(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
for ptr,target in ((0x5210cc,stop+0x100),(0x5210d0,stop+0x200)):put(ptr,target)
for p,n in ((stop+0x100,4),(stop+0x200,4),(0x410320,0),(0x4d4d20,8)):cpu.mem_write(p,b'\xc2'+struct.pack('<H',n))
events=[]
def hook(m,p,size,data):
 sp=m.reg_read(UC_X86_REG_ESP)
 if p==0x410320:
  identity=get(m.reg_read(UC_X86_REG_EBX));events.append(('find',identity));put(holder,node if identity==43 else sentinel);m.reg_write(UC_X86_REG_EAX,holder)
 elif p==0x4d4d20:events.append(('send',m.reg_read(UC_X86_REG_ECX),bytes(m.mem_read(get(sp+4),get(sp+8))),get(customer+0x1738)))
cpu.hook_add(UC_HOOK_CODE,hook)
for ids in ([],[41],[43],[41,43],[43,43]):
 cpu.mem_write(user,bytes(0x7000));cpu.mem_write(customer,bytes(0x7000));events.clear();put(user+0xe0,zone);put(user+0x15dc,user);put(user+0x16f4,len(ids),*ids);put(customer+0x1738,42);put(node+0x10,customer);put(zone+0x7c,sentinel);put(stack+0x8000,stop,user+0x15dc)
 cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.emu_start(0x478720,stop,count=10000)
 expected=[]
 for i in ids:
  expected.append(('find',i))
  if i==43:expected.append(('send',customer,b'\x0c\x23',0))
 assert events==expected,(ids,events,expected)
print('5 original shop customer lists passed; ps_game.exe SHA256',digest)
