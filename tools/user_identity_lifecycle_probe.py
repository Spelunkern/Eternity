#!/usr/bin/env python3
"""Trace original allocation, selection, DB load and entry ID transitions."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,server,packet,stop=0x20000000,0x30000000,0x31000000,0x33000000,0x10000000
for address in (stack,user,server,packet,stop):cpu.mem_map(address,0x10000)
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
def put(a,*v):cpu.mem_write(a,struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v)))
events=[]
def hook(m,a,size,ctx):
    sp=m.reg_read(UC_X86_REG_ESP)
    if a==0x40aa40:m.reg_write(UC_X86_REG_EAX,user)
    elif a==stop+0x100:
        dest=get(sp+4);put(dest,get(dest)+1);m.reg_write(UC_X86_REG_EAX,get(dest))
    elif a in (0x4d4d20,0x4d4f10):events.append(('send',a,bytes(m.mem_read(get(sp+4),get(sp+8))),get(user+0xdc),get(user+0x128)))
    elif a in (0x40ce70,0x4756d0):events.append(('stage',a,get(user+0xdc),get(user+0x128)))
    elif a==0x46c2f0:m.reg_write(UC_X86_REG_EAX,17)
for a,n in ((0x40aa40,0),(stop+0x100,4),(0x4d4d20,8),(0x4d4f10,8),(0x505316,0),(0x454560,0),(0x4899a0,0),(0x487e50,0),(0x457a70,0),(0x46c2f0,0),(0x489ed0,0),(0x40ce70,0),(0x4762c0,0),(0x4756d0,4)):
    cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
put(0x5210f8,stop+0x100)
put(0x546f40,stop+0x200);put(0x546f44,stop+0x300);put(0x546f38,stop+0x400)
def call_alloc(initial):
    cpu.mem_write(user,bytes(0x10000));put(user+0xdc,initial);put(server+0x3a78,40)
    put(stack+0xf000,stop,0);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_ECX,server);cpu.emu_start(0x406340,stop,count=1000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
    return get(user+0xdc),get(user+0x575c),get(server+0x3a78)
assert call_alloc(0)==(1,41,41)
assert call_alloc(777)==(777,0,40)
for previous,selected,loaded in ((1,9001,9001),(777,0x87654321,0x87654321),(0xffffffff,7,9),(1,0,0)):
    cpu.mem_write(user,bytes(0x10000));cpu.mem_write(stack,bytes(0x10000));put(user+0xdc,previous);put(user+0x128,0xdeadbeef)
    # ResetCharacter prefix clears character ID but preserves prior entity ID.
    put(stack+0xf000,stop,user);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.emu_start(0x44a050,0x44a074,count=1000)
    assert (get(user+0xdc),get(user+0x128))==(previous,0)
    # Client 0104 only requests DB 0407; it does not assign either ID.
    put(user+0x578c,42);cpu.mem_write(user+0x12d,b'\x01');cpu.mem_write(user+0x57c0,b'\x01');cpu.mem_write(packet,struct.pack('<HI',0x104,selected));events.clear()
    put(stack+0xf000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_EDI,user);cpu.reg_write(UC_X86_REG_EBX,packet);cpu.emu_start(0x46bf20,stop,count=10000)
    assert cpu.reg_read(UC_X86_REG_EIP)==stop and (get(user+0xdc),get(user+0x128))==(previous,0)
    assert events==[('send',0x4d4f10,struct.pack('<HII',0x407,42,selected),previous,0)]
    # DB 0501 loads +128; it does not copy it into +dc yet.
    put(packet+6,loaded);cpu.reg_write(UC_X86_REG_EBP,user);cpu.reg_write(UC_X86_REG_EBX,packet);cpu.emu_start(0x46d7c9,0x46d7d2,count=10)
    assert (get(user+0xdc),get(user+0x128))==(previous,loaded)
    # 0407 successful branch sets +dc, then registers/waits and initializes friends.
    events.clear();put(stack+0xf000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_EBP,user);cpu.emu_start(0x46d4c1,0x46d5af,count=10000)
    assert (get(user+0xdc),get(user+0x128))==(loaded,loaded)
    stages=[e for e in events if e[0]=='stage'];assert stages==[('stage',0x40ce70,loaded,loaded),('stage',0x4756d0,loaded,loaded)]
# World and zone indexes choose different source fields before entry aliases them.
put(user+0x128,77);put(user+0xdc,88);put(stack+0xf000,0)
cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_EDI,user);cpu.reg_write(UC_X86_REG_ESI,server);cpu.emu_start(0x40d464,0x40d47e,count=100)
world_pair=cpu.reg_read(UC_X86_REG_EBX);assert (get(world_pair),get(world_pair+4))==(77,user)
put(stack+0xf000,0);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_EBX,user);cpu.reg_write(UC_X86_REG_ESI,server);cpu.emu_start(0x415e8e,0x415ea8,count=100)
zone_pair=cpu.reg_read(UC_X86_REG_EBX);assert (get(zone_pair),get(zone_pair+4))==(88,user)
print('2 allocation, 4 selection/load/entry and 2 index-key identity transitions passed; ps_game.exe SHA256',digest)
