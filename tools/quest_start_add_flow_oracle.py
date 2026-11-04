#!/usr/bin/env python3
"""Execute request -> real QuestAdd -> QuestStart, with exact DB/client/log trace.
World lookup/inventory, allocation, OS clock/locks, common log snapshot and
transports are explicit ports. QuestAdd itself is NOT replaced.
"""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
from mob_quest_oracle import quest_block


def main():
    cpu,digest=load_game();stack,user,record,npc,nodes,quest,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000,0x10000000
    for p in (stack,user,record,npc,nodes,quest,stop):cpu.mem_map(p,0x10000)
    def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*v))
    def word(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
    events=bytearray();state={}
    def event(c,*v):events.extend(c.encode()+struct.pack('<'+'I'*len(v),*v))
    def counts():return struct.pack('<III',word(user+0x1214),word(user+0x57c8),word(user+0x57c4))
    def port(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP);result=0
        if address==0x416d10:event('N',word(sp+4));result=npc if state['npc'] else 0
        elif address==0x47e4b0:event('A',word(sp+4));result=state['active']
        elif address==0x47e5b0:event('F',word(sp+4));result=state['finished']
        elif address==0x47f790:
            consume=word(sp+12);event('I',word(sp+4),word(sp+8),consume);result=int(state['inventory']!=0)
            if result and consume:state['inventory']-=1
        elif address==0x40b0b0:event('O');result=quest if state['allocate'] else 0
        elif address==stop+0x200:
            result=state['now'];event('T',result);state['now']=(result+1)&0xffffffff
        elif address==0x4d4f10:
            events.extend(b'B'+counts()+bytes(cpu.mem_read(word(sp+4),word(sp+8))))
        elif address==0x45b0d0:
            event('P');cpu.mem_write(machine.reg_read(UC_X86_REG_ESI),bytes(144))
        elif address in (0x4d4d20,0x4d4e50):
            events.extend((b'C' if address==0x4d4d20 else b'L')+bytes(cpu.mem_read(word(sp+4),word(sp+8))))
            if address==0x4d4d20:state['started']=1
        elif address==0x47e620:event('G',word(sp+4),word(sp+8),machine.reg_read(UC_X86_REG_EBX))
        machine.reg_write(UC_X86_REG_EAX,result)
    for p,n in ((0x416d10,4),(0x47e4b0,4),(0x47e5b0,4),(0x47f790,12),(0x40b0b0,0),(stop+0x200,0),(0x4d4f10,8),(0x45b0d0,0),(0x4d4d20,8),(0x4d4e50,8),(0x47e620,8)):
        cpu.mem_write(p,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,port,begin=p,end=p)
    cpu.mem_write(stop+0x100,b'\xc2\x04\x00')
    for p in (0x5210cc,0x5210d0):put(p,stop+0x100)
    put(0x5210e4,stop+0x200);cpu.mem_write(0x505316,b'\xc3');put(0x3ed35c8,1);put(0x3ed35cc,record)
    rng=random.Random(0x47e01047);bounds=(0,1,65535,65536,0x7fffffff,0x80000000,0xffffffff)
    print(f'# ps_game.exe SHA256 {digest}; QuestStartRequest 0047f220; real QuestAdd 0047e010; QuestStart 0047ea70; 0a01 -> 0902 -> 0501 -> grants')
    for case in range(640):
        q=bytearray(464);struct.pack_into('<H',q,0,rng.choice((0,7,65535)));struct.pack_into('<HH',q,0x102,0,65535);q[0x106]=6;q[0x10a:0x110]=bytes([1]*6);q[0x1cc]=1
        maximum=rng.choice(bounds);minimum=rng.choice(bounds);struct.pack_into('<II',q,0x124,minimum,maximum);q[0x138]=case%5;q[0x139]=2;struct.pack_into('<H',q,0x13a,9);q[0x13c:0x13e]=bytes((5,7));q[0x147]=rng.choice((0,2,3,255))
        for p in (0x13e,0x141,0x144):q[p:p+3]=bytes((rng.choice((0,4)),rng.randrange(256),rng.randrange(256)))
        tick=rng.choice(bounds);billing=rng.randrange(1<<32);mob=rng.choice(bounds);player=rng.choice(bounds);deadline=rng.choice((tick,(tick-1)&0xffffffff,0xffffffff));last=7;target=99 if case%5<=2 else 7
        state.update(npc=1,active=0,finished=0,inventory=1,allocate=1,now=tick,started=0)
        mutation=(case//5)%8
        if mutation==1:state['allocate']=0
        elif mutation==2:state['inventory']=0
        elif mutation==3:state['active']=1
        elif mutation==4:state['finished']=1
        elif mutation==5:state['npc']=0
        elif mutation==6:target=0x10007
        elif mutation==7:last=8
        initial_inventory=state['inventory']
        for p in (stack,user,quest):cpu.mem_write(p,bytes(0x10000))
        cpu.mem_write(record,bytes(q));put(npc+0x14,99);put(npc+0x34,2,9);put(npc+0x50,npc+0x100);cpu.mem_write(npc+0x114,b'NPC\0')
        put(user+0x578c,billing);put(user+0x57c8,mob);put(user+0x57c4,player);cpu.mem_write(user+0x136,struct.pack('<H',20));cpu.mem_write(user+0x134,b'\x01');put(user+0x57d0,deadline);cpu.mem_write(user+0x57d4,bytes([last]))
        for offset,root in ((0x11c8,nodes),(0x11f4,nodes+0x100)):put(user+offset,root);put(root,root,root)
        events.clear();sp=stack+0x8000;put(sp,stop);cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_EAX,target);cpu.reg_write(UC_X86_REG_EDX,1);cpu.reg_write(UC_X86_REG_ECX,user);cpu.emu_start(0x47f220,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original request did not return')
        snapshot=counts()+bytes(cpu.mem_read(quest+8,4))+bytes(cpu.mem_read(quest+16,12))+bytes(cpu.mem_read(quest+12,1))+bytes(cpu.mem_read(quest+28,3))+struct.pack('<I',int(word(quest+32)==record))
        print(case,quest_block([q]).hex(),tick,billing,mob,player,deadline,last,target,state['npc'],state['active'],state['finished'],initial_inventory,state['allocate'],state['started'],state['inventory'],snapshot.hex(),events.hex() or '-')

if __name__=='__main__':main()
