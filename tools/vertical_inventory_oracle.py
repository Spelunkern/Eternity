#!/usr/bin/env python3
"""Execute the original 0205 handler with cancellation and pickup observed ports."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game

def main():
    cpu,digest=load_game()
    stack,user,packet,stop=0x20000000,0x30000000,0x31000000,0x10000000
    for address in (0,stack,user,packet,stop): cpu.mem_map(address,0x10000)
    for address in (0x44b3d0,0x478520,0x45d2e0): cpu.mem_write(address,b'\xc3')
    events=[]; current={}
    def hook(machine,address,size,data):
        if address==0x469c37:
            machine.emu_stop();return
        if address==0x44b3d0:
            if machine.reg_read(UC_X86_REG_EDI)!=user: raise RuntimeError('Wrong cancellation subject')
            events.append('C')
            if current['mutate']==1: machine.mem_write(packet+2,struct.pack('<I',current['replacement']))
            if current['flip']: machine.mem_write(user+0x142c,struct.pack('<I',1))
        elif address==0x478520:
            if machine.reg_read(UC_X86_REG_ECX)!=user+0x15dc: raise RuntimeError('Wrong shop subject')
            events.append('S')
            if current['mutate']==2: machine.mem_write(packet+2,struct.pack('<I',current['replacement']))
        elif address==0x45d2e0:
            if machine.reg_read(UC_X86_REG_EDX)!=user: raise RuntimeError('Wrong pickup subject')
            events.append('P'+str(machine.reg_read(UC_X86_REG_ECX)))
    for address in (0x469c37,0x44b3d0,0x478520,0x45d2e0): cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
    rng=random.Random(0x205)
    print(f'# ps_game.exe SHA256 {digest}; CUser::PacketMaininterface 00468420, opcode 0205, actual dispatch/gate 004686c9..004686f2; cancellation/shop/pickup observed ports')
    print('# mode initial_id mutation_stage replacement flip_life events')
    for case in range(512):
        mode=[0,1,2,0xffffffff,0x80000000,4][case%6]; identity=rng.getrandbits(32); mutation=(case//6)%3; replacement=rng.getrandbits(32); flip=(case//18)%2
        cpu.mem_write(stack,bytes(0x10000));cpu.mem_write(user,bytes(0x10000));cpu.mem_write(packet,bytes(0x10000))
        cpu.mem_write(user+0x142c,struct.pack('<I',mode));cpu.mem_write(packet,struct.pack('<HI',0x205,identity));cpu.mem_write(stack+0x8000,struct.pack('<II',stop,user))
        current.update(mutate=mutation,replacement=replacement,flip=flip);events.clear()
        cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ECX,packet)
        cpu.emu_start(0x468420,stop,count=10000)
        if cpu.reg_read(UC_X86_REG_EIP)!=0x469c37: raise RuntimeError('Handler failed to reach normal exit')
        print(mode,identity,mutation,replacement,flip,','.join(events) or '-',sep='\t')
if __name__=='__main__': main()
