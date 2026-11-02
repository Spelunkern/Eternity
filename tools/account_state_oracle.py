#!/usr/bin/env python3
"""Execute original market cache/save and maximum-grow state/error contracts.

Usage: account_state_oracle.py market|growth
Ghidra: work/market-grow, work/status-replica, work/character-save. Only SQL,
DB-pool, OS-lock, transport and log ports are substituted; original CRT runs.
"""
import itertools
import random
import re
import struct
import sys
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_original


def main(mode):
    cpu,digest=load_original('ps_dbAgent');stack,user,packet,db,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
    for address in (stack,user,packet,db,stop):cpu.mem_map(address,0x10000)
    def word(address):return struct.unpack('<I',cpu.mem_read(address,4))[0]
    def put(address,*values):cpu.mem_write(address,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    def string(address):
        result=bytearray()
        while (byte:=cpu.mem_read(address+len(result),1))!=b'\0':result.extend(byte)
        return result.decode()
    def call(address,end=stop):
        cpu.emu_start(address,end,count=1000000)
        if cpu.reg_read(UC_X86_REG_EIP)!=end:raise RuntimeError('Original account state operation did not finish')
    state={};queries=[];sends=[]
    def port(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x43ccc0:state['alloc']+=1;machine.reg_write(UC_X86_REG_EAX,0 if state['failure']==1 else db)
        elif address==0x43f410:put(word(sp+20),0);state['return']=word(sp+20);machine.reg_write(UC_X86_REG_EAX,0xffffffff if state['failure']==2 else 0)
        elif address==0x43f350:
            queries.append(string(word(sp+4)))
            if mode=='growth':put(state['return'],0xffffffff if state['failure']==5 else 0 if state['failure']==6 else 1);failed=state['failure']==3
            else:failed=len(queries)-1==state['failure']
            machine.reg_write(UC_X86_REG_EAX,0xffffffff if failed else 0)
        elif address==0x43f3d0:machine.reg_write(UC_X86_REG_EAX,0xffffffff if state['failure']==4 else 0)
        elif address==0x4327a0:sends.append(bytes(machine.mem_read(word(sp+4),word(sp+8))).hex());machine.reg_write(UC_X86_REG_EAX,1)
        elif address==0x432d80:state['logs']+=1
        else:machine.reg_write(UC_X86_REG_EAX,0)
    for address,cleanup in ((0x43ccc0,0),(0x43cd80,4),(0x43e6f0,0),(0x43f410,28),(0x43f350,4),(0x43f3d0,0),(0x4327a0,8),(0x432d80,0)):
        cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,port,begin=address,end=address)
    cpu.mem_write(stop+0x100,b'\xc2\x04\x00')
    for at in (0x4600d4,0x4600d8):put(at,stop+0x100)
    sp=stack+0x8000;put(sp,stop);cpu.reg_write(UC_X86_REG_ESP,sp);call(0x445779)
    def query(q):
        pattern=r'\{\?=call (\w+)\((.*)\)\}' if mode=='growth' else r'EXEC (\w+) (.*)'
        match=re.fullmatch(pattern,q)
        if not match:raise RuntimeError(q)
        return match[1].lower()+' '+ ' '.join(str(int(x.strip())) for x in match[2].split(','))
    print(f'# ps_dbAgent.exe SHA256 {digest}; market 061a/00405b00, send 0040ae20, save 004247e2..00424a81; growth 004200c0; original CRT and SaveSuccess')
    rng=random.Random(0x4200c0)
    if mode=='growth':
        cases=[(g,m,l,i%7,False) for i,(g,m,l) in enumerate(itertools.product((0,1,2,3,4,5,255),(0,1,2,3,4,5,255),(0,1,29,30,31,65535)))]
        cases += [(rng.randrange(256),rng.randrange(256),rng.randrange(65536),i%7,False) for i in range(512)]
        cases += [(g,m,l,f,False) for g,m,l in ((2,2,30),(2,0,30),(0,0,0)) for f in range(7)]
        cases += [(2,2,30,f,True) for f in range(3)]
        for grow,maximum,level,failure,null in cases:
            cpu.mem_write(user,bytes(0x10000));account=rng.randrange(1,0x80000000);server=rng.randrange(256);put(user+0x38,account,server);cpu.mem_write(user+0x66,bytes([maximum]));cpu.mem_write(user+0x68,bytes([grow]));cpu.mem_write(user+0x6e,struct.pack('<H',level));print('CASE',server,account,grow,level,maximum,int(null))
            for f in (failure,0):
                state.update(failure=f,logs=0,alloc=0);queries.clear();put(sp,stop);cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_ESI,0 if null else user);call(0x4200c0);code=cpu.reg_read(UC_X86_REG_EAX);code=code-0x100000000 if code&0x80000000 else code
                print('TRY',f,cpu.mem_read(user+0x66,1)[0],code,state['logs'],state['alloc'],len(queries))
                for q in queries:print('QUERY',query(q))
        return
    def snapshot():
        n=cpu.mem_read(user+0x8678,1)[0];return [word(user+0x8674),n,*[word(user+0x867c+i*4) for i in range(n)]]
    def save(failure):
        state.update(failure=failure,logs=0);queries.clear();cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_EBP,user);cpu.reg_write(UC_X86_REG_EDI,db);put(sp+0x10,db);call(0x4247e2,0x424a81);return [query(q) for q in queries]
    for episode in range(16):
        cpu.mem_write(user,bytes(0x10000));account=rng.randrange(1,0x80000000);put(user+0x38,account);put(user+0x60,500001);put(user+0x87d8,db)
        for i,offset in enumerate((0x3b14,0x3b40,0x8704,0x86ac,0x86d8)):
            root=user+0x9000+0x100*i;put(user+offset,root);put(root,root,root)
        n=rng.randrange(11);put(user+0x8678,n);ids=[rng.getrandbits(32) for _ in range(n)]
        for i,value in enumerate(ids):put(user+0x867c+4*i,value)
        print('INIT',account,n,*ids)
        for step in range(64):
            put(sp,stop);cpu.reg_write(UC_X86_REG_ESP,sp)
            if step%13==12:cpu.reg_write(UC_X86_REG_EAX,user);call(0x409560);print('ACK')
            elif step%3!=1:
                n=rng.randrange(11);ids=[rng.choice((0,1,0x7fffffff,0x80000000,0xffffffff,rng.getrandbits(32))) for _ in range(n)];data=struct.pack('<HIB',0x61a,account,n)+b''.join(struct.pack('<I',v) for v in ids);cpu.mem_write(packet,data);cpu.reg_write(UC_X86_REG_EAX,packet);cpu.reg_write(UC_X86_REG_ECX,user);call(0x405b00);print('OP',data.hex())
            else:print('HOLD')
            print('STATE',*snapshot());sends.clear();put(sp,stop);cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_EDX,user);call(0x40ae20);print('PACKET',sends[-1] if sends else '-')
            before=word(user+0x8674);plan=save(-1);put(user+0x8674,before);failure=rng.choice((-1,rng.randrange(len(plan)))) if plan else -1;print('SAVE',failure)
            for q in plan:print('PLAN',q)
            for q in save(failure):print('ATTEMPT',q)
            print('RESULT',int(state['logs']!=0),*snapshot());print('END')


if __name__=='__main__':main(sys.argv[1])
