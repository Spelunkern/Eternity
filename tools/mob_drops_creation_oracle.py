#!/usr/bin/env python3
"""Execute original craft generation and mob item/money creation over reused storage."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    cpu,sha=load_game();base=0x30000000;cpu.mem_map(base,0x100000)
    mob,info,user,party,obj= [base+i*0x2000 for i in range(5)];stack,stop=base+0xf0000,base+0xff000
    state={}
    def put(at,value):cpu.mem_write(at,struct.pack('<I',value&0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def word(at,value):cpu.mem_write(at,struct.pack('<H',value&65535))
    def byte(at,value):cpu.mem_write(at,bytes([value&255]))
    def position(x,z):cpu.mem_write(mob+0x50,struct.pack('<f',x));cpu.mem_write(mob+0x58,struct.pack('<f',z))
    # Allocation executes the real CItem::Init reset before returning the supplied
    # reused block. The actual Linux item pool has independent original fixtures.
    relative=0x4b9f30-(0x40a510+5+5)
    cpu.mem_write(0x40a510,b'\xb8'+struct.pack('<I',obj)+b'\xe8'+struct.pack('<i',relative)+b'\xb8'+struct.pack('<I',obj)+b'\xc3')
    for at in (0x504697,0x4cb8e0,0x4cbe60,0x505316):cpu.mem_write(at,b'\xc3')
    def hook(machine,at,size,_):
        if at==0x40a510:
            if not state['allocation']:
                # Skip reset/return trampoline on allocation failure.
                machine.reg_write(UC_X86_REG_EAX,0);machine.reg_write(UC_X86_REG_EIP,0x40a51f)
        elif at==0x504697:
            state['seed']=(state['seed']*214013+2531011)&0xffffffff
            machine.reg_write(UC_X86_REG_EAX,(state['seed']>>16)&32767);state['calls']+=1
            if state['mutate'] and state['calls']==1:position(11,22);word(mob+0x220,999)
        elif at==0x4cb8e0:
            state['events']+='T';machine.reg_write(UC_X86_REG_EAX,state['time'])
            if state['mutate']:word(info+0x4c,state['quality']+7)
        elif at==0x4cbe60:
            state['events']+='U';raw=state['raw'];machine.reg_write(UC_X86_REG_EAX,raw&0xffffffff);machine.reg_write(UC_X86_REG_EDX,raw>>32)
            if state['mutate']:byte(0x542ca0,state['server_id']+1)
        elif at==0x4a66cc or (at==0x4a64d9 and state['tag']=='M'):machine.emu_stop()
    for at in (0x40a510,0x504697,0x4cb8e0,0x4cbe60,0x4a66cc,0x4a64d9):cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
    def payload():
        fields=[struct.unpack('<Q',cpu.mem_read(obj+0x38,8))[0],*cpu.mem_read(obj+0x40,3),struct.unpack('<H',cpu.mem_read(obj+0x44,2))[0],bytes(cpu.mem_read(obj+0x46,6)),bytes(cpu.mem_read(obj+0x4c,21)),get(obj+0x64),cpu.mem_read(obj+0x68,1)[0],get(obj+0x70),get(obj+0x74),cpu.mem_read(obj+0x78,1)[0],get(obj+0x7c),get(obj+0x80),*struct.unpack('<11H',cpu.mem_read(obj+0x84,22)),*struct.unpack('<3f',cpu.mem_read(obj+8,12)),get(obj+0x14),int(get(obj+0x30)!=0)]
        return struct.pack('<QBBBH6s21sIBIIBII11H3fIB',*fields)
    print(f'# ps_game.exe SHA256 {sha}; complete CItem::CreateCraftExpansion 004ba2e0 and actual IsAccessory 004b9ee0; CMob::ItemDrop item creation 004a662a..004a66cc and money creation 004a645b..004a64cc. Actual CItem reset/init and drop position; ports: allocation block, explicit CRT stream, make time, raw UID, stack cookie. 101-byte canonical payload snapshot includes retained state.')
    for tag,total in (('C',768),('I',512),('M',128)):
        for case in range(total):
            cpu.mem_write(base,bytes(0x10000));cpu.mem_write(obj,bytes([0xa5])*0x100)
            cpu.mem_write(obj+0x4c,b'abcdefghijklmnopqrst\0');cpu.mem_write(obj+0x84,struct.pack('<11H',*[100+i for i in range(11)]))
            cpu.mem_write(obj+8,struct.pack('<3f',1.25,2.5,3.75));put(obj+0x14,0x12345678)
            server=(0,1,2,3,4,5,6,7,8,9,10,255)[case%12]
            wis=(0,1,15,99,100,65535)[(case//12)%6]
            real=(0,1,15,16,22,23,24,25,27,255)[(case//7)%10]
            quality=(0,1,65535)[(case//5)%3];owner=(0,42)[case%2];party_id=77 if case%3==0 else 0
            server_id=(0,1,255)[(case//3)%3];raw=(0,0x1122334455660000,0xffffffffffffffff)[(case//11)%3]
            time=(0,0x12345678,0xffffffff)[(case//13)%3];seed=case*7919
            x,z=(0.0,0.001,100.0,1234567.0)[case%4],(0.0,0.001,-100.0,12345.0)[(case//4)%4]
            mob_id=(0,1,65535)[(case//17)%3];mutate=(case//19)%2
            have=int(tag!='M' and (tag=='I' or case%17!=0));allocation=int(tag=='C' or case%23!=0)
            put(obj+0x30,info if have else 0);put(mob+0xa8,info+0x1000);position(x,z);word(mob+0x220,mob_id)
            byte(info+0x24,1);byte(info+0x25,2);byte(info+0x4a,server);word(info+0x3e,wis);word(info+0x4c,quality);put(info+0xa0,real)
            byte(0x542ca0,server_id);put(user+0xdc,owner);put(party+8,party_id)
            state.update(tag=tag,seed=seed,calls=0,events='',mutate=mutate,allocation=allocation,raw=raw,time=time,quality=quality,server_id=server_id)
            put(stack,stop);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_FPCW,0x27f)
            if tag=='C':
                cpu.reg_write(UC_X86_REG_ESI,obj);cpu.emu_start(0x4ba2e0,stop,count=100000)
            elif tag=='I':
                put(stack+4,info);put(stack+8,1);put(stack+12,party if party_id else 0);put(stack+16,0)
                cpu.reg_write(UC_X86_REG_EDX,mob);cpu.reg_write(UC_X86_REG_ECX,user if owner else 0);cpu.emu_start(0x4a65a0,stop,count=100000)
            else:
                put(stack+0x38,owner);put(stack+0x14,123456);cpu.reg_write(UC_X86_REG_EBP,mob)
                cpu.emu_start(0x4a645b,0x4a64cc,count=100000)
            expected=stop if tag=='C' or (tag=='I' and not allocation) else (0x4a66cc if tag=='I' else (0x4a64cc if allocation else 0x4a64d9))
            if cpu.reg_read(UC_X86_REG_EIP)!=expected:raise RuntimeError(f'{tag} {case}: creation did not finish at expected boundary')
            print(tag,case,seed,have,allocation,server,wis,real,quality,owner,party_id,server_id,raw,time,struct.unpack('<I',struct.pack('<f',x))[0],struct.unpack('<I',struct.pack('<f',z))[0],mob_id,mutate,payload().hex(),state['seed'],state['calls'],state['events'] or '-',sep='\t')

if __name__=='__main__':main()
