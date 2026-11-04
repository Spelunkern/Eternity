#!/usr/bin/env python3
"""Execute complete original mob loot coordinator, observing required world ports."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game


def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x100000);cpu.mem_map(0,0x1000)
    mob,definition,info,links,ids,party,obj,pos,zone,map_info,guild=[base+i*0x2000 for i in range(11)]
    users=[base+0x20000+i*0x6000 for i in range(5)]
    stack,stop=base+0xf0000,base+0xff000
    state={}
    def put(at,value):cpu.mem_write(at,struct.pack('<I',value&0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def identity(at):return users.index(at)+100 if at in users else 0
    ports={0x402710:4,0x4152f0:4,0x47e4b0:4,0x4bdd10:0,0x4a65a0:16,0x4be490:8,
           0x4a6560:0,0x42c6e0:4,0x4a1810:12,0x40a510:0,0x490c70:0,0x4ba2e0:0,0x417320:8,0x45ec70:0,0x504697:0,stop+16:4,stop+32:4}
    for at,cleanup in ports.items():cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup))
    put(0x5210cc,stop+16);put(0x5210d0,stop+32)
    def event(e):state['events'].append(e)
    def hook(machine,at,size,_):
        sp=machine.reg_read(UC_X86_REG_ESP);eax=machine.reg_read(UC_X86_REG_EAX);ecx=machine.reg_read(UC_X86_REG_ECX);edx=machine.reg_read(UC_X86_REG_EDX)
        if at==0x504697:
            state['seed']=(state['seed']*214013+2531011)&0xffffffff
            machine.reg_write(UC_X86_REG_EAX,(state['seed']>>16)&32767)
        elif at==0x402710:
            event('I');machine.reg_write(UC_X86_REG_EAX,info if state['have_item'] else 0)
        elif at==0x4152f0:
            event('Q');machine.reg_write(UC_X86_REG_EAX,links)
        elif at==0x47e4b0:
            user=identity(machine.reg_read(UC_X86_REG_EDI));quest=get(sp+4)
            event(f'F{user}:{quest}');machine.reg_write(UC_X86_REG_EAX,int(bool(state['quests'] & (1<<((user-100+quest)%8)))))
        elif at==0x4bdd10:
            event('S'+str(eax));machine.reg_write(UC_X86_REG_EAX,info if eax%3 else 0)
        elif at==0x4a65a0:
            event('D'+str(get(sp+16)))
            if get(sp+8)!=1:raise RuntimeError('Loot count differs')
        elif at==0x4be490:
            event('T');machine.reg_write(UC_X86_REG_EAX,info if state['sequence'] else 0)
        elif at==0x4a6560:
            event('Z'+str(identity(edx)));machine.reg_write(UC_X86_REG_EAX,state['zone_ok'])
        elif at==0x42c6e0:event('K'+str(get(sp+4)&65535))
        elif at==0x4a1810:event('M'+str(get(sp+12)))
        elif at==0x40a510:machine.reg_write(UC_X86_REG_EAX,obj)
        elif at==0x490c70:machine.reg_write(UC_X86_REG_EAX,pos)
        elif at==0x417320:event(f'E{get(obj+0x70)}:{get(obj+0x80)}')
        elif at==0x45ec70:event(f'G{identity(edx)}:{ecx}')
    cpu.hook_add(UC_HOOK_CODE,hook)
    print(f'# ps_game.exe SHA256 {digest}; complete CMob::ItemDrop 004a5ea0. Actual SRandom and party money math/range. Required item, quest, guild, allocation and world services observed; explicit CRT stream.')
    for case in range(1024):
        cpu.mem_write(base,bytes(0xf8000))
        seed=case*7919;present=(case//5)%2;have_user=int(present or (case//7)%2)
        quest_type=(case//3)%2;start=(case//11)%2;have_item=int(case%13!=0);quests=(0,1,0x55,255)[(case//17)%4]
        quality=(0,1,30,100,65535)[(case//19)%5];money_base=(0,1,17,65535)[(case//23)%4];money_random=(0,1,500,65535)[(case//29)%4]
        zone_type=5 if case%3==0 else 0;logged=(case//31)%2;mode=case%5;zone_ok=(case//37)%2;sequence=(case//41)%2
        state.update(events=[],seed=seed,have_item=have_item,quests=quests,zone_ok=zone_ok,sequence=sequence)
        put(mob+0xa8,definition);put(mob+0x60,zone);put(zone+0x24,map_info);put(map_info+0x1a4,zone_type);put(mob+0x5c,123);put(mob+0x284,logged)
        put(mob+0x2bc,seed);put(mob+0x2c0+624*4,625)
        cpu.mem_write(definition+0x22,struct.pack('<H',30));cpu.mem_write(definition+0x28,struct.pack('<HHBB',money_base,money_random,quest_type,1))
        cpu.mem_write(info+0x24,b'\x01\x01');cpu.mem_write(info+0x4c,struct.pack('<H',quality));cpu.mem_write(info+0x9c,bytes([start]))
        put(links+8,3);put(links+12,ids);cpu.mem_write(ids,struct.pack('<HHH',1,2,3))
        put(party+0x10,5);put(party+0x104,mode);put(party+0xc,2)
        for i,user in enumerate(users):
            put(user+0xdc,100+i);put(party+0x18+i*8,user);put(user+0x1758,guild)
            put(user+0x142c,1 if i==4 else 0);put(user+0x5758,4);put(user+0xe0,1 if i==3 else 0)
            cpu.mem_write(user+0xd0,struct.pack('<f',float((0,99,100,0,101)[i])))
            put(user+0x58b0,i%4);put(user+0x58b4,(0,-20,15,100,0)[i])
        for i in range(10):
            grade=i+1 if (case+i)%4==0 else 0;rate=(0,30,100,1000000)[(case//43+i)%4]
            cpu.mem_write(definition+0xbc+i*8,struct.pack('<H2xI',grade,rate))
        put(stack,stop);put(stack+4,mob);put(stack+8,users[0] if have_user else 0);put(stack+12,party if present else 0)
        cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_FPCW,0x27f)
        try:cpu.emu_start(0x4a5ea0,stop,count=100000)
        except Exception as e:raise RuntimeError(f'case {case} eip {cpu.reg_read(UC_X86_REG_EIP):x} events {state["events"]}') from e
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Loot did not return')
        print(case,seed,present,have_user,quest_type,start,have_item,quests,quality,money_base,money_random,zone_type,logged,mode,zone_ok,sequence,
              ','.join(state['events']) or '-',state['seed'],sep='\t')
if __name__=='__main__':main()
