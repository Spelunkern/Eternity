#!/usr/bin/env python3
"""Execute the original guild application cancellation with isolated owner hooks."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game(); stack,guild,user,record,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
for address in (stack,guild,user,record,stop):cpu.mem_map(address,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
events=[]; exists=False
def hook(m,a,size,ctx):
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x434460:events.append(('take',get(get(sp+4))));m.reg_write(UC_X86_REG_EAX,record if exists else 0)
 elif a==0x4347e0:events.append(('directory',get(get(sp+4))))
 elif a==0x4d4f10:events.append(('db',bytes(m.mem_read(get(sp+4),get(sp+8)))))
 elif a==0x42c8f0:events.append(('admin',bytes(m.mem_read(m.reg_read(UC_X86_REG_ECX),m.reg_read(UC_X86_REG_EAX)))))
for a,n in ((0x434460,4),(0x4347e0,4),(0x4d4f10,8),(0x42c8f0,4),(stop+0x100,4),(stop+0x200,4)):
 cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
put(0x5210cc,stop+0x100);put(0x5210d0,stop+0x200);put(0x546f44,stop+0x300)
def run(found):
 global exists
 exists=found;events.clear()
 for a in (stack,guild,user,record,0x3ed3000):cpu.mem_write(a,bytes(0x10000))
 put(guild+8,77);put(user+0xdc,43);put(user+0x1750,77);put(record+8,43)
 put(stack+0xf000,stop);put(stack+0xf004,user);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_EBX,guild)
 cpu.emu_start(0x42e040,stop,count=100000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
 return list(events),get(user+0x1750),cpu.reg_read(UC_X86_REG_EAX)&255
assert run(False)==([('take',43)],77,0)
e,pending,ok=run(True);assert pending==0 and ok==1 and e==[('take',43),('directory',43),('db',struct.pack('<HII',0xb08,77,43)),('admin',struct.pack('<HI',0xd18,43))],e
print('2 original guild application cancellation cases passed; ps_game.exe SHA256',digest)
