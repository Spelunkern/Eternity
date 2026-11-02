#!/usr/bin/env python3
"""Execute original DB-agent status mutations and core/body SQL argument generation.

Ghidra: work/status-replica, work/character-save, work/core-format and
work/core-decimal. Original status stores, SQL sprintf, decimal conversion and
rounding execute unchanged, after the original CRT floating-point initializer.
"""
import random
import re
import struct
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBP,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_original

SIZES={0x601:34,0x602:10,0x603:10,0x604:20,0x605:9,0x606:12,0x607:22,0x608:18,0x60a:8,0x60b:10,0x60c:11,0x60d:7,0x60e:8,0x60f:8,0x610:9,0x615:10}


def main():
    cpu,digest=load_original('ps_dbAgent');stack,user,packet,stop=0x20000000,0x30000000,0x31000000,0x10000000
    for address in (stack,user,packet,stop):cpu.mem_map(address,0x10000)
    def word(address):return struct.unpack('<I',cpu.mem_read(address,4))[0]
    def put(address,*values):cpu.mem_write(address,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    def cstring(address):
        result=bytearray()
        while (byte:=cpu.mem_read(address+len(result),1))!=b'\0':result.extend(byte)
        return result.decode()
    cpu.mem_write(stack+0x8000,struct.pack('<I',stop));cpu.reg_write(UC_X86_REG_ESP,stack+0x8000)
    cpu.emu_start(0x445779,stop,count=1000) # _cfltcvt_init installs the original conversion table.
    queries=[]
    def snapshot():return (bytes(cpu.mem_read(user+0x60,98))+bytes(cpu.mem_read(user+0x86a4,1))).hex()
    def call(address,end):
        cpu.emu_start(address,end,count=1000000)
        if cpu.reg_read(UC_X86_REG_EIP)!=end:raise RuntimeError('Original operation did not finish')
    rng=random.Random(0x405b00);ops=list(SIZES)
    print(f'# ps_dbAgent.exe SHA256 {digest}; PacketUserSetStatus 00405b00; SaveCharacter core/body 0042328e..00423483')
    for episode in range(32):
        cpu.mem_write(user,bytes(0x10000));account=rng.randrange(1,0x80000000);put(user+0x38,account);put(user+0x60,rng.randrange(1,0x80000000));cpu.mem_write(user+0x64,bytes(rng.randrange(256) for _ in range(14)));cpu.mem_write(user+0x65,b'\0\0');cpu.mem_write(user+0x70,bytes(rng.randrange(256) for _ in range(12)));cpu.mem_write(user+0x80,bytes(rng.randrange(256) for _ in range(28)));cpu.mem_write(user+0xa8,bytes(rng.randrange(256) for _ in range(26)));cpu.mem_write(user+0xaf,b'\0')
        cpu.mem_write(user+0x9c,struct.pack('<fff',1.0000001192092896,-0.0,-39.1234567))
        print('INIT',account,snapshot())
        for step in range(64):
            op=ops[(episode+step)%len(ops)];data=bytearray(struct.pack('<HI',op,account)+bytes(rng.randrange(256) for _ in range(SIZES[op]-6)))
            if op in (0x605,0x610):data[6]=rng.choice((0,1,2,3,4,5,6,7,255))
            if op==0x60c:data[6]=rng.randrange(6)
            if op in (0x607,0x608):
                at=10 if op==0x607 else 6
                for i in range(3):
                    bits=rng.choice((0,0x80000000,1,0x80000001,0x3f800001,0x3c000000,0xbc000000,0x3d200000,0xbd200000,0x4b800001,0x6cffffff,0x7f7fffff,rng.randrange(0x7f800000)))
                    struct.pack_into('<I',data,at+i*4,bits)
            cpu.mem_write(packet,bytes(data));sp=stack+0x8000;put(sp,stop);cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_EAX,packet);cpu.reg_write(UC_X86_REG_ECX,user);call(0x405b00,stop)
            print('STEP',bytes(data).hex(),cpu.reg_read(UC_X86_REG_EAX)&255,snapshot())
            cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_EBP,user);queries.clear();call(0x42328e,0x42338d);queries.append(cstring(sp+0x230))
            if cpu.reg_read(UC_X86_REG_ESP)!=sp:raise RuntimeError('Core SQL stack imbalance')
            if word(user+0x86a4)&255:
                cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_EBP,user);call(0x4233ff,0x42342c);queries.append(cstring(sp+0x230))
                if cpu.reg_read(UC_X86_REG_ESP)!=sp:raise RuntimeError('Body SQL stack imbalance')
            for query in queries:
                match=re.fullmatch(r'\{\?=call (\w+)\((.*)\)\}',query)
                if not match:raise RuntimeError(query)
                proc,args=match.groups();fields=[x.strip() for x in args.split(',')];parameters=[]
                for i,value in enumerate(fields):parameters.append('s'+value.encode().hex() if proc.lower().endswith('info_e') and 17<=i<=19 else 'i'+str(int(value)))
                print('QUERY',proc.lower(),*parameters)
            print('ENDSTEP')
            if step%11==10:
                cpu.reg_write(UC_X86_REG_ESI,user);cpu.reg_write(UC_X86_REG_EBX,0);call(0x4097b8,0x4097be);print('ACK',snapshot())


if __name__=='__main__':main()
