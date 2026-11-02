#!/usr/bin/env python3
"""Original guild membership owners, actual trees, broadcast and roster bytes.

Ghidra work/world-guild-membership, work/guild-client-membership and
work/guild-member-trees. Allocator, OS locks and packet pool/transport are ports.
All map insertion/removal, member updates, seven officer slots and iteration run.
Undefined name padding is normalized after NUL only.
"""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,guild,users,records,heap,buffer,data,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000,0x35000000,0x10000000
    for at in (0,stack,guild,buffer,data,stop):cpu.mem_map(at,0x10000)
    cpu.mem_map(users,0x30000);cpu.mem_map(records,0x20000);cpu.mem_map(heap,0x200000)
    manager=0x5476e0;pointers=[users+i*0x10000 for i in range(3)];events=[];state={}
    def word(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def put(at,*values):cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    def signed(v):return v if v<0x80000000 else v-0x100000000
    def packet(p):
        p=bytearray(p);op=struct.unpack_from('<H',p)[0]
        if op in (0xd12,0xd13,0xd19,0xd16):
            width,start=(28,7) if op==0xd16 else (29,8)
            for at in range(3,len(p),width):
                end=p.index(0,at+start,at+start+21);p[end+1:at+start+21]=bytes(at+start+21-end-1)
        return p.hex()
    def hook(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x503a3f:
            n=word(sp+4);at=state['allocation'];state['allocation']+=(n+15)&~15;machine.reg_write(UC_X86_REG_EAX,at)
        elif address==0x4df9f0:
            state['packet']=bytes(cpu.mem_read(word(sp+4),word(sp+8)));events.append('ALLOC '+packet(state['packet']));put(buffer+8,1);machine.reg_write(UC_X86_REG_EAX,0 if state['fail'] else buffer)
        elif address in (0x4d4cf0,0x4d4d20):
            peer=machine.reg_read(UC_X86_REG_ECX);payload=state['packet'] if address==0x4d4cf0 else bytes(cpu.mem_read(word(sp+4),word(sp+8)));events.append('SEND '+str(pointers.index(peer))+' '+packet(payload))
        elif address==stop+0x120:
            at=word(sp+4);value=(word(at)-1)&0xffffffff;put(at,value);machine.reg_write(UC_X86_REG_EAX,value)
        elif address==0x4df070:events.append('FREE')
    for at,cleanup in ((0x503a3f,0),(0x503ba1,0),(0x4df9f0,0),(0x4d4cf0,4),(0x4d4d20,8),(0x4df070,0),(stop+0x100,4),(stop+0x120,4),(0x505316,0)):
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
    for iat,target in ((0x5210cc,stop+0x100),(0x5210d0,stop+0x100),(0x5211f0,stop+0x120)):put(iat,target)
    def call(address,args=(),regs=()):
        put(stack+0xf000,stop,*args);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000)
        for reg,value in regs:cpu.reg_write(reg,value)
        cpu.emu_start(address,stop,count=2000000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError(f'Original guild did not return {address:08x}')
        return cpu.reg_read(UC_X86_REG_EAX)&255
    def tree(at,head):put(at,0,head,0);put(head,head,head,head);cpu.mem_write(head+20,b'\x01\x01')
    def insert(tree_at,key,value):
        put(data,key,value);call(0x435300,(data+0x100,),((UC_X86_REG_EAX,tree_at),(UC_X86_REG_EBX,data)))
    def member(ptr,mid,i,phase):
        put(ptr+8,mid);cpu.mem_write(ptr+12,(b'N'+str(mid).encode() if i%3 else b'X'*20)+b'\0');put(ptr+36,0x123+i,70000+i,(-1,0,1,2,3,255)[i%6]);put(ptr+48,pointers[i%3] if phase==0 and i%4!=0 else 0,i%2,77+i)
    def snapshot():
        values=[word(guild+0x514),word(guild+0x540),word(guild+0x56c)]
        for at in pointers:values.extend((word(at+0x1748),signed(word(at+0x174c)),word(at+0x1750),int(bool(word(at+0x1758))),word(at+0x54c4)))
        values += [word(at+8) if (at:=word(guild+0x4d8+4*i)) else 0 for i in range(7)]
        for phase in range(3):
            for i in range(state['count']):
                ptr=records+(phase*128+i)*64;values.extend((pointers.index(word(ptr+48))+1 if word(ptr+48) in pointers else 0,word(ptr+56)))
        print('STATE',*values)
    print(f'# ps_game.exe SHA256 {digest}; guild connect/disconnect 0042d1c0/0042d300, Send 0042c050, lists 0042c980/0042ce10, Init/Release/SendCharacterGuild 004762c0/00476300/00476390; actual trees; pool/allocator/OS/transport ports')
    for scenario in range(72):
        cpu.mem_write(guild,bytes(0x10000));cpu.mem_write(users,bytes(0x30000));cpu.mem_write(records,bytes(0x20000));cpu.mem_write(heap,bytes(0x200000));cpu.mem_write(manager,bytes(0x200));state.update(count=(0,1,39,40,41,81)[scenario%6],allocation=heap+0x1000,fail=scenario%5==0)
        put(guild+8,55)
        for phase,offset in enumerate((0x4f4,0x520,0x54c)):
            tree(guild+offset+24,heap+phase*0x100)
            for i in range(state['count']):
                ptr=records+(phase*128+i)*64;mid=(phase+1)*1000+i;member(ptr,mid,i,phase);insert(guild+offset+24,mid,0 if scenario%11==7 and i==1 else ptr)
        tree(manager+0xa0,heap+0x300)
        for i,at in enumerate(pointers):
            put(at+0xdc,(2000,1000,3000)[i]);put(at+0x1748,123,7,99,0,0);put(at+0x54c4,100+i)
            call(0x432ab0,((2000,1000,3000)[i],guild if scenario%11 else 0),((UC_X86_REG_EAX,manager),))
        if scenario%4==1 and state['count']:insert(guild+0x564,2000,records+2*128*64)
        for i in range(7):
            if (127 if scenario%17==16 else scenario)&(1<<i):put(guild+0x4d8+4*i,records+(128+i%max(state['count'],1))*64)
        print('CASE',scenario)
        for op,i,address,args,regs in (
            ('CONNECT',0,0x42d1c0,(pointers[0],),((UC_X86_REG_EAX,guild),)),
            ('INIT',2,0x4762c0,(),((UC_X86_REG_EAX,pointers[2]),)),
            ('SEND',0,0x476390,(),((UC_X86_REG_ECX,pointers[0]),)),
            ('CONNECT',0,0x42d1c0,(pointers[0],),((UC_X86_REG_EAX,guild),)),
            ('DISCONNECT',1,0x42d300,(guild,),((UC_X86_REG_EAX,pointers[1]),)),
            ('RELEASE',0,0x476300,(),((UC_X86_REG_EBX,pointers[0]),)),
            ('INIT',0,0x4762c0,(),((UC_X86_REG_EAX,pointers[0]),)),
            ('LIST',0,0x42c980,(guild,pointers[0]),()),
            ('APPLICATIONS',1,0x42ce10,(),((UC_X86_REG_ECX,guild),(UC_X86_REG_EDI,pointers[1])))):
            events.clear();result=call(address,args,regs);print('OP',op,i)
            if op in ('CONNECT','DISCONNECT'):print('RETURN',int(bool(result)))
            for event in events:print('EVENT',event)
            snapshot();print('ENDOP')
        print('END')


if __name__=='__main__':main()
