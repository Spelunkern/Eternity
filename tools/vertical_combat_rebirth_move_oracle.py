#!/usr/bin/env python3
"""Trace MoveUser's vehicle prefix and MoveUserLocal dispatch."""
import itertools
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESI, UC_X86_REG_ESP
from original_oracle import load_game


def main():
    cpu,digest=load_game();actor,peer,zone,stack,stop=(0x30000000,0x31000000,0x32000000,0x20000000,0x10000000)
    for base in (actor,peer,zone,stack,stop):cpu.mem_map(base,0x10000)
    def put(at,v):cpu.mem_write(at,struct.pack('<I',v&0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    events=[];state={}
    def hook(machine,address,size,_):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x415f30:
            machine.reg_write(UC_X86_REG_EAX,peer if get(sp+4)==72 else 0)
        elif address==0x4208c0:
            if get(sp+12)!=10:raise RuntimeError('wrong vehicle packet size')
            events.append('V'+bytes(cpu.mem_read(get(sp+8),10)).hex())
        elif address==0x416370:
            if machine.reg_read(UC_X86_REG_EAX)!=actor:raise RuntimeError('wrong MoveUserLocal actor')
            events.append('L');machine.reg_write(UC_X86_REG_EAX,state['result'])
    cpu.hook_add(UC_HOOK_CODE,hook)
    for address,cleanup in ((0x415f30,4),(0x4208c0,12),(0x416370,16)):cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup))
    print(f'# ps_game.exe SHA256 {digest}; CZone::MoveUser 00416260; FindUser, SendView and MoveUserLocal intercepted')
    print('# mode partner peer_zone local_result result actor_mode actor_partner peer_mode peer_partner events')
    for mode,partner,peer_zone,local_result in itertools.product((0,1,2,3,4),(0,72,999),(0,1),(0,1)):
        cpu.mem_write(actor,bytes(0x7000));cpu.mem_write(peer,bytes(0x7000));put(actor+0xdc,71);put(actor+0xe0,zone);put(actor+0x1460,mode);put(actor+0x1470,partner);put(peer+0xdc,72);put(peer+0xe0,zone if peer_zone else 0);put(peer+0x1460,9);put(peer+0x1470,71)
        put(stack+0xf000,stop);put(stack+0xf004,zone);put(stack+0xf008,7);put(stack+0xf00c,0x3f800000);put(stack+0xf010,0x40000000);put(stack+0xf014,0x40400000);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_ESI,actor);state['result']=local_result;events.clear();cpu.emu_start(0x416260,stop,count=1000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('original MoveUser did not return')
        print(mode,partner,peer_zone,local_result,cpu.reg_read(UC_X86_REG_EAX)&255,get(actor+0x1460),get(actor+0x1470),get(peer+0x1460),get(peer+0x1470),','.join(events),sep='\t')

if __name__=='__main__':main()
