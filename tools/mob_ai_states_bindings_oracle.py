#!/usr/bin/env python3
"""Execute original ConnectToFSM, AddStateFunctions and SetStateFunctions."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
cpu,digest=load_game();cpu.mem_map(0,0x1000);base=0x30000000;cpu.mem_map(base,0x60000)
mob=base;definition=base+0x2000;names_at=base+0x3000;registry=base+0x6000;states_at=base+0x20000;stack=base+0x50000;stop=base+0x5f000
context={};states={};records=[]
def put(at,*words):cpu.mem_write(at,struct.pack('<'+'I'*len(words),*[x&0xffffffff for x in words]))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
def ret(cleanup=0,result=None):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+cleanup)
 if result is not None:cpu.reg_write(UC_X86_REG_EAX,result)
def hook(machine,address,size,data):
 if address==0x4926b0:
  raw=bytes(machine.mem_read(machine.reg_read(UC_X86_REG_EAX),144));records.append(raw);machine.mem_write(registry+(len(records)-1)*144,raw);put(mob+0x118,registry,registry+len(records)*144,registry+0x10000);ret()
 elif address==0x495c60:
  key=get(machine.reg_read(UC_X86_REG_ECX))
  if key not in states:
   at=states_at+len(states)*0x200;states[key]=at;machine.mem_write(at,bytes([0x5a])*256)
  ret(result=states[key])
 elif address in (stop+16,stop+32):context['locks'].append('L' if address==stop+16 else 'U');ret(4)
for at in (0x4926b0,0x495c60,stop+16,stop+32):cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
put(0x5210cc,stop+16);put(0x5210d0,stop+32);put(0x3fd3ab4,1)
options=[b'MoveRandom',b'MoveRandomRun',b'MoveAtSelectedPosition',b'MoveRunAway',b'Battle',b'Debug',b'ResetUser',b'Custom',b'moverandom',b'Unknown',b'',b'MoveRandomX']
print(f'# ps_game.exe SHA256 {digest}; ConnectToFSM 00491db0, AddStateFunctions 004954e0, SetStateFunctions 0049b040. Vector growth, state-map lookup and initialized lock calls are observed ports; actual matching/hash/slot publication executes.')
for case in range(384):
 records.clear();states.clear();context['locks']=[];cpu.mem_write(mob,bytes(0x1000));put(mob+0x118,registry,registry,registry+0x10000)
 for name,pointer in [(b'Custom',0x11110000),(b'MoveRandom',0x22220000),(b'Battle',0x33330000)]:
  raw=name+b'\0'+bytes(127-len(name))+struct.pack('<4I',pointer,0,0,0);records.append(raw);cpu.mem_write(registry+(len(records)-1)*144,raw)
 put(mob+0x118,registry,registry+len(records)*144,registry+0x10000)
 names=[]
 for index,state in enumerate([b'Aa',b'B@',b'Idle',bytes([0x80+case%64])+b'ABCDE']):
  enter=options[(case+index)%len(options)];update=options[(case//3+index*3)%len(options)];exit=options[(case//7+index*5)%len(options)];row=[state,enter,update,exit];names.append(','.join(x.hex() or '-' for x in row))
  for slot,text in enumerate(row):cpu.mem_write(names_at+index*512+slot*128,text+b'\0'+bytes(127-len(text)))
 count=[0,1,2,4][case%4];put(definition+0x248,names_at,names_at+count*512);rounds=1+(case//4)%3
 for unused in range(rounds):
  put(stack,stop,mob,definition);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.emu_start(0x491db0,stop,count=500000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
 assert all(get(at+0x30)==mob for at in states.values())
 table=';'.join(str(k)+','+','.join(str(get(at+x)) for x in (0,16,32))+','+str(int(bytes(cpu.mem_read(at+0x34,200))==bytes([0x5a])*200)) for k,at in sorted(states.items())) or '-'
 print(case,count,rounds,';'.join(names),len(records),''.join(context['locks']),table)
