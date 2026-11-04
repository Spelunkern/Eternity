#!/usr/bin/env python3
"""Complete original QuestEnd/Select with actual reward/removal owners."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
from mob_quest_oracle import quest_block
cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x40000)
user,head,finished,manager,nodes,definitions=[base+n for n in (0,0x6000,0x6100,0x7000,0x9000,0x10000)]
stack,stop=base+0x3e000,base+0x3f000;state={}
def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
for at,n in ((0x458240,12),(0x47e6f0,20),(0x47f8e0,16),(0x4d4f10,8),(0x4d4d20,8),(0x4d4e50,8),(0x45b0d0,0),(0x505316,0),(stop+16,4),(stop+32,0)):
    cpu.mem_write(at,b'\xc2'+struct.pack('<H',n))
put(0x5210cc,stop+16);put(0x5210d0,stop+16);put(0x5210e4,stop+32);put(0x546f3c,manager)
def event(c,*values):state['events']+=c+struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values])
def hook(machine,at,size,data):
    sp=machine.reg_read(UC_X86_REG_ESP)
    if at==stop+32:
        event(b'T');machine.reg_write(UC_X86_REG_EAX,state['now'])
        if state['mode']==2:put(nodes+0x20,definitions+464);put(nodes+0x10,0)
    elif at==0x458240:event(b'X',get(sp+4),get(sp+8),get(sp+12))
    elif at==0x47e6f0:
        event(b'I',get(sp+4),machine.reg_read(UC_X86_REG_ECX),get(sp+8));machine.reg_write(UC_X86_REG_EAX,0)
    elif at==0x47f8e0:
        event(b'Q',get(sp+16));assert get(sp+12)==nodes+0x1c
        if state['mode']==5:put(nodes+0x20,definitions+464);cpu.mem_write(nodes+0x1c,b'\x07\x08\x09')
        machine.reg_write(UC_X86_REG_EAX,state['query'])
    elif at==0x47e150:pass  # real owner executes, transport/cursor state is observed
    elif at==0x45b0d0:
        event(b'M');p=machine.reg_read(UC_X86_REG_ESI);cpu.mem_write(p+2,bytes(72))
        if state['mode']==4:put(nodes+8,9);put(nodes+0x20,definitions+464)
    elif at in (0x4d4f10,0x4d4d20,0x4d4e50):
        n=get(sp+8);event({0x4d4f10:b'B',0x4d4d20:b'C',0x4d4e50:b'G'}[at],n)
        packet=bytearray(machine.mem_read(get(sp+4),n))
        if at==0x4d4e50:
            # Original strcpy/empty-string writes leave stack tails undefined;
            # QuestFind's own stack frames can populate them before the reward.
            for begin,end in ((76,108),(118,150)):
                nul=packet.find(0,begin,end)
                if nul>=0:packet[nul:end]=bytes(end-nul)
        state['events']+=packet
        if at==0x4d4f10 and state['mode']==1:
            put(user+0x11f8,nodes+64);event(b'J',2)
        if at==0x4d4d20 and state['mode']==3:
            put(nodes+0x20,definitions+464);cpu.mem_write(nodes+0xc,b'\x02')
cpu.hook_add(UC_HOOK_CODE,hook)
def block(records):
    out=struct.pack('<I',len(records))
    for d in records:
        name=bytes(d[2:258]).split(b'\0')[0];out+=bytes(d[:2])+struct.pack('<I',len(name))+name+bytes(4)+quest_block([d])[14:]
    return out
h=lambda b:bytes(b).hex() or '-'
print('# ps_game.exe SHA256',digest,'complete QuestEnd 0047ec40/QuestEndSelect 0047ebe0 with real QuestSuccessResult 0047ee40, QuestFind 0047e4b0, QuestRemove 0047e150, FreeQuest 0040a820. Ports clock/locks/transport/log header, experience, item receipt and completion inventory query; no reward/remove stubs. Undefined game-log string tails after their first NUL are normalized to zero; initialized fields compare exactly.')
print('# kind case now mode query context success selection lookup_id definition_block start counters pending events final_money final_id final_binding final_start final_counters final_success active finished free cursor')
for kind in ('E','S'):
 for case in range(768):
    cpu.mem_write(base,bytes(0x3f000));now=(0,1,60000,0x80000000,0xffffffff)[case%5];mode=(case//5)%6;query=(case//7)%2
    context=(0,777,0xffffffff)[(case//11)%3];success=case%9!=0;selection=(case//3)%4;lookup_id=99 if case%17==0 else 1
    put(user+0x140,17);put(user+0x578c,7);put(user+0x11f4,head,head);put(user+0x11c8,finished,finished);put(user+0x1214,2)
    put(head,nodes+64,nodes);put(finished,finished,finished)
    records=[]
    for i in range(2):
        d=bytearray(464);struct.pack_into('<H',d,0,i+1);d[2:4]=bytes((65+i,0))
        minimum=(0,1,0x80000000,0xffffffff)[(case//13)%4];struct.pack_into('<I',d,0x124,minimum)
        struct.pack_into('<I',d,0x134,case%2);d[0x147]=(case//19)%4;d[0x148]=(case//23)%2
        d[0x15c]=(case//29)%3;d[0x15d]=(case//31)%4
        for v in range(3):
            at=0x160+32*v;struct.pack_into('<H',d,at,(case+v)%4);d[at+2]=(case//37+v)%5;d[at+3]=(case+v+i)%5;d[at+4]=(case//41+v)%5
            struct.pack_into('<II',d,at+20,v+1+i,v+2+i)
        if i: d[0x15c]=1;d[0x15d]=0
        records.append(d);cpu.mem_write(definitions+i*464,bytes(d))
    minimum=get(definitions+0x124);start=(now-minimum*60000+(-1,0,1)[case%3])&0xffffffff
    counters=bytes((0,1,3,255)[(case//43+s)%4] for s in range(3));pending=(case//47)%3
    for i in range(2):put(nodes+i*64,head if i==0 else nodes,nodes+64 if i==0 else head,i+1);put(nodes+i*64+0x20,definitions)
    put(nodes+0x10,start);cpu.mem_write(nodes+0x1c,counters);cpu.mem_write(nodes+0xc,bytes((pending,)))
    state.update(now=now,mode=mode,query=query,events=b'')
    put(stack,stop,*((context,int(success)) if kind=='E' else ()))
    for reg,value in ((UC_X86_REG_ESP,stack),(UC_X86_REG_EAX,lookup_id),(UC_X86_REG_ECX,user if kind=='E' else selection),(UC_X86_REG_EDX,nodes if kind=='E' else user),(UC_X86_REG_EDI,user)):
        cpu.reg_write(reg,value)
    cpu.emu_start(0x47ec40 if kind=='E' else 0x47ebe0,stop,count=1000000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
    def seq(head):
        r=[];p=get(head+4)
        while p!=head:r.append((p-nodes)//64+1);p=get(p+4);assert len(r)<5
        return ','.join(map(str,r)) or '-'
    free=[];p=get(manager+0xf0)
    while p:free.append((p-nodes)//64+1);p=get(p+4);assert len(free)<5
    cursor=get(user+0x11f8);cursor=0 if cursor==head else (cursor-nodes)//64+1
    print(kind,case,now,mode,query,context,int(success),selection,lookup_id,h(block(records)),start,h(counters),pending,h(state['events']),get(user+0x140),get(nodes+8),(get(nodes+0x20)-definitions)//464,get(nodes+0x10),h(cpu.mem_read(nodes+0x1c,3)),cpu.mem_read(nodes+0xc,1)[0],seq(head),seq(finished),','.join(map(str,free)) or '-',cursor)
