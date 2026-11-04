#!/usr/bin/env python3
"""Execute original ps_game.exe quest item requirement routines (Unicorn)."""
import struct
import random
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game

cpu,digest=load_game()
base=0x30000000
cpu.mem_map(base,0x40000)
user,node,definition,out,items,info= [base+x for x in (0,0x6000,0x7000,0x8000,0x9000,0x12000)]
out=node+0x1c  # Both original call sites alias output to the live quest counters.
stack,stop=base+0x3e000,base+0x3f000
state={}
def put(at,*values):cpu.mem_write(at,struct.pack('<'+'I'*len(values),*values))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
for at,cleanup in ((0x4d4f10,8),(0x4d4d20,8),(0x40a590,0),(stop+16,4)):
    cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup))
put(0x5210cc,stop+16);put(0x5210d0,stop+16)
def hook(machine,address,size,data):
    if address in (0x4d4f10,0x4d4d20):
        sp=machine.reg_read(UC_X86_REG_ESP)
        state['events']+=(b'B' if address==0x4d4f10 else b'C')+bytes(machine.mem_read(get(sp+4),get(sp+8)))
cpu.hook_add(UC_HOOK_CODE,hook)
def call(address,*args):
    put(stack,stop,*args)
    cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_EAX,user)
    cpu.emu_start(address,stop,count=1000000)
    assert cpu.reg_read(UC_X86_REG_EIP)==stop
    return cpu.reg_read(UC_X86_REG_EAX)&255
rng=random.Random(745)
print('# ps_game.exe SHA256',digest,'QuestFindItem 0047f790/0047f8e0; original instructions; ports transport, locks and FreeItem.')
print('# mode bags consume type id definition inventory counters result output_counters output_inventory packet_events')
for case in range(768):
    cpu.mem_write(base,bytes(0x30000));state['events']=b''
    bags=case%4
    put(user+0x1304,bags);put(user+0x578c,0x12345678);put(node+0x20,definition)
    put(0x546f3c,base+0x20000)
    d=bytearray(464);d[0x147]=1;d[0x15c]=case%3;d[0x15d]=(case//3)%2
    for i in range(3):d[0x14c+3*i:0x14f+3*i]=bytes((1,(i+case//9)%3+1,rng.randrange(8)))
    # Exercise reward consumption alternatives, including descending selection.
    for i in range(3):d[0x163+32*i:0x165+32*i]=bytes((rng.randrange(4),rng.randrange(8)))
    cpu.mem_write(definition,bytes(d))
    inv=bytearray()
    for i in range(bags*24):
        present=rng.randrange(3)==0
        record=bytes((int(present),1,rng.randrange(1,5),rng.choice((0,1,2,7,255)),rng.choice((0,0,27))))
        inv+=record
        if present:
            at=items+i*128;put(user+0x224+4*i,at);put(at+0x30,info+i*256);put(info+i*256+0xa0,record[4]);cpu.mem_write(at+0x40,record[1:4])
    counters=bytes(rng.choice((0,1,7,255)) for _ in range(3));cpu.mem_write(out,counters)
    mode=case%3;consume=(case//4)%2;typ=1;ident=case%5
    result=call(0x47f790,typ,ident,consume) if mode==0 else call(0x47f8e0,user,node,out,consume)
    final=bytearray()
    for i in range(bags*24):
        at=get(user+0x224+4*i)
        final+=bytes((1,))+bytes(cpu.mem_read(at+0x40,3))+bytes((inv[i*5+4],)) if at else bytes(5)
    # Absent input slots have no original fields.
    for i in range(bags*24):
        if not inv[i*5]:inv[i*5:i*5+5]=bytes(5)
    h=lambda b:bytes(b).hex() or '-'
    print(mode,bags,consume,typ,ident,h(d),h(inv),h(counters),result,h(cpu.mem_read(out,3)),h(final),h(state['events']))
