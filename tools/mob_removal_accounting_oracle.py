#!/usr/bin/env python3
"""Execute the forced-removal accounting block; not the complete removal method."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game

def main():
    cpu,digest=load_game()
    base=0x30000000
    cpu.mem_map(base,0x10000)
    def put(at,v):cpu.mem_write(at,struct.pack('<I',v&0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    clock=base+0x8000
    cpu.mem_write(clock,b'\xc3');put(0x5210e4,clock)
    context={}
    def hook(machine,address,size,data):
        if address==clock:
            context['calls']+=1;machine.reg_write(UC_X86_REG_EAX,context['now'])
    cpu.hook_add(UC_HOOK_CODE,hook)
    print(f'# ps_game.exe SHA256 {digest}; MobRemoveByID accounting 0041e98d..0041e9b7; only GetTickCount is a port. Complete removal not executed.')
    print('# current count old_deadline delay now expected_current expected_deadline clock_calls')
    for current in (0,1,3,0x7fffffff,0x80000000,0xffffffff):
        for count in (0,1,3,0x80000000,0xffffffff):
            for delay in (0,32,180000,604800000):
                for now in (0,1000,0xfffffff0):
                    respawn=base+0x1000;mob=base+0x2000;definition=base+0x4000
                    put(respawn+4,count);put(respawn+8,current);put(respawn+12,123)
                    put(mob+0xc84,respawn);put(mob+0xa8,definition);put(definition+108,delay)
                    context.update(calls=0,now=now)
                    cpu.reg_write(UC_X86_REG_EAX,respawn);cpu.reg_write(UC_X86_REG_EBP,mob);cpu.reg_write(UC_X86_REG_ESP,base+0xf000)
                    cpu.emu_start(0x41e98d,0x41e9b7,count=100)
                    if cpu.reg_read(UC_X86_REG_EIP)!=0x41e9b7:raise RuntimeError('Accounting did not finish')
                    print(current,count,123,delay,now,get(respawn+8),get(respawn+12),context['calls'],sep='\t')
if __name__=='__main__':main()
