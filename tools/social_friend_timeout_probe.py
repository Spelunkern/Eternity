#!/usr/bin/env python3
"""Execute ps_game's friend timeout and leave-zone instruction blocks."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EBP,UC_X86_REG_ESI,UC_X86_REG_ESP
from original_oracle import load_game
cpu,digest=load_game();stack,user,peer,stop=0x20000000,0x30000000,0x31000000,0x10000000
for address in (stack,user,peer,stop):cpu.mem_map(address,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
events=[];found=peer

def hook(m,a,size,ctx):
    sp=m.reg_read(UC_X86_REG_ESP)
    if a==0x415f30:m.reg_write(UC_X86_REG_EAX,found)
    elif a==0x4d4d20:events.append((m.reg_read(UC_X86_REG_ECX),bytes(m.mem_read(get(sp+4),get(sp+8))),get(user+0x5478),get(user+0x583c)))
for a,n in ((0x415f30,4),(0x4d4d20,8)):
    cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
def init(requester,deadline):
    cpu.mem_write(user,bytes(0x10000));put(user+0x5478,requester);put(user+0x583c,deadline)
    cpu.reg_write(UC_X86_REG_ESI,user);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);events.clear()
def timeout(requester,deadline,now):
    init(requester,deadline);cpu.reg_write(UC_X86_REG_EBP,0);cpu.reg_write(UC_X86_REG_ECX,now);cpu.emu_start(0x48e3e0,0x48e402,count=100);return get(user+0x5478),get(user+0x583c)
for values,expected in [((1,100,99),(1,100)),((1,100,100),(0,0)),((1,0,0xffffffff),(1,0)),((0,100,100),(0,100)),((1,10,0xfffffff0),(0,0)),((1,0xfffffff0,10),(1,0xfffffff0))]:assert timeout(*values)==expected
for requester,exists in ((1,True),(1,False),(0,True)):
    init(requester,555);found=peer if exists else 0;cpu.reg_write(UC_X86_REG_EBX,0);cpu.emu_start(0x44b15d,0x44b19c,count=1000)
    assert (get(user+0x5478),get(user+0x583c))==((0,0) if requester else (0,555))
    assert events==([(peer,b'\x03\x22\x00',1,555)] if requester and exists else [])
print('9 original friend timeout/leave cases passed; ps_game.exe SHA256',digest)
