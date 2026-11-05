#!/usr/bin/env python3
"""Trace original exchange money logs, capturing preexisting stack bytes."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,actor,peer,stop=0x20000000,0x30000000,0x30010000,0x10000000
    for address,size in ((0,0x10000),(stack,0x10000),(actor,0x20000),(stop,0x1000)):cpu.mem_map(address,size)
    cpu.mem_write(stop+0x100,b'\xc2\x04\x00');cpu.mem_write(stop+0x200,b'\xc2\x04\x00')
    for iat in (0x5210cc,0x5210d0):cpu.mem_write(iat,struct.pack('<I',stop+0x100))
    cpu.mem_write(0x5210dc,struct.pack('<I',stop+0x200));cpu.mem_write(0x4d4e50,b'\xc2\x08\x00');records=[];pending=[]
    def hook(machine,address,size,data):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x46f71c:machine.emu_stop();return
        if address==stop+0x200:
            ptr=struct.unpack('<I',machine.mem_read(sp+4,4))[0];machine.mem_write(ptr,struct.pack('<8H',2026,11,0,2,13,47,59,0));return
        if address==0x45b0d0:
            ret=struct.unpack('<I',machine.mem_read(sp,4))[0]
            if ret not in (0x46f567,0x46f5d3,0x46f657,0x46f6c3):raise RuntimeError('unexpected log header caller')
            subject=machine.reg_read(UC_X86_REG_EDI);ptr=machine.reg_read(UC_X86_REG_ESI)
            if subject not in (actor,peer):raise RuntimeError('unexpected header subject')
            kind=0 if ret in (0x46f567,0x46f657) else 1;source=actor if ret in (0x46f567,0x46f5d3) else peer
            amount=struct.unpack('<I',machine.mem_read(source+0x1570,4))[0];balance=struct.unpack('<I',machine.mem_read(subject+0x140,4))[0]
            pending.append((kind,int(subject==peer),amount,balance,bytes(machine.mem_read(ptr,126 if kind==0 else 111)).hex()));return
        sp=machine.reg_read(UC_X86_REG_ESP);ptr,length=struct.unpack('<II',machine.mem_read(sp+4,8));kind,subject,amount,balance,before=pending.pop(0)
        if length!=(126 if kind==0 else 111):raise RuntimeError('unexpected money log size')
        records.append((kind,subject,amount,balance,before,bytes(machine.mem_read(ptr,length)).hex()))
    for address in (0x45b0d0,0x4d4e50,stop+0x200,0x46f71c):cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
    rng=random.Random(0x46f53e)
    print(f'# ps_game.exe SHA256 {digest}; CUser::Exchange 0046f53e..0046f71c and SetGameLogMain 0045b0d0; pre-header scratch and complete 0804/0803 packets')
    print('# kind subject amount balance scratch packet; actor=account 12345/account-a character 1001/Actor level 30 exp 123 map 7 pos 10,-1,20; peer=account 12346/account-b character 1002/Peer level 31 exp 456 map 8 pos 30,-2,40; initiator field+70=0x12345678; time 2026-11-02 13:47:59')
    for case in range(256):
        a_money=rng.randrange(1,10001);p_money=rng.randrange(1,10001);a_offer=rng.choice((0,0,1,rng.randrange(a_money+1)));p_offer=rng.choice((0,0,1,rng.randrange(p_money+1)))
        cpu.mem_write(stack,bytes((case%256,))*0x10000);cpu.mem_write(actor,bytes(0x20000));records.clear();pending.clear()
        for ptr,account,character,level,experience,mapid,pos,account_name,char_name,balance,offer in ((actor,12345,1001,30,123,7,(10.,-1.,20.),b'account-a',b'Actor',a_money,a_offer),(peer,12346,1002,31,456,8,(30.,-2.,40.),b'account-b',b'Peer',p_money,p_offer)):
            cpu.mem_write(ptr+0x70,struct.pack('<I',0x12345678 if ptr==actor else 0x87654321));cpu.mem_write(ptr+0x578c,struct.pack('<I',account));cpu.mem_write(ptr+0x128,struct.pack('<I',character));cpu.mem_write(ptr+0x136,struct.pack('<H',level));cpu.mem_write(ptr+0x13c,struct.pack('<I',experience));cpu.mem_write(ptr+0x164,struct.pack('<H',mapid));cpu.mem_write(ptr+0xd0,struct.pack('<3f',*pos));cpu.mem_write(ptr+0x5794,account_name+b'\0');cpu.mem_write(ptr+0x188,char_name+b'\0');cpu.mem_write(ptr+0x140,struct.pack('<I',balance));cpu.mem_write(ptr+0x1570,struct.pack('<I',offer))
        cpu.mem_write(stack+0x8000,struct.pack('<II',stop,peer));cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EBX,actor);cpu.emu_start(0x46f4a0,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=0x46f71c or pending:raise RuntimeError('original exchange money phase did not finish')
        for record in records:print(*record,sep='\t')

if __name__=='__main__':main()
