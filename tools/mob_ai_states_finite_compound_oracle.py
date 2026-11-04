#!/usr/bin/env python3
"""Execute original finite numeric compound assignment, embedded writeback IDs and retained values."""
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
print(f'# ps_game.exe SHA256 {digest}; actual TravelCondition 00498cf0 compound opcodes 11..16 at 00499d38..0049a835, writeback key second operand VariableID, comparison branch 0049a83a, TokenStr hash 0048fb10, real map ctor/lookup/dtor 00495a80/0049b7e0/004959c0 and AI_Param assignments. Only allocation/free and security cookie adapted; no map or interpreter port. Finite numeric compound streams with existing aliased writeback targets; native undefined/fault paths excluded.')
edges=[0,1,0xffffffff,0x80000000,0x7fffffff,0x01000001,0xaaaaaaaa,2,3,0xfffffffe,0x7ffffffe,0x80000001,0x12345678,17,0x00ffffff,0x01000003]
floats=[0,0x80000000,0x3f800000,0xbf800000,0x3fc00000,0xbfc00000,0x4b800000,0x4b800001,0x4f000000,0xcf000000,0x7f7fffff,0xff7fffff,1,0x80000001,0x00800000,0x3eaaaaab]
for case in range(3456):
 context.update(heap=heap,frees=0);blocks.clear();cpu.mem_write(fsm,bytes(0x100));call(0x495a80,registers=[(UC_X86_REG_ESI,fsm)])
 pair=(case//6)%9;types=[1+pair%3,1+pair//3];a=(case//54)%16;b=(a*7+(case//864))%16;operands=[(floats if types[0]==2 else edges)[a],(floats if types[1]==2 else edges)[b]];names=[b'Left',b'Right'];opcode=11+case%6
 for index,name in enumerate(names):
  raw=bytearray((case+index*31+j*7)&255 for j in range(148));struct.pack_into('<5I',raw,0,types[index],operands[index],operands[index],operands[index],code(names[(index+(case//16)%2)%2]));raw[20:148]=b'X'*127+b'\0';cpu.mem_write(parameter,bytes(raw));call(0x4ade70,(code(name),),[(UC_X86_REG_ECX,fsm),(UC_X86_REG_EDX,parameter)])
 initial_vars=variables();capacity=12+(case//9)%5;limited=2 in types and opcode not in (13,14);rounds=1 if limited else 1+(case//256)%3;raw_tokens=[]
 def token(kind,number=1,value=0,op=0,name=b''):
  raw=bytearray((case+j*7)&255 for j in range(288));struct.pack_into('<8I',raw,0,0x12340000,value,0,op,value,value,kind,number);raw[160:288]=name+b'\0'+bytes(127-len(name));raw_tokens.append(bytes(raw))
 for group in range(1 if limited else 1+(case//48)%3):
  token(3,name=names[group%2])
  if case%2 and opcode!=16:token(3,name=names[1-group%2])
  else:token(2,number=types[1],value=(0x40000000 if types[1]==2 else 2) if opcode==16 and not limited else operands[1])
  token(8,op=opcode)
  if case%3==0:token(2,number=4)
  if case%7==0:token(3,name=names[group%2])
 count=len(raw_tokens);declared=[count,max(0,count-1),0,0xffffffff,count][case%5];put(fsm+0x3c,capacity,99,values);put(condition,case%4,declared,0x12345678);put(condition+16,tokens,tokens+count*288,tokens+count*288)
 for i,raw in enumerate(raw_tokens):cpu.mem_write(tokens+i*288,raw)
 slots=capacity+2
 for i in range(slots):
  raw=bytearray((case*11+i*31+j*3)&255 for j in range(148));struct.pack_into('<5I',raw,0,4,i+7,0x80000000,0xaabbcc00+i,0x11220000+i);cpu.mem_write(values+i*148,bytes(raw))
 initial=snapshot(slots);results=[]
 for unused in range(rounds):
  cpu.mem_write(stack-0x2000,bytes([case%256])*0x2000);cpu.reg_write(UC_X86_REG_FPCW,0x27f);results.append(call(0x498cf0,(fsm,),[(UC_X86_REG_ECX,condition)]))
 assert len(set(results))==1 and cpu.reg_read(UC_X86_REG_FPCW)==0x27f
 final_vars=variables()
 cursor,top=get(condition+8),get(fsm+0x40);final=snapshot(slots);call(0x4959c0,registers=[(UC_X86_REG_EAX,fsm)]);assert not blocks
 print(case,count,capacity,declared,rounds,';'.join(raw.hex() for raw in raw_tokens),initial_vars,initial,cursor,top,final,results[-1],context['frees'],final_vars)
