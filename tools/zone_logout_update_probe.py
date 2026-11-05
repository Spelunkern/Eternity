#!/usr/bin/env python3
"""CZone logout deadline traversal, ps_game.exe 00421830."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_ESP
from original_oracle import load_game
cpu,digest=load_game();stop,stack,user,zone,sentinel,waiting=(0x10000000,0x20000000,0x30000000,0x31000000,0x32000000,0x33000000)
for p in (stop,stack,user,zone,sentinel,waiting):cpu.mem_map(p,0x10000)
def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
def get(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
for ptr,target in ((0x5210cc,stop+0x100),(0x5210d0,stop+0x200)):put(ptr,target)
for addr in (stop+0x100,stop+0x200,0x40c860):cpu.mem_write(addr,b'\xc2\x04\x00')
events=[]
def hook(m,p,size,data):
 if p==0x40c860:events.append((get(m.reg_read(UC_X86_REG_ESP)+4),get(user+0x57dc)))
cpu.hook_add(UC_HOOK_CODE,hook)
count=0
for typ in (0,1,2,3):
 for deadline in (0,1,100,101,0xffffffff):
  for now in (0,100,101,0xffffffff):
   cpu.mem_write(user,bytes(0x7000));cpu.mem_write(zone,bytes(0x1000));cpu.mem_write(sentinel,bytes(0x1000));cpu.mem_write(waiting,bytes(0x1000));events.clear()
   put(zone+0x34,waiting);put(waiting+0xc,0);put(zone+0x110,sentinel);put(sentinel+4,user+0xec);put(user+0xec+4,sentinel);put(user+0xec+8,user+0xd0)
   put(user+0x57d8,typ);put(user+0x57dc,deadline);put(stack+0x8000,stop,now)
   cpu.reg_write(UC_X86_REG_ESI,zone);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.emu_start(0x421830,stop,count=10000)
   due=deadline!=0 and deadline<=now
   assert events==([(user,0)] if due else []),(typ,deadline,now,events)
   assert get(user+0x57dc)==(0 if due else deadline)
   count+=1
print(count,'original zone-user deadline cases passed; ps_game.exe SHA256',digest)
count=0
for typ in (0,1,2,3):
 for deadline in (0,1,100,101,0xffffffff):
  for now in (0,100,101,0xffffffff):
   cpu.mem_write(user,bytes(0x7000));cpu.mem_write(zone,bytes(0x1000));cpu.mem_write(sentinel,bytes(0x1000));cpu.mem_write(waiting,bytes(0x1000));events.clear()
   put(zone+0x34,waiting);put(waiting+0xc,user+8);put(user+0xc,waiting+8);put(zone+0x110,sentinel);put(sentinel+4,sentinel)
   put(user+0x57d8,typ);put(user+0x57dc,deadline);put(stack+0x8000,stop,now)
   cpu.reg_write(UC_X86_REG_ESI,zone);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.emu_start(0x421830,stop,count=10000)
   due=typ==2 and deadline!=0 and deadline<=now
   assert events==([(user,0)] if due else []),(typ,deadline,now,events)
   assert get(user+0x57dc)==(0 if due else deadline)
   count+=1
print(count,'original zone-wait deadline cases passed; ps_game.exe SHA256',digest)
