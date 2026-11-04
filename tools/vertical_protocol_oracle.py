#!/usr/bin/env python3
"""Original PacketProcessing gate for the client zone-entry request."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_EIP, UC_X86_REG_ESP
from original_oracle import load_game

cpu,digest=load_game()
stack,user,packet,stop=0x20000000,0x30000000,0x31000000,0x10000000
for address in (stack,user,packet,stop): cpu.mem_map(address,0x10000)
events=[]
def hook(machine,address,size,context):
    events.append('ENTER' if address==0x44b8a0 else 'OTHER')
    machine.reg_write(UC_X86_REG_EIP,stop)
for address in (0x44b8a0,0x4674f8): cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
print(f'# ps_game.exe SHA256 {digest}; CUser::PacketProcessing 00466da0; gate 00466f01..00466f67; hooked entry and fallback')
for location in (1,2,3,4,5,0xffffffff):
    for opcode in (0x0200,0x0201,0x0202,0x02ff,0x0301):
        for suffix in (0,0x78563412):
            cpu.mem_write(user,bytes(0x6000));cpu.mem_write(user+0x5758,struct.pack('<I',location));cpu.mem_write(packet,struct.pack('<HI',opcode,suffix))
            cpu.mem_write(stack+0x8000,struct.pack('<I',stop));cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ECX,user);cpu.reg_write(UC_X86_REG_EDX,packet);events.clear()
            cpu.emu_start(0x466da0,stop,count=10000)
            if len(events)!=1: raise RuntimeError('Original packet gate did not terminate')
            print(location,opcode,suffix,events[0])
