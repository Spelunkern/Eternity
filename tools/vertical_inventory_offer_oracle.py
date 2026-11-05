#!/usr/bin/env python3
"""Trace original 0a06 item offers with real item bytes and observed sends."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    stack, actor, peer, packet, item, definition, stop = 0x20000000, 0x30000000, 0x30010000, 0x31000000, 0x32000000, 0x33000000, 0x10000000
    for address, size in ((0,0x10000),(stack,0x10000),(actor,0x20000),(packet,0x10000),(item,0x10000),(definition,0x10000),(stop,0x1000)):
        cpu.mem_map(address,size)
    for address, ret in ((0x478520,b'\xc3'),(0x4d4d20,b'\xc2\x08\x00'),(0x505316,b'\xc3')):
        cpu.mem_write(address,ret)
    events=[]
    def hook(machine,address,size,data):
        if address==0x478520:
            if machine.reg_read(UC_X86_REG_ECX)!=actor+0x15dc: raise RuntimeError('wrong shop owner')
            events.append('E');return
        sp=machine.reg_read(UC_X86_REG_ESP);ptr,length=struct.unpack('<II',machine.mem_read(sp+4,8));target=machine.reg_read(UC_X86_REG_ECX)
        if target not in (actor,peer): raise RuntimeError('wrong send owner')
        events.append(('A' if target==actor else 'B')+bytes(machine.mem_read(ptr,length)).hex())
    for address in (0x478520,0x4d4d20): cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
    rng=random.Random(0xa06)
    print(f'# ps_game.exe SHA256 {digest}; CUser::PacketExchange 0046ec90 item offer 0046efc5..0046f1bf; ExchangeCancleReady 0046f450; observed shop and sends')
    print('# mode peer ready peer_ready max_bags bag slot count index item_present real_type item_count initial_offers item_bytes events final_ready final_peer_ready final_offers')
    for case in range(1024):
        mode=3 if case%5 else rng.choice((0,1,2,4));present=0 if case%11==0 else 1
        ready=rng.choice((0,1,255));peer_ready=rng.choice((0,1,255));max_bags=rng.choice((0,1,2,9))
        bag=rng.choice((0,1,1,1,2,9));slot=rng.choice((2,2,2,23,24));count=rng.choice((0,1,5,20,255));index=rng.choice((0,1,3,7,8,255));has_item=case%7!=0
        real_type=27 if case%13==0 else 5;item_count=rng.choice((1,5,20,255));offers=bytearray(24)
        if case%6==0 and index<8: offers[index*3:index*3+3]=bytes((9,4,2))
        if case%9==0: offers[21:24]=bytes((bag,slot,3))
        raw=bytes((5,1,item_count,0))+struct.pack('<H',0xabcd)+bytes((1,2,3,4,5,6))+b'hello\0'+bytes(range(15))
        for address,size in ((stack,0x10000),(actor,0x20000),(packet,0x10000),(item,0x10000),(definition,0x10000)): cpu.mem_write(address,bytes(size))
        cpu.mem_write(actor+0x1568,struct.pack('<II',mode,peer if present else 0));cpu.mem_write(actor+0x1304,struct.pack('<i',max_bags));cpu.mem_write(actor+0x158c,bytes((ready,)));cpu.mem_write(peer+0x158c,bytes((peer_ready,)));cpu.mem_write(actor+0x1574,bytes(offers))
        cpu.mem_write(item+0x30,struct.pack('<I',definition));cpu.mem_write(item+0x40,raw);cpu.mem_write(definition+0xa0,struct.pack('<I',real_type))
        if has_item and 0<bag<=9 and slot<24: cpu.mem_write(actor+0x1c4+4*(bag*24+slot),struct.pack('<I',item))
        cpu.mem_write(packet,struct.pack('<HBBBB',0xa06,bag,slot,count,index));cpu.mem_write(stack+0x8000,struct.pack('<I',stop));events.clear()
        cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ECX,actor);cpu.reg_write(UC_X86_REG_EDX,packet);cpu.emu_start(0x46ec90,stop,count=10000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop: raise RuntimeError('original handler did not return')
        print(mode,present,ready,peer_ready,max_bags,bag,slot,count,index,int(has_item),real_type,item_count,offers.hex(),raw.hex(),','.join(events),cpu.mem_read(actor+0x158c,1)[0],cpu.mem_read(peer+0x158c,1)[0],bytes(cpu.mem_read(actor+0x1574,24)).hex(),sep='\t')

if __name__=='__main__': main()
