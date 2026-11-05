#!/usr/bin/env python3
"""Execute original recursive numeric argument assignments and conversion payloads."""
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
 cpu.emu_start(at,stop,count=1000000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
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
observations={'calls':0,'peak':0}
def observe_argument(machine,address,size,data):
 if address==0x49d020:
  context['frames'].append(machine.reg_read(UC_X86_REG_ESP));context['calcs']+=1;context['peak']=max(context['peak'],len(context['frames']))
 else:assert context['frames'].pop()==machine.reg_read(UC_X86_REG_ESP)
for at in (0x49d020,0x49ecef):cpu.hook_add(UC_HOOK_CODE,observe_argument,begin=at,end=at)
functions=[0x0ff3a2fd,0x36824cb7,0x368302a7,0xa45c668b,0xaec1b010,0xbbfc6d86]
def semantic(raw):
 out=raw[:20]
 if struct.unpack_from('<I',raw)[0]==4:
  end=raw[20:148].index(0);out+=raw[20:21+end]
 return out.hex()
def observe_dispatch(machine,address,size,data):
 sp=machine.reg_read(UC_X86_REG_ESP);vector=get(sp+12);begin,end,capacity=get(vector+4),get(vector+8),get(vector+12)
 assert machine.reg_read(UC_X86_REG_ECX)==mob and (end-begin)%148==0 and begin<=end<=capacity
 arity=context['arities'][get(sp+8)];assert (end-begin)//148==arity and (arity==0 or begin in blocks)
 args=[semantic(bytes(machine.mem_read(begin+i*148,148))) for i in range(arity)]
 context['calls'].append(','.join([str(get(sp+8))]+args));context['maps'].append(variables())
cpu.hook_add(UC_HOOK_CODE,observe_dispatch,begin=0x4917a0,end=0x4917a0)
print(f'# ps_game.exe SHA256 {digest}; full TravelCondition 00498cf0 case4 input loop 00498f80, CalculateParam 0049d020, real AI_ParamVector push/growth 0049b6d0, CMob virtual DoFunction 004917a0, F_OUT loop 00498ff0..00499037. Only allocation/free/security cookie adapted; no argument/map/query/vector-result port. Nested CalculateParam dispatch 0049d250..0049d431; explicit metadata projections on six known queries; no factory-asset equivalence claim.')
def subject():return [cpu.mem_read(mob+0x252,1)[0],cpu.mem_read(mob+0x251,1)[0],cpu.mem_read(mob+0x253,1)[0],get(mob+0x25c),get(mob+0xcd0),get(mob+0x128),get(mob+0x12c)]
edges=[0,1,0xffffffff,0x80000000,0x7fffffff,0x3fc00000,0x7f801234,0x7fc05678]
for case in range(768):
 arities={function:(case//6+i)%3 for i,function in enumerate(functions)};depth=(case//6)%5;context.update(heap=heap,frees=0,calls=[],maps=[],arities=arities,frames=[],calcs=0,peak=0);blocks.clear();cpu.mem_write(fsm,bytes(0x100));call(0x495a80,registers=[(UC_X86_REG_ESI,fsm)]);call(0x495b10,registers=[(UC_X86_REG_ESI,fsm+0x58)])
 metadata=[]
 for index,function in enumerate(functions):
  arity=arities[function];infos=[([1,4,0x7fffffff][(case+i)%3],[0,1,2,0xffffffff,1][(case//4+i+index)%5]) for i in range(arity)];metadata.append((function,arity,infos));put(parameter,function);entry=call(0x49b870,(fsm+0x58,),[(UC_X86_REG_ECX,parameter)]);put(entry+4,arity)
  if arity:
   buffer=call(0x503a3f,(arity*8,));put(entry+0x90,buffer,buffer+arity*8,buffer+arity*8)
   for i,info in enumerate(infos):put(buffer+i*8,*info)
 names=[b'Left',b'Right']
 for index,name in enumerate(names):
  raw=bytearray((case+index*31+j*7)&255 for j in range(148));struct.pack_into('<5I',raw,0,1+(case//8+index)%3,edges[(case//16+index)%8],edges[(case//32+index)%8],edges[(case//64+index)%8],0 if case%11==0 else code(names[(index+(case//4)%2)%2]));raw[20+(case//16)%128]=0;cpu.mem_write(parameter,bytes(raw));call(0x4ade70,(code(name),),[(UC_X86_REG_ECX,fsm),(UC_X86_REG_EDX,parameter)])
 raw=bytearray(b'Z'*148);struct.pack_into('<5I',raw,0,4,7,0x80000000,0x12345678,0);raw[147]=0;cpu.mem_write(parameter,bytes(raw));call(0x4ade70,(0,),[(UC_X86_REG_ECX,fsm),(UC_X86_REG_EDX,parameter)])
 initial_vars=variables();cpu.mem_write(mob,bytes((case+j*7)&255 for j in range(0x1000)));put(mob,0x538850);put(fsm+0x48,mob);cpu.mem_write(mob+0x251,bytes([(case>>1)&1,case&1,(case>>2)&1]));put(mob+0x25c,edges[(case//6)%8]);put(mob+0xcd0,edges[(case//48)%8]);put(mob+0x128,[0,1,2,0xffffffff][(case//12)%4],edges[(case//96)%8]);initial_mob=subject();before=bytearray(cpu.mem_read(mob,0x1000))
 capacity=0 if case%7==0 else 12+(case//9)%3;rounds=1+(case//128)%3;raw_tokens=[]
 def token(kind,number=1,value=0,op=0,function=0,name=b'Left'):
  raw=bytearray((case+j*3)&255 for j in range(288));struct.pack_into('<8I',raw,0,function,value,0x76543210,op,value,value,kind,number);raw[160:288]=name+b'\0'+bytes(127-len(name));raw[32+[0,1,5,63,127][case%5]]=0;raw_tokens.append(bytes(raw))
 def invocation(function,level):
  token(4,function=function)
  for i in range(arities[function]):
   if level and i==0:
    prime=case%3==0
    if prime:token(2,value=7+level)
    active=[item for item in functions if arities[item]>0];child=active[(functions.index(function)+case+level)%len(active)] if level>1 else functions[(functions.index(function)+1+case%2)%6];invocation(child,level-1)
    if case%2:token(2,value=2+case%2);token(8,op=6+(case//4+level)%4)
    elif prime:token(2,number=4)
   elif i==0 or (case+i)%3==0:
    token(3,name=names[(case+i+level)%2]);token(2,number=1+(case+i)%3,value=edges[(case//8+i)%8]);token(8,op=10)
   elif (case+i)%3==1:token(7)
   elif case%8!=0:token(2,number=1+(case+i)%3,value=edges[(case//8+i)%8])
   token(5)
 token(2,value=case*13)
 for group in range(1+(case//24)%3):
  invocation(functions[(case+group)%6],depth)
  if capacity and case%2:token(2,value=0);token(9,op=(case//2+group)%6)
  if case%3==0:token(2,number=4)
 token(6)
 count=len(raw_tokens);declared=[count,count-1,0,0xffffffff,count][case%5];put(fsm+0x3c,capacity,99,values);put(condition,case%4,declared,0x12345678);put(condition+16,tokens,tokens+count*288,tokens+count*288)
 for i,raw in enumerate(raw_tokens):cpu.mem_write(tokens+i*288,raw)
 for i in range(capacity+2):
  raw=bytearray((case*11+i*31+j*3)&255 for j in range(148));struct.pack_into('<5I',raw,0,4,i+7,0x80000000,0xaabbcc00+i,0x11220000+i);cpu.mem_write(values+i*148,bytes(raw))
 initial=snapshot(capacity+2);results=[]
 for unused in range(rounds):
  cpu.mem_write(stack-0x2000,bytes([case%256])*0x2000);cpu.reg_write(UC_X86_REG_FPCW,0x27f);results.append(call(0x498cf0,(fsm,),[(UC_X86_REG_ECX,condition)]))
 assert not context['frames'];observations['calls']+=context['calcs'];observations['peak']=max(observations['peak'],context['peak'])
 assert cpu.reg_read(UC_X86_REG_FPCW)==0x27f
 final_mob=subject();after=bytearray(cpu.mem_read(mob,0x1000));before[0x253]=after[0x253];before[0x25c:0x260]=after[0x25c:0x260];assert before==after
 final_vars=variables();cursor,top=get(condition+8),get(fsm+0x40);final=snapshot(capacity+2);call(0x495a50,registers=[(UC_X86_REG_EAX,fsm+0x58)]);call(0x4959c0,registers=[(UC_X86_REG_EAX,fsm)]);assert not blocks
 print(case,count,capacity,declared,rounds,depth,';'.join(','.join(map(str,[function,arity]+[v for info in infos for v in info])) for function,arity,infos in metadata),';'.join(raw.hex() for raw in raw_tokens),initial_vars,final_vars,initial,','.join(map(str,initial_mob)),','.join(map(str,final_mob)),cursor,top,final,','.join(map(str,results)),context['frees'],';'.join(context['calls']) or '-','|'.join(context['maps']) or '-')

print("# Observed original CalculateParam calls:",observations["calls"],"; maximum simultaneous argument frames:",observations["peak"])
