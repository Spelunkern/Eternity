#!/usr/bin/env python3
"""Execute original 0041f290 view-cell traversal with the cell sender ported."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game()
stop,stack,zone,grid,user=(0x10000000,0x20000000,0x30000000,0x31000000,0x32000000)
for p in (stop,stack,zone,grid,user):cpu.mem_map(p,0x10000)
def put(p,*values):cpu.mem_write(p,struct.pack('<'+'I'*len(values),*values))
cpu.mem_write(0x423aa0,b'\xc3')
events=[]
def hook(m,address,size,data):
    if address==0x423aa0:
        offset=m.reg_read(UC_X86_REG_EDI)-(grid+0x1000);z,rem=divmod(offset,5*292);x,r=divmod(rem,292)
        assert r==0;events.append((x,z))
cpu.hook_add(UC_HOOK_CODE,hook)
put(zone+0x28,5,5,grid);put(grid,*(grid+0x1000+z*5*292 for z in range(5)))
for z in range(5):
    for x in range(5):
        events.clear();put(stack+0x8000,stop,zone,user);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000)
        cpu.reg_write(UC_X86_REG_EAX,z);cpu.reg_write(UC_X86_REG_ECX,x);cpu.emu_start(0x41f290,stop,count=100000)
        assert cpu.reg_read(UC_X86_REG_EIP)==stop
        expected=[(xx,zz) for zz in range(max(z-2,0),min(z+2,4)+1) for xx in range(max(x-2,0),min(x+2,4)+1)]
        assert events==expected,(x,z,events,expected)
print('25 original entry-view traversals passed; ps_game.exe SHA256',digest)
