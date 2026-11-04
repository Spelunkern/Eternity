#!/usr/bin/env python3
"""Execute original numeric comparison matrix, accumulator and retained stacks."""
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
print(f'# ps_game.exe SHA256 {digest}; actual TravelCondition 00498cf0 comparison branch 0049a83a and opcodes 0..5, TokenStr hash 0048fb10, real map ctor/lookup/dtor 00495a80/0049b7e0/004959c0 and AI_Param assignments. Only allocation/free and security cookie adapted; no map or interpreter port. All int/float/DWORD pairings, masked x87 default precision, NaN/infinity/subnormal inputs; undefined pop cases excluded.')
edges=[0,0x80000000,0x3f800000,0xbf800000,0x01000001,0x4b800000,0x4b800001,0x7fffffff,0xffffffff,0x7f800000,0xff800000,0x7fc01234,0xffc05678,0x7f801234,1,0x80000001]
for case in range(4096):
 context.update(heap=heap,frees=0);blocks.clear();cpu.mem_write(fsm,bytes(0x100));call(0x495a80,registers=[(UC_X86_REG_ESI,fsm)])
 pair=(case//6)%9;types=[1+pair%3,1+pair//3];a=(case//54)%16;b=(a*7+3+(case//864)*5)%16;operands=[edges[a],edges[b]];names=[b'Left',b'Right']
 if case>=3840:
  precision=[(1,2,0x01000001,0x4b800000),(3,2,0x7fffffff,0x4f000000),(3,2,0xffffffff,0x4f800000),(1,2,0x80000000,0xcf000000),(2,1,0x4b800000,0x01000001),(2,1,0xcf000000,0x80000000),(2,2,0,0x80000000),(2,2,0x7fc01234,0x7f801234)]
  left,right,x,y=precision[(case//6)%len(precision)];types=[left,right];operands=[x,y]
 for index,name in enumerate(names):
  raw=bytearray((case+index*31+j*7)&255 for j in range(148));struct.pack_into('<5I',raw,0,types[index],operands[index],operands[index],operands[index],0x99990000+index);raw[20:148]=b'X'*127+b'\0';cpu.mem_write(parameter,bytes(raw));call(0x4ade70,(code(name),),[(UC_X86_REG_ECX,fsm),(UC_X86_REG_EDX,parameter)])
 initial_vars=variables();capacity=2+(case//9)%5;rounds=1+(case//256)%3;raw_tokens=[]
 for group in range(1+(case//48)%3):
  for side in range(2):
   local=(case//3+group+side)%2==0;kind=3 if local else 2;raw=bytearray((case+side*17+group*31+j*7)&255 for j in range(288));value=operands[side] if local else (operands[side]+group)&0xffffffff;struct.pack_into('<8I',raw,0,0x12340000+side,value,0,19,value,value,kind,types[side]);raw[160:288]=names[side]+b'\0'+bytes(127-len(names[side]));raw_tokens.append(bytes(raw))
  opcode=(case+group)%6 if case<3456 or case>=3840 or group else [0xffffffff,6,19,0x7fffffff][case%4]
  raw=bytearray(288);struct.pack_into('<8I',raw,0,777,888,999,opcode,111,222,9,333);raw_tokens.append(bytes(raw))
 count=len(raw_tokens);declared=[count,max(0,count-1),0,0xffffffff,count][case%5];put(fsm+0x3c,capacity,99,values);put(condition,case%4,declared,0x12345678);put(condition+16,tokens,tokens+count*288,tokens+count*288)
 for i,raw in enumerate(raw_tokens):cpu.mem_write(tokens+i*288,raw)
 slots=capacity+2
 for i in range(slots):
  raw=bytearray((case*11+i*31+j*3)&255 for j in range(148));struct.pack_into('<5I',raw,0,4,i+7,0x80000000,0xaabbcc00+i,0x11220000+i);cpu.mem_write(values+i*148,bytes(raw))
 initial=snapshot(slots);results=[]
 for unused in range(rounds):
  cpu.mem_write(stack-0x2000,bytes([case%256])*0x2000);cpu.reg_write(UC_X86_REG_FPCW,0x27f);results.append(call(0x498cf0,(fsm,),[(UC_X86_REG_ECX,condition)]))
 assert len(set(results))==1 and variables()==initial_vars
 cursor,top=get(condition+8),get(fsm+0x40);final=snapshot(slots);call(0x4959c0,registers=[(UC_X86_REG_EAX,fsm)]);assert not blocks
 print(case,count,capacity,declared,rounds,';'.join(raw.hex() for raw in raw_tokens),initial_vars,initial,cursor,top,final,results[-1],context['frees'])
