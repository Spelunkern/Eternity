#!/usr/bin/env python3
"""Probe the original CUser::UpdateTimeover guild invitation branch."""
import struct
from unicorn.x86_const import UC_X86_REG_EBP,UC_X86_REG_ECX,UC_X86_REG_ESI
from original_oracle import load_game
cpu,digest=load_game();user=0x30000000;cpu.mem_map(user,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
for pending,deadline,tick in ((0,10,20),(3,0,20),(3,10,9),(3,10,10),(3,10,11),(3,0xfffffff0,2),(3,1,0xffffffff)):
 put(user+0x1754,pending);put(user+0x5838,deadline)
 cpu.reg_write(UC_X86_REG_EBP,0);cpu.reg_write(UC_X86_REG_ECX,tick);cpu.reg_write(UC_X86_REG_ESI,user)
 cpu.emu_start(0x48e3be,0x48e3e0,count=100)
 expired=bool(pending and deadline and tick>=deadline)
 assert (get(user+0x1754),get(user+0x5838))==((0,0) if expired else (pending,deadline))
print('7 original guild invitation timeout cases passed; ps_game.exe SHA256',digest)
