#!/usr/bin/env python3
"""Execute original skill cache, pool/list mutations, reset and SQL save phase.

Ghidra exports: work/skill-replica, work/skill-owners and work/character-save.
Only OS locks, raw allocation, transport and SDatabase ports are substituted;
original sprintf, heap reuse, linked lists and skill handlers execute unchanged.
"""
import random
import re
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_ECX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_original


def main():
    cpu,digest=load_original('ps_dbAgent');stack,user,items,packet,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
    for address in (stack,user,items,packet,stop):cpu.mem_map(address,0x10000)
    def word(address):return struct.unpack('<I',cpu.mem_read(address,4))[0]
    def put(address,*values):cpu.mem_write(address,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    def string(address):
        result=bytearray()
        while (byte:=cpu.mem_read(address+len(result),1))!=b'\0':result.extend(byte)
        return result.decode()
    def call(address,end=stop):
        cpu.emu_start(address,end,count=1000000)
        if cpu.reg_read(UC_X86_REG_EIP)!=end:raise RuntimeError('Original skill operation did not finish')
    state={};queries=[];sends=[]
    def port(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x443986:
            if word(sp+4)!=16:raise RuntimeError('Unexpected skill allocation size')
            pointer=items+state['serial']*32 if state['allocate'] else 0
            if pointer:state['serial']+=1
            machine.reg_write(UC_X86_REG_EAX,pointer)
        elif address==0x43f410:put(word(sp+20),1);machine.reg_write(UC_X86_REG_EAX,0)
        elif address==0x43f350:queries.append(string(word(sp+4)));machine.reg_write(UC_X86_REG_EAX,0)
        elif address==0x4327a0:sends.append(bytes(machine.mem_read(word(sp+4),word(sp+8))).hex());machine.reg_write(UC_X86_REG_EAX,1)
        else:machine.reg_write(UC_X86_REG_EAX,0)
    for address,cleanup in ((0x443986,0),(0x43e6f0,0),(0x43f410,28),(0x43f350,4),(0x43f3d0,0),(0x4327a0,8)):
        cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,port,begin=address,end=address)
    cpu.mem_write(stop+0x100,b'\xc2\x04\x00')
    for at in (0x4600d4,0x4600d8):put(at,stop+0x100)
    sp=stack+0x8000;put(sp,stop);cpu.reg_write(UC_X86_REG_ESP,sp);call(0x445779)
    rng=random.Random(0x406100);ops=(0x801,0x802,0x803,0x804,0x901,0x902,0x903,0x904,0x905)
    def learned():return [word(user+0x3410+i*4) for i in range(word(user+0x340c))]
    def effects():
        result=[];root=word(user+0x33e0);node=word(root+4)
        while node!=root:
            result.append(node);node=word(node+4)
            if len(result)>512:raise RuntimeError('Original effect list did not terminate')
        return result
    def entry(pointer):return struct.unpack('<HBBI',cpu.mem_read(pointer+8,8))
    def snapshot():
        a,b=learned(),effects();result=[struct.unpack('<H',cpu.mem_read(user+0x72,2))[0],word(user+0x86a8),cpu.mem_read(user+0x86a5,1)[0],len(a)]
        for p in a:result.extend(entry(p))
        result.append(len(b))
        for p in b:result.extend(entry(p))
        result.extend((word(0x470b04),word(0x470b08),state['serial']-len(a)-len(b)-word(0x470b08)))
        return result
    print(f'# ps_dbAgent.exe SHA256 {digest}; learned 00406100, applied 00406250, reset 00404c40, save 00423f55..004242c2; original CRT/pool/list code')
    for episode in range(32):
        cpu.mem_write(user,bytes(0x10000));cpu.mem_write(items,bytes(0x10000));cpu.mem_write(0x470b00,bytes(0x20));state.update(serial=0,allocate=True);account=rng.randrange(1,0x80000000);points=rng.randrange(65536);put(user+0x38,account);put(user+0x60,500001);cpu.mem_write(user+0x72,struct.pack('<H',points));put(user+0x87d8,0x12345678)
        root=user+0x9000;put(user+0x33e0,root);put(root,root,root)
        initial=[]
        for i in range(6):
            pointer=items+state['serial']*32;state['serial']+=1;ident=rng.choice((0,5,7,7,32767,32768,65535));level=rng.choice((0,1,2,255));delay=rng.choice((0,1,32767,32768,65535));put(user+0x3410+i*4,pointer);cpu.mem_write(pointer+8,struct.pack('<HBBI',ident,level,i,delay));initial.extend((ident,level,delay))
        put(user+0x340c,6);put(user+0x86a8,6)
        initial_effects=[];nodes=[]
        for i in range(6):
            pointer=items+state['serial']*32;state['serial']+=1;nodes.append(pointer);ident=rng.choice((5,7,7,32767,32768,65535));level=rng.choice((1,1,2,255));delay=rng.choice((0,1,0x7fffffff,0x80000000,0xffffffff));cpu.mem_write(pointer+8,struct.pack('<HBBI',ident,level,0,delay));initial_effects.extend((ident,level,delay))
        put(root,nodes[-1],nodes[0]);put(user+0x3400,len(nodes))
        for i,pointer in enumerate(nodes):put(pointer,root if i==0 else nodes[i-1],root if i+1==len(nodes) else nodes[i+1])
        put(0x470b04,state['serial']);print('INIT',account,points,6,*initial,6,*initial_effects)
        for step in range(64):
            queries.clear();sends.clear();state['allocate']=rng.randrange(5)!=0
            op=ops[(episode+step)%len(ops)];payload=b''
            existing=effects();pair=lambda:entry(rng.choice(existing))[:2] if existing and rng.randrange(3)!=0 else (rng.choice((0,5,7,32768,65535)),rng.choice((0,1,2,255)))
            if op==0x801:payload=struct.pack('<HHB',rng.randrange(65536),rng.choice((0,5,7,32767,32768,65535)),rng.choice((0,1,1,2,255)))
            elif op==0x802:payload=struct.pack('<H',rng.randrange(65536))
            elif op==0x803:payload=struct.pack('<BH',rng.randrange(256),rng.randrange(65536))
            elif op==0x804:
                records=[(rng.choice((rng.randrange(256),rng.randrange(max(1,len(learned()))))),rng.randrange(65536)) for _ in range(rng.randrange(9))];payload=bytes([len(records)])+b''.join(struct.pack('<BH',*x) for x in records)
            elif op in (0x901,0x903):payload=struct.pack('<HBI',*pair(),rng.choice((0,1,0x7fffffff,0x80000000,0xffffffff,rng.getrandbits(32))))
            elif op==0x902:payload=struct.pack('<HB',*pair())
            elif op==0x904:
                records=[(*pair(),rng.getrandbits(32)) for _ in range(rng.randrange(9))];payload=bytes([len(records)])+b''.join(struct.pack('<HBI',*x) for x in records)
            put(sp,stop);cpu.reg_write(UC_X86_REG_ESP,sp)
            if step%17==16:
                points=rng.randrange(65536);cpu.mem_write(user+0x8080,struct.pack('<H',points));cpu.reg_write(UC_X86_REG_ESI,user);call(0x404c40);print('RESET',points)
            elif step%13==12:
                cpu.reg_write(UC_X86_REG_ESI,user);cpu.reg_write(UC_X86_REG_EBX,0);call(0x4097a6,0x4097b8);print('ACK')
            else:
                data=struct.pack('<HI',op,account)+payload;cpu.mem_write(packet,data)
                if op<0x900:put(sp,stop,packet);cpu.reg_write(UC_X86_REG_EAX,user);call(0x406100)
                else:cpu.reg_write(UC_X86_REG_EAX,packet);cpu.reg_write(UC_X86_REG_ECX,user);call(0x406250)
                print('OP',int(state['allocate']),data.hex(),cpu.reg_read(UC_X86_REG_EAX)&255)
            for sent in sends:print('SEND',sent)
            print('STATE',*snapshot())
            cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_EBP,user);cpu.reg_write(UC_X86_REG_EBX,0x12340000);put(sp+0x10,0x12340000);put(sp+0x14,1);call(0x423f55,0x4242c2)
            if cpu.reg_read(UC_X86_REG_ESP)!=sp:raise RuntimeError('Skill save phase stack imbalance')
            for query in queries:
                match=re.fullmatch(r'\{\?=call (\w+)\((.*)\)\}',query)
                if not match:raise RuntimeError(query)
                print('QUERY',match[1].lower(),*[int(x.strip()) for x in match[2].split(',')])
            print('END')


if __name__=='__main__':main()
