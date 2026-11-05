#!/usr/bin/env python3
"""Run CUser::Exchange through its real ordered checks and failure sends."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,actor,peer,stop=0x20000000,0x30000000,0x30010000,0x10000000
    for address,size in ((stack,0x10000),(actor,0x20000),(stop,0x1000)):cpu.mem_map(address,size)
    for address,ret in ((0x4d4d20,b'\xc2\x08\x00'),(0x505316,b'\xc3')):cpu.mem_write(address,ret)
    events=[]
    def hook(machine,address,size,data):
        if address==0x46f53e:machine.emu_stop();return
        sp=machine.reg_read(UC_X86_REG_ESP);ptr,length=struct.unpack('<II',machine.mem_read(sp+4,8));target=machine.reg_read(UC_X86_REG_ECX)
        if target not in (actor,peer):raise RuntimeError('wrong send owner')
        events.append(('A' if target==actor else 'B')+bytes(machine.mem_read(ptr,length)).hex())
    for address in (0x4d4d20,0x46f53e):cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
    rng=random.Random(0x46f4a0)
    print(f'# ps_game.exe SHA256 {digest}; CUser::Exchange 0046f4a0 ordered money/slot checks and failure sends, success stops before transfers 0046f53e')
    print('# scenario actor_money peer_money actor_offer peer_offer actor_max peer_max actor_mask peer_mask actor_offers peer_offers events outcome')
    for case in range(1024):
        scenario=case%5;a_money=100;p_money=100;a_offer=p_offer=0;a_max=p_max=1;a_mask=p_mask=0;a_offers=bytearray(24);p_offers=bytearray(24)
        if scenario==0:p_money=5;p_offer=6;a_offer=255
        elif scenario==1:a_money=5;a_offer=6
        elif scenario==2:a_mask=0xffffff;p_offers[0:3]=bytes((1,rng.randrange(24),rng.randrange(256)))
        elif scenario==3:p_mask=0xffffff;a_offers[0:3]=bytes((1,rng.randrange(24),rng.randrange(256)))
        else:
            a_money=rng.choice((0,1,100,0xffffffff));p_money=rng.choice((0,1,100,0xffffffff));a_offer=p_offer=0
            a_offers[0:3]=bytes((1,rng.randrange(24),rng.randrange(256)));p_offers[0:3]=bytes((1,rng.randrange(24),rng.randrange(256)))
            a_mask=rng.choice((0,0xfffffe,0xfffffc));p_mask=rng.choice((0,0xfffffe,0xfffffc))
        for base,size in ((stack,0x10000),(actor,0x20000)):cpu.mem_write(base,bytes(size))
        for base,money,amount,maximum,mask,offers in ((actor,a_money,a_offer,a_max,a_mask,a_offers),(peer,p_money,p_offer,p_max,p_mask,p_offers)):
            cpu.mem_write(base+0x140,struct.pack('<I',money));cpu.mem_write(base+0x1304,struct.pack('<i',maximum));cpu.mem_write(base+0x1570,struct.pack('<I',amount));cpu.mem_write(base+0x1574,bytes(offers))
            for slot in range(24):
                if mask&(1<<slot):cpu.mem_write(base+0x1c4+4*(24+slot),struct.pack('<I',1))
        cpu.mem_write(stack+0x8000,struct.pack('<II',stop,peer));cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EBX,actor);events.clear();cpu.emu_start(0x46f4a0,stop,count=20000)
        ip=cpu.reg_read(UC_X86_REG_EIP)
        if ip not in (stop,0x46f53e):raise RuntimeError('original preflight did not reach failure or transfer')
        expected={'A040a06,B040a07':0,'A040a07,B040a06':1,'A040a04,B040a05':2,'A040a05,B040a04':3}.get(','.join(events),4 if ip==0x46f53e else -1)
        if expected!=scenario:raise RuntimeError(f'unexpected branch {scenario}: {events}')
        print(scenario,a_money,p_money,a_offer,p_offer,a_max,p_max,f'{a_mask:06x}',f'{p_mask:06x}',a_offers.hex(),p_offers.hex(),','.join(events) or '-',expected,sep='\t')

if __name__=='__main__':main()
