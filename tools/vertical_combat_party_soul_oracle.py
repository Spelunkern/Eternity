#!/usr/bin/env python3
"""Trace KillSoulParty with original GetPartyBoth and EqualZone."""
import itertools
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_EIP, UC_X86_REG_ESI, UC_X86_REG_ESP
from original_oracle import load_game


def main():
    cpu,digest=load_game();user,leader,party,stack,stop=(0x30000000,0x31000000,0x32000000,0x20000000,0x10000000)
    for base in (user,leader,party,stack,stop):cpu.mem_map(base,0x10000)
    def put(at,v):cpu.mem_write(at,struct.pack('<I',v&0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    events=[]
    def hook(machine,address,size,_):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x4599e0:events.append('S')
        elif address==0x45ad50:
            if machine.reg_read(UC_X86_REG_ESI)!=user or get(sp+4)!=17:raise RuntimeError('Wrong ExpDrop arguments')
            events.append('D')
        elif address==0x459ec0:
            if machine.reg_read(UC_X86_REG_ECX)!=user or [get(sp+i) for i in (4,8,12,16)]!=[leader+0xd0,5,7,1]:raise RuntimeError('Wrong Rebirth arguments')
            events.append('R')
        elif address==stop+16:events.append('L')
        elif address==stop+32:events.append('U')
    cpu.hook_add(UC_HOOK_CODE,hook)
    for address,cleanup in ((0x4599e0,0),(0x45ad50,4),(0x459ec0,16),(stop+16,4),(stop+32,4)):
        cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup))
    put(0x5210cc,stop+16);put(0x5210d0,stop+32)
    print(f'# ps_game.exe SHA256 {digest}; KillSoulParty 004596a0; real GetPartyBoth 00444f20 and EqualZone 0044bd20; ExpDrop/Rebirth/KillSoul intercepted')
    print('# zone life soul grow party count leader leader_life leader_phase same_zone same_instance events')
    cases=itertools.product((0,1),(0,1),(0,2),(0,3),(0,1),(0,1),(0,1),(0,1),(3,4),(0,1),(0,1))
    for zone,life,soul,grow,has_party,count,present,other_life,phase,same_zone,same_instance in cases:
        cpu.mem_write(user,bytes(0x7000));cpu.mem_write(leader,bytes(0x7000));cpu.mem_write(party,bytes(0x1000))
        put(user+0xe0,0x33330000 if zone else 0);put(user+0x142c,life);put(user+0x1430,soul);put(user+0x173c,party if has_party else 0);put(user+0x145c,17);cpu.mem_write(user+0x12f,bytes([grow]));cpu.mem_write(user+0x164,struct.pack('<H',7))
        put(party+0xc,0);put(party+0x10,count);put(party+0x18,leader if present else 0)
        put(leader+0x142c,other_life);put(leader+0x5758,phase);put(leader+0xe0,0x33330000 if same_zone else 0x44440000);cpu.mem_write(leader+0x164,struct.pack('<H',7 if same_instance else 8))
        put(stack+0xf000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_EDI,user);events.clear()
        cpu.emu_start(0x4596a0,stop,count=1000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original KillSoulParty did not return')
        print(zone,life,soul,grow,has_party,count,present,other_life,phase,same_zone,same_instance,''.join(events) or '-',sep='\t')

if __name__=='__main__':main()
