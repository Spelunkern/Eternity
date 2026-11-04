#!/usr/bin/env python3
"""Execute original MobItems loading, slot loops, catalog/sequence and positions."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    base = 0x30000000
    cpu.mem_map(base, 0x100000)
    mob, definition, stack, stop = base, base+0x3000, base+0x80000, base+0xff000
    state = {}
    def put(at, value):
        cpu.mem_write(at, struct.pack('<I', value & 0xffffffff))
    def get(at):
        return struct.unpack('<I', cpu.mem_read(at, 4))[0]
    # Real SRandom executes unchanged. CRT randomness is an explicit stream;
    # catalog selection/delivery are observed external ports in the slot test.
    for at, cleanup in ((0x504697,0),(0x4bdd10,0),(0x4a65a0,16),(stop+16,0),(stop+32,4),(stop+48,4)):
        cpu.mem_write(at, b'\xc2'+struct.pack('<H',cleanup))
    put(0x5210e4,stop+16)
    put(0x5210cc,stop+32)
    put(0x5210d0,stop+48)
    original_get = None
    # Save original GetItem from a fresh immutable image before port patching.
    fresh, _ = load_game()
    original_get = bytes(fresh.mem_read(0x4bdd10,0xa0))
    def hook(machine, at, size, unused):
        sp = machine.reg_read(UC_X86_REG_ESP)
        if at == 0x504697:
            value = (state['seed']*214013+2531011)&0xffffffff
            state['seed']=value
            machine.reg_write(UC_X86_REG_EAX,(value>>16)&32767)
            state['calls']+=1
        elif at == 0x4bdd10 and state['mode']=='slots':
            grade=machine.reg_read(UC_X86_REG_EAX)
            state['events'].extend(('S'+str(grade),))
            machine.reg_write(UC_X86_REG_EAX,0 if grade%3==0 else grade+10000)
        elif at == 0x4a65a0:
            state['events'].append('D'+str(get(sp+4)))
            if get(sp+8)!=1 or get(sp+16)!=0:
                raise RuntimeError('Unexpected delivery arguments')
        elif at == stop+16:
            machine.reg_write(UC_X86_REG_EAX,state['tick'])
        elif at in (stop+32,stop+48):
            state['events'].append('L' if at==stop+32 else 'U')
    cpu.hook_add(UC_HOOK_CODE,hook)
    print(f'# ps_game.exe SHA256 {digest}; CMob::ItemDrop slot loop 004a60be..004a614b; CItemDrop::GetItem 004bdd10. LoadConstMobItem assignment 004c6300..004c63a5, GetSequenceItem 004be490, GetDropPosRand 00490c70. Actual SRandom; explicit CRT stream. Catalog/delivery observed only for slot cases; locks/clock observed for catalog cases.')
    for case in range(512):
        cpu.mem_write(base,bytes(0x90000))
        seed=(case*7919)&0xffffffff
        state.update(mode='slots',seed=seed,calls=0,events=[])
        put(mob+0xa8,definition)
        put(mob+0x2bc,seed)
        put(mob+0x2c0+624*4,625)
        slots=[]
        for i in range(10):
            grade=0 if (case+i)%7==0 else i+1
            rate=(0,1,29,99,100,101,999999,1000000,0xffffffff)[(case//7+i)%9]
            cpu.mem_write(definition+0xbc+i*8,struct.pack('<H2xI',grade,rate))
            slots.extend((grade,rate))
        put(stack+0x3c,0)
        put(stack+0x40,0)
        cpu.reg_write(UC_X86_REG_ESP,stack)
        cpu.reg_write(UC_X86_REG_EBP,mob)
        cpu.reg_write(UC_X86_REG_EBX,0)
        cpu.emu_start(0x4a60be,0x4a614b,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=0x4a614b:
            raise RuntimeError('Slot loop failed to finish')
        print('S',case,seed,*slots,state['seed'],state['calls'],','.join(state['events']) or '-',sep='\t')
    cpu.mem_write(0x4bdd10,original_get)
    cpu.ctl_remove_cache(0x4bdd10,0x4bddb0)
    catalog,item,array=base+0x10000,base+0x20000,base+0x21000
    for case in range(256):
        cpu.mem_write(base,bytes(0x90000))
        seed=case*3571
        maximum=(0,1,3,65535)[case%4]
        count=(-1,0,1,3,65535)[(case//4)%5]
        tick=(0,1,1000,0xffffffff)[(case//20)%4]
        next_tick=(0,1,1000,0xffffffff)[(case//7)%4]
        delay=(0,100,0xfffffffe)[(case//3)%3]
        bound=(1,2,1000000)[(case//11)%3]
        state.update(mode='catalog',seed=seed,calls=0,events=[],tick=tick)
        put(catalog+12,1);put(catalog+20,array);put(array,item)
        cpu.mem_write(item+0x7e,struct.pack('<H',maximum))
        for off,value in ((0x8c,count),(0x90,delay),(0x94,bound),(0x98,next_tick)):
            put(item+off,value)
        put(stack,stop)
        cpu.reg_write(UC_X86_REG_ESP,stack)
        cpu.reg_write(UC_X86_REG_ECX,catalog)
        cpu.reg_write(UC_X86_REG_EAX,1)
        cpu.emu_start(0x4bdd10,stop,count=10000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:
            raise RuntimeError('GetItem failed to return')
        print('L',case,seed,maximum,count,tick,next_tick,delay,bound,int(cpu.reg_read(UC_X86_REG_EAX)==item),get(item+0x8c),get(item+0x98),state['seed'],state['calls'],sep='\t')


    for case in range(128):
        cpu.mem_write(base,bytes(0x90000))
        seed=case*12347
        level=(0,1,9,10,30,99,100,65535)[case%8]
        object_id=(0,1,2,3,6,12,0xffffffff)[(case//8)%7]
        first_grade=(0,1)[(case//3)%2]
        state.update(mode='catalog',seed=seed,calls=0,events=[],tick=100)
        put(catalog+12,1);put(catalog+20,array);put(array,item)
        put(catalog+0x3184,2)
        for i,divisor in enumerate((2,3)):
            put(catalog+0x3024+i*44,divisor)
            for j in range(10):put(catalog+0x3028+i*44+j*4,first_grade if i==0 else 1)
        put(stack,stop);put(stack+4,level);put(stack+8,object_id)
        cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ECX,catalog)
        cpu.emu_start(0x4be490,stop,count=10000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Sequence failed to return')
        print('Q',case,seed,level,object_id,first_grade,int(cpu.reg_read(UC_X86_REG_EAX)==item),state['seed'],state['calls'],sep='\t')


    cpu.mem_write(0x4d9510,b'\xc3')
    pointer=base+0x70000
    for case in range(128):
        cpu.mem_write(base,bytes(0x90000))
        mob_count=4;mob_id=(0,1,3,4,65535)[case%5];order=(0,1,9,10,255)[(case//5)%5]
        grade=(0,1,1024,65535)[(case//25)%4];rate=(0,100,1000000,0xffffffff)[(case//7)%4]
        put(pointer,mob_count);put(pointer+4,definition)
        cpu.mem_write(stack+8,struct.pack('<H',mob_id));cpu.mem_write(stack+7,bytes([order]));cpu.mem_write(stack+12,struct.pack('<H',grade));put(stack+20,rate)
        state.update(mode='load',events=[])
        cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_EDI,pointer-0xa81804)
        cpu.emu_start(0x4c6300,0x4c63a5,count=1000)
        if cpu.reg_read(UC_X86_REG_EIP)!=0x4c63a5:raise RuntimeError('Definition assignment failed')
        result=b''.join(bytes(cpu.mem_read(definition+m*0x10c+0xbc+i*8,8)) for m in range(mob_count) for i in range(10))
        print('R',case,mob_count,mob_id,order,grade,rate,result.hex(),sep='\t')


    for case in range(128):
        seed=case*4711
        x=(0.0,0.001,-1000.0,12345.0)[case%4];z=(0.0,1.0,0.0000001,1234567.0)[(case//4)%4]
        cpu.mem_write(mob+0x50,struct.pack('<f',x));cpu.mem_write(mob+0x58,struct.pack('<f',z))
        state.update(mode='position',seed=seed,calls=0,events=[])
        put(stack,stop);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_EDI,mob);cpu.reg_write(UC_X86_REG_ESI,definition);cpu.reg_write(UC_X86_REG_FPCW,0x27f)
        cpu.emu_start(0x490c70,stop,count=1000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Drop position failed')
        xbits=struct.unpack('<I',struct.pack('<f',x))[0];zbits=struct.unpack('<I',struct.pack('<f',z))[0]
        result=struct.unpack('<III',cpu.mem_read(definition,12))
        print('P',case,seed,xbits,zbits,*result,state['seed'],sep='\t')

if __name__=='__main__':
    main()
