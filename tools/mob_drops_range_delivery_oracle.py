#!/usr/bin/env python3
"""Execute complete item delivery with the real original zone/range bodies."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    cpu,sha=load_game();base=0x30000000;cpu.mem_map(base,0x100000);cpu.mem_map(0,0x1000)
    mob,info,party,obj,pos=[base+i*0x2000 for i in range(5)]
    users=[base+0x10000+i*0x6000 for i in range(5)];stack,stop=base+0xf0000,base+0xff000
    cpu.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',base+0x80000)+b'\xc3')
    state={}
    def put(at,value):cpu.mem_write(at,struct.pack('<I',value&0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def identity(at):return users.index(at)+100 if at in users else 0
    ports={0x40a510:0,0x4cb8e0:0,0x4b9f80:20,0x4cbe60:0,0x490c70:0,0x4ba2e0:0,
           0x44c610:0,0x4a1810:12,0x45df40:4,0x417320:8,0x505316:0,0x504697:0}
    for at,cleanup in ports.items():cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup))
    def hook(machine,at,size,_):
        if at==0x40a510:state['events'].append('A');machine.reg_write(UC_X86_REG_EAX,obj if state['allocation'] else 0)
        elif at==0x4cb8e0:machine.reg_write(UC_X86_REG_EAX,12345)
        elif at==0x4cbe60:machine.reg_write(UC_X86_REG_EAX,10000);machine.reg_write(UC_X86_REG_EDX,0)
        elif at==0x490c70:machine.reg_write(UC_X86_REG_EAX,pos)
        elif at==0x44c610:machine.reg_write(UC_X86_REG_EAX,state['logged'])
        elif at==0x4a1810:
            who=machine.reg_read(UC_X86_REG_EDI);state['events'].append('L'+str(identity(who-0x188) if who else 0))
        elif at==0x45df40:state['events'].append('G'+str(identity(machine.reg_read(UC_X86_REG_ECX))))
        elif at==0x417320:state['events'].append('E')
        elif at==0x504697:
            state['seed']=(state['seed']*214013+2531011)&0xffffffff;machine.reg_write(UC_X86_REG_EAX,(state['seed']>>16)&32767)
    for at in ports:cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
    def bits(value):return struct.unpack('<I',struct.pack('<f',value))[0]
    print(f'# ps_game.exe SHA256 {sha}; complete ItemDrop 004a65a0 with actual EnableApplyRangeItem 004a64f0 / EnableApplyZoneItem 004a6560 / Distance2 0041b1f0 / CRT sqrt. Ports: allocation/init/craft/ID/position, grants, logs, transport, CRT TLS, explicit rand stream. Sequence branch disabled; validated separately.')
    for case in range(1024):
        cpu.mem_write(base,bytes(0x80000))
        seed=case*7919;mode=case%5;kind=(case//5)%2;cursor=(0,2,4,5,9)[(case//10)%5];present=(case//50)%2
        dex=(case//7)%2;real=27 if case%11==0 else 1;logged=(case//17)%2;allocation=int(case%31!=0);have_item=int(case%37!=0)
        zone=(0,7,0xffffffff)[(case//19)%3];nulls=(0,1,0b10101)[(case//29)%3]
        if mode==4:nulls&=~4 # Original owner helper requires a retained leader.
        state.update(events=[],seed=seed,allocation=allocation,logged=logged)
        put(mob+0x60,zone);put(mob+0x284,logged);put(info+0xa0,real);cpu.mem_write(info+0x38,struct.pack('<H',dex));put(info,42)
        put(party+8,77);put(party+0xc,2);put(party+0x10,5);put(party+0x104,mode);put(party+0x108,cursor);cpu.mem_write(party+0x114,bytes([kind]))
        projections=[]
        for i,user in enumerate(users):
            status=4 if (case//23+i)%7 else 3;map_id=zone if (case//31+i)%9 else zone^1;instance=65535 if case%3==0 else 0
            if i and (case//41+i)%11==0:instance^=1
            dead=(0,1,2,-1)[(case//13+i)%4]
            x,z=((0,0),(bits(99),0),(bits(100),0),(bits(100)+1,0),((0x7fc00000 if case%2 else bits(60)),bits(80)))[i]
            values=(status,map_id,instance,dead,x,z);projections.extend(values)
            put(user+0xdc,100+i);put(user+0x5758,status);put(user+0xe0,map_id);cpu.mem_write(user+0x164,struct.pack('<H',instance));put(user+0x142c,dead);put(user+0xd0,x);put(user+0xd8,z)
            put(party+0x18+i*8,0 if nulls&(1<<i) else user)
        put(stack,stop);put(stack+4,info if have_item else 0);put(stack+8,1);put(stack+12,party if present else 0);put(stack+16,0)
        cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ECX,users[0]);cpu.reg_write(UC_X86_REG_EDX,mob);cpu.reg_write(UC_X86_REG_FPCW,0x27f)
        cpu.emu_start(0x4a65a0,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Real range delivery did not return')
        print(case,seed,mode,kind,cursor,present,dex,real,logged,nulls,allocation,have_item,zone,*projections,
              ','.join(state['events']) or '-',get(obj+0x70),get(obj+0x74),get(party+0x108),state['seed'],sep='\t')

if __name__=='__main__':main()
