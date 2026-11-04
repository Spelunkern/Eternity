#!/usr/bin/env python3
"""Run original WhileIdle, GetMoveSpeed, Distance2, RotateY and CRT math."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
cpu,digest=load_game();base=0x30000000
cpu.mem_map(0,0x10000);cpu.mem_map(base,0x40000)
mob,definition,zone,stack,stop=[base+x for x in (0,0x2000,0x4000,0x30000,0x3f000)]
context={};offsets=[0xcc8,0xcbc,0xcd0,0x12c,0x23c,0x244,0x24c,0x248,0x294]
def put(at,*values):cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
def floats(at,*values):cpu.mem_write(at,struct.pack('<'+'f'*len(values),*values))
def ret(cleanup=0,result=None):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+cleanup)
 if result is not None:cpu.reg_write(UC_X86_REG_EAX,result)
def emit(tag,*values):context['trace'].append(tag+':'+','.join(str(v) for v in values))
def hook(machine,address,size,data):
 sp=machine.reg_read(UC_X86_REG_ESP)
 if address==stop+16:
  emit('T')
  if context['mutation']==1:put(mob+0xcc8,0xffffffff)
  ret(result=context['now'])
 elif address==0x4b6330:
  assert machine.reg_read(UC_X86_REG_EDI)==mob;emit('F')
  if context['mutation']==2:put(mob+0x12c,0xabcdef01);put(mob+0xcd0,700);put(mob+0xcc8,888)
  ret(result=context['found'])
 elif address==0x4925f0:
  assert machine.reg_read(UC_X86_REG_ESI)==mob+4
  emit('D',get(sp+4),machine.reg_read(UC_X86_REG_ECX));ret(4)
 elif address==0x504697:
  index=context['random_index'];value=context['random'][index];context['random_index']+=1;emit('Q',value)
  if context['mutation']==3 and index==0:put(mob+0x23c,4);cpu.mem_write(definition+0x60,b'\x09')
  if context['mutation']==4 and index==1:put(mob+0x60,0)
  if context['mutation']==5 and index==2:put(mob+0xcd0,0xfffffff0);put(definition+0x5c,0xffffffff)
  ret(result=value)
 elif address==0x42ab80:
  assert machine.reg_read(UC_X86_REG_ECX)==zone+0x100
  emit('L',get(sp+4),get(sp+8),get(sp+12),machine.reg_read(UC_X86_REG_EAX))
  if context['mutation']==6:put(mob+0xcd0,900)
  ret(20,context['line'])
 elif address==0x4169d0:
  assert machine.reg_read(UC_X86_REG_EAX)==mob and get(sp+4)==zone
  emit('M',get(sp+8),get(sp+12),get(sp+16),get(sp+20))
  if context['mutation']==7:put(mob+0xcd0,1000);put(definition+0x5c,12345)
  ret(20,1)
cpu.hook_add(UC_HOOK_CODE,hook);put(0x5210e4,stop+16);put(zone+0x24,zone+0x100)
# Original CRT thread-local errno storage; sqrt/sin/cos instructions stay intact.
cpu.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x1000)+b'\xc3')
print(f'# ps_game.exe SHA256 {digest}; WhileIdle 004b52c0, GetMoveSpeed 004908c0/00490900, Distance2 0041b1f0, RotateY 0049f750 and original CRT math; tick/target/dispatch/rand/line/move observed ports.')
print('# case mutation now found line ai def_speed step normal_time obelisk chase zone input[9] pos_bits[3] home_bits[3] random[3] trace final[9] final_normal_time final_step final_zone')
positions=[(0,0,0),(0.5,1,0),(0.50001,1,0),(3,2,4),(6,0,8),(10.00001,0,0),(-1.25,2.5,-2.75),(1e10,0,1e10)]
for case in range(1536):
 now=[0,1,1000,0xfffffff0,0xffffffff][case%5]
 deadline=[0,now,(now+1)&0xffffffff,(now-1)&0xffffffff][(case//3)%4]
 move_time=[0,now,(now+1)&0xffffffff][(case//7)%3]
 found=(case//11)%3==1;line=(case//13)%2;ai=[0,1,4][(case//17)%3];ds=[0,1,3][(case//19)%3];step=[0,1,5,10,255][(case//23)%5]
 normal=[0,100,0xffffffff][(case//29)%3];obelisk=(case//31)%5==1;chase=(case//37)%2;has_zone=(case//41)%3!=1
 speed=[-1,0,1,2,3,4,255][(case//43)%7]
 stop_flag=int(case%13==0);sleep=int(case%17==0);stun=int(case%19==0);hold=int(case%23==0)
 position=positions[(case//5)%len(positions)];home=(0,0,0) if position!=positions[-1] else positions[-1]
 values=[deadline,move_time,123,0x80000000+case,speed&0xffffffff,stop_flag,sleep,stun,hold]
 randoms=[(case*109+13)%32768,(case*277+411)%32768,(case*491+1)%32768]
 mutation=(case//47)%8
 for offset,value in zip(offsets,values):put(mob+offset,value)
 put(mob+0xa8,definition);put(mob+0xa4,1 if chase else 0);put(mob+0x60,zone if has_zone else 0)
 floats(mob+0x50,*position);floats(mob+0xc88,*home)
 cpu.mem_write(definition+0x26,bytes([ai]));cpu.mem_write(definition+0x60,bytes([step]));cpu.mem_write(definition+0x68,bytes([ds]));cpu.mem_write(definition+0x7c,bytes([obelisk]));put(definition+0x5c,normal)
 context.update(trace=[],now=now,found=int(found),line=line,mutation=mutation,random=randoms,random_index=0)
 cpu.reg_write(UC_X86_REG_FPCW,0x27f);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ECX,mob);put(stack,stop)
 cpu.emu_start(0x4b52c0,stop,count=20000)
 assert cpu.reg_read(UC_X86_REG_EIP)==stop and cpu.reg_read(UC_X86_REG_ESP)==stack+4
 bits=lambda at:struct.unpack('<3I',cpu.mem_read(at,12))
 print(case,mutation,now,int(found),line,ai,ds,step,normal,int(obelisk),chase,int(has_zone),*values,*bits(mob+0x50),*bits(mob+0xc88),*randoms,';'.join(context['trace']),*[get(mob+x) for x in offsets],get(definition+0x5c),cpu.mem_read(definition+0x60,1)[0],int(get(mob+0x60)!=0))
