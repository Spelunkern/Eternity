#!/usr/bin/env python3
"""Execute original Lua/FSM ownership and retained-field lifecycle instructions."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
cpu,digest=load_game();base=0x30000000
cpu.mem_map(base,0x30000)
mob=base;stack=base+0x20000;stop=base+0x2f000
nodes=[base+0x3000+i*0x100 for i in range(5)];states=[base+0x4000+i*0x100 for i in range(5)]
handles=[0,base+0x6000,base+0x7000,base+0x8000]
offsets=[0x288,0x28c,0x290,0x294,0x29c,0x2a0,0x2a4,0x2a8,0x2ac,0x2b4,0x284,0x280,0x2b0,0xcd0,0x124]
context={}
def put(at,*values):cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
def handle(at):return handles.index(at)
def snapshot():return [handle(get(mob+0x27c)),int(get(mob+0x44)!=0),context['stack']]+[get(mob+x) for x in offsets]
def event(tag,*values):context['trace'].append(tag+':'+','.join(str(v) for v in [*values,*snapshot()]))
def ret(cleanup=0,result=None):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+cleanup)
 if result is not None:cpu.reg_write(UC_X86_REG_EAX,result)
def hook(machine,address,size,data):
 sp=machine.reg_read(UC_X86_REG_ESP);eax=machine.reg_read(UC_X86_REG_EAX);ecx=machine.reg_read(UC_X86_REG_ECX)
 if address==0x4f2670:
  event('N')
  if context['mutation']==1:put(mob+0x27c,handles[3]);put(mob+0x284,77)
  ret(result=handles[context['allocated']])
 elif address in (0x4f4050,0x4f47c0,0x4f5150):
  library={0x4f4050:'B',0x4f47c0:'M',0x4f5150:'O'}[address];event(library,handle(get(sp+4)))
  if context['mutation']==2 and library=='B':put(mob+0x27c,handles[3]);put(mob+0x284,88)
  if context['mutation']==3 and library=='M':put(mob+0x27c,0);put(mob+0x290,99)
  ret()
 elif address==0x4b1310:
  assert get(sp+4)==mob;event('R',handle(ecx))
  if context['mutation']==4:put(mob+0x284,0);put(mob+0x290,0)
  if context['mutation']==5:put(mob+0x284,5);put(mob+0x2b4,123)
  ret(4)
 elif address==0x4f55b0:
  event('C',handle(get(sp+4)))
  if context['mutation']==6:put(mob+0x27c,handles[3]);put(mob+0x284,6);put(mob+0x290,7)
  ret()
 elif address==0x4b1010:
  name=bytes(machine.mem_read(machine.reg_read(UC_X86_REG_EBX),16)).split(b'\0')[0].decode();assert name=='Init'
  assert get(sp+4)==mob;event('I',handle(machine.reg_read(UC_X86_REG_EDI)))
  if context['mutation']==7:put(mob+0x284,0);put(mob+0x290,0)
  ret(4)
 elif address==0x4b7710:
  identity=get(sp+8);assert get(sp+12)==[0x4b52c0,0x4b55c0,0x4b5780,0x4b5c40,0x4b5790][identity]
  context['registered']+=1;put(nodes[identity]+12,states[identity]);ret(56)
 elif address in (0x4b7820,0x4b78f0):context['transitions']+=1;ret(28)
 elif address==0x4b7c20:context['transitions']+=1;ret(28)
 elif address==0x493fc0:put(eax,nodes[get(ecx)]);ret()
 elif address==0x493150:
  assert get(sp+4)==0 and context['registered']==5 and context['transitions']==23
  context['stack']=1;ret(4)
 elif address==0x492b90:
  context['stack']=0;put(mob+0x44,0);ret()
cpu.hook_add(UC_HOOK_CODE,hook)
print(f'# ps_game.exe SHA256 {digest}; InitLua 004b14f0, ReleaseLua 004b1530, InitLuaFSM 004b4c20, ReleaseLuaFSM 004b5260, LuaError 004b52b0, LuaInitCall 004b1550. Lua VM/library/registration/call/close and STL storage are observed ports; fixed table registration is asserted.')
print('# case operation mutation allocated initial[18] trace final[18] result (stack codes: 0 empty,1 idle,2 move/combat)')
functions=[0x4b14f0,0x4b1530,0x4b4c20,0x4b5260,0x4b52b0,0x4b1550]
for case in range(768):
 operation=case%6;scenario=case//6;mutation=scenario%8;allocated=(scenario//3)%3
 active=(scenario//5)%2;state=2 if active else 0;pointer=(scenario//7)%3
 values=[0x80000000+scenario,scenario+1,(scenario//2)%3,scenario+3,scenario+4,scenario+5,0x3f800000,0x40000000,0x40400000,scenario+9,[0,1,0xffffffff][(scenario//11)%3],scenario+11,scenario+12,scenario+13,scenario+14]
 for offset,value in zip(offsets,values):put(mob+offset,value)
 put(mob+0x27c,handles[pointer]);put(mob+0x44,active)
 context.update(trace=[],stack=state,mutation=mutation,allocated=allocated,registered=0,transitions=0)
 initial=snapshot();put(stack,stop);cpu.reg_write(UC_X86_REG_ESP,stack)
 for reg,value in [(UC_X86_REG_ESI,mob),(UC_X86_REG_EAX,mob),(UC_X86_REG_ECX,mob)]:cpu.reg_write(reg,value)
 cpu.emu_start(functions[operation],stop,count=5000)
 assert cpu.reg_read(UC_X86_REG_EIP)==stop and cpu.reg_read(UC_X86_REG_ESP)==stack+4
 result=cpu.reg_read(UC_X86_REG_EAX)&0xffffffff if operation==0 else 0
 print(case,operation,mutation,allocated,*initial,';'.join(context['trace']) or '-',*snapshot(),result)
