#!/usr/bin/env python3
"""Execute original SetFSM startup with prepopulated states and zero assembly lists."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
cpu,digest=load_game();cpu.mem_map(0,0x1000);base=0x30000000;cpu.mem_map(base,0x60000)
mob=base;definition=base+0x2000;catalog=base+0x2800;entries=base+0x2900;names_at=base+0x3000;registry=base+0x6000;state=base+0x20000;expressions=base+0x22000;stack=base+0x50000;stop=base+0x5f000
context={};records=[]
def put(at,*words):cpu.mem_write(at,struct.pack('<'+'I'*len(words),*[x&0xffffffff for x in words]))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
def code(name):
 value=5381
 for byte in name:value=(value*33+(byte if byte<128 else byte-256))&0xffffffff
 return value
def ret(cleanup=0,result=None):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+cleanup)
 if result is not None:cpu.reg_write(UC_X86_REG_EAX,result)
def fields():return [get(mob+x) for x in (0xe0,0xe4,0x124,0x128,0x12c)]+list(cpu.mem_read(mob+0x251,2))+[get(mob+0x2b8),get(base+0x24000),len(records),get(state+0xe8)]
def emit(tag,extra=()):context['trace'].append(tag+':'+','.join([*[str(x) for x in extra],*[str(x) for x in fields()]]))
def hook(machine,address,size,data):
 sp=machine.reg_read(UC_X86_REG_ESP);mode=context['mutation']
 if address==0x4926b0:
  raw=bytes(machine.mem_read(machine.reg_read(UC_X86_REG_EAX),144));records.append(raw);machine.mem_write(registry+(len(records)-1)*144,raw);put(mob+0x118,registry,registry+len(records)*144,registry+0x10000);ret()
 elif address==0x495c60:
  assert get(machine.reg_read(UC_X86_REG_ECX))==context['code'];ret(result=state)
 elif address==0x498cf0:
  assert get(sp+4)==mob+0xac;emit('Q',[get(machine.reg_read(UC_X86_REG_ECX))])
  if mode==1:put(mob+0xe4,77)
  if mode==2:put(state,0)
  if mode==4:put(state+0xe8,0);put(mob+0x12c,55)
  ret(4,1)
 elif address==0x504d84:
  text=bytes(machine.mem_read(get(sp+4),32)).split(b'\0')[0];assert text==b'PAI Debug on\n';emit('D',[text.hex()])
  if mode==3:put(mob+0xe0,99);put(mob+0xe4,88);put(mob+0x12c,77)
  if mode==5:put(mob+0x2b8,9)
  ret(result=len(text))
 elif address==stop+0x100:
  emit('E')
  if mode==3:put(mob+0xe0,99);put(mob+0xe4,88);put(mob+0x12c,77)
  if mode==5:put(mob+0x124,18)
  ret()
 elif address in (stop+16,stop+32):context['locks']+='L' if address==stop+16 else 'U';ret(4)
 elif address==0x505316:ret()
for at in (0x4926b0,0x495c60,0x498cf0,0x504d84,stop+0x100,stop+16,stop+32,0x505316):cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
put(0x5210cc,stop+16);put(0x5210d0,stop+32);put(0x3fd3ab4,1)
print(f'# ps_game.exe SHA256 {digest}; SetFSM 004a9130 with zero local/system/state assembly counts, actual ConnectToFSM 00491db0, state OnEnter 00495da0, Debug 004afce0 and ResetUser 004b05b0. Vector/map, TravelCondition, printf, entry callback and initialized lock/cookie ports. Factory assembly/interpreter not claimed.')
for case in range(768):
 name=[b'Aa',b'B@',b'',b'Idle',bytes([0x80+case%64])+b'ABCDE'][case%5];key=code(name);binding=[b'Debug',b'ResetUser',b'Unknown',b''][case%4];bind=(case//4)%2;enabled=(case//8)%2;count=[0,1,3,0xffffffff][(case//16)%4];callback=(case//64)%2;mutation=(case//3)%6;rounds=1+(case//5)%3
 records.clear();context.update(code=key,trace=[],mutation=mutation,locks='');cpu.mem_write(mob,bytes(0x1000));cpu.mem_write(definition,bytes(0x300));cpu.mem_write(state,bytes(0x100));put(state,stop+0x100 if callback else 0);put(state+0x30,mob);put(state+0xf4,mob+0xac);put(state+0xe8,count);cpu.mem_write(state+0xf8,bytes([enabled]));put(state+0xbc,expressions,expressions+3*28,expressions+3*28)
 for i in range(3):put(expressions+i*28,i+1)
 cpu.mem_write(definition+0x108,name+b'\0');put(definition+0x248,names_at,names_at+512*bind);cpu.mem_write(names_at,bytes(512));cpu.mem_write(names_at,name+b'\0');cpu.mem_write(names_at+128,binding+b'\0')
 put(catalog+4,entries,entries+4);put(entries,definition);put(mob+0xe0,222,111);put(mob+0x124,17,23,42);cpu.mem_write(mob+0x251,b'\1\1');put(mob+0x2b8,0 if case%3 else 123);put(base+0x24000,77);put(mob+0x118,registry,registry,registry+0x10000);initial=fields()
 for unused in range(rounds):
  put(stack,stop,0);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ECX,mob);cpu.reg_write(UC_X86_REG_EDX,catalog);cpu.emu_start(0x4a9130,stop,count=200000);assert cpu.reg_read(UC_X86_REG_EIP)==stop and cpu.reg_read(UC_X86_REG_EAX)&255==1
 print(case,name.hex() or '-',binding.hex() or '-',bind,enabled,callback,mutation,rounds,*initial,';'.join(context['trace']) or '-',context['locks'],*fields())
