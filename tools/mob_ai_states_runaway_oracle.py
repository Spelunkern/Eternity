#!/usr/bin/env python3
"""Execute original run-away movement with real distance, status and map line."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
cpu,digest=load_game();base=0x30000000;cpu.mem_map(0,0x10000);cpu.mem_map(base,0x50000)
mob=base;zone=base+0x2000;map_object=base+0x3000;tiles=base+0x4000;table=base+0x5000;block=base+0x6000;state=base+0x7000;stack=base+0x40000;stop=base+0x4f000
context={};coverage={'line':0,'status':0,'move':0}
def put(at,*words):cpu.mem_write(at,struct.pack('<'+'I'*len(words),*[x&0xffffffff for x in words]))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
def floats(at,*numbers):cpu.mem_write(at,struct.pack('<3f',*numbers))
def bits(at):return struct.unpack('<3I',cpu.mem_read(at,12))
def fields():return [get(mob+0x124),*bits(mob+0x50),*bits(mob+0xc88),*bits(mob+0xca0)]
def hook(machine,address,size,data):
 if address==0x42ab80:coverage['line']+=1
 elif address==0x41d760:coverage['status']+=1
 elif address==0x4169d0:
  coverage['move']+=1;sp=machine.reg_read(UC_X86_REG_ESP);assert machine.reg_read(UC_X86_REG_EAX)==mob and get(sp+4)==zone
  args=struct.unpack('<4I',cpu.mem_read(sp+8,16));context['trace']='M:'+','.join(str(x) for x in args)
  if context['success']:cpu.mem_write(mob+0x50,struct.pack('<3I',*args[:3]))
  if context['mutation']==1:floats(mob+0xca0,7,17,9);floats(mob+0xc88,4,5,6)
  if context['mutation']==2:floats(mob+0x50,11,5,12);put(mob+0x124,18)
  machine.reg_write(UC_X86_REG_EIP,get(sp));machine.reg_write(UC_X86_REG_ESP,sp+24);machine.reg_write(UC_X86_REG_EAX,context['success'])
for at in (0x42ab80,0x41d760,0x4169d0):cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
cpu.mem_write(stop+32,b'\xc2\x04\x00')
for at in (0x5210cc,0x5210d0):put(at,stop+32)
cpu.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x1000)+b'\xc3')
print(f'# ps_game.exe SHA256 {digest}; onMoveRunAway 004b0360; actual SVector::Distance2 0041b1f0, GetStatus 0041d760 and CMap::EnableLine 0042ab80 execute. Movement and Windows CRT TLS/lock platform ports only.')
maps=[]
for index in range(4):
 data=bytearray(128)
 for z in range(32):
  for x in range(32):
   if (index==1 and x==12 and z!=12) or (index==2 and x==12) or (index==3 and (x,z) in ((2,2),(8,8),(10,8),(29,29))):data[(z*32+x)//8]|=1<<((z*32+x)%8)
 maps.append(bytes(data));print('N',index,32,data.hex())
for case in range(1536):
 scenario=case//4;map_id=case%4;zone_present=scenario%13!=0;battle=(scenario//3)%2;trail=(scenario//7)%2;stack_mode=(scenario//5)%4;raw_status=2 if scenario%11==0 else 9;stack_code=2 if stack_mode==2 else 1;mutation=(scenario//9)%3;success=(scenario//11)%2
 home_x=[0,8,12][(scenario//8)%3];home=(home_x,6,8);shift=[0,.5,.5000000596046448,20,20.000001907348633,-.75,1.75][scenario%7];position=(home_x+shift,5,[8,12][(scenario//10)%2])
 selected=([0,.5,2,12,19.999998092651367,20,20.000001907348633,-2][(scenario//2)%8],[7,-5,.25][(scenario//6)%3],[0,.75,3][(scenario//4)%3])
 cpu.mem_write(mob,bytes(0x1000));put(mob+0x60,zone if zone_present else 0);put(zone+0x24,map_object);put(map_object+4,32,tiles);cpu.mem_write(tiles,maps[map_id]);floats(mob+0x50,*position);floats(mob+0xc88,*home);floats(mob+0xca0,*selected);put(mob+0x124,raw_status);cpu.mem_write(mob+0x252,bytes([battle]));put(mob+0xa4,base+0x8000 if trail else 0)
 if stack_mode:
  put(mob+0x34,table,4,0,0 if stack_mode==1 else 1,1);put(table,block);put(block,state);put(state+0x28,stack_code)
 retained_stack=bytes(cpu.mem_read(mob+0x30,24));retained_trail=get(mob+0xa4)
 context.update(trace='-',mutation=mutation,success=success);initial=fields();put(stack,stop);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ECX,mob);cpu.reg_write(UC_X86_REG_FPCW,0x27f);cpu.emu_start(0x4b0360,stop,count=1000000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
 assert bytes(cpu.mem_read(mob+0x30,24))==retained_stack and get(mob+0xa4)==retained_trail
 print('C',case,map_id,int(zone_present),battle,trail,stack_mode,stack_code,mutation,success,*initial,context['trace'],*fields())
print('# coverage',coverage)
