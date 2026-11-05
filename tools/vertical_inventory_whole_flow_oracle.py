#!/usr/bin/env python3
"""Trace ExchangeItemAdd whole-item DB, client, and log sends."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EBX, UC_X86_REG_EBP, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    stack, source, definition, offer, actor, peer, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x33000000, 0x33010000, 0x10000000
    new = 0x34000000
    for address, size in ((0,0x10000),(stack,0x10000),(source,0x10000),(definition,0x10000),(offer,0x10000),(actor,0x20000),(new,0x10000),(stop,0x1000)):
        cpu.mem_map(address,size)
    cpu.mem_write(0x40a510,b'\xb8'+struct.pack('<I',new)+b'\xc3')
    cpu.mem_write(stop+0x300,b'\xb8\x64\x00\x00\x00\xc3');cpu.mem_write(0x5210e4,struct.pack('<I',stop+0x300));cpu.mem_write(0x542ca0,b'\x07')
    cpu.mem_write(stop+0x100,b'\xc2\x04\x00');cpu.mem_write(stop+0x200,b'\xc2\x04\x00')
    for iat in (0x5210cc,0x5210d0):cpu.mem_write(iat,struct.pack('<I',stop+0x100))
    cpu.mem_write(0x5210dc,struct.pack('<I',stop+0x200))
    for address in (0x4d4e50,0x4d4d20,0x4d4f10):cpu.mem_write(address,b'\xc2\x08\x00')
    events=[];scratches=[]
    def hook(machine,address,size,data):
        if address==0x4700de:machine.emu_stop();return
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==stop+0x200:
            ptr=struct.unpack('<I',machine.mem_read(sp+4,4))[0];machine.mem_write(ptr,struct.pack('<8H',2026,11,0,2,13,47,59,0));return
        if address==0x45b0d0:
            ret=struct.unpack('<I',machine.mem_read(sp,4))[0];kind=0 if ret==0x46fe60 else 1 if ret==0x470000 else -1
            if kind<0:raise RuntimeError('unexpected log header')
            scratches.append(bytes(machine.mem_read(machine.reg_read(UC_X86_REG_ESI),181 if kind==0 else 179)).hex());return
        if address==0x46fa10:
            scratches.append(bytes(machine.mem_read(sp+0xd4,57)).hex());return
        if address==0x46ff56:
            scratches.append(bytes(machine.mem_read(sp+0x74,36)).hex());return
        ptr,length=struct.unpack('<II',machine.mem_read(sp+4,8));target=machine.reg_read(UC_X86_REG_ECX)
        prefix='G' if address==0x4d4e50 else 'D' if address==0x4d4f10 else 'A' if target==actor else 'B' if target==peer else '?'
        events.append(prefix+bytes(machine.mem_read(ptr,length)).hex())
    for address in (0x45b0d0,0x46fa10,0x46ff56,0x4d4e50,0x4d4d20,0x4d4f10,stop+0x200,0x4700de):cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
    rng=random.Random(0x46fa10)
    print(f'# ps_game.exe SHA256 {digest}; CUser::ExchangeItemAdd 0046fa10..004700de, whole item and sends')
    print('# uid item_id offered bag slot payload made db_scratch outgoing_scratch receiver_scratch incoming_scratch db_add db_remove sender log_out receiver log_in final_count')
    for case in range(64):
        cpu.mem_write(0x547ce4,bytes(16));cpu.mem_write(new,bytes(0x10000));cpu.mem_write(stack,bytes((case,))*0x10000);cpu.mem_write(source,bytes(0x10000));cpu.mem_write(definition,bytes(0x10000));cpu.mem_write(offer,bytes(0x10000));cpu.mem_write(actor,bytes(0x20000));events.clear();scratches.clear()
        for ptr,account,character,level,experience,mapid,pos,account_name,char_name in ((actor,12345,1001,30,123,7,(10.,-1.,20.),b'account-a',b'Actor'),(peer,12346,1002,31,456,8,(30.,-2.,40.),b'account-b',b'Peer')):
            cpu.mem_write(ptr+0x578c,struct.pack('<I',account));cpu.mem_write(ptr+0x128,struct.pack('<I',character));cpu.mem_write(ptr+0x136,struct.pack('<H',level));cpu.mem_write(ptr+0x13c,struct.pack('<I',experience));cpu.mem_write(ptr+0x164,struct.pack('<H',mapid));cpu.mem_write(ptr+0xd0,struct.pack('<3f',*pos));cpu.mem_write(ptr+0x5794,account_name+b'\0');cpu.mem_write(ptr+0x188,char_name+b'\0')
        uid=rng.getrandbits(64);item_id=rng.getrandbits(32);offered=rng.randrange(1,20);bag=rng.randrange(1,6);slot=rng.randrange(24);count=rng.randrange(21,255);made=rng.getrandbits(32)
        offered=count
        payload=bytes((rng.randrange(256),rng.randrange(256),count,0))+struct.pack('<H',rng.randrange(65536))+bytes(rng.randrange(256) for _ in range(6))+b'craft\0'+bytes(rng.randrange(256) for _ in range(15))
        cpu.mem_write(definition,struct.pack('<I',item_id)+b'Strange item\0');cpu.mem_write(source+0x30,struct.pack('<I',definition));cpu.mem_write(source+0x38,struct.pack('<Q',uid));cpu.mem_write(source+0x40,payload);cpu.mem_write(source+0x64,struct.pack('<I',made));cpu.mem_write(offer,bytes((1,2,offered)))
        sp=stack+0x8000;cpu.mem_write(sp+0x13,b'\0');cpu.mem_write(sp+0x14,struct.pack('<I',offer));cpu.mem_write(sp+0x18,struct.pack('<I',actor));cpu.mem_write(sp+0x24,struct.pack('<I',bag));cpu.mem_write(sp+0x28,struct.pack('<I',peer));cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_EBP,source);cpu.reg_write(UC_X86_REG_EBX,slot);cpu.reg_write(UC_X86_REG_EDI,offer);cpu.reg_write(UC_X86_REG_ESI,peer);cpu.reg_write(UC_X86_REG_ECX,bag);cpu.emu_start(0x46fa10,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=0x4700de or len(scratches)!=4 or len(events)!=6 or [e[0] for e in events]!=['D','D','A','G','B','G']:raise RuntimeError(f'unexpected failure path {events}')
        print(uid,item_id,offered,bag,slot,payload.hex(),made,*scratches,*[e[1:] for e in events],cpu.mem_read(source+0x42,1)[0],sep='\t')


if __name__=='__main__':main()
