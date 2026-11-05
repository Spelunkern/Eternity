#!/usr/bin/env python3
"""Trace original nonzero 0a05 exchange controls and both exchange states."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,actor,peer,packet,stop=0x20000000,0x30000000,0x30010000,0x31000000,0x10000000
    for address,size in ((0,0x10000),(stack,0x10000),(actor,0x20000),(packet,0x10000),(stop,0x1000)):cpu.mem_map(address,size)
    for address,ret in ((0x478520,b'\xc3'),(0x4d4d20,b'\xc2\x08\x00'),(0x505316,b'\xc3')):cpu.mem_write(address,ret)
    events=[]
    def hook(machine,address,size,data):
        if address==0x478520:
            if machine.reg_read(UC_X86_REG_ECX)!=actor+0x15dc:raise RuntimeError('wrong shop owner')
            events.append('E');return
        sp=machine.reg_read(UC_X86_REG_ESP);ptr,length=struct.unpack('<II',machine.mem_read(sp+4,8));target=machine.reg_read(UC_X86_REG_ECX)
        if target not in (actor,peer):raise RuntimeError('wrong send owner')
        events.append(('A' if target==actor else 'B')+bytes(machine.mem_read(ptr,length)).hex())
    for address in (0x478520,0x4d4d20):cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
    rng=random.Random(0xa05)
    print(f'# ps_game.exe SHA256 {digest}; CUser::PacketExchange 0046ec90 control 0046eea1..0046ef44; CExchange::Reset 00449660; observed shop and sends')
    print('# mode peer_mode peer_present response ready peer_ready money peer_money offers peer_offers events final_mode final_peer_mode final_peer_present final_remote_peer final_ready final_peer_ready final_money final_peer_money final_offers final_peer_offers')
    def get(base):
        mode,partner,money=struct.unpack('<III',cpu.mem_read(base+0x1568,12));offers=bytes(cpu.mem_read(base+0x1574,24)).hex();ready=cpu.mem_read(base+0x158c,1)[0]
        return mode,int(partner!=0),ready,money,offers
    for case in range(512):
        mode=rng.choice((0,1,2,2,2,3,4));peer_mode=rng.choice((0,1,1,1,2,3));present=rng.randrange(2);response=rng.choice((1,1,2,2,3,255));ready=rng.randrange(256);peer_ready=rng.randrange(256);money=rng.getrandbits(32);peer_money=rng.getrandbits(32);offers=bytes(rng.randrange(256) for _ in range(24));remote=bytes(rng.randrange(256) for _ in range(24))
        for base,size in ((stack,0x10000),(actor,0x20000),(packet,0x10000)):cpu.mem_write(base,bytes(size))
        cpu.mem_write(actor+0xdc,struct.pack('<I',1001));cpu.mem_write(peer+0xdc,struct.pack('<I',1002))
        cpu.mem_write(actor+0x1568,struct.pack('<III',mode,peer if present else 0,money));cpu.mem_write(peer+0x1568,struct.pack('<III',peer_mode,actor,peer_money));cpu.mem_write(actor+0x1574,offers);cpu.mem_write(peer+0x1574,remote);cpu.mem_write(actor+0x158c,bytes((ready,)));cpu.mem_write(peer+0x158c,bytes((peer_ready,)))
        cpu.mem_write(packet,struct.pack('<HB',0xa05,response));cpu.mem_write(stack+0x8000,struct.pack('<I',stop));events.clear();cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ECX,actor);cpu.reg_write(UC_X86_REG_EDX,packet);cpu.emu_start(0x46ec90,stop,count=10000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('original handler did not return')
        a=get(actor);b=get(peer)
        print(mode,peer_mode,present,response,ready,peer_ready,money,peer_money,offers.hex(),remote.hex(),','.join(events),a[0],b[0],a[1],b[1],a[2],b[2],a[3],b[3],a[4],b[4],sep='\t')

if __name__=='__main__':main()
