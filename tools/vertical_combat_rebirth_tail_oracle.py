#!/usr/bin/env python3
"""Trace original UpdateRebirth's death-timer tail, with callbacks intercepted."""
import itertools
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_EIP, UC_X86_REG_ESI, UC_X86_REG_ESP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    user, stack, stop = 0x30000000, 0x20000000, 0x10000000
    for base in (user,stack,stop):cpu.mem_map(base,0x10000)
    def put(at,value):cpu.mem_write(at,struct.pack('<I',value))
    events=[]; mutation={}
    def hook(machine,address,size,_):
        if address==0x45b0d0:events.append('H')
        elif address==0x4d4e50:
            events.append('L')
            if mutation['enabled']:
                put(user+0x1458,mutation['code']);put(user+0x173c,mutation['party'])
        elif address==0x4596a0:events.append('P')
        elif address==0x4599e0:events.append('S')
        elif address==0x48d3df:machine.reg_write(UC_X86_REG_EIP,0x48d3f9)
    cpu.hook_add(UC_HOOK_CODE,hook)
    for address,cleanup in ((0x45b0d0,0),(0x4d4e50,8),(0x4596a0,0),(0x4599e0,0)):
        cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup))
    put(0x546f4c,stop)
    print(f'# ps_game.exe SHA256 {digest}; CUser::UpdateRebirth 0048d240 tail 0048d38a..0048d3f9')
    print('# mode deadline tick code party mutate replacement_code replacement_party trace')
    for mode,deadline,tick,code,party,mutate in itertools.product((0,1,2),(100,0xffffffff),(99,100,0xffffffff),(0,1,2),(0,1),(0,1)):
        replacement_code=1-code if code<2 else 1;replacement_party=1-party
        cpu.mem_write(user,bytes(0x7000));put(user+0x142c,mode);put(user+0x1454,deadline);put(user+0x1458,code);put(user+0x173c,party)
        cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_EDI,user);cpu.reg_write(UC_X86_REG_ESI,tick);cpu.reg_write(UC_X86_REG_EBX,1);cpu.reg_write(UC_X86_REG_EBP,0)
        mutation.update(enabled=mutate,code=replacement_code,party=replacement_party);events.clear()
        cpu.emu_start(0x48d38a,0x48d3f9,count=1000)
        if cpu.reg_read(UC_X86_REG_EIP)!=0x48d3f9:raise RuntimeError('Original rebirth tail did not return to boundary')
        print(mode,deadline,tick,code,party,mutate,replacement_code,replacement_party,''.join(events) or '-',sep='\t')

if __name__=='__main__':main()
