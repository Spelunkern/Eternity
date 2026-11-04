#!/usr/bin/env python3
"""Execute original UpdateQuest plus real failing QuestEnd/QuestRemove/FreeQuest."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game
from mob_quest_oracle import quest_block
cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x40000)
user,head,finished,manager,nodes,definitions=[base+n for n in (0,0x6000,0x6100,0x7000,0x9000,0x10000)]
stack,stop=base+0x3e000,base+0x3f000
state={}
def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
for at,n in ((0x4d4f10,8),(0x4d4d20,8),(0x4d4e50,8),(0x45b0d0,0),(0x505316,0),(stop+16,4),(stop+32,4)):
    cpu.mem_write(at,b'\xc2'+struct.pack('<H',n))
put(0x5210cc,stop+16);put(0x5210d0,stop+32);put(0x546f3c,manager)
def sequence():
    r=[];p=get(head+4)
    while p!=head:
        assert p and len(r)<8;r.append((p-nodes)//64);p=get(p+4)
    return r
def event(tag,*values):state['events']+=tag+struct.pack('<'+'I'*len(values),*values)
def external_unlink(i):
    p=nodes+i*64;previous,following=get(p),get(p+4)
    if get(user+0x11f8)==p:put(user+0x11f8,previous)
    put(previous+4,following);put(following,previous);put(p,0,0)
    put(user+0x1214,get(user+0x1214)-1);state['external'].add(i)
def hook(machine,at,size,data):
    sp=machine.reg_read(UC_X86_REG_ESP)
    if at in (stop+16,stop+32):
        lock=get(sp+4);kind=0 if lock==user+0x11fc else 1 if lock==user+0x11d0 else 2
        state['events']+=(b'L' if at==stop+16 else b'U')+bytes((kind,))
    elif at==0x47ec40:
        p=machine.reg_read(UC_X86_REG_EDX);state['current']=(p-nodes)//64
        event(b'E',get(p+8),get(sp+4),get(sp+8))
    elif at==0x47e150:event(b'R',get(machine.reg_read(UC_X86_REG_EAX)+8),get(sp+4))
    elif at==0x40a820:event(b'F',get(machine.reg_read(UC_X86_REG_EDI)+8))
    elif at in (0x4d4f10,0x4d4d20):
        p=bytes(machine.mem_read(get(sp+4),get(sp+8)))
        state['events']+=(b'B' if at==0x4d4f10 else b'C')+p
        active=sequence();mode=state['mode'];current=state['current']
        if at==0x4d4d20 and mode==1:
            future=next((i for i in active if i>current),None)
            if future is not None:
                put(nodes+future*64+0x14,0);put(nodes+future*64+0x20,definitions+(future+6)*464);event(b'M',future+1)
        if at==0x4d4d20 and mode==4 and active and active[-1]!=current:
            i=active[-1];external_unlink(i);event(b'X',i+1)
        if at==0x4d4f10 and mode in (2,3):
            cursor=active[0 if mode==2 else -1] if active else -1
            put(user+0x11f8,head if cursor<0 else nodes+cursor*64);event(b'J',cursor+1)
    elif at==0x4d4e50:
        p=bytes(machine.mem_read(get(sp+4),get(sp+8)))
        assert len(p)==150
        state['events']+=b'G'+p[:2]+p[74:76]
cpu.hook_add(UC_HOOK_CODE,hook)
h=lambda b:bytes(b).hex() or '-'
print('# ps_game.exe SHA256',digest,'whole CUser::UpdateQuest 0048d1a0 -> real QuestEnd failure 0047ec40 -> real QuestRemove 0047e150 and FreeQuest 0040a820. Ports locks, transports, SetGameLogMain and cookie check; G captures initialized log opcode/quest ID, not uninitialized log-main bytes. M changes future definition/deadline; X externally unlinks future quest; J changes shared cursor at DB callback.')
print('# case now mode initial_count definition_block initial_deadlines expected_active expected_finished expected_free expected_cursor final_deadlines final_definitions events')
for case in range(512):
    cpu.mem_write(base,bytes(0x30000));now=(0,1,999,1000,0x7fffffff,0x80000000,0xfffffffe,0xffffffff)[case%8]
    count=(case//8)%7;mode=(case//56)%5;state.update(events=b'',mode=mode,current=0,external=set())
    put(user+0x578c,7);put(user+0x11f4,head,head);put(user+0x11c8,finished,finished)
    put(user+0x1214,count);put(head,nodes+(count-1)*64 if count else head,nodes if count else head);put(finished,finished,finished)
    records=[];initial=b''
    for i in range(12):
        d=bytearray(464);struct.pack_into('<H',d,0,i+1)
        struct.pack_into('<I',d,0x128,1 if i>=6 else (0,1,0x80000000)[(case//3+i)%3])
        struct.pack_into('<I',d,0x134,(case+i)%2)
        d[0x147]=(case+i)%4;records.append(d);cpu.mem_write(definitions+i*464,bytes(d))
        if i<6:
            p=nodes+i*64;put(p,nodes+(i-1)*64 if i else head,nodes+(i+1)*64 if i+1<count else head,i+1)
            if i>=count:put(p,0,0,i+1)
            deadline=(0,now,(now-1)&0xffffffff,(now+1)&0xffffffff,0x80000000,0xffffffff)[(case//5+i)%6]
            put(p+0x14,deadline);put(p+0x20,definitions+i*464);initial+=struct.pack('<I',deadline)
    put(user+0x57c4,sum(records[i][0x147]==3 for i in range(count)));put(user+0x57c8,sum(records[i][0x147]==2 for i in range(count)))
    put(stack,stop);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ESI,user);cpu.reg_write(UC_X86_REG_EBX,now)
    cpu.emu_start(0x48d1a0,stop,count=1000000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
    def seq(other):
        r=[];p=get(other+4)
        while p!=other:r.append((p-nodes)//64+1);p=get(p+4);assert len(r)<8
        return ','.join(map(str,r)) or '-'
    free=[];p=get(manager+0xf0)
    while p:free.append((p-nodes)//64+1);p=get(p+4);assert len(free)<8
    cursor=get(user+0x11f8);cursor_index=0 if cursor==head else (cursor-nodes)//64+1
    print(case,now,mode,count,h(quest_block(records)),h(initial),seq(head),seq(finished),','.join(map(str,free)) or '-',cursor_index,h(b''.join(bytes(cpu.mem_read(nodes+i*64+0x14,4)) for i in range(6))),h(bytes((get(nodes+i*64+0x20)-definitions)//464 for i in range(6))),h(state['events']))
