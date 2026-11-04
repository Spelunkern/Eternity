#!/usr/bin/env python3
"""Original QuestEndRequest/check execution with observed QuestEnd provider."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EDX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
from mob_quest_oracle import quest_block
cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x40000)
user,node,definitions,npc= [base+x for x in (0,0x6000,0x7000,0x9000)]
stack,stop=base+0x3e000,base+0x3f000
state={}
def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*v))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
for at,n in ((0x47e4b0,4),(0x416d10,4),(0x47ec40,8),(0x47f8e0,16)):
    cpu.mem_write(at,b'\xc2'+struct.pack('<H',n))
def hook(machine,at,size,data):
    sp=machine.reg_read(UC_X86_REG_ESP)
    if at==0x47e4b0:machine.reg_write(UC_X86_REG_EAX,node if state['active'] else 0)
    elif at==0x416d10:
        state['lookups']+=1;machine.reg_write(UC_X86_REG_EAX,npc if state['found'] else 0)
    elif at==0x47f8e0:
        state['consume']=get(sp+16);machine.reg_write(UC_X86_REG_EAX,state['items'])
    elif at==0x47ec40:
        assert machine.reg_read(UC_X86_REG_EDX)==node
        state['end']=(get(sp+4),get(sp+8))
cpu.hook_add(UC_HOOK_CODE,hook)
print('# ps_game.exe',digest,'QuestEndRequest 0047f2a0 and real checks 0047f630/670/6e0/740. Ports QuestFind, FindNpc, QuestFindItem and QuestEnd.')
print('# id count npc active found npc_type npc_id items definition_block counters lookups consume completion_selection completion_success')
for case in range(1024):
    cpu.mem_write(base,bytes(0x20000))
    d=bytearray(464);struct.pack_into('<H',d,0,1)
    d[0x147]=case%6;d[0x148]=(case//6)%2;struct.pack_into('<H',d,0x14a,12)
    d[0x155]=2;d[0x15a]=2;d[0x15b]=3;d[0x15d]=(case//12)%2
    counters=bytes(((case//24)%4,(case//96)%5,0))
    ident=(0,1,1,1,2,65535)[(case//160)%6];count=1
    requested=(0,0xabcdef01)[(case//3)%2]
    state.update(active=(case//7)%4!=0,found=(case//11)%4!=0,items=(case//13)%2,lookups=0,consume=-1,end=(-1,0))
    d[0x14c]=1;d[0x14d]=1;d[0x14e]=1 if state['items'] else 3
    nt=d[0x148] if case%7 else 99;ni=12 if case%9 else 13
    put(npc+0x34,nt,ni);put(node+0x20,definitions);cpu.mem_write(node+0x1c,counters)
    put(0x3ed35c8,count,definitions);cpu.mem_write(definitions,bytes(d))
    put(stack,stop,user,requested);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_EAX,ident)
    cpu.emu_start(0x47f2a0,stop,count=100000)
    assert cpu.reg_read(UC_X86_REG_EIP)==stop
    print(ident,count,requested,int(state['active']),int(state['found']),nt,ni,state['items'],quest_block([d]).hex(),counters.hex(),state['lookups'],state['consume'],*state['end'])
