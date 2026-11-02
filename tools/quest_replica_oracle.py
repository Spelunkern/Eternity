#!/usr/bin/env python3
"""Execute original quest cache, pool/list/journal, full SaveSuccess and SQL phase.

Ghidra: work/quest-replica, work/character-save, work/status-replica.
Only raw allocation, OS locks and SDatabase are ports. Original sprintf and
all quest/list/heap code execute, including SaveSuccess's retained journal.
"""
import random
import re
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_original


def main():
    cpu,digest=load_original('ps_dbAgent');stack,user,items,packet,journal,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x10000000
    for address in (stack,user,items,packet,journal,stop):cpu.mem_map(address,0x10000)
    def word(address):return struct.unpack('<I',cpu.mem_read(address,4))[0]
    def put(address,*values):cpu.mem_write(address,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    def string(address):
        result=bytearray()
        while (byte:=cpu.mem_read(address+len(result),1))!=b'\0':result.extend(byte)
        return result.decode()
    def call(address,end=stop):
        cpu.emu_start(address,end,count=1000000)
        if cpu.reg_read(UC_X86_REG_EIP)!=end:raise RuntimeError('Original quest operation did not finish')
    state={};queries=[]
    def port(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x443986:
            size=word(sp+4)
            if size==24:
                pointer=items+state['serial']*32 if state['allocate'] else 0
                if pointer:state['serial']+=1
            elif size==12:pointer=journal+state['journals']*16;state['journals']+=1
            else:raise RuntimeError('Unexpected quest allocation size')
            machine.reg_write(UC_X86_REG_EAX,pointer)
        elif address==0x43f410:put(word(sp+20),1);machine.reg_write(UC_X86_REG_EAX,0)
        elif address==0x43f350:queries.append(string(word(sp+4)));machine.reg_write(UC_X86_REG_EAX,0)
        else:machine.reg_write(UC_X86_REG_EAX,0)
    for address,cleanup in ((0x443986,0),(0x43e6f0,0),(0x43f410,28),(0x43f350,4),(0x43f3d0,0)):
        cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,port,begin=address,end=address)
    cpu.mem_write(stop+0x100,b'\xc2\x04\x00')
    for at in (0x4600d4,0x4600d8):put(at,stop+0x100)
    sp=stack+0x8000;put(sp,stop);cpu.reg_write(UC_X86_REG_ESP,sp);call(0x445779)
    def nodes(offset):
        root=word(user+offset);node=word(root+4);result=[]
        while node!=root:
            result.append(node);node=word(node+4)
            if len(result)>512:raise RuntimeError('Quest list did not terminate')
        return result
    def entry(pointer):return struct.unpack('<iHBxH3B',cpu.mem_read(pointer+8,13))
    def snapshot():
        result=[]
        for offset in (0x3b14,0x3b40):
            current=nodes(offset);result.append(len(current))
            for node in current:result.extend(entry(node))
        deleted=nodes(0x8704);result.extend((len(deleted),*[struct.unpack('<H',cpu.mem_read(p+8,2))[0] for p in deleted],word(0x470ae0),word(0x470ae4)))
        return result
    rng=random.Random(0x406590)
    print(f'# ps_dbAgent.exe SHA256 {digest}; PacketUserQuest 00406590; AddDeleteQuest 00405380; complete SaveSuccess 00409560; save 004242c2..00424617')
    for episode in range(32):
        for address in (user,items,journal):cpu.mem_write(address,bytes(0x10000))
        cpu.mem_write(0x470adc,bytes(0x20));cpu.mem_write(0x470b90,bytes(0x20));state.update(serial=0,journals=0,allocate=True);account=rng.randrange(1,0x80000000);put(user+0x38,account);put(user+0x60,500001)
        for i,offset in enumerate((0x3b14,0x3b40,0x8704,0x86ac,0x86d8)):
            root=user+0x9000+0x100*i;put(user+offset,root);put(root,root,root)
        initial=[]
        for offset,count in ((0x3b14,6),(0x3b40,3)):
            current=[];initial.append(count)
            for i in range(count):
                pointer=items+state['serial']*32;state['serial']+=1;current.append(pointer);ident=rng.choice((0,5,7,7,10,20,32767,32768,65535))
                if offset==0x3b14:
                    delay=rng.choice((0,1,32767,32768,65535));counters=tuple(rng.randrange(256) for _ in range(3));cpu.mem_write(pointer+8,struct.pack('<iHBxH3B',2,ident,0,delay,*counters));initial.extend((ident,delay,*counters))
                else:
                    success=rng.choice((0,1,2,255));cpu.mem_write(pointer+8,struct.pack('<iHBxH3B',0,ident,success,0,0,0,0));initial.extend((ident,success))
            root=word(user+offset);put(root,current[-1],current[0]);put(user+offset+32,count)
            for i,pointer in enumerate(current):put(pointer,root if i==0 else current[i-1],root if i+1==count else current[i+1])
        put(0x470ae0,state['serial']);print('INIT',account,*initial)
        for step in range(64):
            state['allocate']=rng.randrange(4)!=0;put(sp,stop);cpu.reg_write(UC_X86_REG_ESP,sp)
            if step%11==10:
                cpu.reg_write(UC_X86_REG_EAX,user);call(0x409560);print('ACK')
            else:
                op=0xa01+(episode+step)%5;current=nodes(0x3b40 if op==0xa01 else 0x3b14)
                ident=entry(rng.choice(current))[1] if current and rng.randrange(3)!=0 else rng.choice((0,5,7,10,20,32767,32768,65535))
                if op==0xa01:payload=struct.pack('<HH',ident,rng.choice((0,1,32767,32768,65535)))
                elif op==0xa02:payload=struct.pack('<HB',ident,rng.choice((0,1,2,2,255)))
                elif op==0xa03:payload=struct.pack('<HH3B',ident,rng.randrange(65536),*[rng.randrange(256) for _ in range(3)])
                elif op==0xa04:
                    records=[]
                    for i in range(rng.randrange(9)):
                        ident=entry(rng.choice(current))[1] if current and rng.randrange(3)!=0 else rng.choice((0,5,7,65535));records.append((ident,rng.randrange(65536),*[rng.randrange(256) for _ in range(3)]))
                    payload=bytes([len(records)])+b''.join(struct.pack('<HH3B',*r) for r in records)
                else:payload=struct.pack('<HBB',ident,rng.randrange(3),rng.randrange(256))
                data=struct.pack('<HI',op,account)+payload;cpu.mem_write(packet,data);put(sp,stop,packet);cpu.reg_write(UC_X86_REG_EBX,user);call(0x406590);print('OP',int(state['allocate']),data.hex(),cpu.reg_read(UC_X86_REG_EAX)&255)
            print('STATE',*snapshot());queries.clear();cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_EBP,user);put(sp+0x10,0x12340000);put(sp+0x14,1);call(0x4242c2,0x424617)
            if cpu.reg_read(UC_X86_REG_ESP)!=sp:raise RuntimeError('Quest SQL phase stack imbalance')
            for query in queries:
                match=re.fullmatch(r'\{\?=call (\w+)\((.*)\)\}',query)
                if not match:raise RuntimeError(query)
                print('QUERY',match[1].lower(),*[int(x.strip()) for x in match[2].split(',')])
            print('END')


if __name__=='__main__':main()
