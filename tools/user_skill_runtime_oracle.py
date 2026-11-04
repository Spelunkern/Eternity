#!/usr/bin/env python3
"""Whole learned 0504 admission, actual shared Create/FreeSkill and user teardown."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game

def main():
    c,digest=load_game();base=0x30000000;c.mem_map(base,0x80000);c.mem_map(0,0x10000)
    user,packet,manager,heap,definition,stack,stop=base,base+0x10000,base+0x20000,base+0x30000,base+0x40000,base+0x60000,base+0x7f000
    state={};events=[]
    def put(p,*v):c.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(p):return struct.unpack('<I',c.mem_read(p,4))[0]
    def hook(cpu,at,size,data):
        pop=0;result=None
        if at==stop+16 or at==stop+32:pop=4
        elif at==stop+48:result=state['tick']
        elif at==stop+64:
            p=get(cpu.reg_read(UC_X86_REG_ESP)+4);put(p,get(p)+1);result=get(p);pop=4
        elif at==0x503a3f:
            state['attempts']+=1
            if state['attempts']==state['fail']:result=0
            else:
                result=heap+state['allocated']*0x100;state['allocated']+=1
                # Actual new CSkill initializes only ID in SSyncHeap; retain
                # seeded allocator bytes in all other untouched payload fields.
                put(result,*([0x11223344]*13))
        elif at==0x402770:
            id=cpu.reg_read(UC_X86_REG_EAX);level=cpu.reg_read(UC_X86_REG_ESI)
            events.append(f'Q,{id},{level}');result=0 if id==999 else definition
        elif at==0x489560:pop=4
        elif at==0x44cd80:result=base+0x8000
        elif at==0x44c610:result=0
        elif at in (0x44f2d0,0x450cf0):
            p=cpu.reg_read(UC_X86_REG_ECX if at==0x450cf0 else UC_X86_REG_EAX);events.append(('A' if at==0x450cf0 else 'U')+','+str(get(p+8))+','+str(get(p+20)));pop=4
        elif at==0x4d4d20:
            sp=cpu.reg_read(UC_X86_REG_ESP);events.append('P,'+bytes(cpu.mem_read(get(sp+4),get(sp+8))).hex());pop=8
        elif at==0x4d43a0:events.append('C');pop=8
        elif at in (0x504495,): # sprintf: log text formatting port, cdecl
            p=get(cpu.reg_read(UC_X86_REG_ESP)+4);cpu.mem_write(p,b'bad skill\0');result=0
        elif at in (0x4d4f10,):pop=8
        elif at in (0x4d9510,):pass # cdecl log port
        elif at==0x503ba1:
            p=get(cpu.reg_read(UC_X86_REG_ESP)+4)
            if heap<=p<definition:events.append('D,'+str(get(p+8)))
        elif at in (0x4dfd00,0x4d42a0):pass # unrelated crypto/connection destructors
        else:return
        sp=cpu.reg_read(UC_X86_REG_ESP);ret=get(sp);cpu.reg_write(UC_X86_REG_ESP,sp+4+pop)
        if result is not None:cpu.reg_write(UC_X86_REG_EAX,result)
        cpu.reg_write(UC_X86_REG_EIP,ret)
    c.hook_add(UC_HOOK_CODE,hook)
    for imported,target in ((0x5210cc,stop+16),(0x5210d0,stop+32),(0x5210d8,stop+16),(0x5210e4,stop+48),(0x5210f8,stop+64)):put(imported,target)
    def call(at,args=(),regs=()):
        put(stack,stop,*args);c.reg_write(UC_X86_REG_ESP,stack)
        for r,v in regs:c.reg_write(r,v)
        try:c.emu_start(at,stop,count=500000)
        except Exception as e:raise RuntimeError(f'case {state["case"]} entry {at:x} pc {c.reg_read(UC_X86_REG_EIP):x}') from e
        if c.reg_read(UC_X86_REG_EIP)!=stop or c.reg_read(UC_X86_REG_ESP)!=stack+4+len(args)*4:raise RuntimeError('unbalanced complete function')
    print(f'# ps_game.exe SHA256 {digest}; complete PacketUserDBSetInfo 0046d780 0504, CreateSkill 0040a780, FreeSkill 0040a7c0, ResetCharacter 0044a050 / destructor 004496a0; allocator/OS/definition/log/connection ports only.')
    print('# case fail reset tick packet count selected slots events manager_counter free_count poll_selected poll_mode poll_final')
    for case in range(64):
        c.mem_write(base,bytes(0x60000));state=dict(case=case,tick=(100,0xfffffff0)[case%2],attempts=0,allocated=0,fail=(0,2,3,1)[(case//4)%4]);events=[]
        put(0x546f3c,manager);put(0x546f48,manager);put(manager+0x10,500)
        # Recycle then reacquire an actual shared object with retained ID/ticks.
        call(0x40a780,regs=((UC_X86_REG_EAX,manager),));old=c.reg_read(UC_X86_REG_EAX)
        if old:
            put(old+0x18,71,72,73,74,75,76);call(0x40a7c0,regs=((UC_X86_REG_ESI,manager),(UC_X86_REG_EDI,old)))
        state['attempts']=0
        rows=[(11,1,0,3),(12,2,1 if case%8<4 else 255,65535),(11,1,0 if case%8<4 else 2,0)]
        if case%16==7:rows[1]=(999,1,1,0)
        if case%16==15:rows[1]=(0,1,1,0)
        wire=struct.pack('<HIB',0x504,42,len(rows))+b''.join(struct.pack('<HBBH',*r) for r in rows);c.mem_write(packet,wire)
        call(0x46d780,(user,),((UC_X86_REG_ECX,packet),));assert c.reg_read(UC_X86_REG_EAX)&255==1
        poll_selected=255 if case%2==0 else 0;poll_mode=2+(case//2)%2
        put(user+0x1444,poll_selected);put(user+0x142c,poll_mode);put(user+0x1440,2);put(user+0x1528,2,77);put(user+0xdc,42);put(user+0x12ec,4)
        call(0x48ca50,(state['tick'],),((UC_X86_REG_ECX,user),))
        poll_final=','.join(str(get(user+x)) for x in (0x142c,0x1274,0x143c,0x1440,0x1450))+','+str(c.mem_read(user+0x1448,1)[0])
        reset=int(case%4==0)
        for i,offset in enumerate((0x1538,0x11f4,0x11c8,0xa94)):
            head=base+0x9000+i*0x100;put(head,head,head);put(user+offset,head)
        # ResetCharacter's empty owner trees and reusable applied-skill heads.
        tree=base+0x9800;put(tree,tree,tree,tree);c.mem_write(tree+0x15,b'\x01');put(user+0x5d14,tree)
        if reset:call(0x44a050,(user,))
        slots=[]
        for i in range(256):
            p=get(user+0xac4+i*4)
            if p:slots.append(str(i)+':'+','.join(str(get(p+x)) for x in (8,12,16,20,24,28,32,36,40,44)))
        count=get(user+0xac0);selection=get(user+0x1444)
        call(0x4496a0,(user,))
        print(case,state['fail'],reset,state['tick'],wire.hex(),count,selection,';'.join(slots) or '-', '|'.join(events) or '-',get(manager+0x10),get(manager+0x11c),poll_selected,poll_mode,poll_final,sep='\t')

if __name__=='__main__':main()
