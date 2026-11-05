#!/usr/bin/env python3
"""Trace zone GetBindPos overrides and local fallback."""
import itertools
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EDI, UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    cpu,digest=load_game();current,records,boxes,positions,world,catalog,stack,stop=(0x30000000,0x31000000,0x32000000,0x33000000,0x34000000,0x35000000,0x20000000,0x10000000)
    for base in (current,records,boxes,positions,world,catalog,stack,stop):cpu.mem_map(base,0x10000)
    def put(at,v):cpu.mem_write(at,struct.pack('<I',v&0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    values=[];draws=[]
    def hook(machine,address,size,_):
        if address==0x504697:
            if len(draws)>=len(values):raise RuntimeError('too many rand calls')
            value=values[len(draws)];draws.append(value);machine.reg_write(UC_X86_REG_EAX,value)
    cpu.hook_add(UC_HOOK_CODE,hook);cpu.mem_write(0x504697,b'\xc3');cpu.mem_write(0x5048a4,b'\xdd\x44\x24\x04\xd9\xfa\xc3');put(0x546f38,world);put(world+8,3);put(world+12,catalog)
    for index in range(3):put(catalog+index*0x1f0+4,120)
    print(f'# ps_game.exe SHA256 {digest}; CMap::GetBindPos 004295a0; rand intercepted, finite CRT sqrt replaced by x87 fsqrt; x87 control=027f')
    print('# layout country position_index stream override result map xbits ybits zbits draws')
    positions_list=((10.,2.,10.),(50.,3.,50.),(-1.,7.,150.));streams=((0,0,0),(1,17,29))
    groups=((1,((10.,1.,10.,20.,1.,20.),)),(2,((50.,2.,50.,60.,2.,60.),(70.,3.,70.,80.,3.,80.))),(3,((90.,4.,90.,100.,4.,100.),)))
    for layout,country,position_index,stream,override in itertools.product((0,1),(0,1),range(3),range(2),range(4)):
        cpu.mem_write(current,bytes(0x1000));cpu.mem_write(records,bytes(0x1000));cpu.mem_write(boxes,bytes(0x1000));put(current+4,120);put(current+0x58,3 if layout else 0);put(current+0x5c,records);put(current+0x78,7)
        for side in range(2):
            base=current+0x184+side*16;put(base,(-1,side+1,side+1,3)[override]);cpu.mem_write(base+4,struct.pack('<3f',130. if override==2 else 15.+side*10,4.+side,25.+side*10))
        offset=0
        for i,(side,items) in enumerate(groups):
            put(records+i*16,len(items));put(records+i*16+4,side);put(records+i*16+12,boxes+offset)
            for box in items:cpu.mem_write(boxes+offset,struct.pack('<6f',*box));offset+=24
        cpu.mem_write(positions,struct.pack('<3f',*positions_list[position_index]));cpu.mem_write(positions+0x100,bytes(12));put(positions+0x200,0xdeadbeef);put(stack+0xf000,stop);put(stack+0xf004,country);put(stack+0xf008,positions);put(stack+0xf00c,positions+0x100);put(stack+0xf010,positions+0x200);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_EDI,current);cpu.reg_write(UC_X86_REG_FPCW,0x027f);values[:]=streams[stream];draws.clear();cpu.emu_start(0x4295a0,stop,count=10000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('original zone bind did not return')
        print(layout,country,position_index,stream,override,cpu.reg_read(UC_X86_REG_EAX)&255,get(positions+0x200),*[get(positions+0x100+i*4) for i in range(3)],len(draws),sep='\t')

if __name__=='__main__':main()
