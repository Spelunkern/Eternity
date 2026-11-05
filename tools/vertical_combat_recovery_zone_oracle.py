#!/usr/bin/env python3
"""Trace the original zone user-recovery loop and required continuations."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_EDX, UC_X86_REG_EIP, UC_X86_REG_ESP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    base = 0x30000000
    cpu.mem_map(base, 0x40000)
    zone, head, nodes, users, stack, stubs = (base, base+0x3000, base+0x4000, base+0x8000, base+0x30000, base+0x31000)
    actors = [users + i*0x8000 for i in range(3)]
    def put(at, value): cpu.mem_write(at, struct.pack('<I', value))
    def get(at): return struct.unpack('<I', cpu.mem_read(at, 4))[0]
    events = []
    def hook(machine, address, size, _):
        sp = machine.reg_read(UC_X86_REG_ESP)
        if address == 0x48cfb0: kind, actor, tick = 'R', machine.reg_read(UC_X86_REG_ECX), machine.reg_read(UC_X86_REG_EAX)
        elif address == 0x4865b0: kind, actor, tick = 'E', machine.reg_read(UC_X86_REG_EAX), get(sp+4)
        elif address == 0x47c210: kind, actor, tick = 'D', get(sp+4), get(sp+8)
        elif address == 0x48d240: kind, actor, tick = 'B', machine.reg_read(UC_X86_REG_EDX), machine.reg_read(UC_X86_REG_ECX)
        else: return
        if actor not in actors: raise RuntimeError('Foreign original actor')
        events.append(f'{kind}{actors.index(actor)}:{tick}')
    cpu.hook_add(UC_HOOK_CODE, hook)
    for address, cleanup in ((0x48cfb0,0),(0x4865b0,4),(0x47c210,8),(0x48d240,0),(stubs,4),(stubs+16,4)):
        cpu.mem_write(address, b'\xc2' + struct.pack('<H', cleanup))
    put(0x5210cc, stubs);put(0x5210d0, stubs+16)
    print(f'# ps_game.exe SHA256 {digest}; zone update 00421f80 user slice; recovery 0048cfb0, ChkEndTimeSkill 004865b0, duel 0047c210, UpdateRebirth 0048d240')
    print('# count tick ordered events')
    for count in range(4):
        for tick in (0,100,0xffffffff):
            cpu.mem_write(zone, bytes(0x1000));cpu.mem_write(head, bytes(0x1000))
            put(zone+0x110,head);put(head+4,nodes if count else head)
            for i in range(count):
                node=nodes+i*16;put(node+4, nodes+(i+1)*16 if i+1<count else head);put(node+8,actors[i]+0xd0)
            cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_EDI,zone);cpu.reg_write(UC_X86_REG_EBX,tick)
            events.clear();cpu.emu_start(0x421f80,0x422025,count=10000)
            if cpu.reg_read(UC_X86_REG_EIP)!=0x422025:raise RuntimeError('Original zone user slice did not finish')
            print(count,tick,','.join(events),sep='\t')

if __name__ == '__main__':
    main()
