#!/usr/bin/env python3
"""Execute original guild donation item path with isolated OS/transport owners."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBP,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,guild,item,definition,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x10000000
for at in (stack,user,guild,item,definition,stop):cpu.mem_map(at,0x10000)
def put(at,value):cpu.mem_write(at,struct.pack('<I',value&0xffffffff))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
events=[]
def hook(m,at,size,ctx):
 sp=m.reg_read(UC_X86_REG_ESP)
 if at in (0x4d4d20,0x4d4f10,0x4d4e50):
  label={0x4d4d20:'client',0x4d4f10:'db',0x4d4e50:'log'}[at];events.append((label,bytes(m.mem_read(get(sp+4),get(sp+8)))))
 elif at==0x40a590:events.append(('free',m.reg_read(UC_X86_REG_EDI),get(user+0x224)))
 elif at==0x430eb0:events.append(('notice',get(guild+0x4c),get(guild+0x9c)))
 elif at==stop+0x300:
  ptr,delta=get(sp+4),get(sp+8);old=get(ptr);put(ptr,old+delta);m.reg_write(UC_X86_REG_EAX,old)
 elif at==stop+0x400:
  ptr=get(sp+4);put(ptr,get(ptr)+1);m.reg_write(UC_X86_REG_EAX,get(ptr))
for at,n in ((stop+0x100,4),(stop+0x200,4),(stop+0x300,8),(stop+0x400,4),(0x4d4d20,8),(0x4d4f10,8),(0x4d4e50,8),(0x45b0d0,0),(0x40a590,0),(0x430eb0,0)):
 cpu.mem_write(at,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
for at,target in ((0x5210cc,stop+0x100),(0x5210d0,stop+0x200),(0x52111c,stop+0x300),(0x5210f8,stop+0x400),(0x546f44,stop+0x500),(0x546f4c,stop+0x600),(0x546f3c,stop+0x700)):put(at,target)
def run(typeid,count):
 events.clear()
 for at in (stack,user,guild,item,definition):cpu.mem_write(at,bytes(0x10000))
 put(user+0x1758,guild);put(user+0x1304,1);put(user+0x578c,71);put(user+0x224,item);put(guild+8,77);put(guild+0x4c,300);put(guild+0x9c,5);put(item+0x30,definition);cpu.mem_write(item+0x40,b'+');cpu.mem_write(item+0x42,bytes([count]));cpu.mem_write(definition+0x46,bytes([typeid]));
 cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_EBP,user);cpu.emu_start(0x47734d,0x47768c,count=200000)
 assert cpu.reg_read(UC_X86_REG_EIP)==0x47768c
 return list(events),get(guild+0x4c),get(guild+0x9c)
for typeid,multiplier in ((52,1),(53,10),(54,100),(55,1000),(56,0)):
 e,total,returned=run(typeid,3);amount=multiplier*3
 assert e[0]==('db',struct.pack('<HIBBB',0x702,71,1,0,3)) and e[1][0]=='log' and len(e[1][1])==152 and e[1][1][:2]==b'\x15\x04' and e[1][1][149:151]==b'\x01\x00' and e[2]==('free',item,0) and e[3]==('client',b'\x35\x0d\x01\x01\x00'),e
 if amount:
  assert e[4]==('notice',300+amount,6) and e[5]==('db',struct.pack('<HIIIII',0xb12,77,300+amount,amount,71,6)) and e[6][0]=='log' and len(e[6][1])==118 and e[6][1][:2]==b'\x0a\x0b' and e[6][1][110:118]==struct.pack('<II',300+amount,amount) and (total,returned)==(300+amount,6),e
 else:assert len(e)==4 and (total,returned)==(300,5),e
print('5 original guild donation item cases passed; ps_game.exe SHA256',digest)
