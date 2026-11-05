#!/usr/bin/env python3
"""Original guild PvP broadcasts 00438320/004383c0, tree traversal real."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,guild,head,nodes,records,users,buf,packet,stop=(0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000,0x35000000,0x36000000,0x10000000)
for p in (stack,guild,head,nodes,records,buf,packet,stop):cpu.mem_map(p,0x10000)
cpu.mem_map(users,0x30000)
def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
for ptr,target in ((0x5210cc,stop+0x100),(0x5210d0,stop+0x100),(0x5211f0,stop+0x200)):put(ptr,target)
cpu.mem_write(stop+0x100,b'\xc2\x04\x00');cpu.mem_write(stop+0x200,b'\xc2\x04\x00')
for p,n in ((0x4df9f0,0),(0x4d4cf0,4)):cpu.mem_write(p,b'\xc2'+struct.pack('<H',n))
events=[];fail=False
def hook(m,p,size,data):
 if p==0x4df9f0:events.append('alloc');m.reg_write(UC_X86_REG_EAX,0 if fail else buf)
 elif p==0x4d4cf0:events.append(('send',m.reg_read(UC_X86_REG_ECX)))
 elif p==stop+0x200:events.append('release');m.reg_write(UC_X86_REG_EAX,1)
cpu.hook_add(UC_HOOK_CODE,hook);cpu.mem_write(packet,b'\x05\x25\x01')
for kind in (0,1):
 for status in (0,3):
  for fail in (False,True):
   for null_record in (None,1):
    cpu.mem_write(guild,bytes(0x1000));cpu.mem_write(head,bytes(0x100));cpu.mem_write(nodes,bytes(0x300));cpu.mem_write(records,bytes(0x300));cpu.mem_write(users,bytes(0x30000));events.clear()
    n=[nodes+i*0x40 for i in range(3)];r=[records+i*0x40 for i in range(3)];u=[users+i*0x10000 for i in range(3)]
    put(guild+0x4b4,status);put(guild+0x510,head);put(head,n[0],n[1],n[2]);cpu.mem_write(head+0x15,b'\x01')
    put(n[0],head,n[1],head,10,r[0]);put(n[1],n[0],head,n[2],20,0 if null_record==1 else r[1]);put(n[2],head,n[1],head,30,r[2])
    for i in range(3):put(r[i]+0x30,u[i]);put(u[i]+0x54e4,1 if i!=1 else 0);cpu.mem_write(u[i]+0x54f8,bytes([int(i!=0)]))
    put(stack+0x8000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,3);cpu.reg_write(UC_X86_REG_ECX,packet);cpu.reg_write(UC_X86_REG_EDX,guild)
    cpu.emu_start(0x438320 if kind==0 else 0x4383c0,stop,count=100000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
    expected=[]
    if status:
     expected=['alloc']
     if not fail:
      expected += [('send',u[i]) for i in range(3) if (null_record is None or i<null_record) and ((i!=0) if kind==0 else (i!=1))]
      expected += ['release']
    assert events==expected,(kind,status,fail,null_record,events,expected)
print('16 original guild duel broadcast cases passed; ps_game.exe SHA256',digest)
