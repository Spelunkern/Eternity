#!/usr/bin/env python3
"""SendDBAgentCharGetInfo quest phase 0046ccd4..0046ce4a."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESI,UC_X86_REG_ESP
from original_oracle import load_game
cpu,digest=load_game();stop,stack,user,head,nodes,defs=(0x10000000,0x20000000,0x30000000,0x31000000,0x32000000,0x33000000)
for p in (stop,stack,user,head):cpu.mem_map(p,0x10000)
for p in (nodes,defs):cpu.mem_map(p,0x40000)
def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
def get(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
for ptr,target in ((0x5210cc,stop+0x100),(0x5210d0,stop+0x200)):put(ptr,target)
for p,n in ((stop+0x100,4),(stop+0x200,4),(0x4d4f10,8)):cpu.mem_write(p,b'\xc2'+struct.pack('<H',n))
events=[]
def hook(m,p,size,data):
 if p==0x4d4f10:
  sp=m.reg_read(UC_X86_REG_ESP);events.append(bytes(m.mem_read(get(sp+4),get(sp+8))))
cpu.hook_add(UC_HOOK_CODE,hook)
count_cases=0
for count in (0,1,200,201,202,450):
 for tick in (1000,0xffffffff):
  cpu.mem_write(user,bytes(0x7000));events.clear();put(user+0x578c,42);put(user+0x11f4,head);put(head+4,nodes if count else head);put(stack+0x8010,tick);rows=[]
  for i in range(count):
   n=nodes+i*0x100;d=defs+i*0x200;put(n+4,nodes+(i+1)*0x100 if i+1<count else head);put(n+0x20,d);cpu.mem_write(n+8,struct.pack('<H',i+1));start=0 if i%2 else 0xffffffff;end=0 if i%4==0 else 0xffffffff;put(n+0x10,start,end);cpu.mem_write(n+0x1c,bytes([i%256,2,3]));minimum=3;maximum=7 if i%2 else 0;put(d+0x124,minimum,maximum)
   delay=(1 if end<=tick else min((end-tick)//60000,maximum)) if maximum else (min((tick-start)//60000,minimum) if tick>start else 0)
   rows.append(struct.pack('<HHBBB',i+1,delay,i%256,2,3))
  cpu.reg_write(UC_X86_REG_ESI,user);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.emu_start(0x46ccd4,0x46ce4a,count=100000)
  expected=[]
  for i in range(0,len(rows),201):
   chunk=rows[i:i+201];expected.append(struct.pack('<HIB',0xa04,42,len(chunk))+b''.join(chunk))
  assert events==expected,(count,tick,[len(x) for x in events],[len(x) for x in expected]);count_cases+=1
print(count_cases,'original quest persistence cases passed; ps_game.exe SHA256',digest)
