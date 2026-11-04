#!/usr/bin/env python3
"""Complete request/check/add/start with real quest lists and callback mutations.
Only NPC/requirement, allocation, clocks/locks and transports are ports.
SetGameLogMain executes after its output storage is explicitly initialized.
"""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
from mob_quest_oracle import quest_block


def main():
    cpu,digest=load_game();stack,user,record,npc,nodes,quest,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000,0x10000000
    items=0x35000000
    for p in (stack,user,record,npc,nodes,quest,items,stop):cpu.mem_map(p,0x10000)
    def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
    def word(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
    def short(p):return struct.unpack('<H',cpu.mem_read(p,2))[0]
    events=bytearray();state={}
    def event(c,*v):events.extend(c.encode()+struct.pack('<'+'I'*len(v),*v))
    def append_node(root,p,ident,definition,success=0):
        previous=word(root);put(p,previous,root,ident,success);put(p+0x20,definition);put(previous+4,p);put(root,p)
    def counts():return struct.pack('<III',word(user+0x1214),word(user+0x57c8),word(user+0x57c4))
    def hook(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x416d10:
            event('N',word(sp+4));profile=state['profile']
            if profile==0:cpu.mem_write(user+0x136,b'\0\0')
            elif profile==1:append_node(nodes,nodes+0x400,9,record+464,1);put(user+0x11e8,1)
            elif profile==4:
                append_node(nodes+0x100,nodes+0x500,7,record);put(user+0x1214,1)
                if state['end']==2:put(user+0x57c8,1)
                elif state['end']==3:put(user+0x57c4,1)
            machine.reg_write(UC_X86_REG_EAX,0 if profile==5 else npc)
        elif address==0x47f790:
            event('I',word(sp+4),word(sp+8),word(sp+12));found=state['profile']!=8
            if found and word(sp+12):state['inventory']-=1
            machine.reg_write(UC_X86_REG_EAX,int(found))
        elif address==0x40b0b0:machine.reg_write(UC_X86_REG_EAX,quest if state['allocation'] else 0)
        elif address==0x40a510:
            pointer=items+state['items']*0x100;state['items']+=1;state['make_pending']=True;machine.reg_write(UC_X86_REG_EAX,pointer)
        elif address==stop+0x300:
            if state['make_pending']:event('M');state['make_pending']=False
            cpu.mem_write(word(sp+4),struct.pack('<8H',2026,11,0,3,15,30,40,999))
        elif address==stop+0x100:
            tick=state['tick'];event('T',tick);state['tick']=(tick+1)&0xffffffff
            if state['clock_calls']==0:
                if state['profile']==2:put(user+0x57d0,tick)
                elif state['profile']==3:cpu.mem_write(user+0x57d4,b'\x08')
            state['clock_calls']+=1;machine.reg_write(UC_X86_REG_EAX,tick)
        elif address==0x4cb8e0:machine.reg_write(UC_X86_REG_EAX,0x12345678)
        elif address==0x45b0d0:
            event('P');cpu.mem_write(machine.reg_read(UC_X86_REG_ESI),bytes(144))
        elif address==0x4d4f10:
            packet=bytes(cpu.mem_read(word(sp+4),word(sp+8)));events.extend(b'B'+(counts() if packet[:2]==b'\x01\x0a' else b'')+packet)
        elif address==0x4d4d20:
            events.extend(b'C'+bytes(cpu.mem_read(word(sp+4),word(sp+8))));state['started']=1
            if state['profile']==7:cpu.mem_write(user+0x136,b'\x01\0');put(npc+0x34,9);cpu.mem_write(npc+0x114,b'LATE\0')
        elif address==0x4d4e50:events.extend(b'L'+bytes(cpu.mem_read(word(sp+4),word(sp+8))))
    for p,n in ((0x416d10,4),(0x47f790,12),(0x40b0b0,0),(0x40a510,0),(stop+0x300,4),(stop+0x100,0),(0x4cb8e0,0),(0x4d4f10,8),(0x4d4d20,8),(0x4d4e50,8),(0x505316,0)):
        cpu.mem_write(p,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=p,end=p)
    cpu.hook_add(UC_HOOK_CODE,hook,begin=0x45b0d0,end=0x45b0d0)
    cpu.mem_write(stop+0x200,b'\xc2\x04\x00')
    for p in (0x5210cc,0x5210d0):put(p,stop+0x200)
    put(0x5210dc,stop+0x300);put(0x5210e4,stop+0x100);put(0x3ed35c8,2);put(0x3ed35cc,record)
    print(f'# ps_game.exe SHA256 {digest}; complete QuestStartRequest 0047f220, QuestEnableStart 0047f080, checks 0047f390..0047f570, QuestAdd 0047e010, SetGameLogMain 0045b0d0, QuestStart 0047ea70; real active/finished lists, item grants and shared UID; callback admission/log snapshots')
    for case in range(224):
        profile=case%14;q=bytearray(464);other=bytearray(464);struct.pack_into('<H',q,0,7);struct.pack_into('<H',other,0,9);struct.pack_into('<HH',q,0x102,10,65535);q[0x106]=6;q[0x10a:0x110]=bytes([1]*6);q[0x1cc]=1
        q[0x138]=0 if profile==1 else 4 if profile in (2,3) else 2 if profile in (8,9) else 255 if profile==11 else 1
        q[0x139]=2;struct.pack_into('<H',q,0x13a,9);q[0x13c:0x13e]=bytes((5,7));q[0x147]=(0,2,3)[case%3]
        if profile==1:struct.pack_into('<H',q,0x118,9)
        if case%3!=1:
            for i,p in enumerate((0x13e,0x141,0x144)):q[p:p+3]=bytes((5,i+1,(1,0,255)[i]))
        family=grow=0;job=1
        if profile==6:
            variation=case//14
            if variation<6:q[0x106]=variation;family=(0,1,2,3,2,3)[variation]
            elif variation<10:grow=variation-6;q[0x107]=grow
            else:
                job=variation-10;q[0x10a:0x110]=bytes(6);q[0x10a+job]=1
        country=0 if family<2 else 1
        tick=0xffffffff if case%4==0 else 100;deadline=(tick-1)&0xffffffff if profile==2 else tick;request=0 if profile==12 else 3 if profile==13 else 1;target=7 if profile in (2,3) else 99;billing=77+case
        for p in (stack,user,nodes,quest,items):cpu.mem_write(p,bytes(0x10000))
        cpu.mem_write(record,bytes(q+other));put(user+0x578c,billing);put(user+0x128,9001);cpu.mem_write(user+0x136,struct.pack('<H',20));cpu.mem_write(user+0x12d,bytes((country,family,grow)));cpu.mem_write(user+0x134,bytes([job]));put(user+0x57d0,deadline);cpu.mem_write(user+0x57d4,b'\x07')
        for offset,root in ((0x11c8,nodes),(0x11f4,nodes+0x100)):put(user+offset,root);put(root,root,root)
        put(user+0x1304,1);put(0x547ce4,0,0,0,0);cpu.mem_write(0x542ca0,b'\x03')
        for ident in range(1,4):
            definition=0x54baa8+((5<<8)+ident)*168+0x1804;cpu.mem_write(definition,bytes(168));put(definition,5000+ident);cpu.mem_write(definition+0x24,bytes((5,ident)));cpu.mem_write(definition+0x4a,b'\x01');cpu.mem_write(definition+0x3e,struct.pack('<H',99));cpu.mem_write(definition+0x4c,struct.pack('<H',256+ident))
        put(npc+0x14,99);put(npc+0x34,2,9);put(npc+0x50,npc+0x100);cpu.mem_write(npc+0x114,b'NPC\0')
        state.update(profile=profile,end=q[0x147],allocation=profile not in (9,10),tick=tick,clock_calls=0,inventory=1,started=0,items=0,make_pending=False)
        events.clear();sp=stack+0x8000;put(sp,stop);cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_EAX,target);cpu.reg_write(UC_X86_REG_EDX,request);cpu.reg_write(UC_X86_REG_ECX,user);cpu.emu_start(0x47f220,stop,count=500000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original request did not finish')
        active=word(nodes+0x104);present=active!=nodes+0x100
        snapshot=counts()+struct.pack('<HBI',short(user+0x136),cpu.mem_read(user+0x57d4,1)[0],word(user+0x57d0))+bytes([state['inventory']])+struct.pack('<IHIII',word(user+0x11e8),short(active+8) if present else 0,word(active+16) if present else 0,word(active+20) if present else 0,word(active+24) if present else 0)
        snapshot+=struct.pack('<I',state['items'])
        print(case,profile,country,family,grow,job,quest_block([q,other]).hex(),tick,deadline,request,target,state['started'],snapshot.hex(),events.hex() or '-')

if __name__=='__main__':main()
