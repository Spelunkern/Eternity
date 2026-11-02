#!/usr/bin/env python3
"""Execute original client select gate and complete DB 0407 control branch.

Ghidra work/character-selection, work/world-entry-owner, work/entry-guild-lock.
Initialization, actor/catalog sends, time and transport are ports. The original
CGuildLock body, clamps, envelope sends, auto branches and flush sequencing run.
"""
import itertools
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,user,guild,data,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
    for at in (stack,user,guild,data,stop):cpu.mem_map(at,0x10000)
    def word(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def put(at,*values):cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    def signed(value):return value if value<0x80000000 else value-0x100000000
    events=[];state={}
    methods=((0x454560,0),(0x4899a0,0),(0x487e50,0),(0x457a70,0),(0x489ed0,0),(0x40ce70,0),(0x4762c0,0),(0x4756d0,4),(0x483140,0),(0x4832b0,4),(0x483410,4),(0x483540,4),(0x483890,4),(0x4835f0,0),(0x483730,0),(0x483ac0,4),(0x482730,0),(0x481920,0),(0x483d60,0),(0x483ea0,0),(0x483ed0,4),(0x483f80,0),(0x42f250,0),(0x42f0e0,4),(0x47b1e0,0),(0x4826c0,8),(0x483800,4),(0x4d4490,0))
    indices={at:i for i,(at,_) in enumerate(methods)}
    def hook(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address in indices:
            i=indices[address];events.append('STEP '+str(i)+(' '+str(int(bool(cpu.mem_read(user+0x5756,1)[0]))) if i==27 else ''))
        elif address in (0x4d4d20,0x4d4f10):
            ptr,length=word(sp+4),word(sp+8);events.append(('CLIENT ' if address==0x4d4d20 else 'DB ' if state['kind']=='SELECT' else 'MANAGER ')+bytes(cpu.mem_read(ptr,length)).hex())
        elif address==0x4d43a0:events.append('CLOSE '+str(word(sp+4)))
        elif address==0x46c2f0:events.append('WORLD_DAY');machine.reg_write(UC_X86_REG_EAX,state['day'])
        elif address==0x4cb8e0:events.append('SYSTEM_TIME');machine.reg_write(UC_X86_REG_EAX,state['time'])
        elif address==stop+0x100:
            events.append('GUILD_LOCK')
            if state['guild']==2:put(user+0x1758,0)
        elif address==stop+0x120:events.append('GUILD_UNLOCK')
        elif address==0x46d726:machine.emu_stop()
    for at,cleanup in (*methods,(0x4d4d20,8),(0x4d4f10,8),(0x4d43a0,8),(0x46c2f0,0),(0x4cb8e0,0),(stop+0x100,4),(stop+0x120,4),(0x505316,0)):
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
    cpu.hook_add(UC_HOOK_CODE,hook,begin=0x46d726,end=0x46d726)
    put(0x5210cc,stop+0x100);put(0x5210d0,stop+0x120)
    print(f'# ps_game.exe SHA256 {digest}; PacketCharacter 0046bf20/client 0104, PacketUserDBChar 0046cf60/0407 branch 0046d4c1..0046d717; method/time/transport bodies are ports')
    for uid,country,enabled,cid,extra in itertools.product((0,530001,0xffffffff),(0,1,2,255),(False,True),(0,9001,0xffffffff),(False,True)):
        cpu.mem_write(user,bytes(0x10000));put(user+0x578c,uid);cpu.mem_write(user+0x12d,bytes([country]));cpu.mem_write(user+0x57c0,bytes([enabled]));packet=struct.pack('<HI',0x104,cid)+(b'\x81\x82\x83' if extra else b'');cpu.mem_write(data,packet);put(stack+0x8000,stop);events.clear();state.update(kind='SELECT')
        cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EDI,user);cpu.reg_write(UC_X86_REG_EBX,data);cpu.emu_start(0x46bf20,stop,count=10000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original select gate did not return')
        print('SELECT',uid,country,int(enabled),packet.hex());print('SELECTED',int(bool(cpu.mem_read(user+0x57c0,1)[0])))
        for event in events:print('EVENT',event)
        print('END')
    rng=random.Random(0x46d4c1)
    profiles=(((0,0,0),(0,0,0)),((1,2,3),(5,5,5)),((6,7,8),(1,2,3)),((-1,0,1),(0,-1,0x7fffffff)),((0x7fffffff,0x7fffffff,0x7fffffff),(-0x80000000,0,-1)),((-0x80000000,-1,0),(-1,-0x80000000,-1)),((123,456,789),(122,457,788)),((-99,-98,-97),(-98,-99,-97)))
    for auto,guild_mode,motion,(current,maximum),(flag_a,flag_b) in itertools.product((0,123,0xffffffff),range(3),(0,13,255),profiles,((0,0),(1,1),(128,255),(255,128))):
        uid,cid,day,time=(rng.getrandbits(32) for _ in range(4));level=rng.getrandbits(16);cpu.mem_write(user,bytes(0x10000));cpu.mem_write(stack,bytes(0x10000));put(user+0x578c,uid);put(user+0x128,cid);put(user+0xdc,0x87654321);put(user+0x57e8,auto);cpu.mem_write(user+0x164,struct.pack('<H',level));put(user+0x1238,*current);put(user+0x17c,*maximum);cpu.mem_write(user+0x1354,bytes([motion]));cpu.mem_write(user+0x584c,bytes([flag_a,flag_b]));put(user+0x1758,guild if guild_mode else 0);events.clear();state.update(kind='ENTRY',day=day,time=time,guild=guild_mode)
        cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EBP,user);cpu.emu_start(0x46d4c1,stop,count=10000)
        if cpu.reg_read(UC_X86_REG_EIP)!=0x46d726:raise RuntimeError('Original entry control did not finish')
        print('ENTRY',uid,cid,auto,level,guild_mode,motion,flag_a,flag_b,day,time,*current,*maximum)
        print('READY',word(user+0xdc),*[signed(word(user+0x1238+i*4)) for i in range(3)],int(bool(cpu.mem_read(user+0x5756,1)[0])),int(bool(word(user+0x1758))))
        for event in events:print('EVENT',event)
        print('END')


if __name__=='__main__':main()
