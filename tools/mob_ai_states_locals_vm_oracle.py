#!/usr/bin/env python3
"""Execute original local-variable reads on the real typed map and value stack."""
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
print(f'# ps_game.exe SHA256 {digest}; actual TravelCondition 00498cf0 local branch 00498e24, TokenStr hash 0048fb10, real map ctor/lookup/dtor 00495a80/0049b7e0/004959c0 and AI_Param assignments. Only allocation/free and security cookie adapted; no map or interpreter port. Missing typed entries excluded.')
for case in range(1024):
 context.update(heap=heap,frees=0);blocks.clear();cpu.mem_write(fsm,bytes(0x100));call(0x495a80,registers=[(UC_X86_REG_ESI,fsm)])
 names=[b'',b'Aa',b'B@',b'Local',bytes([0x80+case%64])+b'ABCDE',b'Q'*127]
 for index,name in enumerate(names):
  raw=bytearray((case+index*31+j*7)&255 for j in range(148));kind=(case+index)%6;struct.pack_into('<5I',raw,0,kind,0xffffffff-case-index,[0x80000000,0x7fc01234,0x3fc00000][index%3],0x80000000+case+index,0x99990000+index);raw[20:148]=bytes(65+(j+case+index)%26 for j in range(128));raw[20+[0,1,63,127][(case+index)%4]]=0;cpu.mem_write(parameter,bytes(raw));call(0x4ade70,(code(name),),[(UC_X86_REG_ECX,fsm),(UC_X86_REG_EDX,parameter)])
 initial_vars=variables();count=case%9;capacity=(case//9)%8;declared=[count,max(0,count-1),0,0xffffffff,1 if count==0 else count][(case//72)%5];rounds=1+(case//128)%3
 put(fsm+0x3c,capacity,99,values);put(condition,[0,1,9,0xffffffff][case%4],declared,0x12345678);put(condition+16,tokens if count else 0,tokens+count*288 if count else 0,tokens+count*288 if count else 0);raw_tokens=[];known=False
 for i in range(count):
  kind=[3,2,7,3,5,6,10,3][(case+i)%8];number=[1,2,3,4,0,-1][(case//3+i)%6]
  if kind==2:
   if not known:number=1+(case+i)%3
   if number in (1,2,3):known=True
  raw=bytearray((case+i*17+j*7)&255 for j in range(288));struct.pack_into('<8I',raw,0,0x12340000+i,0x80000000+case+i,i,19,0xffffffff-case-i,[0x80000000,0x7fc01234,0x3fc00000][i%3],kind&0xffffffff,number&0xffffffff);raw[32:160]=bytes(65+(j+case+i)%26 for j in range(128));raw[32+[0,1,63,127][(case+i)%4]]=0;name=names[(case+i)%len(names)];raw[160:288]=name+b'\0'+bytes(127-len(name));raw_tokens.append(bytes(raw).hex());cpu.mem_write(tokens+i*288,bytes(raw))
 slots=capacity+2
 for i in range(slots):
  raw=bytearray((case*11+i*31+j*3)&255 for j in range(148));struct.pack_into('<5I',raw,0,4,i+7,0x80000000,0xaabbcc00+i,0x11220000+i);cpu.mem_write(values+i*148,bytes(raw))
 initial=snapshot(slots)
 for unused in range(rounds):
  cpu.mem_write(stack-0x2000,bytes([case%256])*0x2000);assert call(0x498cf0,(fsm,),[(UC_X86_REG_ECX,condition)])==0
 assert variables()==initial_vars
 cursor,top=get(condition+8),get(fsm+0x40);final=snapshot(slots);call(0x4959c0,registers=[(UC_X86_REG_EAX,fsm)]);assert not blocks
 print(case,count,capacity,declared,rounds,';'.join(raw_tokens) or '-',initial_vars,initial,cursor,top,final,context['frees'])
