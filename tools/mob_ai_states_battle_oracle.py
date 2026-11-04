#!/usr/bin/env python3
"""Execute original compiled Battle/PAI_Attack and the actual ResetFSM chain."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x50000)
mob=base;definition=base+0x2000;user=base+0x4000;record=base+0xc000;locals_at=base+0xd000;variable=base+0xe000;state=base+0xf000;stack=base+0x40000;stop=base+0x4f000
battle_code=5381
for byte in b'BattleTest':battle_code=(battle_code*33+byte)&0xffffffff
context={}
def put(at,*words):cpu.mem_write(at,struct.pack('<'+'I'*len(words),*[x&0xffffffff for x in words]))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
def ret(cleanup=0,result=None):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+cleanup)
 if result is not None:cpu.reg_write(UC_X86_REG_EAX,result)
def fields():return [get(mob+x) for x in (0x124,0x128,0x12c,0xcb4,0xcd0,0x24c,0x248)]+list(cpu.mem_read(mob+0x251,2))+[sum((b!=0)<<i for i,b in enumerate(cpu.mem_read(mob+0x278,4))),get(mob+0xe0),get(mob+0xe4),get(variable+4),get(definition+0x78),get(user+0x142c)]
def emit(tag,extra=()):context['trace'].append(tag+':'+','.join(str(x) for x in [*extra,*fields()]))
def hook(machine,address,size,data):
 sp=machine.reg_read(UC_X86_REG_ESP);mode=context['mutation']
 if address==0x415f30:
  emit('F',[get(sp+4)])
  if mode==1:put(mob+0xcd0,333);put(mob+0x12c,99)
  if mode==2:put(definition+0x78,cpu.mem_read(user+0x12d,1)[0])
  if mode==3:put(user+0x142c,1)
  ret(4,user if context['present'] else 0)
 elif address==0x4a1580:
  assert get(sp+4)==mob;emit('V')
  if mode==4:put(mob+0x12c,88);put(mob+0x124,17)
  ret(4)
 elif address==0x49fd20:
  emit('T')
  if mode==5:put(mob+0xcb4,999);put(mob+0xcd0,555);put(mob+0x24c,2);put(mob+0x248,3);put(mob+0x124,18);put(mob+0x12c,89)
  ret(result=user if context['found'] else 0)
 elif address==0x4a1ab0:
  assert machine.reg_read(UC_X86_REG_EAX)==mob and get(sp+8)==user;emit('A',[get(sp+4)])
  if mode==7:put(mob+0x12c,90);put(mob+0xcd0,666);put(mob+0x248,4)
  ret(8)
 elif address==0x49b7e0:assert get(machine.reg_read(UC_X86_REG_EAX))==7;ret(result=variable)
 elif address==0x495c60:ret(result=state)
 elif address==stop+0x100:
  assert machine.reg_read(UC_X86_REG_ECX)==mob;emit('E')
  if mode==6:put(mob+0x124,17);put(mob+0x12c,88);put(mob+0xe0,77);put(variable+4,99)
  ret()
for at in (0x415f30,0x4a1580,0x49fd20,0x4a1ab0,0x49b7e0,0x495c60,stop+0x100):cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
print(f'# ps_game.exe SHA256 {digest}; onBattle 004b0520, PAI_Attack 004af9c0 and actual ResetFSM 00492310/ClearVariable 004a9840/OnEnter 00495da0. User lookup, vision cleanup, retarget, AttackAI, STL lookup and state-entry callback are observed required ports.')
for case in range(1536):
 scenario=case//2;operation=case%2;profile=scenario%16;mutation=(scenario//32)%8;now=[0,1,100,0xffffffff,0x80000000,0x7fffffff][(scenario//16)%6];deadline=(now+(1 if profile==1 else -1 if profile==2 else 0))&0xffffffff
 zone=profile!=0;present=profile!=6;found=(scenario//16)%2;speed=[0,6,7,255,0xffffffff,0x80000000][scenario%6];stop_value=scenario%3;only_lua=scenario%2;country=2 if profile==8 else 1;life=1 if profile==7 else 2 if profile==12 else 0;target_country=2;immune=255 if profile==9 else 0;attackable=0 if profile==10 else 255;visible=0 if profile==11 else 2;passed=0x12340000+scenario
 cpu.mem_write(mob,bytes(0x1000));cpu.mem_write(definition,bytes(0x100));cpu.mem_write(user,bytes(0x6000));cpu.mem_write(state,bytes(0x100));put(state,stop+0x100);put(state+0x30,mob)
 put(mob+0x60,base+0x10000 if zone else 0);put(mob+0xa8,definition);put(definition+0x78,country);put(mob+0x124,9,2,42);put(mob+0xcb4,deadline);put(mob+0xcd0,now);put(mob+0x240,speed);put(mob+0x24c,1 if profile==4 else 0);put(mob+0x248,1 if profile==5 else 0);put(mob+0x244,stop_value);put(mob+0x290,only_lua);cpu.mem_write(mob+0x251,b'\1\1');cpu.mem_write(mob+0x278,b'\0\1\0\1');put(mob+0xe0,battle_code,10)
 put(user+0x142c,life);cpu.mem_write(user+0x12d,bytes([target_country]));cpu.mem_write(user+0x1366,bytes([immune]));cpu.mem_write(user+0x5775,bytes([attackable]));cpu.mem_write(user+0x5774,bytes([visible]));put(variable,1,77,0,0,7)
 put(0xfdcae0+4,base+0x11000,base+0x11004);put(base+0x11000,record);put(record+0x21c,1);put(record+0x224,locals_at);put(locals_at+16,7)
 context.update(mutation=mutation,present=present,found=found,trace=[]);initial=fields();put(stack,stop,*([passed] if operation else []));cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ECX,mob);cpu.reg_write(UC_X86_REG_EAX,user)
 cpu.emu_start(0x4af9c0 if operation else 0x4b0520,stop,count=30000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
 print(case,operation,mutation,int(zone),int(present),found,speed,stop_value,only_lua,target_country,immune,attackable,visible,passed,*initial,';'.join(context['trace']) or '-',*fields())
