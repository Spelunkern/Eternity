#!/usr/bin/env python3
"""Execute original parameter initialization, variable clearing and mob FSM reset."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x40000)
mob=base;definition=base+0x2000;catalog=0xfdcae0;entries=base+0x3000;stack=base+0x30000;stop=base+0x3f000
value_addresses={};context={};keys=[1,2,7,99,0xffffffff,0x80000000,123]
field_offsets=[0x124,0x128,0x12c,0x2b8,0xe0,0xe4]
def put(at,*values):cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
def ret(cleanup=0,result=None):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+cleanup)
 if result is not None:cpu.reg_write(UC_X86_REG_EAX,result)
def values():return ';'.join(str(key)+','+bytes(cpu.mem_read(at,148)).hex() for key,at in sorted(value_addresses.items()))
def fields():return [get(mob+x) for x in field_offsets]+list(cpu.mem_read(mob+0x251,3))+list(cpu.mem_read(mob+0x278,4))
def hook(machine,address,size,data):
 sp=machine.reg_read(UC_X86_REG_ESP)
 if address==0x49b7e0:
  key=get(machine.reg_read(UC_X86_REG_EAX));assert key in value_addresses;ret(result=value_addresses[key])
 elif address==0x495c60:ret(result=base+0x5000)
 elif address==0x495da0:
  context['trace']='E:'+','.join(str(x) for x in fields())+':'+values()
  if context['mutation']==1:put(mob+0xe0,30);put(mob+0x12c,88);put(mob+0x124,17);cpu.mem_write(mob+0x278,b'\0'*4)
  if context['mutation']==2:put(mob+0xe4,30);put(mob+0xe0,0xffffffff)
  if context['mutation']==3:put(value_addresses[2],4);cpu.mem_write(value_addresses[2]+20,b'Z')
  ret()
for at in (0x49b7e0,0x495c60,0x495da0):cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
def execute(address,args=(),registers=()):
 put(stack,stop,*args);cpu.reg_write(UC_X86_REG_ESP,stack)
 for reg,value in registers:cpu.reg_write(reg,value)
 cpu.emu_start(address,stop,count=20000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
print(f'# ps_game.exe SHA256 {digest}; AI_Param constructor 0048fa30, ClearVariable 004a9840, ResetFSM 00492310 and onResetUser 004b05b0. STL lookup and state-entry callback are observed ports; actual clear/reset/constructor instructions execute.')
for case in range(768):
 scenario=case//3;operation=case%3;ai=[0,1,2,3,4,255][scenario%6];declared=[0,1,4,8,-1][(scenario//3)%5];mutation=(scenario//7)%4
 put(catalog+4,entries,entries+16);local_ids=[1,2,2,0xffffffff,0x80000000,7,123,1]
 for index in range(4):
  record=base+0x10000+index*0x3000;put(entries+index*4,0 if index==1 else record);put(record+0x21c,declared);put(record+0x224,record+0x300)
  for i,key in enumerate(local_ids):put(record+0x300+i*148+16,key)
 value_addresses.clear();cpu.mem_write(mob,bytes(0x1000));put(mob+0xa8,definition);cpu.mem_write(definition+0x26,bytes([ai]));put(mob+0x124,77);put(mob+0x128,0x80000000+scenario);put(mob+0x12c,0xffffffff-scenario);put(mob+0x2b8,0 if scenario%2==0 else 123);put(mob+0xe0,10);put(mob+0xe4,[10,20,0x80000000][scenario%3]);cpu.mem_write(mob+0x251,bytes([scenario%2,(scenario//2)%2,1]));cpu.mem_write(mob+0x278,bytes([(scenario>>i)&1 for i in range(4)]))
 for index,key in enumerate(keys):
  at=base+0x6000+index*0x100;value_addresses[key]=at
  text=bytes(65+(j+index+scenario)%26 for j in range(128));kind=[0,1,2,3,4,5,0xffffffff,7][(scenario+index)%8]
  cpu.mem_write(at,struct.pack('<5I',kind,0xaabbcc00+scenario, [0x80000000,0x7fc01234,0x3fc00000][(scenario+index)%3],0x99887700+index,0x12340000+index)+text)
 initial_fields=fields();initial_values=values();context.update(trace='-',mutation=mutation)
 if operation==0:execute(0x4a9840,(mob,),[(UC_X86_REG_EAX,catalog),(UC_X86_REG_EDX,ai)])
 elif operation==1:execute(0x492310,registers=[(UC_X86_REG_EAX,mob)])
 else:execute(0x4b05b0,registers=[(UC_X86_REG_ECX,mob)])
 print('C',case,operation,ai,declared&0xffffffff,mutation,*initial_fields,initial_values,context['trace'],*fields(),values())
for case in range(64):
 at=base+0x6000;raw=bytes((case*13+j*7)&255 for j in range(148));cpu.mem_write(at,raw)
 execute(0x48fa30,registers=[(UC_X86_REG_ECX,at)])
 print('P',case,raw.hex(),bytes(cpu.mem_read(at,148)).hex())
