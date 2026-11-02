#!/usr/bin/env python3
"""Execute original DBCharacter::SaveCharacter inventory/bank SQL generation.

Ghidra exports: work/character-save and work/item-save-time. Only SDatabase
ports and the CRT sprintf are substituted; original loops, journals, argument
loads and packed-time conversion execute unchanged. No original database used.
"""
import csv
import random
import re
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_original


def main():
    cpu,digest=load_original('ps_dbAgent')
    stack,user,items,nodes=0x20000000,0x30000000,0x31000000,0x32000000
    for address in (stack,user,items,nodes):cpu.mem_map(address,0x10000)
    def word(address):return struct.unpack('<I',cpu.mem_read(address,4))[0]
    def put(address,*values):cpu.mem_write(address,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    def string(address):
        result=bytearray()
        while (byte:=cpu.mem_read(address+len(result),1))!=b'\0':result.extend(byte)
        return result.decode('ascii')
    queries=[]
    def hook(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x44447f:
            dest,fmt=word(sp+4),string(word(sp+8));cursor=sp+12;values=[]
            for match in re.finditer(r'%(I64d|(?:\.\d+)?[ducs])',fmt):
                kind=match[1];value=word(cursor);cursor+=4
                if kind=='I64d':value=struct.unpack('<q',machine.mem_read(cursor-4,8))[0];cursor+=4
                elif kind.endswith('d') and value>=0x80000000:value-=0x100000000
                elif kind=='s':value=string(value)
                elif kind=='c':value=chr(value&255)
                values.append(value)
            rendered=fmt.replace('%I64d','%d')%tuple(values)
            machine.mem_write(dest,rendered.encode('ascii')+b'\0');machine.reg_write(UC_X86_REG_EAX,len(rendered))
        elif address==0x43f410:put(word(sp+20),1);machine.reg_write(UC_X86_REG_EAX,0)
        elif address==0x43f350:queries.append(string(word(sp+4)));machine.reg_write(UC_X86_REG_EAX,0)
        else:machine.reg_write(UC_X86_REG_EAX,0)
    for address,cleanup in ((0x44447f,0),(0x43e6f0,0),(0x43f410,28),(0x43f350,4),(0x43f3d0,0)):
        cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
    rng=random.Random(0x423190)
    print(f'# ps_dbAgent.exe SHA256 {digest}; SaveCharacter 00423190 inventory/bank phase, ConvTimeToString 0042ad20')
    for case in range(128):
        cpu.mem_write(user,bytes(0x10000));cpu.mem_write(stack,bytes(0x10000));cpu.mem_write(items,bytes(0x10000));cpu.mem_write(nodes,bytes(0x10000));queries.clear()
        server=rng.randrange(256);account=rng.randrange(1,0x80000000);character=rng.randrange(1,0x80000000)
        put(user+0x38,account,server);put(user+0x60,character);print('CASE',server,account,character)
        serial=0
        for bank in (False,True):
            root=nodes+bank*0x8000;count=(0,1,39,40,41,80,81)[(case+bank)%7];pointers=[root+32+i*16 for i in range(count)]
            put(root,pointers[-1] if pointers else root,pointers[0] if pointers else root);put(user+(0x86d8 if bank else 0x86ac),root);put(user+(0x86f8 if bank else 0x86cc),count)
            for i,pointer in enumerate(pointers):
                value=rng.choice((0,1,0xffffffffffffffff,0x8000000000000000,rng.getrandbits(64)))
                put(pointer,root if i==0 else pointers[i-1],root if i+1==count else pointers[i+1]);cpu.mem_write(pointer+8,struct.pack('<Q',value));print('DEL',int(bank),value)
            for index in sorted(rng.sample(range(240 if bank else 144),12)):
                pointer=items+128*serial;serial+=1;put(user+(0x320 if bank else 0xe0)+index*4,pointer)
                value=rng.getrandbits(64);ident=rng.getrandbits(32);kind,tid,count=rng.randrange(256),rng.randrange(256),rng.randrange(256);quality=rng.choice((0,4999,5000,32767,32768,65535));gems=bytes(rng.randrange(256) for _ in range(6));craft=rng.choice(('', '01020304050607080900','12345'));made=rng.choice((0,0xffffffff,0x60442001,rng.getrandbits(32)));make=rng.choice((77,85,83));save=rng.choice((-1,0,1,1,2,2,3))
                put(pointer+8,save,0);cpu.mem_write(pointer+0x10,struct.pack('<QI3BxH',value,ident,kind,tid,count,quality));cpu.mem_write(pointer+0x22,gems);cpu.mem_write(pointer+0x28,craft.encode()+b'\0');put(pointer+0x40,made);cpu.mem_write(pointer+0x44,bytes([make]))
                print('ITEM',int(bank),index,value,ident,kind,tid,count,quality,gems.hex(),craft or '-',made,make,save)
        sp=stack+0x8000;put(sp+0x10,0x12340000);put(sp+0x14,1);cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_EBP,user)
        cpu.emu_start(0x42366d,0x423f55,count=2000000)
        if cpu.reg_read(UC_X86_REG_EIP)!=0x423f55:raise RuntimeError('Original save phase did not finish')
        if cpu.reg_read(UC_X86_REG_ESP)!=sp:raise RuntimeError('Original stack did not balance')
        for query in queries:
            match=re.fullmatch(r'\{\?=call (\w+)\((.*)\)\}',query)
            if not match:raise RuntimeError(query)
            proc,args=match.groups();fields=next(csv.reader([args],quotechar="'",skipinitialspace=True))
            # csv removes quotes, so locate quoted fields using a simple original-literal scanner.
            raw=re.findall(r"'(?:[^']*)'|[^,]+",args);parameters=[]
            for token in raw:
                token=token.strip()
                parameters.append('s'+token[1:-1].encode().hex() if token.startswith("'") else 'i'+str(int(token)))
            if len(fields)!=len(parameters):raise RuntimeError('SQL argument scan changed')
            print('QUERY',proc.lower(),*parameters)
        print('END')


if __name__=='__main__':main()
