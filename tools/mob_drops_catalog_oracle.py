#!/usr/bin/env python3
"""Execute original CItemDrop::Init and real InitLimit over projected item grids."""
import argparse
import json
import struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game

ITEM_BASE=0x54d2ac

def rows_for(case):
    rows=[]
    for i in range(48):
        table_type=(0,1,2,127,255)[i%5]
        table_id=0 if i==0 else (i*5+case)%255+1
        if i and table_type==0 and table_id==1:table_id=254
        item_id=case*100+i+1
        item_type=0 if i%13==0 else (table_type or 1)
        item_type_id=0 if i%17==0 else table_id
        real=27 if i%11==0 else 1
        grade=(0,1,3,1024,1025,65535)[(i+case)%6]
        server=(0,0,1,9)[(i//3+case)%4]
        drop=(0,1,2,3,4,5,6)[(i+case//3)%7]
        rows.append([table_type,table_id,item_id,item_type,item_type_id,real,grade,server,drop,-i-1,0xffffffff-i,123+i,456+i])
    # Deliberately reverse SQL/input order; original scans table coordinates.
    return list(reversed(rows))

def digest(values):
    result=14695981039346656037
    for value in values:
        for byte in struct.pack('<I',value&0xffffffff):result=((result^byte)*1099511628211)&0xffffffffffffffff
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--items',type=Path);args=parser.parse_args()
    cpu,sha=load_game();base=0x30000000;cpu.mem_map(base,0x1000000)
    catalog,heap,stack,stop=base,base+0x100000,base+0xf00000,base+0xff0000
    cpu.mem_write(ITEM_BASE,bytes(256*256*168));state={}
    def put(at,value):cpu.mem_write(at,struct.pack('<I',value&0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    for at,cleanup in ((0x503ae3,0),(0x4be840,4),(0x4be520,0)):
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup))
    def allocation(machine,at,size,unused):
        amount=get(machine.reg_read(UC_X86_REG_ESP)+4)
        if state['heap']+amount>stack:raise RuntimeError('Original allocation exceeds oracle heap')
        machine.reg_write(UC_X86_REG_EAX,state['heap']);state['heap']+=(amount+15)&~15
    cpu.hook_add(UC_HOOK_CODE,allocation,begin=0x503ae3,end=0x503ae3)
    print(f'# ps_game.exe SHA256 {sha}; complete CItemDrop::Init 004bde50 and InitLimit 004be3e0. Allocation port only; unrelated ItemCreate/sequence-config initializers observed as external ports. Hash includes all 1025 grade orders/counts and every supplied limit state.')
    previous=[]
    cases=[('S',case,rows_for(case)) for case in range(32)]
    if args.items:cases.append(('A',0,json.loads(args.items.read_text())))
    for tag,case,rows in cases:
        for at in previous:cpu.mem_write(at,bytes(168))
        previous=[];state['heap']=heap
        for row in rows:
            table_type,table_id,item_id,item_type,item_type_id,real,grade,server,drop,count,next_tick,delay,random_delay=row
            at=ITEM_BASE+(table_type*256+table_id)*168;previous.append(at)
            put(at,item_id);cpu.mem_write(at+36,bytes([item_type,item_type_id]));put(at+160,real)
            cpu.mem_write(at+74,bytes([server]));cpu.mem_write(at+124,struct.pack('<HH',grade,drop))
            for off,value in ((140,count),(144,delay),(148,random_delay),(152,next_tick)):put(at+off,value)
        put(stack,stop);put(stack+4,catalog);cpu.reg_write(UC_X86_REG_ESP,stack)
        cpu.emu_start(0x4bde50,stop,count=10000000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original catalog initialization did not return')
        values=[]
        for grade in range(1025):
            count=get(catalog+grade*12);values.extend((count,get(catalog+grade*12+4)))
            pointer=get(catalog+grade*12+8)
            values.extend(get(get(pointer+i*4)) for i in range(count))
        for at in previous:values.extend(get(at+off) for off in (140,152,144,148))
        print(tag,case,len(rows),digest(values),sep='\t')
        if tag=='S':
            for row in rows:print('R',*row,sep='\t')

if __name__=='__main__':main()
