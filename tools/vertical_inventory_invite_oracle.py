#!/usr/bin/env python3
"""Trace original 0a01 invitations, same-zone lookup, packets and timeout."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,actor,peer,packet,stop=0x20000000,0x30000000,0x30010000,0x31000000,0x10000000
    for address,size in ((0,0x10000),(stack,0x10000),(actor,0x20000),(packet,0x10000),(stop,0x1000)):cpu.mem_map(address,size)
    for address,ret in ((0x478520,b'\xc3'),(0x415f30,b'\xc2\x04\x00'),(0x4d4d20,b'\xc2\x08\x00'),(0x505316,b'\xc3'),(stop+0x100,b'\xc3')):cpu.mem_write(address,ret)
    cpu.mem_write(0x5210e4,struct.pack('<I',stop+0x100));events=[];current={}
    def hook(machine,address,size,data):
        if address==0x478520:
            if machine.reg_read(UC_X86_REG_ECX)!=actor+0x15dc:raise RuntimeError('wrong shop owner')
            events.append('E');return
        if address==stop+0x100:
            events.append('T');machine.reg_write(UC_X86_REG_EAX,current['tick']);return
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x415f30:
            if machine.reg_read(UC_X86_REG_EAX)!=5:raise RuntimeError('wrong lookup zone')
            identity=struct.unpack('<I',machine.mem_read(sp+4,4))[0];machine.reg_write(UC_X86_REG_EAX,{1001:actor,1002:peer}.get(identity,0));return
        ptr,length=struct.unpack('<II',machine.mem_read(sp+4,8));target=machine.reg_read(UC_X86_REG_ECX)
        if target not in (actor,peer):raise RuntimeError('wrong send owner')
        events.append(('A' if target==actor else 'B')+bytes(machine.mem_read(ptr,length)).hex())
    for address in (0x478520,0x415f30,0x4d4d20,stop+0x100):cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
    rng=random.Random(0xa01)
    print(f'# ps_game.exe SHA256 {digest}; CUser::PacketExchange 0046ec90 invite 0046ecc9..0046edce; CZone::FindUser 00415f30; observed shop, lookup, send and tick')
    print('# mode peer_mode country peer_country target tick timeout events final_mode final_peer_mode final_peer final_remote_peer final_timeout')
    def state(base):
        mode,partner=struct.unpack('<II',cpu.mem_read(base+0x1568,8));return mode,{0:0,actor:1,peer:2}[partner]
    for case in range(512):
        mode=rng.choice((0,0,0,1,2,3,4));peer_mode=rng.choice((0,0,0,1,2,3));country=rng.choice((0,1,2,255));peer_country=country if case%2 else rng.choice((0,1,2,255));target=rng.choice((1001,1002,1002,9999));tick=rng.choice((0,100,0xfffffff0,rng.getrandbits(32)));timeout=rng.getrandbits(32)
        for base,size in ((stack,0x10000),(actor,0x20000),(packet,0x10000)):cpu.mem_write(base,bytes(size))
        cpu.mem_write(actor+0xdc,struct.pack('<II',1001,5));cpu.mem_write(peer+0xdc,struct.pack('<II',1002,5));cpu.mem_write(actor+0x12d,bytes((country,)));cpu.mem_write(peer+0x12d,bytes((peer_country,)))
        cpu.mem_write(actor+0x1568,struct.pack('<II',mode,0));cpu.mem_write(peer+0x1568,struct.pack('<II',peer_mode,0));cpu.mem_write(actor+0x5830,struct.pack('<I',timeout));cpu.mem_write(packet,struct.pack('<HI',0xa01,target));cpu.mem_write(stack+0x8000,struct.pack('<I',stop));events.clear();current['tick']=tick
        cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ECX,actor);cpu.reg_write(UC_X86_REG_EDX,packet);cpu.emu_start(0x46ec90,stop,count=10000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('original handler did not return')
        a=state(actor);b=state(peer);final_timeout=struct.unpack('<I',cpu.mem_read(actor+0x5830,4))[0]
        print(mode,peer_mode,country,peer_country,target,tick,timeout,','.join(events),a[0],b[0],a[1],b[1],final_timeout,sep='\t')

if __name__=='__main__':main()
