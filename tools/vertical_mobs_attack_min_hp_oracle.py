#!/usr/bin/env python3
"""Trace FindAttackUserMinHP using real original ledger traversal and gates."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EIP,UC_X86_REG_ESP
from original_oracle import load_game
cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x100000)
mob,zone,definition,head,nodes,users,tree_nodes,stack,stop=(base+x for x in (0,0x2000,0x4000,0x5000,0x6000,0x10000,0x60000,0x80000,0x90000))
def put(a,*v):cpu.mem_write(a,struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v)))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
lookup={}
def hook(machine,address,size,context):
    if address==0x410320:
        key=get(machine.reg_read(UC_X86_REG_EBX));out=machine.reg_read(UC_X86_REG_EAX);put(out,lookup.get(key,head+0x100));machine.reg_write(UC_X86_REG_EAX,out)
cpu.mem_write(stop+16,b'\xc2\x04\x00');put(0x5210cc,stop+16);put(0x5210d0,stop+16);cpu.mem_write(0x410320,b'\xc3');cpu.hook_add(UC_HOOK_CODE,hook,begin=0x410320,end=0x410320)
print('# ps_game.exe SHA256 '+digest+'; CMob::FindAttackUserMinHP 004a0810; locks/tree lookup intercepted')
print('# case | selected_id')
cases=[[],[(1,1,10,1,0,0,1,1)],[(2,1,1,1,0,0,1,1),(1,2,30,1,0,0,1,1),(1,3,10,1,0,0,1,1)],[(1,1,10,1,0,0,1,1),(1,2,10,1,0,0,1,1)],[(1,1,1,2,0,0,1,1),(1,2,2,1,1,0,1,1),(1,3,3,1,0,1,1,1),(1,4,4,1,0,0,0,1),(1,5,5,1,0,0,1,0),(1,6,6,1,0,0,1,1)],[(1,1,10,1,0,0,1,1),(1,2,0xffffffff,1,0,0,1,1)],[(1,1,10,1,0,0,1,1),(1,9,0,0,0,0,1,1)]]
for index,rows in enumerate(cases):
    cpu.mem_write(base,bytes(0x80000));put(mob+0xa8,definition);put(mob+0x60,zone);put(definition+0x78,2);put(mob+0x1f4,head,head);put(zone+0x7c,head+0x100);lookup.clear();put(head,head,head)
    for i,(kind,uid,hp,country,life,immune,attackable,visible) in enumerate(rows):
        node=nodes+i*32;prev=head if i==0 else nodes+(i-1)*32;next_=head if i+1==len(rows) else nodes+(i+1)*32;put(node,prev,next_);cpu.mem_write(node+8,bytes([kind]));put(node+12,uid)
        if country:
            actor=users+i*0x6000;tnode=tree_nodes+i*32;put(tnode+16,actor);lookup[uid]=tnode;put(actor+0x1238,hp);put(actor+0x142c,life)
            for offset,value in ((0x12d,country),(0x1366,immune),(0x5775,attackable),(0x5774,visible)):cpu.mem_write(actor+offset,bytes([value]))
    if rows:put(head, nodes+(len(rows)-1)*32,nodes)
    put(stack,stop,mob);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.emu_start(0x4a0810,stop,count=100000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('minimum HP selector did not return')
    chosen=cpu.reg_read(UC_X86_REG_EAX);selected=next((row[1] for i,row in enumerate(rows) if chosen==users+i*0x6000),0)
    print(index,selected,sep='\t')
