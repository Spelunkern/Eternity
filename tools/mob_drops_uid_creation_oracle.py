#!/usr/bin/env python3
"""Execute original character starter UID/make-time loop and actual DB packet send."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP)
from original_oracle import load_game


def main():
    cpu,sha=load_game();base=0x30000000;cpu.mem_map(base,0x10000)
    stack,stop,items=base+0x8000,base+0xf000,base+0x1000;gen=0x547ce0;state={}
    def put(at,value):cpu.mem_write(at,struct.pack('<I',value&0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    ports={0x5210cc:('L',4),0x5210d0:('U',4),0x5210d4:('I',4),0x5210e4:('T',0),0x5210dc:('G',4)}
    tick_sets=[(0,1,1999,2000),(0xfffffff0,0xffffffff,0,1999),(10000,11999,12000,13999),(0,0,0,0)]
    def hook(machine,at,size,_):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if at==0x4d4f10:
            length=get(sp+8);packet=get(sp+4)
            if length!=75+23*state['count']:raise RuntimeError(f'Original DB packet length {length} differs from {75+23*state["count"]}; made {state["make"]}')
            state['payload']=bytes(cpu.mem_read(packet+75,23*state['count']));state['events']+='P';return
        event=state['ports'][at]
        if event=='T':
            state['index']+=1;machine.reg_write(UC_X86_REG_EAX,tick_sets[state['case']%4][state['index']%4])
        elif event=='G':
            make=get(sp)==0x481835
            if make:second=30+state['make'];state['make']+=1;event='M'
            else:second=0 if state['index']<0 else 10+state['index']%20
            machine.mem_write(get(sp+4),struct.pack('<8H',2032,1,0,1,0,0,second,0))
        state['events']+=event
    state['ports']={}
    for i,(iat,(event,cleanup)) in enumerate(ports.items()):
        target=stop+16+i*16;put(iat,target);cpu.mem_write(target,b'\xc2'+struct.pack('<H',cleanup));state['ports'][target]=event
        cpu.hook_add(UC_HOOK_CODE,hook,begin=target,end=target)
    cpu.mem_write(0x4d4f10,b'\xc2\x08\x00');cpu.hook_add(UC_HOOK_CODE,hook,begin=0x4d4f10,end=0x4d4f10)
    def execute(start,end=stop):
        put(stack,stop);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.emu_start(start,end,count=10000)
        if cpu.reg_read(UC_X86_REG_EIP)!=end:raise RuntimeError('Incomplete original creation UID loop')
    def external():
        cpu.reg_write(UC_X86_REG_EAX,gen);execute(0x457a50)
        return cpu.reg_read(UC_X86_REG_EAX)|(cpu.reg_read(UC_X86_REG_EDX)<<32)
    print(f'# ps_game.exe SHA256 {sha}; ProcessCharacterCreate 00481750..004818ce complete starter loop through original 0404 DB packet send, actual UID/local packing. Shared original server GenID before/after. OS tick/calendar/lock and final DB transport are ports; character stats/header preparation and SQL execution are not claimed by this oracle.')
    for case in range(64):
        count=2+case%9;server=(0,3,255)[case%3];baseline=(0,65534,0xfffffffe)[case%3]
        state.update(case=case,count=count,index=-1,make=0,events='',payload=b'')
        cpu.mem_write(gen,bytes(0x30));cpu.mem_write(0x542ca0,bytes([server]));cpu.reg_write(UC_X86_REG_ESI,gen);execute(0x4011c0)
        cpu.reg_write(UC_X86_REG_EAX,gen);cpu.reg_write(UC_X86_REG_ECX,baseline);execute(0x4494d0)
        state['events']='';before=external()
        cpu.mem_write(stack,bytes(0x400));cpu.mem_write(items,bytes([count]))
        for i in range(count):
            cpu.mem_write(items+4+i*12,struct.pack('<4BIBBH',1,i,25,i+1,25001+i,i+1,0,1200+i))
        put(stack+0x14,items);put(stack+0x10,0)
        cpu.reg_write(UC_X86_REG_ESI,stack+0x8d);cpu.reg_write(UC_X86_REG_EDI,items+6)
        execute(0x481750,0x4818ce);after=external()
        if state['make']!=count:raise RuntimeError('Missing make-time samples')
        print(case,count,server,baseline,before,after,state['events'],state['payload'].hex(),sep='\t')

if __name__=='__main__':main()
