#!/usr/bin/env python3
"""Run original exchange money and item capacity preflight checks."""
import random
import struct
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,actor,recipient,exchange,stop=0x20000000,0x30000000,0x30010000,0x31000000,0x10000000
    for address,size in ((stack,0x10000),(actor,0x20000),(exchange,0x10000),(stop,0x1000)):cpu.mem_map(address,size)
    rng=random.Random(0x46f860)
    print(f'# ps_game.exe SHA256 {digest}; CUser::ExchangeMoneyAddEnable 0046f830 and ExchangeItemAddEnable 0046f860; actual original code')
    print('# operation max_bags offer_bytes bag_masks payer_money recipient_money amount result')
    for case in range(2048):
        op=case%2;maximum=rng.choice((-1,0,1,1,2,9));offers=bytearray(rng.randrange(256) for _ in range(24));count=rng.randrange(9)
        for index in range(8):offers[3*index]=rng.choice((1,9,255)) if index<count else 0
        masks=[rng.choice((0,0xffffff,0xfffffe,0xfffffc,0xffff00,rng.getrandbits(24))) for _ in range(9)]
        payer=rng.choice((0,1,100,0xfffffffe,0xffffffff,rng.getrandbits(32)));balance=rng.choice((0,1,0xfffffffe,0xffffffff,rng.getrandbits(32)));amount=rng.choice((0,1,payer,0xffffffff,(0xffffffff-balance),rng.getrandbits(32)))
        for address,size in ((stack,0x10000),(actor,0x20000),(exchange,0x10000)):cpu.mem_write(address,bytes(size))
        cpu.mem_write(exchange+8,struct.pack('<I',amount));cpu.mem_write(exchange+12,bytes(offers));cpu.mem_write(actor+0x1304,struct.pack('<i',maximum));cpu.mem_write(actor+0x140,struct.pack('<I',payer));cpu.mem_write(recipient+0x140,struct.pack('<I',balance))
        for bag,mask in enumerate(masks,1):
            for slot in range(24):
                if mask & (1<<slot):cpu.mem_write(actor+0x1c4+4*(bag*24+slot),struct.pack('<I',1))
        cpu.mem_write(stack+0x8000,struct.pack('<II',stop,recipient));cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,exchange);cpu.reg_write(UC_X86_REG_ECX,actor);cpu.emu_start(0x46f830 if op==0 else 0x46f860,stop,count=10000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('original capacity check did not return')
        print(op,maximum,offers.hex(),','.join(f'{mask:06x}' for mask in masks),payer,balance,amount,cpu.reg_read(UC_X86_REG_EAX)&255,sep='\t')

if __name__=='__main__':main()
