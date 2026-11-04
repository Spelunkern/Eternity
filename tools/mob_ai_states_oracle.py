#!/usr/bin/env python3
"""Run original InitLuaFSM, stack dispatch/selection/poll and event handlers.
Only container storage, Lua allocation/calls, tick and diagnostic ports are adapted.
"""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
cpu,digest=load_game()
base=0x30000000
cpu.mem_map(0,0x1000)
cpu.mem_map(base,0x40000)
mob=base; fsm=mob+4; stack=base+0x30000; stop=base+0x3f000
states=[base+0x2000+i*0x100 for i in range(5)]
nodes=[base+0x3000+i*0x100 for i in range(5)]
map_at=base+0x4000; block=base+0x4100; event_at=base+0x5000; missing=base+0x6000
transition_nodes={}; transitions=[]; context={}; allocations=base+0x8000
fields_offsets=[0x284,0x290,0x280,0x130,0x2a0,0x2b4,0x28c]
def put(at,*values): cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[x&0xffffffff for x in values]))
def get(at): return struct.unpack('<I',cpu.mem_read(at,4))[0]
def ret(cleanup=0,value=None):
    sp=cpu.reg_read(UC_X86_REG_ESP)
    cpu.reg_write(UC_X86_REG_EIP,get(sp)); cpu.reg_write(UC_X86_REG_ESP,sp+4+cleanup)
    if value is not None: cpu.reg_write(UC_X86_REG_EAX,value)
def set_stack(values):
    assert len(values)<=16
    put(fsm+0x30,map_at,16,0,len(values))
    for i in range(4): put(map_at+4*i,block+16*i)
    for i,value in enumerate(values): put(block+4*i,states[value])
def get_stack(): return [states.index(get(block+4*i)) for i in range(get(fsm+0x3c))]
def execute(address,registers,args=()):
    put(stack,stop,*args)
    cpu.reg_write(UC_X86_REG_ESP,stack)
    for reg,value in registers: cpu.reg_write(reg,value)
    cpu.emu_start(address,stop,count=20000)
    assert cpu.reg_read(UC_X86_REG_EIP)==stop

def hook(machine,address,size,data):
    global allocations
    sp=machine.reg_read(UC_X86_REG_ESP); eax=machine.reg_read(UC_X86_REG_EAX)
    ecx=machine.reg_read(UC_X86_REG_ECX); esi=machine.reg_read(UC_X86_REG_ESI)
    if address==0x4f2670: ret(value=0) # Lua allocation failure; table still built.
    elif address==0x4b7710:
        identity=get(sp+8); ptr=states[identity]
        put(ptr+0x28,identity); cpu.mem_write(ptr+0x30,bytes(cpu.mem_read(sp+12,48)))
        context['states'].append((identity,get(sp+12),get(sp+28),get(sp+44)))
        put(nodes[identity]+12,ptr); ret(56)
    elif address in (0x4b7820,0x4b7890,0x4b78f0):
        source,ev=get(sp+4),get(sp+8)
        target=get(sp+12) if address!=0x4b7890 else 0
        op={0x4b7820:0,0x4b7890:1,0x4b78f0:2}[address]
        callback=get(sp+(16 if op!=1 else 12))
        transitions.append((source,ev,target,op,callback)); ret(28 if op!=1 else 24)
    elif address==0x4b7c20:
        transitions.append((get(eax+0x28),get(sp+4),0,get(sp+12),get(sp+16))); ret(28)
    elif address==0x493fc0:
        identity=get(ecx); put(eax,nodes[identity] if identity<5 else missing); ret()
    elif address==0x4943c0:
        key=(get(esi+0x28),get(ecx)); put(eax,transition_nodes.get(key,missing)); ret()
    elif address==0x493d40:
        values=get_stack(); values.append(states.index(get(get(sp+4)))); set_stack(values); ret(4)
    elif address==0x493410:
        values=get_stack(); set_stack(values[:-1]); ret()
    elif address==0x493150 and context['phase']=='register':
        context['initial']=get(sp+4); ret(4)
    elif address==stop+16:
        machine.reg_write(UC_X86_REG_EAX,context['tick']); ret()
    elif address in (0x4b1090,0x4b1140):
        name=bytes(cpu.mem_read(machine.reg_read(UC_X86_REG_EBX),32)).split(b'\0')[0].decode()
        context['calls'].append((name,get(sp+8),get(sp+12) if address==0x4b1140 else 0,tuple(get_stack())))
        if context.get('mutate'):
            set_stack(context['mutate']); context['mutate']=None
        ret(8 if address==0x4b1090 else 12)
    elif address==stop+32:
        # Error format identifies original failure branch. Preserve cdecl args.
        context['errors'].append(get(sp+8)); ret()
    elif address in (0x4b52c0,0x4b55c0,0x4b5c40,0x4b5790):
        context['updates'].append(states.index(next(p for p in states if get(p+0x30)==address))); ret()
    elif address==0x492b90:
        context['released']=True; set_stack([]); put(mob+0x44,0); ret()
