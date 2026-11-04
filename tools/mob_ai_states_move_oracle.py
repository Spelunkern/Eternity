#!/usr/bin/env python3
"""Execute complete original WhileMove with real maps, A*, trail and math."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
cpu,digest=load_game();base=0x30000000;heap=0x42000000
cpu.mem_map(0,0x10000);cpu.mem_map(base,0x40000);cpu.mem_map(heap,0x1000000)
mob,definition,zone,map_object,tiles,stack,stop=[base+x for x in (0,0x2000,0x4000,0x5000,0x6000,0x30000,0x3f000)]
context={}
coverage={a:0 for a in (0x4b57c9,0x4b5896,0x4b58e4,0x4b58d5,0x4b59b3,0x4b5b6b,0x4b5b8b,0x4b5ba4,0x4b5af5,0x4b5bde)}
def put(at,*values):cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
def floats(at,*values):cpu.mem_write(at,struct.pack('<'+'f'*len(values),*values))
def bits(at):return struct.unpack('<3I',cpu.mem_read(at,12))
def ret(cleanup=0,result=None):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+cleanup)
 if result is not None:cpu.reg_write(UC_X86_REG_EAX,result)
def emit(tag,*values):context['trace'].append(tag+':'+','.join(str(x) for x in values))
def hook(machine,address,size,data):
 sp=machine.reg_read(UC_X86_REG_ESP)
 if address in coverage:coverage[address]+=1
 if address==0x503a3f:
  amount=get(sp+4);at=context['allocation'];context['allocation']+=(max(amount,1)+15)&~15
  assert context['allocation']<heap+0x1000000
  cpu.mem_write(at,bytes(amount));ret(result=at)
 elif address==stop+16:
  index=context['clocks'];value=(context['now']+(17 if index else 0))&0xffffffff;context['clocks']+=1;emit('T',value)
  if index==0 and context['mutation']==3:put(mob+0xcb8,(context['now']+1)&0xffffffff)
  if index==0 and context['mutation']==4:cpu.mem_write(definition+0x60,b'\x09');cpu.mem_write(definition+0x3f,b'\x02')
  ret(result=value)
 elif address==0x4905c0:
  assert machine.reg_read(UC_X86_REG_EAX)==mob;emit('A');put(mob+0x23c,2);ret()
 elif address==0x4b8d10:
  assert machine.reg_read(UC_X86_REG_ESI)==mob+0x1f0
  context['damage_resets']+=1;ret()
 elif address==0x4925f0:
  emit('D',get(sp+4),machine.reg_read(UC_X86_REG_ECX))
  if context['mutation']==1:floats(mob+0x50,11,4,12);put(mob+0x2a0,3)
  ret(4)
 elif address==0x4b6330:
  emit('F')
  if context['mutation']==2:put(mob+0x2a0,1);put(mob+0x12c,0xabcdef01);put(mob+0xcb8,(context['now']+1)&0xffffffff)
  ret(result=context['found'])
 elif address==0x504697:
  emit('Q',context['random'])
  if context['mutation']==5:floats(mob+0x2a4,1,0,1);cpu.mem_write(definition+0x60,b'\x00')
  ret(result=context['random'])
 elif address==0x4b1090:
  name=bytes(machine.mem_read(machine.reg_read(UC_X86_REG_EBX),32)).split(b'\0')[0].decode();assert name=='OnMoveEnd'
  emit('L',get(sp+8))
  if context['mutation']==6:put(mob+0x2b4,111);put(mob+0x2a0,4)
  ret(8)
 elif address==0x4169d0:
  assert machine.reg_read(UC_X86_REG_EAX)==mob and get(sp+4)==zone
  x,y,z,mode=struct.unpack('<4I',cpu.mem_read(sp+8,16));emit('M',x,y,z,mode)
  if context['success']:cpu.mem_write(mob+0x50,struct.pack('<3I',x,y,z))
  if context['mutation']==2:floats(mob+0x50,11,5,12);put(definition+0x5c,333)
  ret(20,context['success'])
for address in (0x503a3f,stop+16,0x4905c0,0x4b8d10,0x4925f0,0x4169d0,0x4b6330,0x504697,0x4b1090,*coverage):
 cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
put(0x5210e4,stop+16)
cpu.mem_write(stop+32,b'\xc2\x04\x00')
for iat in (0x5210cc,0x5210d0):put(iat,stop+32)
cpu.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x1000)+b'\xc3')
def call(address,args=(),registers=()):
 put(stack,stop,*args);cpu.reg_write(UC_X86_REG_ESP,stack)
 for reg,value in registers:cpu.reg_write(reg,value)
 cpu.reg_write(UC_X86_REG_FPCW,0x27f);cpu.emu_start(address,stop,count=500000000)
 assert cpu.reg_read(UC_X86_REG_EIP)==stop, (hex(cpu.reg_read(UC_X86_REG_EIP)),context)
def pairs(node,next_offset,position_offset):
 result=[]
 while node:
  assert len(result)<4096
  result.extend([get(node+position_offset),get(node+position_offset+4)]);node=get(node+next_offset)
 return ','.join(str(x) for x in result) or '-'
print(f'# ps_game.exe SHA256 {digest}; complete WhileMove 004b5790, OnMoveEnd 004b60f0, real map/A*/trail/vector/CRT math. Allocation/Windows locks/TLS, tick, target, random, ResetAttack, damage reset, dispatch, Lua and movement observed ports.')
print('# N map extent tiles; C id map mutation now success deadline speed stop sleep stun step size interval mode lua found random pos[3] target[3] path_mode point[2] trail_count trail_pairs trace pos[3] home[3] target[3] deadline speed damage_resets interval step size found mode function_type target_id cursor chain trail')
for index in range(4):
 size=32;data=bytearray(128)
 for z in range(size):
  for x in range(size):
   blocked=(index==1 and x==12 and z!=12) or (index==2 and x==12) or (index==3 and (x,z) in ((25,25),(2,2),(5,5),(29,29)))
   if blocked:data[(z*size+x)//8]|=1<<((z*size+x)%8)
 # Maps remain available for each original independent scenario.
 if index==0:maps=[]
 maps.append(bytes(data));print('N',index,size,data.hex())
for case in range(1536):
 map_id=case%4;context.update(allocation=heap,trace=[],damage_resets=0)
 cpu.mem_write(mob,bytes(0x1000));put(0x3ed34b8,0,0,0);put(0x3ed34dc,0,0,0)
 put(zone+0x24,map_object);put(map_object+4,32,tiles);cpu.mem_write(tiles,maps[map_id]);put(mob+0x60,zone);put(mob+0xa8,definition)
 now=[0,1000,0x7fffffff,0x80000000,0xffffffff][(case//3)%5];deadline=[0,now,(now+1)&0xffffffff][(case//7)%3]
 speed=[-1,0,2,4,255][(case//11)%5];stop_flag=int(case%17==0);sleep=int(case%19==0);stun=int(case%23==0)
 step=[0,1,5,15,255][(case//13)%5];size=[0,1,10,255][(case//29)%4];interval=[0,100,500,0xffffffff][(case//31)%4]
 values=[2,5.75,11.5,15,20,25,29,31.75]
 position=(values[(case//4)%8],3.25,values[(case//9)%8]);target=(values[(case//5+3)%8],9.5,values[(case//14+5)%8])
 path_mode=(case//37)%2;point=((case//41)%25+2,(case//43)%25+2)
 trail=[(8,8),(15,15)] if (case//47)%3==1 else []
 mode=[0,1,2,3,4,0xffffffff][(case//5)%6];lua=[0,1,0xffffffff][(case//67)%3];found=int((case//71)%3==1)
 mutation=(case//53)%7;success=(case//59)%2;random=(case*17+3)%32768
 floats(mob+0x50,*position);floats(mob+0xc88,1,2,3);floats(mob+0x2a4,*target)
 put(mob+0xcb8,deadline);put(mob+0x23c,speed);put(mob+0x244,stop_flag,stun,sleep);put(mob+0x2a0,mode);put(mob+0x284,lua);put(mob+0x27c,base+0x7000);put(mob+0x2b4,99);put(mob+0x12c,0x80000000+case)
 cpu.mem_write(definition+0x60,bytes([step]));cpu.mem_write(definition+0x3f,bytes([size]));put(definition+0x5c,interval)
 if path_mode:call(0x4a6b20,point,[(UC_X86_REG_EDI,mob+0x7c)])
 for x,z in trail:call(0x48f660,(x,),[(UC_X86_REG_EDI,mob+0xa4),(UC_X86_REG_EBX,z),(UC_X86_REG_EDX,map_object)])
 context.update(trace=[],now=now,mutation=mutation,success=success,clocks=0,found=found,random=random)
 initial=bits(mob+0x50);initial_target=bits(mob+0x2a4)
 call(0x4b5790,registers=[(UC_X86_REG_ECX,mob)])
 cursor=get(mob+0x8c);cursor_text=','.join(str(x) for x in struct.unpack('<2I',cpu.mem_read(cursor+4,8))) if cursor else '-'
 print('C',case,map_id,mutation,now,success,deadline,speed&0xffffffff,stop_flag,sleep,stun,step,size,interval,mode,lua,found,random,*initial,*initial_target,path_mode,*point,len(trail),*[v for p in trail for v in p],';'.join(context['trace']),*bits(mob+0x50),*bits(mob+0xc88),*bits(mob+0x2a4),get(mob+0xcb8),get(mob+0x23c),context['damage_resets'],get(definition+0x5c),cpu.mem_read(definition+0x60,1)[0],cpu.mem_read(definition+0x3f,1)[0],cpu.mem_read(mob+0xa0,1)[0],get(mob+0x2a0),get(mob+0x2b4),get(mob+0x12c),cursor_text,pairs(get(mob+0x84),0x1c,4),pairs(get(mob+0xa4),8,0))
assert all(coverage.values()), coverage
print('# Original branch coverage',', '.join(f'{address:08x}={count}' for address,count in coverage.items()))
