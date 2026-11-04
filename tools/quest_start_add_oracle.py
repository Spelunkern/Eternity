#!/usr/bin/env python3
"""Execute the complete original CUser::QuestAdd 0047e010, including list writes.
Only allocation, OS clock/locks and DB transport are ports. Snapshots verify
initialization before clock, insertion before accounting and accounting before DB.
"""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
from mob_quest_oracle import quest_block


def main():
    cpu,digest=load_game();stack,user,record,quest,nodes,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x10000000
    for p in (stack,user,record,quest,nodes,stop):cpu.mem_map(p,0x10000)
    def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
    def word(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
    events=bytearray();state={}
    def snapshot():
        return bytes(cpu.mem_read(quest+8,4))+bytes(cpu.mem_read(quest+12,1))+bytes(cpu.mem_read(quest+28,3))+bytes(cpu.mem_read(quest+16,12))+struct.pack('<I',int(word(quest+32)==record))
    def counts():return struct.pack('<IIII',word(user+0x1214),word(user+0x57c8),word(user+0x57c4),word(user+0x578c))
    def port(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x40b0b0:
            events.extend(b'A');machine.reg_write(UC_X86_REG_EAX,quest if state['allocate'] else 0)
        elif address==stop+0x100:
            events.extend(b'T'+snapshot());machine.reg_write(UC_X86_REG_EAX,state['tick'])
        elif address==stop+0x200:events.extend(b'E'+snapshot()+counts())
        elif address==stop+0x300:
            events.extend(b'L'+snapshot()+counts())
            if state['mutate']:put(user+0x578c,state['new_billing']);put(user+0x57c8,state['new_mob']);put(user+0x57c4,state['new_player'])
        elif address==0x4d4f10:
            events.extend(b'B'+counts()+bytes(cpu.mem_read(word(sp+4),word(sp+8))))
            if word(sp+8)!=10:raise RuntimeError('Unexpected DB packet size')
    for p,n in ((0x40b0b0,0),(stop+0x100,0),(stop+0x200,4),(stop+0x300,4),(0x4d4f10,8)):
        cpu.mem_write(p,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,port,begin=p,end=p)
    put(0x5210e4,stop+0x100);put(0x5210cc,stop+0x200);put(0x5210d0,stop+0x300)
    rng=random.Random(0x47e010);boundary=(0,1,2,65535,65536,71582,0x7fffffff,0x80000000,0xffffffff)
    print(f'# ps_game.exe SHA256 {digest}; CUser::QuestAdd 0047e010; complete list insertion/accounting/0a01')
    for case in range(512):
        q=bytearray(464);ident=rng.choice((0,1,7,32768,65535));struct.pack_into('<H',q,0,ident)
        maximum=rng.choice(boundary);minimum=rng.choice(boundary);struct.pack_into('<II',q,0x124,minimum,maximum);q[0x147]=rng.choice((0,1,2,2,3,3,4,255))
        tick=rng.choice(boundary)
        if case%13==0 and maximum:tick=(-maximum*60000)&0xffffffff
        if case%17==0 and minimum:tick=(-minimum*60000)&0xffffffff
        billing=rng.randrange(1<<32);mob=rng.choice(boundary);player=rng.choice(boundary);initial=case%4;poison=rng.randrange(1,256)
        state.update(allocate=case%7!=0,tick=tick,mutate=case%5==0,new_billing=rng.randrange(1<<32),new_mob=rng.choice(boundary),new_player=rng.choice(boundary))
        cpu.mem_write(user,bytes(0x10000));cpu.mem_write(record,bytes(q));cpu.mem_write(quest,bytes([poison])*64)
        # Retained definition is deliberately different before allocation succeeds.
        put(quest+32,0);put(quest+8,0xface);put(user+0x578c,billing);put(user+0x57c8,mob);put(user+0x57c4,player);put(user+0x1214,initial);put(user+0x11f4,nodes)
        put(nodes,nodes+32*initial if initial else nodes,nodes+32 if initial else nodes)
        for i in range(initial):
            p=nodes+32*(i+1);put(p,nodes if i==0 else p-32,nodes if i==initial-1 else p+32,0xaa00+i)
        events.clear();sp=stack+0x8000;put(sp,stop,record);cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_EDI,user);cpu.emu_start(0x47e010,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original QuestAdd did not return')
        result=cpu.reg_read(UC_X86_REG_EAX)&255;active=[];node=word(nodes+4)
        while node!=nodes:
            active.append(word(node+8));node=word(node+4)
            if len(active)>8:raise RuntimeError('Invalid original active list')
        print(case,quest_block([q]).hex(),int(state['allocate']),tick,billing,mob,player,initial,poison,int(state['mutate']),state['new_billing'],state['new_mob'],state['new_player'],result,snapshot().hex(),counts().hex(),','.join(map(str,active)) or '-',events.hex())

if __name__=='__main__':main()
