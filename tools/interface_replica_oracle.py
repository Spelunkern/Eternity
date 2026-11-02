#!/usr/bin/env python3
"""Execute original quick slots/save points, packets, save phase and error paths.

Ghidra: work/interface-replica, work/interface-init, work/character-save.
Only OS locks, SDatabase and transport/log ports are substituted. Original CRT
formatting and complete SaveSuccess run unchanged.
"""
import random
import re
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_original


def main():
    cpu,digest=load_original('ps_dbAgent');stack,user,packet,db,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
    for address in (stack,user,packet,db,stop):cpu.mem_map(address,0x10000)
    def word(address):return struct.unpack('<I',cpu.mem_read(address,4))[0]
    def put(address,*values):cpu.mem_write(address,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    def string(address):
        result=bytearray()
        while (byte:=cpu.mem_read(address+len(result),1))!=b'\0':result.extend(byte)
        return result.decode()
    state={};queries=[];sends=[]
    def port(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x43f350:
            queries.append(string(word(sp+4)));machine.reg_write(UC_X86_REG_EAX,0xffffffff if len(queries)-1==state['failure'] else 0)
        elif address==0x4327a0:sends.append(bytes(machine.mem_read(word(sp+4),word(sp+8))).hex());machine.reg_write(UC_X86_REG_EAX,1)
        elif address==0x432d80:state['logs']+=1
        else:machine.reg_write(UC_X86_REG_EAX,0)
    for address,cleanup in ((0x43e6f0,0),(0x43f350,4),(0x4327a0,8),(0x432d80,0)):
        cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,port,begin=address,end=address)
    def fatal(machine,address,size,context):state['fatal']=True;machine.emu_stop()
    for address in (0x424869,0x42487a):cpu.hook_add(UC_HOOK_CODE,fatal,begin=address,end=address)
    cpu.mem_write(stop+0x100,b'\xc2\x04\x00')
    for at in (0x4600d4,0x4600d8):put(at,stop+0x100)
    sp=stack+0x8000;put(sp,stop);cpu.reg_write(UC_X86_REG_ESP,sp);cpu.emu_start(0x445779,stop,count=1000)
    def call(address,end=stop):
        cpu.emu_start(address,end,count=1000000)
        if cpu.reg_read(UC_X86_REG_EIP)!=end:raise RuntimeError('Original interface operation did not finish')
    def snapshot():
        n=word(user+0x3810);result=[n]
        for i in range(n):
            p=user+0x3814+6*i;result.extend((*cpu.mem_read(p,3),struct.unpack('<H',cpu.mem_read(p+4,2))[0]))
        for i in range(4):
            p=user+0x80a4+20*i;result.extend((struct.unpack('<i',cpu.mem_read(p,4))[0],struct.unpack('<H',cpu.mem_read(p+4,2))[0],word(p+8),word(p+12),word(p+16)))
        return result
    def sql(query):
        match=re.fullmatch(r'EXEC (\w+) (.*)',query)
        if not match:raise RuntimeError(query)
        proc,args=match.groups();result=[proc.lower()]
        for i,p in enumerate(args.split(',')):
            p=p.strip();result.append('s'+p.encode().hex() if 'savepoint' in proc.lower() and i>=3 else 'i'+str(int(p)))
        return ' '.join(result)
    def save(quick,failure):
        state.update(failure=failure,logs=0,fatal=False);queries.clear();cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_EBP,user);put(sp+0x10,db);put(sp+0x223c,int(quick));cpu.emu_start(0x424617,0x4247e2,count=2000000)
        if not state['fatal'] and cpu.reg_read(UC_X86_REG_EIP)!=0x4247e2:raise RuntimeError('Original interface save did not finish')
        return [sql(q) for q in queries]
    rng=random.Random(0x406c20)
    print(f'# ps_dbAgent.exe SHA256 {digest}; quick 00405b00/0040a770; points 00406c20/0040aa10; full SaveSuccess 00409560; save 00424617..004247e2')
    for episode in range(16):
        cpu.mem_write(user,bytes(0x10000));account=rng.randrange(1,0x80000000);put(user+0x38,account);put(user+0x60,500001);put(user+0x87d8,db)
        for i,offset in enumerate((0x3b14,0x3b40,0x8704,0x86ac,0x86d8)):
            root=user+0x9000+0x100*i;put(user+offset,root);put(root,root,root)
        loaded=[]
        for i in range(4):
            mapid=rng.choice((65535,0,1,2,32767));bits=[rng.choice((0,0x80000000,0x3c000000,0xbc000000,0x3f800001)) for _ in range(3)];cpu.mem_write(user+0x80a4+i*20,struct.pack('<iH2xIII',0,mapid,*bits));loaded.extend((mapid,*bits))
        print('INIT',account,*loaded)
        for step in range(64):
            put(sp,stop);cpu.reg_write(UC_X86_REG_ESP,sp)
            if step%13==12:cpu.reg_write(UC_X86_REG_EAX,user);call(0x409560);print('ACK')
            elif step%3==0:
                n=128 if step==0 and episode==0 else rng.randrange(9);payload=bytes([n])+b''.join(struct.pack('<BBBH',rng.randrange(256),rng.randrange(256),rng.randrange(256),rng.choice((0,32767,32768,65535,rng.randrange(65536)))) for _ in range(n));data=struct.pack('<HI',0x609,account)+payload;cpu.mem_write(packet,data);cpu.reg_write(UC_X86_REG_EAX,packet);cpu.reg_write(UC_X86_REG_ECX,user);call(0x405b00);print('OP',data.hex())
            else:
                bits=[rng.choice((0,0x80000000,1,0x80000001,0x3c000000,0xbc000000,0x3f800001,0x7f7fffff,rng.randrange(0x7f800000))) for _ in range(3)];data=struct.pack('<HIBHIII',0xd02,account,rng.randrange(4),rng.choice((65535,0,1,2,32767,32768,65534)),*bits);cpu.mem_write(packet,data);cpu.reg_write(UC_X86_REG_ESI,packet);cpu.reg_write(UC_X86_REG_EDI,user);call(0x406c20);print('OP',data.hex())
            print('STATE',*snapshot());sends.clear();put(sp,stop);cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_EDI,user);call(0x40a770);print('QUICK',sends[-1]);put(sp,stop);cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_EAX,user);call(0x40aa10);print('POINTS',sends[-1]);quick=rng.choice((False,True));plan=save(quick,-1);failure=rng.choice((-1,rng.randrange(len(plan)))) if plan else -1;print('SAVE',int(quick),failure)
            for q in plan:print('PLAN',q)
            for q in save(quick,failure):print('ATTEMPT',q)
            print('RESULT',int(state['logs']!=0),int(state['fatal']));print('END')


if __name__=='__main__':main()
