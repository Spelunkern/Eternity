#!/usr/bin/env python3
"""Execute original TravelCondition's literal/ignored-kind subset and retained stack."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
cpu,digest=load_game();cpu.mem_map(0,0x1000);base=0x30000000;cpu.mem_map(base,0x50000)
fsm=base;condition=base+0x2000;tokens=base+0x3000;values=base+0x10000;stack=base+0x40000;stop=base+0x4f000
context={}
def put(at,*words):cpu.mem_write(at,struct.pack('<'+'I'*len(words),*[x&0xffffffff for x in words]))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
def hook(machine,address,size,data):
 if address==0x503ae3:
  sp=machine.reg_read(UC_X86_REG_ESP);amount=get(sp+4);cpu.mem_write(base+0x20000,b'\xa5'*amount);machine.reg_write(UC_X86_REG_EAX,base+0x20000);machine.reg_write(UC_X86_REG_EIP,get(sp));machine.reg_write(UC_X86_REG_ESP,sp+4)
 elif address==0x505316:
  sp=machine.reg_read(UC_X86_REG_ESP);machine.reg_write(UC_X86_REG_EIP,get(sp));machine.reg_write(UC_X86_REG_ESP,sp+4)
for at in (0x503ae3,0x505316):cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
def snapshot(count):return ';'.join(bytes(cpu.mem_read(values+i*148,148)).hex() for i in range(count)) or '-'
print(f'# ps_game.exe SHA256 {digest}; actual TravelCondition 00498cf0 numeric case 00498d9d, string case, ignored token kinds, AI_Param assignment 0048fa90. Literal paths adapt only security cookie; KStack constructor 0049af80 also executes with an array allocation port (CKFSM passes capacity 64 at 00495226). First numeric literals are typed; undefined initial scratch-type paths excluded.')
put(stack,stop);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ESI,64);cpu.reg_write(UC_X86_REG_EDI,fsm+0x3c);cpu.emu_start(0x49af80,stop,count=100000)
assert cpu.reg_read(UC_X86_REG_EIP)==stop
array=get(fsm+0x44);numeric=all(get(array+i*148+x)==0 for i in range(64) for x in (4,8,12,16));unknown=all(get(array+i*148)==0xa5a5a5a5 and bytes(cpu.mem_read(array+i*148+20,128))==b'\xa5'*128 for i in range(64))
print('S',get(fsm+0x3c),get(fsm+0x40),int(numeric),int(unknown))
for case in range(1536):
 count=case%9;capacity=(case//9)%7;declared=[count,max(0,count-1),0,0xffffffff,1 if count==0 else count][(case//63)%5];rounds=1+(case//252)%3
 cpu.mem_write(fsm,bytes(0x100));put(fsm+0x3c,capacity,99,values);put(condition,[0,1,9,0xffffffff][case%4],declared,0x12345678);put(condition+16,tokens if count else 0,tokens+count*288 if count else 0,tokens+count*288 if count else 0)
 raw_tokens=[];known=False
 for i in range(count):
  kind=[2,7,2,5,6,0,1,10,-1][(case+i)%9]
  number=[1,2,3,4,0,-1][(case//3+i)%6]
  if kind==2:
   if not known:number=1+(case+i)%3
   if number in (1,2,3):known=True
  raw=bytearray((case+i*17+j*7)&255 for j in range(288));struct.pack_into('<8I',raw,0,case+i,0x80000000+case+i,i,19,0xffffffff-case-i,[0x80000000,0x7fc01234,0x3fc00000][i%3],kind&0xffffffff,number&0xffffffff);raw[32:160]=bytes(65+(j+case+i)%26 for j in range(128));raw[32+[0,1,63,127][(case+i)%4]]=0
  raw_tokens.append(bytes(raw).hex());cpu.mem_write(tokens+i*288,bytes(raw))
 slots=capacity+2
 for i in range(slots):
  raw=bytearray((case*11+i*31+j*3)&255 for j in range(148));struct.pack_into('<5I',raw,0,4,i+7,0x80000000,0xaabbcc00+i,0x11220000+i);cpu.mem_write(values+i*148,bytes(raw))
 initial=snapshot(slots);results=[]
 for unused in range(rounds):
  cpu.mem_write(stack-0x1000,bytes([case%256])*0x1000);put(stack,stop,fsm);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ECX,condition);cpu.emu_start(0x498cf0,stop,count=100000);assert cpu.reg_read(UC_X86_REG_EIP)==stop;results.append(cpu.reg_read(UC_X86_REG_EAX));assert results[-1]==0
 print(case,count,capacity,declared,rounds,';'.join(raw_tokens) or '-',initial,get(condition+8),get(fsm+0x40),snapshot(slots))
