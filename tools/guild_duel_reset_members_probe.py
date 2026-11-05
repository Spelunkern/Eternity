#!/usr/bin/env python3
"""Original CGuild::PvPResetUsers 00438070 with tree traversal and cleanup ports."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,guild,heads,nodes,records,users,stop=(0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000,0x10000000)
for p in (stack,guild,heads,nodes,records,stop):cpu.mem_map(p,0x10000)
cpu.mem_map(users,0x30000)
def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
def get(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
for ptr in (0x5210cc,0x5210d0):put(ptr,stop+0x100)
cpu.mem_write(stop+0x100,b'\xc2\x04\x00')
for p,n in ((0x47c610,4),(0x4b8aa0,0)):cpu.mem_write(p,b'\xc2'+struct.pack('<H',n))
events=[]
def hook(m,p,size,data):
 if p==0x47c610:
  sp=m.reg_read(UC_X86_REG_ESP);events.append(('clear',get(sp+4)))
 elif p==0x4b8aa0:events.append(('damage',m.reg_read(UC_X86_REG_ESI)-0x1538))
cpu.hook_add(UC_HOOK_CODE,hook)
for null_record in (None,0,1,2):
 for missing_user in (None,0,1,2):
  cpu.mem_write(guild,bytes(0x1000));cpu.mem_write(heads,bytes(0x100));cpu.mem_write(nodes,bytes(0x300));cpu.mem_write(records,bytes(0x300));cpu.mem_write(users,bytes(0x30000));events.clear()
  h=heads;n=[nodes+i*0x40 for i in range(3)];r=[records+i*0x40 for i in range(3)];u=[users+i*0x10000 for i in range(3)]
  put(guild+0x510,h);put(h,n[0],n[1],n[2]);cpu.mem_write(h+0x15,b'\x01')
  put(n[0],h,n[1],h,10,0 if null_record==0 else r[0]);put(n[1],n[0],h,n[2],20,0 if null_record==1 else r[1]);put(n[2],h,n[1],h,30,0 if null_record==2 else r[2])
  for i in range(3):
   put(r[i]+0x30,0 if missing_user==i else u[i]);put(u[i]+0x54e4,123);put(u[i]+0x54f4,8);put(u[i]+0x54fc,9);put(u[i]+0x1528,4);cpu.mem_write(u[i]+0x54f8,b'\x01')
  put(stack+0x8000,stop,guild);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.emu_start(0x438070,stop,count=100000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
  visited=[i for i in range(3) if (null_record is None or i<null_record) and i!=missing_user]
  assert events==[event for i in visited for event in [('clear',u[i]),('damage',u[i])]],(null_record,missing_user,events)
  for i in range(3):
   reset=i in visited
   assert get(u[i]+0x54e4)==(0 if reset else 123) and get(u[i]+0x54f4)==(0xffffffff if reset else 8) and get(u[i]+0x54fc)==(0 if reset else 9) and get(u[i]+0x1528)==(0 if reset else 4)
   assert cpu.mem_read(u[i]+0x54f8,1)==bytes([0 if reset else 1])
print('16 original guild member reset tree cases passed; ps_game.exe SHA256',digest)
