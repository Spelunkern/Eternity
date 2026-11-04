#!/usr/bin/env python3
"""Actual QuestGetStartItem -> three actual QuestAddItem calls, persistent slots/UID.
CreateItem, OS clocks/locks and transports are ports. Every successful definition
has a full 20-digit craft string, so all 57 DB bytes are initialized by original code.
"""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,user,items,record,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
    for p in (stack,user,items,record,stop):cpu.mem_map(p,0x10000)
    def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def word(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
    events=bytearray();state={}
    def event(c,*v):events.extend(c.encode()+struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def observe(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x402710:event('D',machine.reg_read(UC_X86_REG_EAX),machine.reg_read(UC_X86_REG_ECX))
        elif address==0x47e770:
            index=(machine.reg_read(UC_X86_REG_EAX)-user-0x224)//4;event('K',1+index//24,index%24)
        elif address==0x40a510:
            event('O');attempt=state['attempts'];state['attempts']+=1
            pointer=items+attempt*0x100 if state['allocation_mask']&(1<<attempt) else 0
            if pointer:
                put(pointer+0x38,0xface,0);cpu.mem_write(pointer+0x4c,b'\0'+bytes([0xa5])*20);state['pending_calendar']=True
            machine.reg_write(UC_X86_REG_EAX,pointer)
        elif address==stop+0x100:
            if state['pending_calendar']:event('M');state['pending_calendar']=False
            cpu.mem_write(word(sp+4),struct.pack('<8H',2026,11,0,3,15,30,40,999))
        elif address==stop+0x200:
            machine.reg_write(UC_X86_REG_EAX,state['tick']);state['tick']=(state['tick']+1)&0xffffffff
        elif address==0x47e8ff:event('U')
        elif address==0x47e934:
            bag=machine.reg_read(UC_X86_REG_EAX);slot=machine.reg_read(UC_X86_REG_EBX);event('S',bag,slot)
            pointer=word(user+0x224+((bag-1)*24+slot)*4);state['stored'].append((bag,slot,pointer))
        elif address==0x4d4f10:
            packet=bytes(cpu.mem_read(word(sp+4),word(sp+8)));events.extend(b'B'+packet)
            state['uids'].append(struct.unpack_from('<Q',packet,6)[0]);state['db']+=1
            if state['db']==1 and state['new_bags']!=-99:put(user+0x1304,state['new_bags'])
        elif address==0x4d4d20:events.extend(b'C'+bytes(cpu.mem_read(word(sp+4),word(sp+8))))
    for p,n in ((0x40a510,0),(stop+0x100,4),(stop+0x200,0),(0x4d4f10,8),(0x4d4d20,8),(0x505316,0)):
        cpu.mem_write(p,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,observe,begin=p,end=p)
    for p in (0x402710,0x47e770,0x47e8ff,0x47e934):cpu.hook_add(UC_HOOK_CODE,observe,begin=p,end=p)
    cpu.mem_write(stop+0x300,b'\xc2\x04\x00')
    for p in (0x5210cc,0x5210d0):put(p,stop+0x300)
    put(0x5210dc,stop+0x100);put(0x5210e4,stop+0x200)
    rng=random.Random(0x47eb70)
    print(f'# ps_game.exe SHA256 {digest}; QuestGetStartItem 0047eb70 -> QuestAddItem 0047e620/0047e6f0; persistent inventory/UID; all packet bytes initialized')
    for case in range(240):
        profile=case%10;bags=2 if profile==2 else 1;holes=[0,1,2];allocation_mask=7;lookup_mask=7;new_bags=-99
        if profile==1:holes=[0,23]
        elif profile==2:holes=[47]
        elif profile==3:holes=[]
        elif profile==4:allocation_mask=5
        elif profile==5:bags=rng.choice((-1,0))
        elif profile==7:lookup_mask=5
        elif profile==8:holes=[0,24,25];new_bags=2
        elif profile==9:new_bags=0
        q=bytearray(464);struct.pack_into('<H',q,0,1)
        for i,p in enumerate((0x13e,0x141,0x144)):q[p:p+3]=bytes((0 if profile==6 and i==0 else 5,1 if case%4==0 else i+1,rng.choice((0,1,2,255))))
        for ident in range(1,4):
            definition=0x54baa8+((5<<8)+ident)*168+0x1804;cpu.mem_write(definition,bytes(168));put(definition,5000+ident)
            if lookup_mask&(1<<(ident-1)):cpu.mem_write(definition+0x24,bytes((5,ident)))
            cpu.mem_write(definition+0x4a,b'\x01');cpu.mem_write(definition+0x3e,struct.pack('<H',99));cpu.mem_write(definition+0x4c,struct.pack('<H',256+ident))
        for p in (stack,user,items):cpu.mem_write(p,bytes(0x10000))
        cpu.mem_write(record,bytes(q));billing=rng.randrange(1<<32);put(user+0x1304,bags);put(user+0x578c,billing)
        for i in range(48):put(user+0x224+i*4,0 if i in holes else items+0x1000)
        put(0x547ce4,rng.choice((0,65535,0xffffffff)),0,0,0);cpu.mem_write(0x542ca0,bytes([rng.randrange(256)]))
        state.update(attempts=0,allocation_mask=allocation_mask,pending_calendar=False,tick=rng.choice((0,100,0xffffffff)),stored=[],uids=[],db=0,new_bags=new_bags)
        events.clear();sp=stack+0x8000;put(sp,stop);cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_EAX,user);cpu.reg_write(UC_X86_REG_EDI,record);cpu.emu_start(0x47eb70,stop,count=500000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original sequence did not return')
        snapshot=bytearray(struct.pack('<I',len(state['stored'])))
        for bag,slot,p in state['stored']:
            snapshot.extend(bytes((bag,slot))+bytes(cpu.mem_read(p+0x38,8))+bytes(cpu.mem_read(p+0x40,3))+bytes(cpu.mem_read(p+0x44,2))+bytes(cpu.mem_read(p+0x46,6))+bytes(cpu.mem_read(p+0x4c,21))+bytes(cpu.mem_read(p+0x64,4))+bytes(cpu.mem_read(p+0x68,1))+bytes(cpu.mem_read(p+0x84,22))+struct.pack('<I',word(word(p+0x30))))
        grid=bytes(0 if word(user+0x224+i*4)==0 else 1 if word(user+0x224+i*4)==items+0x1000 else 2 for i in range(48))
        print(case,bytes(q).hex(),bags,','.join(map(str,holes)) or '-',allocation_mask,lookup_mask,new_bags,billing,','.join(map(str,state['uids'])) or '-',word(user+0x1304),grid.hex(),snapshot.hex(),events.hex() or '-')

if __name__=='__main__':main()
