#!/usr/bin/env python3
"""Execute ps_game's character-info ID assignment with distinct entity ID."""
import struct
from unicorn.x86_const import UC_X86_REG_EBP,UC_X86_REG_EBX
from original_oracle import load_game
cpu,digest=load_game();user,packet=0x30000000,0x33000000
for address in (user,packet):cpu.mem_map(address,0x10000)
def put(address,value):cpu.mem_write(address,struct.pack('<I',value))
def get(address):return struct.unpack('<I',cpu.mem_read(address,4))[0]
for entity,character in ((123,456),(0xffffffff,0x80000000),(0,1)):
    put(user+0xdc,entity);put(user+0x128,0xdeadbeef);put(packet+6,character)
    cpu.reg_write(UC_X86_REG_EBP,user);cpu.reg_write(UC_X86_REG_EBX,packet)
    cpu.emu_start(0x46d7c9,0x46d7d2,count=10)
    assert get(user+0x128)==character and get(user+0xdc)==entity
print('3 original character-info ID load cases passed; ps_game.exe SHA256',digest)
