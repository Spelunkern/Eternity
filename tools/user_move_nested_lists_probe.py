#!/usr/bin/env python3
"""Execute nested original 00416550 with actual original cell unlink/link."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_game
cpu,digest=load_game();stop,stack,user,zone,map_object,grid,heads=0x10000000,0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000
for address in (stop,stack,user,zone,map_object,grid,heads):cpu.mem_map(address,0x10000)
def put(address,*values):cpu.mem_write(address,struct.pack('<'+'I'*len(values),*[x&0xffffffff for x in values]))
def get(address):return struct.unpack('<I',cpu.mem_read(address,4))[0]
cpu.reg_write(UC_X86_REG_FPCW,0x27f)
for pointer in (0x5210cc,0x5210d0):put(pointer,stop+0x100)
cpu.mem_write(stop+0x100,b'\xc2\x04\x00');cpu.mem_write(0x41f350,b'\xc2\x0c\x00')
put(map_object+4,160);put(zone+0x24,map_object,5,5,grid);put(grid,*(grid+0x1000+row*5*292 for row in range(5)))
for z in range(5):
    for x in range(5):
        index=z*5+x;cell=grid+0x1000+index*292;sentinel=heads+index*32
        put(cell,sentinel);put(sentinel+8,sentinel+8,sentinel+8)
        cpu.mem_write(cell+0x114,struct.pack('<4f',x*40-10,x*40+50,z*40-10,z*40+50))
first=grid+0x1000;sentinel=heads;put(first+0x20,1);put(sentinel+8,user+8,user+8);put(user+8,sentinel+8,sentinel+8)
cpu.mem_write(user+0xd0,struct.pack('<3f',20.,3.,20.));put(user+0xe4,0,0);put(user+0x5758,4)
outer_sp=0;nested=True;resume=False;events=[]
def hook(machine,address,size,_):
    global outer_sp,nested,resume
    if address==stop+0x200:
        machine.reg_write(UC_X86_REG_ESP,outer_sp);machine.reg_write(UC_X86_REG_EIP,0x41f350);resume=True;return
    if address!=0x41f350:return
    if resume:resume=False;return
    events.append((tuple(struct.unpack('<3f',machine.mem_read(user+0xd0,12))),tuple(struct.unpack('<2I',machine.mem_read(user+0xe4,8)))))
    if nested:
        nested=False;outer_sp=machine.reg_read(UC_X86_REG_ESP);bits=struct.unpack('<3I',struct.pack('<3f',120.,9.,20.))
        put(outer_sp-20,stop+0x200,zone,*bits);machine.reg_write(UC_X86_REG_ESP,outer_sp-20);machine.reg_write(UC_X86_REG_EAX,user);machine.reg_write(UC_X86_REG_EIP,0x416550)
cpu.hook_add(UC_HOOK_CODE,hook)
put(stack+0x8000,stop,zone,*struct.unpack('<3I',struct.pack('<3f',65.,7.,20.)))
cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,user);cpu.emu_start(0x416550,stop,count=100000)
if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original nested list move did not return')
position=tuple(struct.unpack('<3f',cpu.mem_read(user+0xd0,12)));public=tuple(struct.unpack('<2I',cpu.mem_read(user+0xe4,8)))
counts=[get(grid+0x1000+x*292+0x20) for x in range(4)]
links=[(get(heads+x*32+8),get(heads+x*32+12)) for x in range(4)]
expected_links=[(heads+x*32+8,heads+x*32+8) for x in range(3)]+[(user+8,user+8)]
if position!=(120.,9.,20.) or public!=(1,0) or counts!=[0xffffffff,1,0,1] or links!=expected_links:
    raise AssertionError((position,public,counts,links,events))
print('ps_game.exe',digest,'original list ops; lock and notification ported')
print('position',position,'public',public,'counts',counts,'heads',[(hex(a),hex(b)) for a,b in links],'events',events)
