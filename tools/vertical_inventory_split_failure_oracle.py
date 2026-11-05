#!/usr/bin/env python3
"""Trace ExchangeItemAdd when the real allocator returns null."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EBX, UC_X86_REG_EBP, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    stack, source, definition, offer, actor, peer, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x33000000, 0x33010000, 0x10000000
    for address, size in ((0,0x10000),(stack,0x10000),(source,0x10000),(definition,0x10000),(offer,0x10000),(actor,0x20000),(stop,0x1000)):
        cpu.mem_map(address,size)
    cpu.mem_write(0x40a510,b'\x31\xc0\xc3')  # allocator failure, not a successful service
    cpu.mem_write(stop+0x100,b'\xc2\x04\x00');cpu.mem_write(stop+0x200,b'\xc2\x04\x00')
    for iat in (0x5210cc,0x5210d0):cpu.mem_write(iat,struct.pack('<I',stop+0x100))
    cpu.mem_write(0x5210dc,struct.pack('<I',stop+0x200))
    for address in (0x4d4e50,0x4d4d20,0x4d4c80):cpu.mem_write(address,b'\xc2\x08\x00')
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
        if address==0x46ff56:
            scratches.append(bytes(machine.mem_read(sp+0x74,36)).hex());return
        ptr,length=struct.unpack('<II',machine.mem_read(sp+4,8));target=machine.reg_read(UC_X86_REG_ECX)
        prefix='G' if address==0x4d4e50 else 'D' if address==0x4d4c80 else 'A' if target==actor else 'B' if target==peer else '?'
        events.append(prefix+bytes(machine.mem_read(ptr,length)).hex())
    for address in (0x45b0d0,0x46ff56,0x4d4e50,0x4d4d20,0x4d4c80,stop+0x200,0x4700de):cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
    rng=random.Random(0x46fb21)
    print(f'# ps_game.exe SHA256 {digest}; CUser::ExchangeItemAdd 0046fb21..004700de, CreateItem returns null')
    print('# uid item_id offered bag slot payload made outgoing_scratch receiver_scratch incoming_scratch sender log_out receiver log_in final_count')
    for case in range(64):
        cpu.mem_write(stack,bytes((case,))*0x10000);cpu.mem_write(source,bytes(0x10000));cpu.mem_write(definition,bytes(0x10000));cpu.mem_write(offer,bytes(0x10000));cpu.mem_write(actor,bytes(0x20000));events.clear();scratches.clear()
        for ptr,account,character,level,experience,mapid,pos,account_name,char_name in ((actor,12345,1001,30,123,7,(10.,-1.,20.),b'account-a',b'Actor'),(peer,12346,1002,31,456,8,(30.,-2.,40.),b'account-b',b'Peer')):
            cpu.mem_write(ptr+0x578c,struct.pack('<I',account));cpu.mem_write(ptr+0x128,struct.pack('<I',character));cpu.mem_write(ptr+0x136,struct.pack('<H',level));cpu.mem_write(ptr+0x13c,struct.pack('<I',experience));cpu.mem_write(ptr+0x164,struct.pack('<H',mapid));cpu.mem_write(ptr+0xd0,struct.pack('<3f',*pos));cpu.mem_write(ptr+0x5794,account_name+b'\0');cpu.mem_write(ptr+0x188,char_name+b'\0')
        uid=rng.getrandbits(64);item_id=rng.getrandbits(32);offered=rng.randrange(1,20);bag=rng.randrange(1,10);slot=rng.randrange(24);count=rng.randrange(21,255);made=rng.getrandbits(32)
        payload=bytes((rng.randrange(256),rng.randrange(256),count,0))+struct.pack('<H',rng.randrange(65536))+bytes(rng.randrange(256) for _ in range(6))+b'craft\0'+bytes(rng.randrange(256) for _ in range(15))
        cpu.mem_write(definition,struct.pack('<I',item_id)+b'Strange item\0');cpu.mem_write(source+0x30,struct.pack('<I',definition));cpu.mem_write(source+0x38,struct.pack('<Q',uid));cpu.mem_write(source+0x40,payload);cpu.mem_write(source+0x64,struct.pack('<I',made));cpu.mem_write(offer,bytes((1,2,offered)))
        sp=stack+0x8000;cpu.mem_write(sp+0x13,b'\0');cpu.mem_write(sp+0x14,struct.pack('<I',offer));cpu.mem_write(sp+0x18,struct.pack('<I',actor));cpu.mem_write(sp+0x24,struct.pack('<I',bag));cpu.mem_write(sp+0x28,struct.pack('<I',peer));cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_EBP,source);cpu.reg_write(UC_X86_REG_EBX,slot);cpu.reg_write(UC_X86_REG_EDI,offer);cpu.emu_start(0x46fb21,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=0x4700de or len(scratches)!=3 or len(events)!=4 or [e[0] for e in events]!=['A','G','B','G']:raise RuntimeError(f'unexpected failure path {events}')
        print(uid,item_id,offered,bag,slot,payload.hex(),made,*scratches,*[e[1:] for e in events],cpu.mem_read(source+0x42,1)[0],sep='\t')


if __name__=='__main__':main()
