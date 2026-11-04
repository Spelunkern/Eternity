#!/usr/bin/env python3
"""Execute original zero-argument system calls through the actual CMob vtable."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
cpu,digest=load_game();cpu.mem_map(0,0x1000);base=0x30000000;heap=0x42000000;cpu.mem_map(base,0x50000);cpu.mem_map(heap,0x1000000)
fsm=base;condition=base+0x2000;tokens=base+0x3000;values=base+0x10000;parameter=base+0x24000;stack=base+0x40000;stop=base+0x4f000;blocks={};context={}
def put(at,*words):cpu.mem_write(at,struct.pack('<'+'I'*len(words),*[x&0xffffffff for x in words]))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
def ret(result=None):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
 if result is not None:cpu.reg_write(UC_X86_REG_EAX,result)
def hook(machine,address,size,data):
 if address==0x503a3f:
  amount=get(machine.reg_read(UC_X86_REG_ESP)+4);at=context['heap'];context['heap']+=(max(amount,1)+15)&~15;blocks[at]=amount;cpu.mem_write(at,bytes(max(amount,1)));ret(at)
 elif address==0x503ba1:
  at=get(machine.reg_read(UC_X86_REG_ESP)+4)
  if at:assert at in blocks;cpu.mem_write(at,b'\xdd'*max(blocks.pop(at),1));context['frees']+=1
  ret()
 elif address==0x505316:ret()
for at in (0x503a3f,0x503ba1,0x505316):cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
def call(at,args=(),registers=()):
 put(stack,stop,*args);cpu.reg_write(UC_X86_REG_ESP,stack)
 for reg,value in registers:cpu.reg_write(reg,value)
 cpu.emu_start(at,stop,count=100000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
 return cpu.reg_read(UC_X86_REG_EAX)
def code(text):
 value=5381
 for byte in text:value=(value*33+(byte if byte<128 else byte-256))&0xffffffff
 return value
def snapshot(count):return ';'.join(bytes(cpu.mem_read(values+i*148,148)).hex() for i in range(count)) or '-'
def variables():
 head=get(fsm+4);nodes=[]
 def walk(node):
  if node==head:return
  walk(get(node));nodes.append(node);walk(get(node+8))
 walk(get(head+4));return ';'.join(str(get(node+12))+','+bytes(cpu.mem_read(node+16,148)).hex() for node in nodes)
mob=base+0x28000
dispatch_calls=[]
def observe_dispatch(machine,address,size,data):
 sp=machine.reg_read(UC_X86_REG_ESP);vector=get(sp+12)
 assert machine.reg_read(UC_X86_REG_ECX)==mob
 assert get(vector+4)==0 and get(vector+8)==0 and get(vector+12)==0
 dispatch_calls.append(get(sp+8))
cpu.hook_add(UC_HOOK_CODE,observe_dispatch,begin=0x4917a0,end=0x4917a0)
functions=[0x0ff3a2fd,0x36824cb7,0x368302a7,0xa45c668b,0xaec1b010,0xbbfc6d86]
assert get(0x538854)==0x4917a0
print(f'# ps_game.exe SHA256 {digest}; TravelCondition 00498cf0 case 4 at 00498f12..004990eb, real CMob vtable 00538850 -> DoFunction 004917a0, real signed SysFuncMap 00495b10/0049b870/00495a50 and local map ctor/dtor. Only allocator/free/security-cookie imports adapted; no dispatch/query/map/VM port. Zero or negative metadata argument counts; real instance reads and destructive queries.')
def subject():return [cpu.mem_read(mob+0x252,1)[0],cpu.mem_read(mob+0x251,1)[0],cpu.mem_read(mob+0x253,1)[0],get(mob+0x25c),get(mob+0xcd0),get(mob+0x128),get(mob+0x12c)]
edges=[0,1,0xffffffff,0x80000000,0x7fffffff,17,0xfffffff9,2]
for case in range(768):
 context.update(heap=heap,frees=0);blocks.clear();cpu.mem_write(fsm,bytes(0x100));call(0x495a80,registers=[(UC_X86_REG_ESI,fsm)]);call(0x495b10,registers=[(UC_X86_REG_ESI,fsm+0x58)])
 arity=[0,0xffffffff,0x80000000,0][case%4];entries=[]
 for function in functions:
  put(parameter,function);entry=call(0x49b870,(fsm+0x58,),[(UC_X86_REG_ECX,parameter)]);put(entry+4,arity);entries.append(entry)
 cpu.mem_write(mob,bytes((case+j*7)&255 for j in range(0x1000)));put(mob,0x538850);put(fsm+0x48,mob)
 cpu.mem_write(mob+0x251,bytes([(case>>1)&1,case&1,(case>>2)&1]));put(mob+0x25c,edges[(case//6)%8]);put(mob+0xcd0,edges[(case//48)%8]);put(mob+0x128,[0,1,2,0xffffffff][(case//12)%4],edges[(case//96)%8]);initial_mob=subject();before=bytearray(cpu.mem_read(mob,0x1000))
 capacity=0 if case%7==0 else 12+(case//9)%3;rounds=1+(case//128)%3;raw_tokens=[]
 def token(kind,number=1,value=0,op=0,function=0):
  raw=bytearray((case+j*3)&255 for j in range(288));struct.pack_into('<8I',raw,0,function,value,0x76543210,op,value,value,kind,number);raw[160:288]=b'WrongName\0'+bytes(118);raw_tokens.append(bytes(raw))
 token(2,value=case*13)
 for group in range(1+(case//24)%3):
  function=functions[(case+group)%6];token(4,function=function)
  if capacity and case%2:token(2,value=0);token(9,op=(case//2+group)%6)
  if case%3==0:token(2,number=4)
  if case%5==0:token(4,function=function)
 count=len(raw_tokens);declared=[count,count-1,0,0xffffffff,count][case%5];put(fsm+0x3c,capacity,99,values);put(condition,case%4,declared,0x12345678);put(condition+16,tokens,tokens+count*288,tokens+count*288)
 for i,raw in enumerate(raw_tokens):cpu.mem_write(tokens+i*288,raw)
 for i in range(capacity+2):
  raw=bytearray((case*11+i*31+j*3)&255 for j in range(148));struct.pack_into('<5I',raw,0,4,i+7,0x80000000,0xaabbcc00+i,0x11220000+i);cpu.mem_write(values+i*148,bytes(raw))
 initial=snapshot(capacity+2);results=[]
 for unused in range(rounds):
  cpu.mem_write(stack-0x2000,bytes([case%256])*0x2000);results.append(call(0x498cf0,(fsm,),[(UC_X86_REG_ECX,condition)]))
 final_mob=subject();after=bytearray(cpu.mem_read(mob,0x1000));before[0x253]=after[0x253];before[0x25c:0x260]=after[0x25c:0x260];assert before==after
 assert all(get(entry+4)==arity and get(entry+0x90)==0 and get(entry+0x94)==0 and get(entry+0x98)==0 for entry in entries)
 cursor,top=get(condition+8),get(fsm+0x40);final=snapshot(capacity+2);call(0x495a50,registers=[(UC_X86_REG_EAX,fsm+0x58)]);call(0x4959c0,registers=[(UC_X86_REG_EAX,fsm)]);assert not blocks
 print(case,count,capacity,declared,rounds,arity,';'.join(raw.hex() for raw in raw_tokens),initial,','.join(map(str,initial_mob)),','.join(map(str,final_mob)),cursor,top,final,','.join(map(str,results)),context['frees'])

print("# Observed original DoFunction calls:",len(dispatch_calls),"; function IDs:",",".join(hex(x) for x in sorted(set(dispatch_calls))))
