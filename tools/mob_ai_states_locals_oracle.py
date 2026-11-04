#!/usr/bin/env python3
"""Execute SetFSM's local phase and complete original AddLocalVariable copies."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
cpu,digest=load_game();cpu.mem_map(0,0x1000);base=0x30000000;cpu.mem_map(base,0x40000)
mob=base;catalog=base+0x2000;entries=base+0x3000;record=base+0x4000;locals_at=base+0x5000;stack=base+0x30000;stop=base+0x3f000
values={};trace=[]
def put(at,*words):cpu.mem_write(at,struct.pack('<'+'I'*len(words),*[x&0xffffffff for x in words]))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
def defined(raw):
 raw=bytearray(raw);end=20
 if struct.unpack_from('<I',raw)[0]==4:end=raw.index(0,20)+1
 raw[end:]=bytes(148-end);return bytes(raw)
def encode():return ';'.join(str(k)+','+v.hex() for k,v in sorted(values.items())) or '-'
def hook(machine,address,size,data):
 if address==0x49c500:
  pair=machine.reg_read(UC_X86_REG_EBX);key=get(pair);payload=defined(bytes(machine.mem_read(pair+4,148)));trace.append(str(key)+','+payload.hex());values.setdefault(key,payload)
  sp=machine.reg_read(UC_X86_REG_ESP);machine.reg_write(UC_X86_REG_EIP,get(sp));machine.reg_write(UC_X86_REG_ESP,sp+8)
 elif address==0x505316:
  sp=machine.reg_read(UC_X86_REG_ESP);machine.reg_write(UC_X86_REG_EIP,get(sp));machine.reg_write(UC_X86_REG_ESP,sp+4)
 elif address==0x4a91c9:assert machine.reg_read(UC_X86_REG_EAX)&255==1
 elif address in (0x4a91e0,0x4a918d):machine.emu_stop()
for at in (0x49c500,0x505316,0x4a91c9,0x4a91e0,0x4a918d):cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
print(f'# ps_game.exe SHA256 {digest}; SetFSM 004a9130 local phase through 004a91e0, AddLocalVariable 004ade70. STL insert and security cookie are ports. Inserted inactive text and string tails are undefined and normalized to zero for comparison. Remaining SetFSM phases NOT executed.')
for case in range(512):
 values.clear();trace.clear();ai=[0,1,2,3,4,255][case%6];count=[0,1,3,8,-1][(case//6)%5];ids=[1,2,1,0xffffffff,0x80000000,2,7,1]
 parameters=[]
 for i,key in enumerate(ids):
  raw=bytearray((case*17+i*31+j*7)&255 for j in range(148));struct.pack_into('<I',raw,0,[0,1,2,3,4,5,7,0xffffffff][(case+i)%8]);struct.pack_into('<I',raw,16,key);raw[20+[0,1,63,127][(case+i)%4]]=0;parameters.append(bytes(raw));cpu.mem_write(locals_at+i*148,bytes(raw))
 if case%3==0:values[1]=bytes((case+j)&255 for j in range(148))
 if case%7==0:values[99]=bytes((case*3+j)&255 for j in range(148))
 initial=encode();put(catalog+4,entries,entries+16);put(entries,record,0,record,record);put(record+0x21c,count);put(record+0x224,locals_at);cpu.mem_write(mob,bytes(0x1000));put(stack,stop,ai)
 cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ECX,mob);cpu.reg_write(UC_X86_REG_EDX,catalog)
 cpu.emu_start(0x4a9130,stop,count=100000)
 assert cpu.reg_read(UC_X86_REG_EIP) in (0x4a91e0,0x4a918d)
 success=cpu.reg_read(UC_X86_REG_EIP)==0x4a91e0
 print(case,ai,count&0xffffffff,initial,';'.join(p.hex() for p in parameters),int(success),encode(),';'.join(trace) or '-')
