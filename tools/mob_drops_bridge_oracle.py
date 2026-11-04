#!/usr/bin/env python3
"""Execute complete ItemDrop with real creation/craft/range and observed transfers."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    cpu,sha=load_game();base=0x30000000;cpu.mem_map(base,0x100000);cpu.mem_map(0,0x1000)
    mob,info,party,obj,zone,map_info,mob_info=[base+i*0x2000 for i in range(7)]
    users=[base+0x10000+i*0x6000 for i in range(5)];stack,stop=base+0xf0000,base+0xff000;state={}
    cpu.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',base+0x80000)+b'\xc3')
    relative=0x4b9f30-(0x40a510+10)
    cpu.mem_write(0x40a510,b'\xb8'+struct.pack('<I',obj)+b'\xe8'+struct.pack('<i',relative)+b'\xb8'+struct.pack('<I',obj)+b'\xc3')
    for at,cleanup in ((0x4cb8e0,0),(0x4cbe60,0),(0x4d4e50,8),(0x4a1810,12),(0x45df40,4),(0x417320,8),(0x505316,0),(0x504697,0)):
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup))
    def put(at,value):cpu.mem_write(at,struct.pack('<I',value&0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def identity(at):return users.index(at)+100 if at in users else 0
    def payload():
        fields=[struct.unpack('<Q',cpu.mem_read(obj+0x38,8))[0],*cpu.mem_read(obj+0x40,3),struct.unpack('<H',cpu.mem_read(obj+0x44,2))[0],bytes(cpu.mem_read(obj+0x46,6)),bytes(cpu.mem_read(obj+0x4c,21)),get(obj+0x64),cpu.mem_read(obj+0x68,1)[0],get(obj+0x70),get(obj+0x74),cpu.mem_read(obj+0x78,1)[0],get(obj+0x7c),get(obj+0x80),*struct.unpack('<11H',cpu.mem_read(obj+0x84,22)),*struct.unpack('<3f',cpu.mem_read(obj+8,12)),get(obj+0x14),int(get(obj+0x30)!=0)]
        return struct.pack('<QBBBH6s21sIBIIBII11H3fIB',*fields).hex()
    def hook(machine,at,size,_):
        if at==0x40a510:
            state['events'].append('A')
            if not state['allocation']:machine.reg_write(UC_X86_REG_EAX,0);machine.reg_write(UC_X86_REG_EIP,0x40a51f)
        elif at==0x4cb8e0:state['events'].append('T');machine.reg_write(UC_X86_REG_EAX,0x12345678)
        elif at==0x4cbe60:
            state['events'].append('U');machine.reg_write(UC_X86_REG_EAX,state['raw']&0xffffffff);machine.reg_write(UC_X86_REG_EDX,state['raw']>>32)
        elif at==0x504697:
            state['seed']=(state['seed']*214013+2531011)&0xffffffff;machine.reg_write(UC_X86_REG_EAX,(state['seed']>>16)&32767)
        elif at==0x4d4e50:state['events'].append('V:'+payload())
        elif at==0x4a1810:
            who=machine.reg_read(UC_X86_REG_EDI);state['events'].append('L'+str(identity(who-0x188) if who else 0)+':'+payload())
        elif at==0x45df40:state['events'].append('G'+str(identity(machine.reg_read(UC_X86_REG_ECX)))+':'+payload())
        elif at==0x417320:state['events'].append('E:'+payload())
    for at in (0x40a510,0x4cb8e0,0x4cbe60,0x504697,0x4d4e50,0x4a1810,0x45df40,0x417320):cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
    print(f'# ps_game.exe SHA256 {sha}; complete CMob::ItemDrop 004a65a0 with real CItem reset/init, craft, position, zone/range and distribution. Ports: allocated block, clocks/raw UID, explicit CRT/TLS/cookie, log sinks, ItemGet and EnterItem transfer observers. Native bridge executes real Actor admission and ZoneItems publication beyond those observed ports. Every transfer/log captures 101-byte payload.')
    for case in range(512):
        cpu.mem_write(base,bytes(0x80000));put(obj+0x14,0x1234)
        mode=case%5;kind=(case//5)%2;cursor=(0,2,4,5,9)[(case//10)%5];present=(case//17)%2;dex=(case//7)%2
        real=(1,16,22,27)[(case//11)%4];logged=(case//13)%2;sequence=(case//19)%2;allocation=int(case%23!=0);have=int(case%29!=0)
        server=(0,1,2,5,8,9)[(case//3)%6];wis=(0,15,99)[(case//37)%3];quality=(0,1,65535)[(case//41)%3];server_id=(0,1,255)[(case//43)%3]
        raw=0xffffffffffffffff if case%31==0 else 0x1122334455660000;nulls=(0,1,0b10101)[(case//47)%3]
        if mode==4:nulls&=~4
        seed=case*7919;state.update(events=[],allocation=allocation,seed=seed,raw=raw)
        put(mob+0xa8,mob_info);put(mob+0x60,zone);put(zone+0x24,map_info);put(mob+0x284,logged)
        cpu.mem_write(mob+0x50,struct.pack('<f',80));cpu.mem_write(mob+0x58,struct.pack('<f',80));cpu.mem_write(mob+0x220,struct.pack('<H',17))
        put(info,1002);cpu.mem_write(info+36,b'\x01\x02');cpu.mem_write(info+0x38,struct.pack('<H',dex));cpu.mem_write(info+0x3e,struct.pack('<H',wis));cpu.mem_write(info+0x4a,bytes([server]));cpu.mem_write(info+0x4c,struct.pack('<H',quality));put(info+0xa0,real);cpu.mem_write(0x542ca0,bytes([server_id]))
        put(party+8,77);put(party+0xc,2);put(party+0x10,5);put(party+0x104,mode);put(party+0x108,cursor);cpu.mem_write(party+0x114,bytes([kind]))
        projections=[]
        for i,user in enumerate(users):
            status=4 if (case//53+i)%7 else 3;map_value=zone if (case//59+i)%9 else zone+1;instance=int(i>0 and (case//61+i)%11==0);dead=(0,1,2)[(case//67+i)%3]
            x=80+i*25;z=80;xbits=struct.unpack('<I',struct.pack('<f',x))[0];zbits=struct.unpack('<I',struct.pack('<f',z))[0]
            values=(status,map_value,instance,dead,xbits,zbits);projections.extend(values)
            put(user+0xdc,100+i);put(user+0x5758,status);put(user+0xe0,map_value);cpu.mem_write(user+0x164,struct.pack('<H',instance));put(user+0x142c,dead);put(user+0xd0,xbits);put(user+0xd8,zbits);put(party+0x18+i*8,0 if nulls&(1<<i) else user)
        put(stack,stop);put(stack+4,info if have else 0);put(stack+8,1);put(stack+12,party if present else 0);put(stack+16,sequence)
        cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ECX,users[0]);cpu.reg_write(UC_X86_REG_EDX,mob);cpu.reg_write(UC_X86_REG_FPCW,0x27f)
        cpu.emu_start(0x4a65a0,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Complete bridge ItemDrop did not return')
        print(case,seed,mode,kind,cursor,present,dex,real,logged,sequence,allocation,have,server,wis,quality,server_id,raw,nulls,zone,*projections,
              ','.join(state['events']) or '-',get(party+0x108),state['seed'],sep='\t')

if __name__=='__main__':main()