cpu.hook_add(UC_HOOK_CODE,hook)
context.update(phase='register',states=[],initial=None)
execute(0x4b4c20,[(UC_X86_REG_EAX,mob)])
assert len(transitions)==23 and context['initial']==0
for source,ev,target,op,callback in transitions:
    node=allocations; allocations+=0x100
    transition_nodes[(source,ev)]=node
    put(node+0x10,states[target],op,callback,0,0,0)
    put(states[source]+8,missing)
put(fsm,base+0x7000); put(base+0x7004,stop+32); put(fsm+12,missing); put(fsm+16,5)
put(0x5210e4,stop+16)
print(f'# ps_game.exe SHA256 {digest}; original InitLuaFSM 004b4c20, DispatchEvent 004929b0, STATE::OnDispatch 004938c0, SetInitialState 00493150, Poll 004a8350 and CMob event handlers.')
for item in context['states']: print('S',*item)
for item in transitions: print('R',*item)
context['phase']='execute'
for ptr in states: put(ptr+8,missing)
# Exhaustive state/event combinations, deep stacks, callback mutations and polls.
for case in range(480):
    source=case%5; event=(case//5)%12-2; depth=(case//60)%4+1
    initial=[(source+i+1)%5 for i in range(depth-1)]+[source]
    if case>=420: initial=[0,4,1,3,source]
    if case in (478,479): initial=[]
    mutation=([4] if case%7==0 else [0,4,1,3,0]) if case%3==0 else None
    context.update(tick=(0xfffffff0+case)&0xffffffff,calls=[],errors=[],updates=[],mutate=mutation,released=False)
    set_stack(initial); put(mob+0x284,0 if case%4==0 else 1); put(mob+0x290,9); put(mob+0x280,123); put(mob+0x130,7); put(mob+0x2a0,8); put(mob+0x2b4,99); put(mob+0x28c,0x80000000+case)
    put(event_at,0,event,0x12340000+case)
    execute(0x4929b0,[(UC_X86_REG_ESI,fsm)],(event_at,))
    execute(0x4a8350,[(UC_X86_REG_EDI,fsm)])
    call=context['calls'][0] if context['calls'] else ('-',0,0,())
    encode=lambda values: ','.join(map(str,values)) or '-'
    errors=','.join(hex(x) for x in context['errors']) or '-'
    print('C',case,event,int(case%4!=0),encode(initial),encode(mutation or []),context['tick'],call[0],call[1],call[2],encode(call[3]),encode(get_stack()),errors,encode(context['updates']),*[get(mob+x) for x in fields_offsets])
# Initial-state ownership and empty/death polling edge paths.
for initial in (-1,0,1,2,3,4,5):
    context.update(errors=[],updates=[],released=False)
    set_stack([4,1]); put(mob+0x44,1)
    execute(0x493150,[(UC_X86_REG_ECX,fsm)],(initial,))
    print('I',initial,encode(get_stack()),int(context['released']),','.join(hex(x) for x in context['errors']) or '-')
for values in ([],[2]):
    context.update(errors=[],updates=[])
    set_stack(values)
    execute(0x4a8350,[(UC_X86_REG_EDI,fsm)])
    print('P',encode(values),cpu.reg_read(UC_X86_REG_EAX)&255,','.join(hex(x) for x in context['errors']) or '-')
