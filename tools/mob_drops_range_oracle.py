#!/usr/bin/env python3
"""Execute original mob drop zone/range helpers, including real distance/sqrt."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    cpu,sha=load_game();base=0x30000000;cpu.mem_map(base,0x100000);cpu.mem_map(0,0x1000)
    mob,credit,candidate,stack,stop=base,base+0x10000,base+0x20000,base+0xf0000,base+0xff000
    # Same isolated Windows CRT TLS boundary used by existing spatial oracles.
    # Actual sqrt and its errno/error handling remain intact for NaN/Inf inputs.
    cpu.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',base+0x30000)+b'\xc3')
    cpu.mem_write(0x45ec70,b'\xc3')
    state={}
    def put(at,value):cpu.mem_write(at,struct.pack('<I',value&0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def hook(machine,at,size,_):
        if at==0x41b1f0:state['distance']+=1
        elif at==0x45ec70:
            if machine.reg_read(UC_X86_REG_EDX)!=candidate or machine.reg_read(UC_X86_REG_ECX)!=17:raise RuntimeError('Unexpected money gate grant')
            state['money']=1
    cpu.hook_add(UC_HOOK_CODE,hook,begin=0x41b1f0,end=0x41b1f0)
    cpu.hook_add(UC_HOOK_CODE,hook,begin=0x45ec70,end=0x45ec70)
    def user(at,values):
        state_value,zone,instance,dead,x,z=values
        put(at+0x5758,state_value);put(at+0xe0,zone);cpu.mem_write(at+0x164,struct.pack('<H',instance));put(at+0x142c,dead);put(at+0xd0,x);put(at+0xd8,z)
    def call(entry):
        put(stack,stop);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_EDI,mob);cpu.reg_write(UC_X86_REG_FPCW,0x27f)
        cpu.emu_start(entry,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Range helper did not return')
        return cpu.reg_read(UC_X86_REG_EAX)&255
    def bits(value):return struct.unpack('<I',struct.pack('<f',value))[0]
    positions=[(bits(0),bits(0)),(bits(60),bits(80)),(bits(100),0),(bits(100)+1,0),(bits(100)-1,0),(bits(-100),0),(bits(1e-30),bits(1e-30)),(0x7fc00000,0),(0x7f800001,0),(0x7f800000,0),(0xff800000,0),(bits(2147483648),0),(0x7f7fffff,0),(0xff7fffff,0)]
    print(f'# ps_game.exe SHA256 {sha}; complete EnableApplyRangeItem 004a64f0, EnableApplyZoneItem 004a6560 and actual SVector::Distance2 0041b1f0 / CRT sqrt. Only CRT TLS storage supplied; no behavior ports. Explicit original x87 control word 027f; range AL, zone AL, actual inline money gate 004a6311..004a6433 grant observer and real Distance2 call count.')
    for case in range(2048):
        zone=(0,7,0xffffffff)[(case//11)%3]
        initial=(4,zone,65535 if case%3==0 else 0,(0,1,2,-1)[case%4],0,0)
        point=positions[(case//2)%len(positions)]
        other=[4,zone,initial[2],(0,1,2,-1)[(case//3)%4],*point]
        gate=(case//28)%8
        if gate==1:initial=(3,*initial[1:])
        elif gate==2:initial=(initial[0],zone^1,*initial[2:])
        elif gate==3:other[0]=3
        elif gate==4:other[1]=zone^1
        elif gate==5:other[2]^=1
        elif gate==6:other[3]=1
        if case%17==0:initial=(*initial[:4],bits(10000000),bits(-10000000))
        radius=(100,0,-1,2147483647,-2147483648)[(case//7)%5]
        put(mob+0x60,zone);user(credit,initial);user(candidate,other);state['distance']=0
        cpu.reg_write(UC_X86_REG_EAX,credit);cpu.reg_write(UC_X86_REG_ECX,candidate);put(stack+4,radius)
        try:enabled=call(0x4a64f0)
        except Exception as error:raise RuntimeError(f'case {case} eip {cpu.reg_read(UC_X86_REG_EIP):x}') from error
        distance=state['distance'];cpu.reg_write(UC_X86_REG_ECX,credit);cpu.reg_write(UC_X86_REG_EDX,candidate);zone_enabled=call(0x4a6560)
        state['money']=0;put(base+0x40000,candidate);put(stack+0x3c,credit);put(stack+0x14,17)
        cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_EBX,base+0x40000);cpu.reg_write(UC_X86_REG_EBP,17);cpu.reg_write(UC_X86_REG_FPCW,0x27f)
        cpu.emu_start(0x4a6311,0x4a6433,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=0x4a6433:raise RuntimeError('Original money gate did not finish')
        print(case,zone,*initial,*other,radius,enabled,zone_enabled,state['money'],distance,sep='\t')

if __name__=='__main__':main()
