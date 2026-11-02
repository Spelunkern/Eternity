#!/usr/bin/env python3
"""Execute original game entry catalog serializers and DB catalog decoders.

Ghidra work/entry-catalogs, work/entry-social-cache, work/entry-social-db.
World social lookup, send, clock, OS locks and CRT strncpy/stack probe are ports.
Original social append/find/serialization and guild-tree iteration execute.
Only unused fixed string bytes after NUL are normalized in transmitted packets.
"""
import itertools
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,user,peer,world,data,manager,heap,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000,0x35000000,0x10000000
    for at in (stack,user,peer,world,data,manager,stop):cpu.mem_map(at,0x10000)
    cpu.mem_map(heap,0x100000);state={};events=[]
    def word(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def put(at,*values):cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    def present(i):return state['presence']==1 or state['presence']==3 or (state['presence']==2 and i%3!=0)
    def mutual(i):return state['presence']==1 and i%2==0 or state['presence']==2 and i%5==1
    def normalize(p):
        op=struct.unpack_from('<H',p)[0]
        if op in (0x2201,0x2208):
            at=3
            for _ in range(p[2]):
                off,width,memo=(6,21,27) if op==0x2201 else (4,21,25);end=p.index(0,at+off,at+off+width);p[end+1:at+off+width]=bytes(at+off+width-end-1);at+=memo+1+p[at+memo]
        elif op==0xd2f:
            for at in range(3,len(p),120):
                for off,width in ((4,25),(29,21),(50,65)):
                    end=p.index(0,at+off,at+off+width);p[end+1:at+off+width]=bytes(at+off+width-end-1)
        return p
    def hook(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x4d4d20:
            p=normalize(bytearray(cpu.mem_read(word(sp+4),word(sp+8))));events.append(('PEER '+str(state['last'])+' ' if state['kind']=='INIT' else 'PACKET ')+p.hex())
        elif address==0x410320:
            key=word(machine.reg_read(UC_X86_REG_EBX));state['last']=key;events.append('LOOKUP '+str(key));out=machine.reg_read(UC_X86_REG_EAX);put(out,data+0x100 if present(key) else data);put(peer+0x1780,int(mutual(key)));put(peer+0x1784,state['character']);machine.reg_write(UC_X86_REG_EAX,out)
        elif address==0x504c60:
            dest,source,n=word(sp+4),word(sp+8),word(sp+12);raw=bytes(cpu.mem_read(source,n));end=raw.find(b'\0');cpu.mem_write(dest,raw if end<0 else raw[:end]+bytes(n-end));machine.reg_write(UC_X86_REG_EAX,dest)
        elif address==0x505bb0:
            amount=machine.reg_read(UC_X86_REG_EAX);ret=word(sp);put(sp-amount,ret);machine.reg_write(UC_X86_REG_ESP,sp-amount)
        elif address==stop+0x120:machine.reg_write(UC_X86_REG_EAX,state['tick'])
    for at,cleanup in ((0x4d4d20,8),(0x410320,0),(0x504c60,0),(0x505bb0,0),(0x505316,0),(stop+0x100,4),(stop+0x120,0)):
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
    for iat,target in ((0x5210cc,stop+0x100),(0x5210d0,stop+0x100),(0x5210e4,stop+0x120)):put(iat,target)
    put(0x546f38,world);put(world+0x2c,data);put(data+0x110,peer)
    def call(address,args=(),regs=()):
        put(stack+0xf000,stop,*args);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000)
        for reg,value in regs:cpu.reg_write(reg,value)
        cpu.emu_start(address,stop,count=1000000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError(f'Original entry catalog did not return {address:08x}')
    def fixed(value,n):return value+b'\0'+bytes(n-len(value)-1)
    def text(value):return value.hex() if value else '-'
    rng=random.Random(0x483ac0)
    print(f'# ps_game.exe SHA256 {digest}; entry catalog serializers 00483ac0/00483d60/00483ea0/00483ed0/00483f80/00481920, guild listing 00432000; DB decoders 0046e770/0046e810/0046e880; CFriend append/find execute')
    for count,blocked,presence in itertools.product((0,1,24,25,26,50,99,100,101,120),(0,1,25,26,100,103),range(4)):
        cid=rng.getrandbits(32);cpu.mem_write(user,bytes(0x10000));put(user+0xdc,cid);state.update(character=cid,presence=presence);events.clear();print('SOCIAL',cid,count,blocked,presence)
        for i in range(count):
            fid=(0,1,2,3,4,5,0xffffffff)[i%7] if i%3 else i;family,grow,job=i&255,(i*7)&255,(i*11)&255;name=(b'',b'A'*20,b'Name'+str(i).encode())[i%3];memo=(b'',b'm',b'X'*49,b'Y'*50)[i%4];cpu.mem_write(data+0x200,fixed(name,21));cpu.mem_write(data+0x300,fixed(memo,51));packet=struct.pack('<HIBIBBB',0xc01,530001,1,fid,family,grow,job)+fixed(name,21)+fixed(memo,51);cpu.mem_write(data+0x1000,packet);call(0x46e770,regs=((UC_X86_REG_EBX,data+0x1000),(UC_X86_REG_ECX,user)));print('FRIEND',fid,family,grow,job,text(name),text(memo))
        for i in range(blocked):
            bid=(i*13)&0xffffffff;name=(b'',b'B'*20,b'Block'+str(i).encode())[i%3];memo=(b'',b'm',b'Z'*50)[i%3];cpu.mem_write(data+0x200,fixed(name,21));cpu.mem_write(data+0x300,fixed(memo,51));packet=struct.pack('<HIBI',0xc05,530001,1,bid)+fixed(name,21)+fixed(memo,51);cpu.mem_write(data+0x1000,packet);call(0x46e770,regs=((UC_X86_REG_EBX,data+0x1000),(UC_X86_REG_ECX,user)));print('BLOCK',bid,text(name),text(memo))
        state['kind']='INIT';call(0x4756d0,(user,));state['kind']='SOCIAL';call(0x483ac0,(user,));print('COUNTS',word(user+0x1780),word(user+0x36c4))
        for event in events:print('EVENT',event)
        print('END')
    # Stateful decoding: empty/partial updates deliberately retain trailing slots.
    for seed in range(96):
        cpu.mem_write(user,bytes(0x10000));put(user+0x5870,*([65535]*4));uid=530001;events.clear();state['kind']='CATALOG';print('CATALOG',uid)
        for round in range(4):
            records=[]
            for i in range((seed+round)%7):
                slot=(i*3+round)%6;records.append(struct.pack('<BHIII',slot,(seed*997+i)&65535,rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32)))
            packet=struct.pack('<HIB',0xd01,uid,len(records))+b''.join(records);cpu.mem_write(data+0x1000,packet);call(0x46e810,regs=((UC_X86_REG_EDI,data+0x1000),(UC_X86_REG_EDX,user)));print('DB',packet.hex())
            n=(seed+round)%11;rows=[]
            for i in range(n):
                code=(bytes([65+i])*21 if i%4==0 else fixed(b'code'+str(seed+i).encode(),21)) if (i+round)%3 else bytes(21);rows.append(code+struct.pack('<II',rng.getrandbits(32),rng.getrandbits(32)))
            packet=struct.pack('<HIIB',0xe01,uid,rng.getrandbits(32),n)+b''.join(rows);cpu.mem_write(data+0x1000,packet);call(0x46e880,(user,),((UC_X86_REG_ESI,data+0x1000),));print('DB',packet.hex())
            rows=[bytes(((i*13+round)%256,(i*7)&255,(i*11)&255,(i+seed+round)&255)) for i in range((seed*5+round)%250)];packet=struct.pack('<HIB',0xe04,uid,len(rows))+b''.join(rows);cpu.mem_write(data+0x1000,packet);call(0x46e880,(user,),((UC_X86_REG_ESI,data+0x1000),));print('DB',packet.hex())
            for address,args,regs in ((0x483d60,(),((UC_X86_REG_ECX,user),)),(0x483ed0,(user,),()),(0x483f80,(),((UC_X86_REG_EDI,user),))):call(address,args,regs)
            print('SEND')
            for event in events:print('EVENT',event)
            events.clear()
        print('END')
    for country,points,expires,tick in itertools.product((0,1,255),(0,0xffffffff),(0,1,1000,0x80000000,0xffffffff),(0,1,999,1000,0x80000000,0xffffffff)):
        state.update(kind='KC',tick=tick);events.clear();cpu.mem_write(user+0x12d,bytes([country]));put(world+country*132+0x200,points);put(world+country*132+0x208,expires);call(0x481920,regs=((UC_X86_REG_EDI,user),));print('KC',country,points,expires,tick)
        for event in events:print('EVENT',event)
        print('END')
    for value in (0,1,255,256,65535):
        state['kind']='HONOR';events.clear();cpu.mem_write(user+0x168,struct.pack('<H',value));call(0x483ea0,regs=((UC_X86_REG_ECX,user),));print('HONOR',value)
        for event in events:print('EVENT',event)
        print('END')
    for count,country in itertools.product((0,1,13,14,15,27,28,29,60),(0,1,2,255)):
        state['kind']='GUILD';events.clear();cpu.mem_write(manager,bytes(0x10000));nil=heap;put(nil,nil,nil,nil);cpu.mem_write(nil+0x15,b'\x01');nodes=[heap+0x100+i*0x20 for i in range(count)];records=[heap+0x10000+i*0x100 for i in range(count)]
        def tree(lo,hi,parent):
            if lo>=hi:return nil
            mid=(lo+hi)//2;node=nodes[mid];put(node,tree(lo,mid,node),parent,tree(mid+1,hi,node));put(node+0x10,records[mid]);cpu.mem_write(node+0x15,b'\0');return node
        root=tree(0,count,nil);put(nil,nodes[0] if count else nil,root,nodes[-1] if count else nil);put(manager+0x20,nil);cpu.mem_write(user+0x12d,bytes([country]));print('GUILD',country)
        for i,record in enumerate(records):
            gid,points=rng.getrandbits(32),rng.getrandbits(32);side=(country if i%2 else (0,1,2,255,256)[i%5]);name=(b'',b'N'*24,b'Guild'+str(i).encode())[i%3];master=(b'',b'M'*20,b'Master'+str(i).encode())[i%3];notice=(b'',b'T'*64,b'Notice'+str(i).encode())[i%3];rank=(i*17)&255;put(record+8,gid);cpu.mem_write(record+12,fixed(name,25));cpu.mem_write(record+0x25,fixed(master,21));put(record+0x40,side,points);cpu.mem_write(record+0x48,bytes([rank]));cpu.mem_write(record+0x56,fixed(notice,65));print('GUILD_ROW',i,gid,side,points,rank,text(name),text(master),text(notice))
        call(0x432000,regs=((UC_X86_REG_ECX,manager),(UC_X86_REG_EDX,user)));print('SEND')
        for event in events:print('EVENT',event)
        print('END')


if __name__=='__main__':main()
