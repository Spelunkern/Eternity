#!/usr/bin/env python3
"""Trace KillSoul after a selected ultimate rebirth item."""
import itertools
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_EIP, UC_X86_REG_ESP
from original_oracle import load_game


def main():
    cpu,digest=load_game();user,zone,world,stack,stop=(0x30000000,0x31000000,0x32000000,0x20000000,0x10000000)
    for base in (user,zone,world,stack,stop):cpu.mem_map(base,0x10000)
    def put(at,v):cpu.mem_write(at,struct.pack('<I',v&0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def word(at,v):cpu.mem_write(at,struct.pack('<H',v))
    events=[];case={}
    def hook(machine,address,size,_):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x45f740:
            if get(sp+4)!=0:raise RuntimeError('wrong item preference')
            put(machine.reg_read(UC_X86_REG_EDX),1);put(machine.reg_read(UC_X86_REG_ECX),5);machine.reg_write(UC_X86_REG_EAX,1)
        elif address==0x465120:
            if machine.reg_read(UC_X86_REG_EBX)!=1 or get(sp+4)!=5 or get(sp+8)!=0:raise RuntimeError('wrong item use arguments')
            events.append('I');put(user+0xe0,zone if case['after_zone'] else 0)
            if case['change_max']:put(user+0x17c,303)
        elif address==0x4295a0:
            events.append('Z');cpu.mem_write(get(sp+12),struct.pack('<3f',1,2,3));put(get(sp+16),9)
        elif address==0x429360:
            events.append('M');cpu.mem_write(get(sp+16),struct.pack('<3f',4,5,6));word(user+0x164,8 if case['change_map'] else 7)
        elif address==0x459ec0:
            pos=get(sp+4);xyz=struct.unpack('<3f',cpu.mem_read(pos,12));events.append(f'R{get(sp+8)}:{get(sp+12)}:{get(sp+16)}:{int(xyz[0])},{int(xyz[1])},{int(xyz[2])}')
    cpu.hook_add(UC_HOOK_CODE,hook)
    for address,cleanup in ((0x45f740,4),(0x465120,8),(0x4295a0,16),(0x429360,16),(0x459ec0,16)):cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup))
    put(0x546f38,world);put(zone+0x24,zone+0x1000)
    print(f'# ps_game.exe SHA256 {digest}; CUser::KillSoul 004599e0 selected item branch; ItemUseNSend/GetBindPos/Rebirth intercepted')
    print('# after_zone map_present change_map change_max final_hp final_sp final_mp events')
    for after_zone,map_present,change_map,change_max in itertools.product((0,1),repeat=4):
        cpu.mem_write(user,bytes(0x7000));put(user+0xe0,zone);put(user+0x142c,1);put(user+0x1430,0);put(user+0x145c,17);cpu.mem_write(user+0x12f,b'\x03');word(user+0x164,7);cpu.mem_write(user+0xd0,struct.pack('<3f',7,8,9));put(user+0x17c,100);put(user+0x180,200);put(user+0x184,300);put(world+8,8 if map_present else 7);put(world+12,0x34000000)
        put(stack+0xf000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_EAX,user);events.clear();case.update(after_zone=after_zone,change_map=change_map,change_max=change_max)
        cpu.emu_start(0x4599e0,stop,count=1000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('original did not return')
        print(after_zone,map_present,change_map,change_max,get(user+0x1238),get(user+0x1240),get(user+0x123c),','.join(events),sep='\t')

if __name__=='__main__':main()
