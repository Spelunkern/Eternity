#!/usr/bin/env python3
"""Trace the original map-local GetBindPos with retained synthetic map boxes."""
import itertools
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    cpu,digest=load_game();game_map,records,boxes,positions,stack,stop=(0x30000000,0x31000000,0x32000000,0x33000000,0x20000000,0x10000000)
    for base in (game_map,records,boxes,positions,stack,stop):cpu.mem_map(base,0x10000)
    def put(at,v):cpu.mem_write(at,struct.pack('<I',v&0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    values=[];draws=[]
    def hook(machine,address,size,_):
        if address==0x504697:
            if len(draws)>=len(values):raise RuntimeError('too many rand calls')
            value=values[len(draws)];draws.append(value);machine.reg_write(UC_X86_REG_EAX,value)
    cpu.hook_add(UC_HOOK_CODE,hook);cpu.mem_write(0x504697,b'\xc3')
    # Exact x87 sqrt of finite nonnegative double, replacing only CRT setup.
    cpu.mem_write(0x5048a4,b'\xdd\x44\x24\x04\xd9\xfa\xc3')
    print(f'# ps_game.exe SHA256 {digest}; CMap::GetBindPos 00429360; rand intercepted, finite sqrt CRT replaced by x87 fsqrt; x87 control=027f')
    print('# layout country position_index stream result xbits ybits zbits draws')
    positions_list=((10.,2.,10.),(50.,3.,50.),(90.,4.,90.),(-1.,7.,150.),(999.,8.,20.))
    streams=((0,0,0),(1,17,29),(32767,42,103))
    groups=((1,((10.,1.,10.,20.,1.,20.),)),(2,((50.,2.,50.,60.,2.,60.),(70.,3.,70.,80.,3.,80.))),(3,((90.,4.,90.,100.,4.,100.),)))
    for layout,country,position_index,stream in itertools.product((0,1),(0,1),range(len(positions_list)),range(len(streams))):
        cpu.mem_write(game_map,bytes(0x1000));cpu.mem_write(records,bytes(0x1000));cpu.mem_write(boxes,bytes(0x1000));put(game_map+4,120);put(game_map+0x58,3 if layout else 0);put(game_map+0x5c,records)
        offset=0
        for i,(side,items) in enumerate(groups):
            put(records+i*16,len(items));put(records+i*16+4,side);put(records+i*16+12,boxes+offset)
            for box in items:cpu.mem_write(boxes+offset,struct.pack('<6f',*box));offset+=24
        cpu.mem_write(positions,struct.pack('<3f',*positions_list[position_index]));cpu.mem_write(positions+0x100,bytes(12));put(stack+0xf000,stop);put(stack+0xf004,game_map);put(stack+0xf008,country);put(stack+0xf00c,positions);put(stack+0xf010,positions+0x100);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_FPCW,0x027f);values[:]=streams[stream];draws.clear();cpu.emu_start(0x429360,stop,count=10000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('original bind did not return')
        print(layout,country,position_index,stream,cpu.reg_read(UC_X86_REG_EAX)&255,*[get(positions+0x100+i*4) for i in range(3)],len(draws),sep='\t')

if __name__=='__main__':main()
