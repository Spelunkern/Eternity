#!/usr/bin/env python3
"""Execute complete original player-kill/item-pick quest progress routines."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EDI, UC_X86_REG_EDX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game
from mob_quest_oracle import quest_block
cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x40000)
user,head,nodes,definitions,ids=[base+n for n in (0,0x6000,0x7000,0x10000,0x20000)]
stack,stop=base+0x3e000,base+0x3f000
state={}
def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
for at,n in ((0x4d4f10,8),(0x4d4d20,8),(0x47ec40,8),(0x47f8e0,16),(stop+16,4),(stop+32,4),(stop+48,0)):
    cpu.mem_write(at,b'\xc2'+struct.pack('<H',n))
put(0x5210cc,stop+16);put(0x5210d0,stop+32);put(0x5210e4,stop+48)
def unlink(node):
    i=(node-nodes)//64
    if state['active'][i]:
        previous,following=get(node),get(node+4)
        put(previous+4,following);put(following,previous)
        if get(user+0x11f8)==node:put(user+0x11f8,previous)
        put(node,0,0);state['active'][i]=0

def hook(machine,at,size,data):
    sp=machine.reg_read(UC_X86_REG_ESP)
    if at in (stop+16,stop+32):state['events']+=b'L' if at==stop+16 else b'U'
    elif at==stop+48:
        state['events']+=b'T';machine.reg_write(UC_X86_REG_EAX,state['now'])
    elif at in (0x4d4f10,0x4d4d20):
        p=bytes(machine.mem_read(get(sp+4),get(sp+8)))
        state['events']+=(b'B' if at==0x4d4f10 else b'C')+p
        i=int.from_bytes(p[6:8] if at==0x4d4f10 else p[2:4],'little')-1
        node=nodes+i*64
        if at==0x4d4f10 and state['mode']==1:
            slot=p[8];cpu.mem_write(node+0x1c+slot,bytes(((p[9]+3)&255,)))
        if at==0x4d4d20 and state['mode']==2:put(node+0x20,definitions+(i+4)*464)
    elif at==0x47f8e0:
        node=get(sp+8);i=(node-nodes)//64
        assert get(sp+12)==node+0x1c
        state['events']+=b'I'+struct.pack('<II',i+1,get(sp+16))
        if state['mode']==3:cpu.mem_write(node+0x1c,bytes((2,3,4)))
        machine.reg_write(UC_X86_REG_EAX,state['query'])
    elif at==0x47ec40:
        node=machine.reg_read(UC_X86_REG_EDX);i=(node-nodes)//64
        state['events']+=b'E'+struct.pack('<III',i+1,get(sp+4),get(sp+8))
        if state['remove']:unlink(node)
        if state['mode']==4:
            put(node+0x20,definitions+(i+4)*464);cpu.mem_write(node+0x1e,b'\xfe')
cpu.hook_add(UC_HOOK_CODE,hook)
rng=random.Random(0x480020)
h=lambda b:bytes(b).hex() or '-'
print('# ps_game.exe SHA256',digest,'complete QuestUpdateKillPC 00480020 and QuestUpdatePickItem 00480390; real QuestFind 0047e4b0 and item completion check 0047f670; observed locks/clock/transport/inventory query/QuestEnd. Callbacks mutate DB-visible count, client-visible definition, query counters, and retained completion payload; optional unlink adjusts live cursor.')
print('# kind scenario have type id now mode query remove links definition_block initial final cursor events')
for kind in ('P','I'):
 for case in range(512):
    cpu.mem_write(base,bytes(0x30000))
    have=(-1,0,1,4)[case%4]
    typ=(-1,0,1,2,255,256)[case%6] if case%5==0 else 1
    ident=(-1,0,1,2,255,256)[(case//5)%6] if case%7==0 else 2
    now=(0,999,1000,0xffffffff)[(case//7)%4]
    state.update(events=b'',active=[1]*4,now=now,mode=(case//4)%5,query=(case//9)%2,remove=(case//11)%2)
    put(user+0x578c,7);put(user+0x57c4,have);put(user+0x11f4,head,head)
    put(head,nodes+3*64,nodes)
    linked=[1,2,1,999,3,4] if case%13 else []
    if 0<typ<256 and 0<ident<256:
        link=0x3ed35d0+(typ*256+ident)*16
        put(link+8,len(linked),ids)
        if linked:cpu.mem_write(ids,struct.pack('<'+'H'*len(linked),*linked))
    records=[];initial=b''
    for i in range(8):
        d=bytearray(464);struct.pack_into('<H',d,0,i+1)
        d[0x147]=(3,0,3,1)[(i+case//17)%4] if kind=='P' else (1,1,0,3)[(i+case//17)%4]
        d[0x148]=(case//19+i)%2
        struct.pack_into('<I',d,0x124,(case//23+i)%2)
        d[0x155]=(0,1,254,255)[(case//29+i)%4]
        d[0x15d]=(case//31+i)%2
        for s in range(3):d[0x14c+3*s:0x14f+3*s]=bytes((1,2 if (case+i+s)%3 else 3,1))
        if i>=4:
            d[0x148]=0;d[0x14f]=2;d[0x152]=1;d[0x153]=2
        records.append(d);cpu.mem_write(definitions+i*464,bytes(d))
        if i<4:
            node=nodes+i*64;put(node,nodes+(i-1)*64 if i else head,nodes+(i+1)*64 if i<3 else head,i+1)
            success=(0,999,1000,0xffffffff)[(case//37+i)%4]
            counters=bytes((0,1,254,255)[(case//41+i+s)%4] for s in range(3))
            put(node+0x18,success);cpu.mem_write(node+0x1c,counters);put(node+0x20,definitions+i*464)
            initial+=struct.pack('<I',success)+counters
    put(stack,stop,*((typ,ident) if kind=='I' else ()))
    cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_EAX,user);cpu.reg_write(UC_X86_REG_EDI,user)
    cpu.emu_start(0x480020 if kind=='P' else 0x480390,stop,count=1000000)
    assert cpu.reg_read(UC_X86_REG_EIP)==stop
    final=b''.join(bytes((state['active'][i],(get(nodes+i*64+0x20)-definitions)//464))+bytes(cpu.mem_read(nodes+i*64+0x18,7)) for i in range(4))
    cursor=get(user+0x11f8);cursor_index=-1 if cursor==head else (cursor-nodes)//64
    print(kind,case,have,typ,ident,now,state['mode'],state['query'],state['remove'],','.join(map(str,linked)) or '-',h(quest_block(records)),h(initial),h(final),cursor_index,h(state['events']))
