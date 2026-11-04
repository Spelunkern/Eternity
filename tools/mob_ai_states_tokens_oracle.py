#!/usr/bin/env python3
"""Execute original nonempty CKToken vector ownership and retained constructor."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
cpu,digest=load_game();cpu.mem_map(0,0x1000);base=0x30000000;heap=0x42000000;cpu.mem_map(base,0x50000);cpu.mem_map(heap,0x1000000)
fsm=base;stack=base+0x40000;stop=base+0x4f000;blocks={};context={}
def put(at,*words):cpu.mem_write(at,struct.pack('<'+'I'*len(words),*[x&0xffffffff for x in words]))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
def allocate(amount):
 at=context['heap'];context['heap']+=(max(amount,1)+15)&~15;assert context['heap']<heap+0x1000000;blocks[at]=amount;cpu.mem_write(at,bytes(max(amount,1)));return at
def ret(result=None):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
 if result is not None:cpu.reg_write(UC_X86_REG_EAX,result)
def hook(machine,address,size,data):
 if address==0x503a3f:ret(allocate(get(machine.reg_read(UC_X86_REG_ESP)+4)))
 elif address==0x503ba1:
  at=get(machine.reg_read(UC_X86_REG_ESP)+4)
  if at:assert at in blocks;cpu.mem_write(at,b'\xdd'*max(blocks.pop(at),1));context['frees']+=1
  ret()
 elif address==0x505316:ret()
for at in (0x503a3f,0x503ba1,0x505316):cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
def string(at,text):
 cpu.mem_write(at,bytes(28));put(at+20,len(text),15 if len(text)<16 else len(text))
 if len(text)<16:cpu.mem_write(at+4,text+b'\0')
 else:
  data=allocate(len(text)+1);cpu.mem_write(data,text+b'\0');put(at+4,data)
def text(at):
 data=at+4 if get(at+24)<16 else get(at+4);return bytes(cpu.mem_read(data,get(at+20))).hex() or '-'
def conditions(at,count,identity):
 if count:
  data=allocate(count*28);put(at+4,data,data+count*28,data+count*28)
  for i in range(count):
   item=data+i*28;put(item,identity+i,0x80000000+i,0xffffffff-i);token_count=(identity+i)%4
   if token_count:
    tokens=allocate(token_count*288);put(item+16,tokens,tokens+token_count*288,tokens+token_count*288)
    for slot in range(token_count):cpu.mem_write(tokens+slot*288,bytes((identity+i*17+slot*31+j*7)&255 for j in range(288)))
def condition_snapshot(at):
 result=[];begin,end=get(at+4),get(at+8);result.append(str((end-begin)//28))
 for item in range(begin,end,28):
  result.extend(str(get(item+x)) for x in (0,4,8))
  first,last=get(item+16),get(item+20);assert (last-first)%288==0
  result.append(str((last-first)//288))
  for token in range(first,last,288):result.append(bytes(cpu.mem_read(token,288)).hex())
 return result
def snapshot(at):
 assert get(at+0x30)==base+0x1000 and get(at+0xf4)==context['owner']
 result=[text(at+0x44),str(get(at+0xb4)),*[str(get(at+i*16)) for i in range(3)]]
 for phase in range(3):result.extend([text(at+0x60+phase*28),str(int(cpu.mem_read(at+0xf8+phase,1)[0]!=0)),str(get(at+0xe8+phase*4)),*condition_snapshot(at+0xb8+phase*16)])
 begin,end=get(at+0x38),get(at+0x3c);result.append(str((end-begin)//116))
 for item in range(begin,end,116):result.extend([*[str(get(item+x)) for x in (0,4,8,12)],*[text(item+x) for x in (16,44,72)],*condition_snapshot(item+100)])
 return result
def call(address,args=b'',registers=()):
 put(stack,stop);cpu.mem_write(stack+4,args);cpu.reg_write(UC_X86_REG_ESP,stack)
 for reg,value in registers:cpu.reg_write(reg,value)
 cpu.emu_start(address,stop,count=1000000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
print(f'# ps_game.exe SHA256 {digest}; AddState 004adef0, actual state copy 004965e0, transition copy 0049f630, actual STL map constructor 00495c30/insertion 00496e40/destructor 00495a20. Only allocation/free and security cookie ports. Freed buffers poisoned; nonempty CKToken vectors copy all 288 bytes; allocator padding not compared. Constructor 004a89d0 also executes.')
for case in range(256):
 context.update(heap=heap,frees=0,owner=fsm if case%2 else base+0x2000);blocks.clear();cpu.mem_write(fsm,bytes(256));put(fsm+0x34,222,111);call(0x495c30,registers=[(UC_X86_REG_ESI,fsm+0x4c)]);inputs=[];sources=[]
 for version in range(3):
  source=allocate(256);sources.append(source);name=[b'Aa',b'B@',[b'Idle\0tail',b'LongState'+b'X'*40,bytes([0x80+case%64])+b'ABCDE',b''][case%4]][version];string(source+0x44,name);put(source+0xb4,case+version+0x80000000);put(source+0x30,base+0x1000);put(source+0xf4,context['owner'])
  for phase in range(3):
   put(source+phase*16,0 if (case+version+phase)%4==0 else 100+version*10+phase);string(source+0x60+phase*28,bytes([65+phase])+b'\0tail'+b'x'*[0,16,40][(case+phase+version)%3]);put(source+0xe8+phase*4,[0,1,4,0xffffffff][(case+phase+version)%4]);cpu.mem_write(source+0xf8+phase,bytes([(case+phase+version)%2]));conditions(source+0xb8+phase*16,(case+phase+version)%4,case*100+phase*10)
  count=(case+version)%3
  if count:
   data=allocate(count*116);put(source+0x38,data,data+count*116,data+count*116)
   for i in range(count):
    item=data+i*116;put(item,case+i,0xffffffff-version,case+version,0x80000000+i);string(item+16,b'Start'+b'a'*version);string(item+44,b'End'+b'b'*20);string(item+72,b'Output\0Tail');conditions(item+100,(case+i)%3,case*1000+i*10)
  inputs.extend(snapshot(source));call(0x4adef0,bytes(cpu.mem_read(source,256)),[(UC_X86_REG_ECX,fsm)])
  # The by-value parameter has been destroyed; the caller's source allocation
  # is only a fixture shell whose owned buffers must no longer be referenced.
  cpu.mem_write(source,b'\xcc'*256)
 head=get(fsm+0x50);nodes=[]
 def walk(node):
  if node==head:return
  walk(get(node));nodes.append(node);walk(get(node+8))
 walk(get(head+4));assert len(nodes)==get(fsm+0x54)==2 and get(fsm+0x34)==222 and get(fsm+0x38)==111
 output=[str(len(nodes))]
 for node in nodes:output.extend([str(get(node+16)),*snapshot(node+24)])
 call(0x495a20,registers=[(UC_X86_REG_EAX,fsm+0x4c)]);assert set(blocks)==set(sources)
 print(case,int(context['owner']==fsm),*inputs,*output,context['frees'])
for case in range(64):
 at=base+0x28000;raw=bytes((case*13+j*7)&255 for j in range(288));cpu.mem_write(at,raw);call(0x4a89d0,registers=[(UC_X86_REG_EAX,at)])
 print('P',case,raw.hex(),bytes(cpu.mem_read(at,288)).hex())
