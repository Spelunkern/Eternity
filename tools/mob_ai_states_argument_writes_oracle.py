#!/usr/bin/env python3
"""Execute original CalculateParam integer writes and retained map alias targets."""
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
output=base+0x26000
constructors=[]
def observe_stack(machine,address,size,data):constructors.append(machine.reg_read(UC_X86_REG_ESI))
cpu.hook_add(UC_HOOK_CODE,observe_stack,begin=0x49af80,end=0x49af80)
print(f'# ps_game.exe SHA256 {digest}; actual CalculateParam 0049d020, private 20-value KStack constructor 0049af80, delimiter 5 at 0049ec48, exhausted-count return 0049ec96, real local map constructor/lookup/destructor. Only allocation/free/security-cookie imports adapted; no parser/value/stack/map result port. Integer/DWORD assignment and compound operators 10..16 at 0049df40..0049ec43; float/undefined pops excluded.')
edges=[0,1,0xffffffff,0x80000000,0x7fffffff,0x7f801234,0x7fc05678,0x3fc00000]
for case in range(2048):
 context.update(heap=heap,frees=0);blocks.clear();constructors.clear();cpu.mem_write(fsm,bytes(0x100));call(0x495a80,registers=[(UC_X86_REG_ESI,fsm)])
 names=[b'Local',b'\x80Alias'];kinds=[1 if (case//4)%2==0 else 3,1 if (case//8)%2==0 else 3]
 operands=[edges[(case//16)%8],edges[(case//128)%8]];opcode=10+case%7
 if opcode==16:operands[1]=2+case%3
 for index,name in enumerate(names):
  raw=bytearray((case+index*31+j*7)&255 for j in range(148));struct.pack_into('<5I',raw,0,kinds[index],operands[index],0x3fc00000,operands[index],0 if case%11==0 else code(names[(index+(case//4)%2)%2]));raw[20+(case//16)%128]=0;cpu.mem_write(parameter,bytes(raw));call(0x4ade70,(code(name),),[(UC_X86_REG_ECX,fsm),(UC_X86_REG_EDX,parameter)])
 raw=bytearray(b'Z'*148);struct.pack_into('<5I',raw,0,4,7,0x80000000,0x12345678,0);raw[147]=0;cpu.mem_write(parameter,bytes(raw));call(0x4ade70,(0,),[(UC_X86_REG_ECX,fsm),(UC_X86_REG_EDX,parameter)])
 initial_vars=variables();raw_tokens=[]
 def token(kind,index=0,number=1):
  raw=bytearray((case+index*13+j*3)&255 for j in range(288));v=edges[(case+index)%8];struct.pack_into('<8I',raw,0,0xdeadbeef,v,17,index,v,v,kind,number);name=names[index%2];raw[160:288]=name+b'\0'+bytes(127-len(name));raw[32+[0,1,5,63,127][(case+index)%5]]=0;raw_tokens.append(bytes(raw))
 start=(case//8)%3
 for i in range(start):token(2,i,1)
 for segment in range(3):
  for i in range([0,17,18][(case//64+segment)%3]):token(2,i,1)
  for side in range(2):
   token(3 if side==0 or (opcode!=16 and (case//4+segment+side)%2) else 2,side,kinds[side])
   raw=bytearray(raw_tokens[-1]);struct.pack_into('<I',raw,4,operands[side]);struct.pack_into('<I',raw,16,operands[side]);raw_tokens[-1]=bytes(raw)
  token(8);raw=bytearray(raw_tokens[-1]);struct.pack_into('<I',raw,12,opcode if case<1792 else [0,5,17,0xffffffff][case%4]);raw_tokens[-1]=bytes(raw)
  for i in range(1+(case//16)%3):token(2,number=4)
  token([6,9,999][segment])
  if not(segment==2 and case%4==0):token(5)
 count=len(raw_tokens);declared=[count,count-1,0,0xffffffff,count][case%5];rounds=1+case%3;put(condition,case%4,declared,start);put(condition+16,tokens,tokens+count*288,tokens+count*288)
 for i,raw in enumerate(raw_tokens):cpu.mem_write(tokens+i*288,raw)
 put(fsm+0x3c,3,1,values)
 for i in range(5):cpu.mem_write(values+i*148,bytes((case+i*17+j*3)&255 for j in range(148)))
 retained_stack=snapshot(5);raw=bytearray((case*11+j*7)&255 for j in range(148));struct.pack_into('<5I',raw,0,[0,1,2,3,4,0x7fffffff][case%6],11,0x80000000,0xaabbccdd,0x11223344);cpu.mem_write(output,bytes(raw));initial_output=bytes(raw).hex();results=[]
 for unused in range(rounds):
  cpu.mem_write(stack-0x2000,bytes([case%256])*0x2000);call(0x49d020,(condition,output),[(UC_X86_REG_ECX,fsm)]);results.append(bytes(cpu.mem_read(output,148)).hex())
 assert constructors==[20]*rounds and snapshot(5)==retained_stack and get(fsm+0x40)==1 and get(fsm+0x3c)==3
 final_vars=variables();cursor=get(condition+8);call(0x4959c0,registers=[(UC_X86_REG_EAX,fsm)]);assert not blocks
 print(case,count,declared,start,rounds,';'.join(raw.hex() for raw in raw_tokens),initial_vars,initial_output,';'.join(results),cursor,context['frees'],retained_stack,final_vars)
