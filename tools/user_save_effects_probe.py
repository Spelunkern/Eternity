#!/usr/bin/env python3
"""SendDBAgentCharGetInfo effect phase 0046cb63..0046ccd4."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESI,UC_X86_REG_ESP
from original_oracle import load_game
cpu,digest=load_game();stop,stack,user,head,nodes,defs=(0x10000000,0x20000000,0x30000000,0x31000000,0x32000000,0x33000000)
for p in (stop,stack,user,head):cpu.mem_map(p,0x10000)
for p in (nodes,defs):cpu.mem_map(p,0x20000)
def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
def get(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
for ptr,target in ((0x5210cc,stop+0x100),(0x5210d0,stop+0x200)):put(ptr,target)
for p,n in ((stop+0x100,4),(stop+0x200,4),(0x4d4f10,8)):cpu.mem_write(p,b'\xc2'+struct.pack('<H',n))
events=[]
def hook(m,p,size,data):
 if p==0x4d4f10:
  sp=m.reg_read(UC_X86_REG_ESP);events.append(bytes(m.mem_read(get(sp+4),get(sp+8))))
cpu.hook_add(UC_HOOK_CODE,hook)
for count in (0,1,10,254,255,256,500):
 for tick in (1000,0xffffffff):
  cpu.mem_write(user,bytes(0x7000));events.clear();put(user+0x578c,42);put(user+0xa94,head);put(head+4,nodes if count else head);put(stack+0x8010,tick)
  rows=[]
  for i in range(count):
   n=nodes+i*0x100;d=defs+i*0x100;put(n+4,nodes+(i+1)*0x100 if i+1<count else head);put(n+0x30,d);cpu.mem_write(n+0xc,struct.pack('<H',i+1));cpu.mem_write(n+0x10,bytes([1]));
   effect=(3,4,2)[i%3];offline=i%2;detail=100 if i%11==0 else 1;end=0xffffffff if i%4 else 0;cap=7
   cpu.mem_write(d+0x31,bytes([effect]));cpu.mem_write(d+0x97,bytes([offline]));cpu.mem_write(d+0x98,struct.pack('<H',detail));put(d+0x5c,cap);put(n+0x20,end)
   if effect in (3,4) and detail!=100:
    now=tick//1000 if offline else tick;remaining=min((end-now if offline else (end-now)//1000) if end>now else 0,cap);rows.append(struct.pack('<HBI',i+1,1,remaining))
  cpu.reg_write(UC_X86_REG_ESI,user);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.emu_start(0x46cb63,0x46ccd4,count=100000)
  expected=[]
  for i in range(0,len(rows),255):
   chunk=rows[i:i+255];expected.append(struct.pack('<HIB',0x904,42,len(chunk))+b''.join(chunk))
  assert events==expected,(count,tick,[len(x) for x in events],[len(x) for x in expected])
print('14 original effect persistence cases passed; ps_game.exe SHA256',digest)
