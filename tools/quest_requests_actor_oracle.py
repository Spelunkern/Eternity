#!/usr/bin/env python3
"""Original Actor quest chains with real item checks, completion and removal."""
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
for at,n in ((0x458240,12),(0x47e6f0,20),(0x416d10,4),(0x40a590,0),(0x4d4f10,8),(0x4d4d20,8),(0x4d4e50,8),(0x45b0d0,0),(0x505316,0),(stop+16,4),(stop+32,0)):
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
    elif at==0x416d10:
        event(b'N',get(sp+4));machine.reg_write(UC_X86_REG_EAX,base+0x8000 if state['found'] else 0)
    elif at in (0x40a590,0x47fccd):
        item=machine.reg_read(UC_X86_REG_EDI);slot=(item-(base+0x12000))//128
        event(b'F',slot//24+1,slot%24,machine.mem_read(item+0x42,1)[0])
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
print('# ps_game.exe SHA256',digest,'real EndRequest/EndSelect/UpdateQuest/PC kill/item pick -> actual QuestFindItem -> QuestEnd -> QuestSuccessResult -> QuestRemove/FreeQuest. Ports transports/clock/NPC/experience/reward item receipt/log-main/locks; F observes inventory release count. Undefined log name tails normalized after NUL.')
print('# kind case now context selection found bags definition_block start pending counters inventory events final_money final_counters final_success active finished free cursor final_inventory')
for case in range(384):
    cpu.mem_write(base,bytes(0x3f000));now=(0,60000,0xffffffff)[case%3];kind=('R','S','F','T','P','K')[case%6];context=777 if case%4 else 0;selection=(case//6)%3
    bags=1+(case//3)%3;found=case%7!=0
    put(user+0x140,17);put(user+0x578c,7);put(user+0x11f4,head,head);put(user+0x11c8,finished,finished);put(user+0x1214,2);put(user+0x1304,bags)
    put(head,nodes+64,nodes);put(finished,finished,finished)
    records=[]
    for i in range(2):
        d=bytearray(464);struct.pack_into('<H',d,0,i+1);d[2:4]=bytes((65+i,0));struct.pack_into('<I',d,0x134,(case//8)%2)
        d[0x147]=3 if kind=='K' else 1;d[0x148]=(case//12)%2 if kind in ('R','S','K') else 0;struct.pack_into('<H',d,0x14a,12)
        d[0x155]=1;d[0x15c]=1 if case%5 else 0;d[0x15d]=2 if kind=='S' or (kind=='R' and case%11==0) else 0
        for v in range(3):
            d[0x14c+v*3:0x14f+v*3]=bytes((1,v+1,1+(case//17)%3));d[0x163+32*v]=v+1;d[0x164+32*v]=1
            struct.pack_into('<II',d,0x174+32*v,v+1,v+2)
        if kind=='T':struct.pack_into('<I',d,0x128,1 if i==0 else 0)
        if kind=='R' and case%13==0:struct.pack_into('<I',d,0x124,1)
        if i: d[0x147]=0;d[0x148]=1
        records.append(d);cpu.mem_write(definitions+i*464,bytes(d))
    start=now;pending=1 if kind=='S' and case%5 else 0;counters=bytes((0,1,0))
    for i in range(2):
        put(nodes+i*64,head if i==0 else nodes,nodes+64 if i==0 else head,i+1);put(nodes+i*64+0x20,definitions+i*464)
    put(nodes+0x10,start);put(nodes+0x14,now);cpu.mem_write(nodes+0x1c,counters);cpu.mem_write(nodes+0xc,bytes((pending,)))
    put(user+0x57c4,1 if kind=='K' else 0)
    put(base+0x8000+0x34,records[0][0x148],12)
    link=0x3ed35d0+(256+2)*16;put(link+8,3,base+0x21000);cpu.mem_write(base+0x21000,struct.pack('<3H',1,1,2))
    put(0x3ed35c8,2,definitions)
    inventory=bytearray()
    for slot in range(bags*24):
        present=slot<8 or (slot+case)%13==0
        record=bytes((1,1,slot%4+1,(1,2,7,255)[(slot+case//9)%4],27 if (slot+case)%5==0 else 0)) if present else bytes(5)
        inventory+=record
        if present:
            item=base+0x12000+slot*128;info=base+0x25000+slot*256
            put(user+0x224+slot*4,item);put(item+0x30,info);put(info+0xa0,record[4]);cpu.mem_write(item+0x40,record[1:4])
    state.update(now=now,mode=1 if case%19==0 else 0,query=0,events=b'',found=found)
    args=(user,context) if kind=='R' else (context,0) if kind=='F' else (1,2) if kind=='P' else ()
    put(stack,stop,*args)
    for reg,value in ((UC_X86_REG_ESP,stack),(UC_X86_REG_EAX,1 if kind in ('R','S') else user),(UC_X86_REG_ECX,selection if kind=='S' else user),(UC_X86_REG_EDX,user if kind=='S' else nodes),(UC_X86_REG_EDI,user),(UC_X86_REG_ESI,user),(UC_X86_REG_EBX,now)):
        cpu.reg_write(reg,value)
    address={'R':0x47f2a0,'S':0x47ebe0,'F':0x47ec40,'T':0x48d1a0,'P':0x480390,'K':0x480020}[kind]
    cpu.emu_start(address,stop,count=1000000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
    def seq(head):
        r=[];p=get(head+4)
        while p!=head:r.append((p-nodes)//64+1);p=get(p+4);assert len(r)<5
        return ','.join(map(str,r)) or '-'
    free=[];p=get(manager+0xf0)
    while p:free.append((p-nodes)//64+1);p=get(p+4);assert len(free)<5
    cursor=get(user+0x11f8);cursor=0 if cursor==head else (cursor-nodes)//64+1
    final=bytearray()
    for slot in range(bags*24):
        item=get(user+0x224+slot*4)
        final+=bytes((1,))+bytes(cpu.mem_read(item+0x40,3))+bytes((inventory[slot*5+4],)) if item else bytes(5)
    print(kind,case,now,context,selection,int(found),bags,h(block(records)),start,pending,h(counters),h(inventory),h(state['events']),get(user+0x140),h(cpu.mem_read(nodes+0x1c,3)),cpu.mem_read(nodes+0xc,1)[0],seq(head),seq(finished),','.join(map(str,free)) or '-',cursor,h(final))
