#!/usr/bin/env python3
"""Execute complete original CMob::ItemDrop delivery coordinator 004a65a0."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game


def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x100000)
    mob,info,party,obj,pos,zone,map_info= [base+i*0x2000 for i in range(7)]
    users=[base+0x10000+i*0x6000 for i in range(5)]
    stack,stop=base+0xf0000,base+0xff000
    state={}
    def put(at,value):cpu.mem_write(at,struct.pack('<I',value&0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def identity(at):return users.index(at)+100 if at in users else 0
    ports={0x40a510:0,0x4cb8e0:0,0x4b9f80:20,0x4cbe60:0,0x490c70:0,0x4ba2e0:0,
           0x4d4e50:8,0x4a6560:0,0x4a64f0:4,0x44c610:0,0x4a1810:12,0x45df40:4,0x417320:8,0x505316:0,0x504697:0}
    for at,cleanup in ports.items():cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup))
    def hook(machine,at,size,_):
        sp=machine.reg_read(UC_X86_REG_ESP);ecx=machine.reg_read(UC_X86_REG_ECX)
        if at==0x40a510:
            state['events'].append('A');machine.reg_write(UC_X86_REG_EAX,obj if state['allocation'] else 0)
        elif at==0x4cb8e0:machine.reg_write(UC_X86_REG_EAX,12345)
        elif at==0x4cbe60:
            machine.reg_write(UC_X86_REG_EAX,10000);machine.reg_write(UC_X86_REG_EDX,0)
        elif at==0x490c70:machine.reg_write(UC_X86_REG_EAX,pos)
        elif at==0x4d4e50:
            state['events'].append('V')
            if get(sp+8)!=127:raise RuntimeError('Sequence log size differs')
        elif at==0x4a6560:
            selected=machine.reg_read(UC_X86_REG_EDX);state['events'].append('Z'+str(identity(selected)))
            machine.reg_write(UC_X86_REG_EAX,state['zone_ok'])
        elif at==0x4a64f0:
            state['events'].append('R'+str(identity(ecx)))
            machine.reg_write(UC_X86_REG_EAX,int(bool(state['eligible'] & (1<<(identity(ecx)-100)))))
        elif at==0x44c610:machine.reg_write(UC_X86_REG_EAX,state['logged'])
        elif at==0x4a1810:
            selected=machine.reg_read(UC_X86_REG_EDI)
            state['events'].append('L'+str(identity(selected-0x188) if selected else 0))
        elif at==0x45df40:state['events'].append('G'+str(identity(ecx)))
        elif at==0x417320:
            state['events'].append('E')
            if get(sp+8)!=2:raise RuntimeError('Enter priority differs')
        elif at==0x504697:
            state['seed']=(state['seed']*214013+2531011)&0xffffffff
            machine.reg_write(UC_X86_REG_EAX,(state['seed']>>16)&32767)
    cpu.hook_add(UC_HOOK_CODE,hook)
    print(f'# ps_game.exe SHA256 {digest}; complete CMob::ItemDrop 004a65a0. Ports: allocation/init/craft/ID/position, zone/range, item grants, transport/logs and explicit CRT stream. Real ownership, distribution, sequence log construction and cursor updates execute.')
    for case in range(1024):
        cpu.mem_write(base,bytes(0xf8000))
        mode=case%5;kind=(case//5)%2;cursor=(0,2,4,5,9)[(case//10)%5]
        present=(case//50)%2;have_user=(case//100)%2
        dex=(case//7)%2;real=27 if case%11==0 else 1;sequence=(case//13)%2
        logged=(case//17)%2;zone_ok=(case//19)%2;eligible=(0,1,0b10101,31)[(case//23)%4]
        nulls=(0,1,0b10101)[(case//29)%3];allocation=int(case%31!=0);have_item=int(case%37!=0)
        zone_ok=int(zone_ok and not (nulls&4))
        seed=case*7919
        state.update(events=[],seed=seed,allocation=allocation,zone_ok=zone_ok,eligible=eligible,logged=logged)
        put(mob+0x60,zone);put(zone+0x24,map_info);put(mob+0xa8,info+0x1000);put(mob+0x284,logged)
        put(info+0xa0,real);cpu.mem_write(info+0x38,struct.pack('<H',dex));put(info,42)
        put(party+8,77);put(party+0xc,2);put(party+0x10,5);put(party+0x104,mode);put(party+0x108,cursor);cpu.mem_write(party+0x114,bytes([kind]))
        for i,user in enumerate(users):
            put(user+0xdc,100+i);put(party+0x18+i*8,0 if nulls&(1<<i) else user)
        put(stack,stop);put(stack+4,info if have_item else 0);put(stack+8,1);put(stack+12,party if present else 0);put(stack+16,sequence)
        cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ECX,users[0] if have_user else 0);cpu.reg_write(UC_X86_REG_EDX,mob)
        try: cpu.emu_start(0x4a65a0,stop,count=100000)
        except Exception as e: raise RuntimeError(f"case {case} eip {cpu.reg_read(UC_X86_REG_EIP):x} events {state['events']}") from e
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Delivery failed to return')
        print(case,seed,mode,kind,cursor,present,have_user,dex,real,sequence,logged,zone_ok,eligible,nulls,allocation,have_item,
              ','.join(state['events']) or '-',get(obj+0x70),get(obj+0x74),get(party+0x108),state['seed'],sep='\t')

if __name__=='__main__':main()
