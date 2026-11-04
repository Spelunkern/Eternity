#!/usr/bin/env python3
"""Immutable original shared item UID constructor, GenID and inline callers."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX,
    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBX, UC_X86_REG_ESP, UC_X86_REG_EIP)
from original_oracle import load_game


def main():
    cpu, sha = load_game()
    base = 0x30000000
    cpu.mem_map(base, 0x10000)
    stack, stop, item, scratch = base+0x8000, base+0xf000, base+0x1000, base+0x2000
    gen = 0x547ce0
    state = dict(date=(2024,1,1,0,0,0), tick=0, events='')
    def put(at, value): cpu.mem_write(at, struct.pack('<I', value & 0xffffffff))
    def get(at): return struct.unpack('<I', cpu.mem_read(at, 4))[0]
    # Only OS boundary functions are replaced. The local-calendar packer itself,
    # every GenID instruction and caller server-byte additions remain original.
    ports = {0x5210cc:('L',4), 0x5210d0:('U',4), 0x5210d4:('I',4),
             0x5210e4:('T',0), 0x5210dc:('C',4)}
    def hook(machine, at, size, _):
        event, _cleanup = state['ports'][at]
        state['events'] += event
        sp = machine.reg_read(UC_X86_REG_ESP)
        if event == 'T': machine.reg_write(UC_X86_REG_EAX, state['tick'])
        elif event == 'C':
            y,m,d,h,minute,second = state['date']
            machine.mem_write(get(sp+4), struct.pack('<8H', y,m,0,d,h,minute,second,0))
        elif event in ('L','U','I'):
            if get(sp+4) != gen+0x14: raise RuntimeError('Wrong global lock owner')
    state['ports'] = {}
    for i,(iat,(event,cleanup)) in enumerate(ports.items()):
        target=stop+16+i*16; put(iat,target)
        cpu.mem_write(target,b'\xc2'+struct.pack('<H',cleanup))
        state['ports'][target]=(event,cleanup)
        cpu.hook_add(UC_HOOK_CODE,hook,begin=target,end=target)
    def execute(start, end=stop):
        put(stack,stop);cpu.reg_write(UC_X86_REG_ESP,stack)
        cpu.emu_start(start,end,count=1000)
        if cpu.reg_read(UC_X86_REG_EIP)!=end: raise RuntimeError(f'Incomplete original trace {start:x}')
    def call(kind):
        for register in (UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBX):cpu.reg_write(register,scratch)
        if kind in ('R','S'):
            cpu.reg_write(UC_X86_REG_EAX,gen);execute(0x4cbe60 if kind=='R' else 0x457a50)
            return cpu.reg_read(UC_X86_REG_EAX)|(cpu.reg_read(UC_X86_REG_EDX)<<32)
        if kind=='D':
            cpu.reg_write(UC_X86_REG_EBX,item);execute(0x4a6663,0x4a668e)
            return struct.unpack('<Q',cpu.mem_read(item+0x38,8))[0]
        if kind=='Q':
            cpu.reg_write(UC_X86_REG_ESI,item);execute(0x47e854,0x47e917)
            return struct.unpack('<Q',cpu.mem_read(item+0x38,8))[0]
        if kind=='C':
            cpu.reg_write(UC_X86_REG_ESI,item+8);execute(0x481750,0x481810)
            return struct.unpack('<Q',cpu.mem_read(item,8))[0]
        raise RuntimeError(kind)
    def fnv(value, data):
        for byte in data: value=((value^byte)*1099511628211)&0xffffffffffffffff
        return value
    print(f'# ps_game.exe SHA256 {sha}; constructors 00402550/004011c0; complete GenID 004cbe60, server adapter 00457a50; complete UID slices QuestAddItem 0047e854..0047e917, CMob::ItemDrop 004a6663..004a668e, ProcessCharacterCreate 00481750..00481810; real local packer 004cb8e0. Only OS tick/calendar/critical-section responses are ports. R=raw,S=server,D=drop,Q=quest,C=character. FNV covers every generated UID, not only final values.')
    dates=[(2024,1,1,0,0,0),(2032,2,29,23,59,59),(2064,1,1,0,0,0),(1999,12,31,23,59,59),(16,0,0,0,0,0)]
    ticks=[0,1,1999,2000,2001,0x7fffffff,0xfffff82f,0xfffff830,0xfffffff0,0xffffffff,0,1,1999,2000]
    for case in range(128):
        date=dates[case%len(dates)];server=(0,1,127,128,255)[case%5]
        state.update(date=date,events='');cpu.mem_write(gen,bytes(0x30));cpu.mem_write(0x542ca0,bytes([server]))
        cpu.reg_write(UC_X86_REG_ESI,gen);execute(0x4011c0 if case%2 else 0x402550)
        initial_stamp=get(gen+16)
        if [get(gen+i) for i in (4,8,12)]!=[0,0,0] or state['events']!='CI': raise RuntimeError('Constructor state differs')
        print('I',case,server,*date,initial_stamp,sep='\t')
        for step in range(18):
            if step in (0,4,9,14):
                baseline=(0,17,65534,65535,0xfffffffe,0xffffffff)[(case+step)%6]
                before=get(gen+4);cpu.reg_write(UC_X86_REG_EAX,gen);cpu.reg_write(UC_X86_REG_ECX,baseline);execute(0x4494d0)
                if get(gen+4)!=before: raise RuntimeError('SetStartID changed current counter')
                print('B',baseline,sep='\t')
            state.update(tick=ticks[(step+case//5)%len(ticks)],date=dates[(case+step//3)%len(dates)],events='')
            kind='QDSRC'[(step+case)%5]
            # One retained sequence actually crosses all 65536 low-counter values
            # through original interleaved quest/drop bodies at one cached tick.
            repeats=65540 if case==0 and step==2 else (1,2,7)[(case+step)%3]
            digest=14695981039346656037;event_hash=14695981039346656037
            for n in range(repeats):
                actual_kind=('QD'[n%2] if repeats>10 else kind)
                state['events']='';uid=call(actual_kind)
                digest=fnv(digest,struct.pack('<Q',uid));event_hash=fnv(event_hash,state['events'].encode())
            print('O',('X' if repeats>10 else kind),repeats,state['tick'],*state['date'],get(gen+16),digest,uid,event_hash,get(gen+4),get(gen+8),get(gen+12),sep='\t')

if __name__=='__main__':main()
