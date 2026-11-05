#!/usr/bin/env python3
"""Run the original exchange timeout prefix with real CExchange resets."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,actor,peer,stop=0x20000000,0x30000000,0x30010000,0x10000000
    for address,size in ((stack,0x10000),(actor,0x20000),(stop,0x1000)):cpu.mem_map(address,size)
    def hook(machine,address,size,data):machine.emu_stop()
    cpu.hook_add(UC_HOOK_CODE,hook,begin=0x48e39c,end=0x48e39c)
    rng=random.Random(0x48e310)
    print(f'# ps_game.exe SHA256 {digest}; CUser::UpdateTimeover 0048e310 exchange prefix ..0048e39c and CExchange::Reset 00449660')
    print('# mode peer_mode peer_kind timeout peer_timeout now money peer_money ready peer_ready offers peer_offers final_mode final_peer_mode final_peer final_remote_peer final_timeout final_peer_timeout final_money final_peer_money final_ready final_peer_ready final_offers final_peer_offers')
    def state(base):
        mode,partner,money=struct.unpack('<III',cpu.mem_read(base+0x1568,12));return mode,{0:0,actor:1,peer:2}[partner],money,cpu.mem_read(base+0x158c,1)[0],bytes(cpu.mem_read(base+0x1574,24)).hex(),struct.unpack('<I',cpu.mem_read(base+0x5830,4))[0]
    for case in range(1024):
        mode=rng.choice((0,1,1,2,2,3,4));peer_mode=rng.choice((0,1,1,2,3));kind=rng.randrange(3);timeout=rng.choice((0,1,100,0xfffffff0,0xffffffff));now=rng.choice((0,1,99,100,101,0xfffffff0,0xffffffff));peer_timeout=rng.getrandbits(32);money=rng.getrandbits(32);peer_money=rng.getrandbits(32);ready=rng.randrange(256);peer_ready=rng.randrange(256);offers=bytes(rng.randrange(256) for _ in range(24));remote=bytes(rng.randrange(256) for _ in range(24))
        for base,size in ((stack,0x10000),(actor,0x20000)):cpu.mem_write(base,bytes(size))
        cpu.mem_write(actor+0x1568,struct.pack('<III',mode,(0,actor,peer)[kind],money));cpu.mem_write(peer+0x1568,struct.pack('<III',peer_mode,actor,peer_money));cpu.mem_write(actor+0x1574,offers);cpu.mem_write(peer+0x1574,remote);cpu.mem_write(actor+0x158c,bytes((ready,)));cpu.mem_write(peer+0x158c,bytes((peer_ready,)));cpu.mem_write(actor+0x5830,struct.pack('<I',timeout));cpu.mem_write(peer+0x5830,struct.pack('<I',peer_timeout))
        cpu.mem_write(stack+0x8000,struct.pack('<II',stop,now));cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ESI,actor);cpu.emu_start(0x48e310,stop,count=10000)
        if cpu.reg_read(UC_X86_REG_EIP)!=0x48e39c:raise RuntimeError('original timeout prefix did not finish')
        a=state(actor);b=state(peer)
        print(mode,peer_mode,kind,timeout,peer_timeout,now,money,peer_money,ready,peer_ready,offers.hex(),remote.hex(),a[0],b[0],a[1],b[1],a[5],b[5],a[2],b[2],a[3],b[3],a[4],b[4],sep='\t')

if __name__=='__main__':main()
