#!/usr/bin/env python3
"""Original ps_game quest start instructions; only world/transport/allocation ports.
Real admission, prerequisite list traversal, request dispatch, checks, packet construction
and start item loop execute. Run with .local/oracle-env/bin/python.
"""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game()
    stack,user,record,npc,nodes,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x10000000
    for p in (stack,user,record,npc,nodes,stop):cpu.mem_map(p,0x10000)
    def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*v))
    def word(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
    events=bytearray();state={}
    def event(tag,*values):
        events.extend(tag.encode());events.extend(struct.pack('<'+'I'*len(values),*values))
    def port(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP);result=0
        if address==0x416d10:event('N',word(sp+4));result=npc if state['npc'] else 0
        elif address==0x47e4b0:event('A',word(sp+4));result=state['active']
        elif address==0x47e5b0:event('F',word(sp+4));result=state['finished']
        elif address==0x47f790:event('I',word(sp+4),word(sp+8),word(sp+12));result=state['item']
        elif address==0x47e010:event('Q');result=state['add']
        elif address==stop+0x200:event('T');result=state['tick']
        elif address==0x45b0d0:
            event('P')
            cpu.mem_write(machine.reg_read(UC_X86_REG_ESI),bytes(144))
        elif address==0x4d4d20:
            data=bytes(cpu.mem_read(word(sp+4),word(sp+8)));events.extend(b'C'+data)
        elif address==0x4d4e50:event('L')
        elif address==0x47e620:
            event('G',word(sp+4),word(sp+8),machine.reg_read(UC_X86_REG_EBX));result=0
        machine.reg_write(UC_X86_REG_EAX,result)
    for p,n in ((0x416d10,4),(0x47e4b0,4),(0x47e5b0,4),(0x47f790,12),(0x47e010,4),(stop+0x200,0),(0x45b0d0,0),(0x4d4d20,8),(0x4d4e50,8),(0x47e620,8)):
        cpu.mem_write(p,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,port,begin=p,end=p)
    # Actual successful-finished traversal retains its original critical-section calls.
    cpu.mem_write(stop+0x100,b'\xc2\x04\x00')
    for p in (0x5210cc,0x5210d0):put(p,stop+0x100)
    put(0x5210e4,stop+0x200)
    cpu.mem_write(0x505316,b'\xc3') # security cookie check is not a world operation
    put(0x3ed35c8,1);put(0x3ed35cc,record)
    rng=random.Random(0x47f080)
    print(f'# ps_game.exe SHA256 {digest}; QuestEnableStart 0047f080; QuestStartRequest 0047f220; QuestStart 0047ea70; checks 0047f390..0047f570')
    for case in range(1536):
        q=bytearray(464)
        def short(p,v):struct.pack_into('<H',q,p,v)
        short(0,rng.choice((1,7,65535)));q[2:7]=b'Quest'
        level=rng.choice((0,1,20,60,65535));country=rng.randrange(4);grow=rng.randrange(4);job=rng.randrange(6)
        short(0x102,0);short(0x104,65535);q[0x106]=6;q[0x107]=0;q[0x10a:0x110]=bytes([1]*6);q[0x1cc]=1
        q[0x138]=case%6;q[0x139]=rng.choice((0,2));short(0x13a,9);q[0x13c]=5;q[0x13d]=7
        for p in (0x13e,0x141,0x144):q[p:p+3]=bytes((rng.choice((0,4)),rng.randrange(256),rng.randrange(256)))
        deadline=rng.choice((0,100,0xffffffff));tick=rng.choice((0,100,101,0xffffffff));last=7
        target=7 if q[0x138]>=3 else 99
        state.update(npc=1,active=0,finished=0,item=1,add=1,tick=tick)
        npc_type=q[0x139];npc_id=9;request=1
        previous=[(7,1),(9,0),(65535,2)]
        mutation=(case//6)%18
        if mutation==1:q[0x1cc]=0
        elif mutation==2:short(0x102,21);short(0x104,60)
        elif mutation==3:q[0x106]=rng.randrange(8)
        elif mutation==4:q[0x107]=rng.randrange(5)
        elif mutation==5:q[0x10a+job]=0
        elif mutation==6:short(0x118,rng.choice((0,7,9,10,65535)))
        elif mutation==7:short(0x110,rng.choice((7,9,10)));short(0x112,65535)
        elif mutation==8:state['npc']=0
        elif mutation==9:npc_type=8
        elif mutation==10:npc_id=10
        elif mutation==11:state['active']=1
        elif mutation==12:state['finished']=1
        elif mutation==13:state['item']=0
        elif mutation==14:state['add']=0
        elif mutation==15:target=0x10007
        elif mutation==16:last=8
        elif mutation==17:request=rng.choice((0,2,65535))
        cpu.mem_write(user,bytes(0x10000));cpu.mem_write(record,bytes(q));put(npc+0x14,99);put(npc+0x34,npc_type,npc_id);put(npc+0x50,npc+0x100);cpu.mem_write(npc+0x114,b'NPC\0')
        cpu.mem_write(user+0x12e,bytes((country,grow)));cpu.mem_write(user+0x134,bytes([job]));cpu.mem_write(user+0x136,struct.pack('<H',level));put(user+0x57d0,deadline);cpu.mem_write(user+0x57d4,bytes([last]))
        root=nodes;put(user+0x11c8,root);put(root,nodes+96,nodes+32)
        for i,(ident,success) in enumerate(previous):
            p=nodes+32*(i+1);put(p,root if i==0 else p-32,root if i==2 else p+32,ident,success)
        sp=stack+0x8000
        def execute(address):
            cpu.emu_start(address,stop,count=100000)
            if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original did not return')
        put(sp,stop);cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_EAX,record);cpu.reg_write(UC_X86_REG_ECX,user);execute(0x47f080);enabled=cpu.reg_read(UC_X86_REG_EAX)&255
        events.clear();put(sp,stop);cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_EAX,target);cpu.reg_write(UC_X86_REG_EDX,request);cpu.reg_write(UC_X86_REG_ECX,user);execute(0x47f220)
        print(case,bytes(q).hex(),level,country,grow,job,last,deadline,tick,request,target,state['npc'],npc_type,npc_id,state['active'],state['finished'],state['item'],state['add'],enabled,events.hex() or '-')

if __name__=='__main__':main()
