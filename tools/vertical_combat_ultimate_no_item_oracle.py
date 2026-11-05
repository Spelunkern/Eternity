#!/usr/bin/env python3
"""Trace ultimate KillSoul's no-item DB, logout, and corpse continuation."""
import itertools
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EIP, UC_X86_REG_ESP
from original_oracle import load_game


def main():
    cpu,digest=load_game();user,zone,world,stack,stop=(0x30000000,0x31000000,0x32000000,0x20000000,0x10000000)
    for base in (user,zone,world,stack,stop):cpu.mem_map(base,0x10000)
    def put(at,v):cpu.mem_write(at,struct.pack('<I',v&0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    events=[];case={};packet=[]
    def hook(machine,address,size,_):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x45f740:machine.reg_write(UC_X86_REG_EAX,0)
        elif address==0x45ad50:
            if get(sp+4)!=17:raise RuntimeError('wrong ExpDrop penalty')
            events.append('D')
            if case['mutate_exp']:put(user+0x578c,0x11223344);put(user+0x128,0x55667788)
        elif address==0x4d4f10:
            if get(sp+8)!=10:raise RuntimeError('wrong DB packet size')
            events.append('S');packet.append(bytes(cpu.mem_read(get(sp+4),10)).hex())
            if case['mutate_send']:put(user+0x57dc,9);put(user+0x68,0xffffffff)
        elif address==0x40c860:
            if get(sp+4)!=user or get(user+0x57d8)!=1 or get(user+0x57dc)!=0:raise RuntimeError('wrong logout state')
            events.append('L')
        elif address==0x40d520:
            if get(sp+4)!=60:raise RuntimeError('wrong corpse id')
            events.append('C')
    cpu.hook_add(UC_HOOK_CODE,hook)
    for address,cleanup in ((0x45f740,4),(0x45ad50,4),(0x4d4f10,8),(0x40c860,4),(0x40d520,4)):cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup))
    put(0x546f38,world)
    print(f'# ps_game.exe SHA256 {digest}; CUser::KillSoul 004599e0 no-item branch; ExpDrop/DB/LogoutUser/LeaveDead intercepted')
    print('# timer socket mutate_exp mutate_send final_timer final_type packet events')
    for timer,socket,mutate_exp,mutate_send in itertools.product((0,9),(0,0xffffffff),(0,1),(0,1)):
        cpu.mem_write(user,bytes(0x7000));put(user+0xe0,zone);put(user+0xdc,60);put(user+0x142c,1);put(user+0x1430,0);put(user+0x145c,17);cpu.mem_write(user+0x12f,b'\x03');put(user+0x578c,123);put(user+0x128,456);put(user+0x57dc,timer);put(user+0x57d8,7);put(user+0x68,socket)
        put(stack+0xf000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_EAX,user);events.clear();packet.clear();case.update(mutate_exp=mutate_exp,mutate_send=mutate_send)
        cpu.emu_start(0x4599e0,stop,count=1000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop or len(packet)!=1:raise RuntimeError('original did not return')
        print(timer,socket,mutate_exp,mutate_send,get(user+0x57dc),get(user+0x57d8),packet[0],','.join(events),sep='\t')

if __name__=='__main__':main()
