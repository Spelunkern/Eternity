#!/usr/bin/env python3
"""Complete ps_game QuestAddItem overloads, real definition lookup and inline UID.
Pool allocation, OS clocks/locks and transports are ports; scan/init/craft/UID
instructions and both packet constructors execute without replacing either grant.
"""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,user,item,outputs,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
    for p in (stack,user,item,outputs,stop):cpu.mem_map(p,0x10000)
    def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
    def word(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
    events=bytearray();state={}
    def event(c,*v):events.extend(c.encode()+struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
    def observe(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x402710:event('D',machine.reg_read(UC_X86_REG_EAX),machine.reg_read(UC_X86_REG_ECX))
        elif address==0x47e770:
            index=(machine.reg_read(UC_X86_REG_EAX)-user-0x224)//4;event('K',1+index//24,index%24)
        elif address==0x40a510:event('O');machine.reg_write(UC_X86_REG_EAX,item if state['allocation'] else 0)
        elif address==stop+0x100:
            if state['calendars']==0:event('M')
            cpu.mem_write(word(sp+4),struct.pack('<8H',2026,11,0,3,15,30,40+state['calendars'],999));state['calendars']+=1
        elif address==stop+0x200:machine.reg_write(UC_X86_REG_EAX,state['tick'])
        elif address==0x47e8ff:event('U')
        elif address==0x47e934:event('S',word(state['bag_output']),word(state['slot_output']))
        elif address in (0x4d4f10,0x4d4d20):events.extend((b'B' if address==0x4d4f10 else b'C')+bytes(cpu.mem_read(word(sp+4),word(sp+8))))
    for p,n in ((0x40a510,0),(stop+0x100,4),(stop+0x200,0),(0x4d4f10,8),(0x4d4d20,8),(0x505316,0)):
        cpu.mem_write(p,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,observe,begin=p,end=p)
    for p in (0x402710,0x47e770,0x47e8ff,0x47e934):cpu.hook_add(UC_HOOK_CODE,observe,begin=p,end=p)
    cpu.mem_write(stop+0x300,b'\xc2\x04\x00')
    for p in (0x5210cc,0x5210d0):put(p,stop+0x300)
    put(0x5210dc,stop+0x100);put(0x5210e4,stop+0x200)
    rng=random.Random(0x47e6f0);previous=None
    print(f'# ps_game.exe SHA256 {digest}; QuestAddItem 0047e620/0047e6f0; GetItemInfo 00402710; inline UID 0047e854..0047e914; complete 0701/0205')
    for case in range(384):
        mode=case%2;typ=rng.choice((1,5,255));ident=rng.choice((1,7,255));count=rng.choice((-1,0,1,2,255,256,513));found=case%7!=0;bags=rng.choice((1,2));hole=rng.choice((0,1,23,24,47));allocation=case%5!=0
        if case%11==0:typ=rng.choice((0,256))
        if case%13==0:ident=rng.choice((0,256))
        if case%9==0:bags=rng.choice((-1,0))
        if case%10==0:hole=-1
        actual_type=rng.choice((1,5,255));actual_id=rng.choice((1,7,255));quality=rng.choice((0,1,255,256,65535));server=rng.choice((0,1,9));wisdom=rng.choice((0,1,99,100,65535));definition_id=rng.randrange(1<<32);billing=rng.randrange(1<<32)
        if previous:cpu.mem_write(previous,bytes(168))
        definition=0x54baa8+((typ<<8)+ident)*168+0x1804 if 1<=typ<=255 and 1<=ident<=255 else 0
        if definition:
            cpu.mem_write(definition,bytes(168));put(definition,definition_id);cpu.mem_write(definition+0x24,bytes((actual_type,actual_id)) if found else b'\0\0');cpu.mem_write(definition+0x4a,bytes([server]));cpu.mem_write(definition+0x3e,struct.pack('<H',wisdom));cpu.mem_write(definition+0x4c,struct.pack('<H',quality));previous=definition
        for p in (stack,user,item,outputs):cpu.mem_write(p,bytes(0x10000))
        put(item+0x38,0xface,0);cpu.mem_write(item+0x4c,b'\0'+bytes([0xa5])*20)
        put(user+0x1304,bags);put(user+0x578c,billing)
        for i in range(max(0,bags)*24):put(user+0x224+i*4,0 if i==hole else item+0x100)
        tick=rng.choice((0,100,0xfffffffe,0xffffffff));counter=rng.choice((0,65535,0xffffffff));baseline=rng.choice((0,65535,0xffffffff));deadline=0 if case%3 else 0xffffffff;stamp=rng.choice((0,0x12345678,0xffffffff));server_id=rng.randrange(256)
        put(0x547ce4,counter,baseline,deadline,stamp);cpu.mem_write(0x542ca0,bytes([server_id]));state.update(allocation=allocation,calendars=0,tick=tick)
        sp=stack+0x8000;put(outputs,17,19,0)
        if mode==0:
            put(sp,stop,typ,count,outputs,outputs+4,outputs+8);cpu.reg_write(UC_X86_REG_EDX,user);cpu.reg_write(UC_X86_REG_ECX,ident);address=0x47e6f0;bag_output=outputs;slot_output=outputs+4;info_output=outputs+8
        else:
            put(sp,stop,typ,ident);cpu.reg_write(UC_X86_REG_EBX,count&0xffffffff);cpu.reg_write(UC_X86_REG_ESI,user);address=0x47e620;info_output=sp-0x34;bag_output=info_output+4;slot_output=info_output+8
        state.update(bag_output=bag_output,slot_output=slot_output);events.clear();cpu.reg_write(UC_X86_REG_ESP,sp);cpu.emu_start(address,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original grant did not return')
        result=cpu.reg_read(UC_X86_REG_EAX)&255
        snapshot=bytes(cpu.mem_read(item+0x38,8))+bytes(cpu.mem_read(item+0x40,3))+bytes(cpu.mem_read(item+0x44,2))+bytes(cpu.mem_read(item+0x46,6))+bytes(cpu.mem_read(item+0x4c,21))+bytes(cpu.mem_read(item+0x64,4))+bytes(cpu.mem_read(item+0x68,1))+bytes(cpu.mem_read(item+0x84,22))+struct.pack('<I',int(word(item+0x30)==definition and definition!=0))
        uid=struct.unpack('<Q',cpu.mem_read(item+0x38,8))[0];made=word(item+0x64)
        print(case,mode,typ,ident,count,int(found),bags,hole,int(allocation),actual_type,actual_id,quality,server,wisdom,definition_id,billing,uid,made,result,word(bag_output),word(slot_output),int(word(info_output)!=0),snapshot.hex(),events.hex())

if __name__=='__main__':main()
