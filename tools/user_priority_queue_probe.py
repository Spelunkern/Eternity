#!/usr/bin/env python3
"""Original CUser::PSend and priority-list insertion; lock/heap/atomic ports."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_game
cpu,digest=load_game();user,stack,stop,buf,frame,nodes=(0x30000000,0x20000000,0x10000000,0x31000000,0x32000000,0x33000000)
for p in (user,stack,stop,buf,frame,nodes):cpu.mem_map(p,0x10000)
def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
def get(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
for ptr in (0x5210cc,0x5210d0):put(ptr,stop+0x100)
for ptr,fn in ((0x5210f8,stop+0x200),(0x5211f0,stop+0x300)):put(ptr,fn)
for p in (stop+0x100,stop+0x200,stop+0x300):cpu.mem_write(p,b'\xc2\x04\x00')
cpu.mem_write(0x4e01c0,b'\xc3');cpu.mem_write(0x4df070,b'\xc3');cpu.reg_write(UC_X86_REG_FPCW,0x27f)
state={'next':0,'fail':False}
def hook(m,p,size,data):
 sp=m.reg_read(UC_X86_REG_ESP)
 if p==0x4e01c0:
  out=0 if state['fail'] else nodes+16*state['next'];state['next']+=not state['fail'];m.reg_write(UC_X86_REG_EAX,out)
 elif p in (stop+0x200,stop+0x300):
  target=get(sp+4);value=(get(target)+(1 if p==stop+0x200 else -1))&0xffffffff;put(target,value);m.reg_write(UC_X86_REG_EAX,value)
cpu.hook_add(UC_HOOK_CODE,hook);put(buf+4,frame,1);cpu.mem_write(frame,struct.pack('<H',21));put(user+0x68,5)
def send(priority,distance):
 put(stack+0x8000,stop,priority,struct.unpack('<I',struct.pack('<f',distance))[0]);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ESI,user);cpu.reg_write(UC_X86_REG_EDI,buf)
 cpu.emu_start(0x4bd6f0,stop,count=10000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
for priority,distance in ((7,15.9),(6,79.9),(7,3.9),(7,3.1),(0,500.)) :send(priority,distance)
head=get(user+0xfc);out=[]
while head:out.append((get(head),get(head+4)));head=get(head+8)
assert out==[(500,buf),(0x6004f,buf),(0x70003,buf),(0x70003,buf),(0x7000f,buf)],out
assert get(user+0x104)==105 and get(buf+8)==6 and get(user+0x120)==5
before=state['next'];state['fail']=True;send(7,3);assert get(buf+8)==6 and get(user+0x104)==105
state['fail']=False;put(user+0x104,2048);send(7,3);assert state['next']==before and get(buf+8)==6
put(user+0x104,0);put(user+0x68,0xffffffff);send(7,3);assert state['next']==before and get(buf+8)==6
print('Original priority queue: sorted/stable entries, allocation failure, socket and capacity passed; ps_game.exe SHA256',digest)
# Original queue extraction batches at most 128 pointers and transfers their refs.
batch=0x34000000;cpu.mem_map(batch,0x10000);cpu.mem_write(0x4e0130,b'\xc3')
old_hook_state={'fail_batch':False}
def batch_hook(m,p,size,data):
 if p==0x4e0130:m.reg_write(UC_X86_REG_EAX,0 if old_hook_state['fail_batch'] else batch)
cpu.hook_add(UC_HOOK_CODE,batch_hook)
cpu.mem_write(user,bytes(0x1000));cpu.mem_write(nodes,bytes(0x10000));state['next']=0;put(buf+8,1);put(user+0x68,5);cpu.mem_write(frame,struct.pack('<H',2))
for _ in range(130):send(7,10)
def extract():
 put(stack+0x8000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ECX,user+0xfc)
 cpu.emu_start(0x4e04c0,stop,count=1000000);assert cpu.reg_read(UC_X86_REG_EIP)==stop;return cpu.reg_read(UC_X86_REG_EAX)
old_hook_state['fail_batch']=True;assert extract()==0 and get(user+0x120)==130 and get(user+0x104)==260
old_hook_state['fail_batch']=False;assert extract()==batch and get(batch+0x24)==128 and get(batch+0x20)==256
assert get(user+0x120)==2 and get(user+0x104)==4 and get(buf+8)==131
assert all(get(batch+0x428+4*i)==buf for i in range(128))
assert extract()==batch and get(batch+0x24)==2 and get(user+0x120)==0 and get(user+0x104)==0
assert extract()==0
print('Original priority extraction: allocation failure and 128+2 batch transfer passed')
