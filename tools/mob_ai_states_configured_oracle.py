#!/usr/bin/env python3
"""Execute original CKFSM transitions and phase wrappers; observe expression/action ports."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x40000)
fsm=base;mob=base+0x1000;stack=base+0x30000;stop=base+0x3f000
codes=[10,20,30,0xffffffff,0x80000000];context={};records={};owners={}
def put(at,*values):cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[x&0xffffffff for x in values]))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
def ret(cleanup=0,result=None):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+cleanup)
 if result is not None:cpu.reg_write(UC_X86_REG_EAX,result)
def phase_event(tag,code):
 context['trace'].append(f'{tag}:{code},{get(fsm+0x34)},{get(mob+0x130)}')
 put(mob+0x130,get(mob+0x130)+{'E':1,'U':2,'X':3}[tag])
 if context['mutation']==3 and tag=='X':put(fsm+0x34,0xffffffff);put(base+0x12000+12,10)
 if context['mutation']==4 and tag=='E':put(fsm+0x34,0x80000000)
 if context['mutation']==5 and tag=='U':put(fsm+0x34,77)
def ensure(code):
 if code not in records:
  at=base+0x4000+len(records)*0x200;cpu.mem_write(at,bytes(0x100));records[code]=at
 return records[code]
def hook(machine,address,size,data):
 sp=machine.reg_read(UC_X86_REG_ESP);ecx=machine.reg_read(UC_X86_REG_ECX)
 if address==0x495c60:ret(result=ensure(get(ecx)))
 elif address==0x498cf0:
  assert get(sp+4)==fsm
  identity,value=get(ecx),get(ecx+4);context['trace'].append(f'Q:{identity},{get(fsm+0x34)},{get(mob+0x130)}')
  owner=owners[identity];mode=context['mutation']
  if owner[0]=='t':
   transition=owner[1]
   if mode==1 and identity==1:put(fsm+0x34,20);put(transition+12,30)
   if mode==2 and identity==1:
    begin,end=get(transition+0x68),get(transition+0x6c);put(transition+8,sum(get(at+4) for at in range(begin,end,0x1c)))
   if mode==8 and identity==1 and get(transition+0x6c)-get(transition+0x68)>=56:put(ecx+0x1c+4,1)
  elif identity%10==0:
   _,state,phase=owner
   if mode==6:put(state+0xe8+phase*4,3);cpu.mem_write(state+0xf8+phase,b'\0')
   if mode==7:put(state+0xe8+phase*4,0);put(state+phase*0x10,0)
  ret(4,value)
 elif stop+0x100<=address<stop+0x200:
  slot=(address-stop-0x100)//4;state_index,phase=divmod(slot,3)
  assert ecx==mob;phase_event('EUX'[phase],codes[state_index]);ret()
for at in [0x495c60,0x498cf0,*[stop+0x100+i*4 for i in range(15)]]:cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
def invoke(at,operation):
 put(stack,stop,*([fsm] if operation==2 else []));cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_EAX,fsm)
 cpu.emu_start(at,stop,count=20000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
def signed(x):return x if x<0x80000000 else x-0x100000000
def snapshot():
 result=[]
 for code,at in sorted(records.items(),key=lambda pair:signed(pair[0])):
  mask=sum((get(at+p*16)!=0)<<p for p in range(3));enabled=sum((cpu.mem_read(at+0xf8+p,1)[0]!=0)<<p for p in range(3))
  fields=[code,mask,enabled,*[get(at+0xe8+p*4) for p in range(3)]]
  begin,end=get(at+0x38),get(at+0x3c)
  for transition in range(begin,end,0x74):
   fields.extend([get(transition+12),get(transition+8)])
   fields.extend(get(c+4) for c in range(get(transition+0x68),get(transition+0x6c),0x1c))
  result.append(','.join(str(x) for x in fields))
 return ';'.join(result)
print(f'# ps_game.exe SHA256 {digest}; CKFSM Start 004954c0, Execute 004a8520, Evaluate 004a83e0; real KFSMState OnEnter/OnUpdate/OnExit wrappers 00495da0/004a8560/00495e00; std::map storage, TravelCondition and compiled callback bodies are observed required boundaries.')
for case in range(768):
 scenario=case//4;operation=case%4;mutation=scenario%9;current=[10,20,99][(scenario//3)%3];start=[10,20,99][(scenario//5)%3];counter=[0,0xffffffff,0x7fffffff][scenario%3]
 records.clear();owners.clear();cpu.mem_write(fsm,bytes(0x200));cpu.mem_write(base+0x4000,bytes(0x20000));put(fsm+0x34,current,start);put(mob+0x130,counter)
 print('C',case,operation,mutation,current,start,counter)
 for index,code in enumerate(codes):
  at=ensure(code);mask=(scenario+index)%8;enabled=(scenario//7+index)%8;counts=[[-1,0,1,2][(scenario//11+p+index)%4] for p in range(3)]
  put(at+0x30,mob);put(at+0xf4,fsm)
  for p in range(3):
   put(at+p*16,stop+0x100+(index*3+p)*4 if mask>>p&1 else 0);put(at+p*16+4,0)
   expressions=base+0x8000+index*0x1000+p*0x100;put(at+0xbc+p*0x10,expressions);put(at+0xe8+p*4,counts[p]);cpu.mem_write(at+0xf8+p,bytes([(enabled>>p)&1]))
   for j in range(3):
    identity=1000+index*100+p*10+j;put(expressions+j*28,identity,j%2,0,0,0,0,0);owners[identity]=('p',at,p)
  print('S',code,mask,enabled,*[x&0xffffffff for x in counts],1000+index*100)
 at=records[10];transitions=base+0x12000;put(at+0x38,transitions,transitions+3*0x74)
 for index in range(3):
  transition=transitions+index*0x74;expected=[0,1,2,0xffffffff,0x80000000][(scenario+index)%5];target=[10,20,30,99][(scenario//4)%4] if index==0 else 20 if index==1 else 99
  count=(scenario//2)%4 if index==0 else 2 if index==1 else 0
  conditions=base+0x16000+index*0x100;put(transition+8,expected,target);put(transition+0x68,conditions,conditions+28*count)
  words=[]
  for j in range(count):
   identity=index*10+j+1;value=[0,1,0xffffffff,0x7fffffff,0x80000000][(scenario+j+index)%5];condition=[identity,value,0,0,0,0,0];put(conditions+j*28,*condition);owners[identity]=('t',transition);words.extend(condition)
  print('T',expected,target,count,*words)
 context.update(trace=[],mutation=mutation)
 if operation==0:invoke(0x4954c0,0)
 elif operation==1:invoke(0x4a8520,1)
 elif operation==2:invoke(0x4a83e0,2)
 else:invoke(0x4a8520,1);invoke(0x4a83e0,2)
 print('E',';'.join(context['trace']) or '-',get(fsm+0x34),get(mob+0x130),snapshot())
