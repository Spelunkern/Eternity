#!/usr/bin/env python3
"""Execute original SRandom construction, seeding and streams; requires Unicorn."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,data,stop=0x20000000,0x30000000,0x10000000
    for at in (stack,data,stop): cpu.mem_map(at,0x10000)
    cpu.mem_write(0x5046b9,b'\xc3');context={'seed':0}
    def hook(machine,address,size,_):machine.reg_write(UC_X86_REG_EAX,context['seed'])
    cpu.hook_add(UC_HOOK_CODE,hook,begin=0x5046b9,end=0x5046b9)
    def call(at):
        cpu.mem_write(stack+0x8000,struct.pack('<I',stop));cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.emu_start(at,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('SRandom did not return')
        return cpu.reg_read(UC_X86_REG_EAX)
    print(f'# ps_game.exe SHA256 {digest}; SRandom constructor 004a8840, Random 004a8860, SetRandomSeed 004a8980; only constructor time port.')
    print('# R scenario seed lazy results_hex final_state_hex; 1300 draws, reseed at draw 650 for scenario divisible by 3. Bound zero must not mutate state.')
    for scenario in range(128):
        seed=(0,1,0xffffffff,0x80000000,scenario*0x1234567)[scenario%5]&0xffffffff;lazy=scenario%2;cpu.mem_write(data,bytes(2504));context['seed']=seed
        if lazy:cpu.reg_write(UC_X86_REG_ESI,data);call(0x4a8840)
        else:cpu.reg_write(UC_X86_REG_EAX,data);cpu.reg_write(UC_X86_REG_EDI,seed);call(0x4a8980)
        results=b''
        for step in range(1300):
            if step==650 and scenario%3==0:
                cpu.reg_write(UC_X86_REG_EAX,data);cpu.reg_write(UC_X86_REG_EDI,seed^0xdeadbeef);call(0x4a8980)
            bound=(0,1,2,3,100,10000,0xffffffff,0x80000000,65535)[(scenario+step)%9]
            before=bytes(cpu.mem_read(data,2504));cpu.reg_write(UC_X86_REG_EAX,data);cpu.reg_write(UC_X86_REG_EBX,bound);result=call(0x4a8860)
            if not bound and bytes(cpu.mem_read(data,2504))!=before:raise RuntimeError('Original zero bound mutated state')
            results+=struct.pack('<I',result)
        print('R',scenario,seed,lazy,results.hex(),bytes(cpu.mem_read(data,2504)).hex(),sep='\t')


if __name__=='__main__':main()
