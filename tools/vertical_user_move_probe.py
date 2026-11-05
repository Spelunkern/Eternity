#!/usr/bin/env python3
"""Probe original 00416550 geometry/order; cell operations and notifications are ports."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game

cpu, digest = load_game()
stop, stack, user, zone, map_object, grid = 0x10000000, 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x33000000
for address in (stop, stack, user, zone, map_object, grid):
    cpu.mem_map(address, 0x10000)
cpu.reg_write(UC_X86_REG_FPCW, 0x27f)
for pointer in (0x5210cc, 0x5210d0):
    cpu.mem_write(pointer, struct.pack('<I', stop + 0x100))
cpu.mem_write(stop + 0x100, b'\xc2\x04\x00')
for address, cleanup in ((0x40f200, 0), (0x423560, 0), (0x41f350, 12), (0x4d43a0, 8)):
    cpu.mem_write(address, b'\xc2' + struct.pack('<H', cleanup))

def put(address, *values):
    cpu.mem_write(address, struct.pack('<' + 'I' * len(values), *values))

nested=False;resume=False;saved_sp=0;events = []
def hook(machine, address, size, _):
    global nested,resume,saved_sp
    if address==stop+0x200:
        machine.reg_write(UC_X86_REG_ESP,saved_sp);machine.reg_write(UC_X86_REG_EIP,0x41f350);resume=True;return
    if address in (0x40f200, 0x423560, 0x41f350, 0x4d43a0):
        if address==0x41f350 and resume:resume=False;return
        events.append((hex(address), tuple(struct.unpack('<3f', machine.mem_read(user+0xd0, 12))),
                       tuple(struct.unpack('<2I', machine.mem_read(user+0xe4, 8)))))
        if address==0x41f350 and nested:
            nested=False;saved_sp=machine.reg_read(UC_X86_REG_ESP);bits=struct.unpack('<3I',struct.pack('<3f',120.,9.,20.))
            put(saved_sp-20,stop+0x200,zone,*bits);machine.reg_write(UC_X86_REG_ESP,saved_sp-20);machine.reg_write(UC_X86_REG_EAX,user);machine.reg_write(UC_X86_REG_EIP,0x416550)
cpu.hook_add(UC_HOOK_CODE, hook)

print('# ps_game.exe', digest, '00416550; lock/list/notification/close ported; geometry executes original')
cases=((4, (20., 7., 20.)), (4, (65., 7., 20.)), (4, (-1., 7., 20.)), (1, (65., 7., 20.)))
expected=((1, (20.,7.,20.), (0,0), []), (1, (65.,7.,20.), (1,0), ['0x40f200','0x423560','0x41f350']),
          (0, (20.,3.,20.), (0,0), []), (0, (20.,3.,20.), (0,0), []))
for (location, destination), want in zip(cases,expected):
    cpu.mem_write(user, bytes(0x7000));cpu.mem_write(zone, bytes(0x1000));cpu.mem_write(map_object, bytes(0x100));cpu.mem_write(grid, bytes(0x10000));events.clear()
    put(map_object+4, 160);put(zone+0x24, map_object, 5, 5, grid)
    put(grid, *(grid+0x1000+z*5*292 for z in range(5)))
    for z in range(5):
        for x in range(5):
            cpu.mem_write(grid+0x1000+(z*5+x)*292+0x114, struct.pack('<4f', x*40-10, x*40+50, z*40-10, z*40+50))
    cpu.mem_write(user+0xd0, struct.pack('<3f', 20., 3., 20.));put(user+0xe4, 0, 0);put(user+0x5758, location)
    bits=struct.unpack('<3I',struct.pack('<3f',*destination));put(stack+0x8000, stop, zone, *bits)
    cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,user)
    cpu.emu_start(0x416550,stop,count=100000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original move did not return')
    result=(cpu.reg_read(UC_X86_REG_EAX)&255, tuple(struct.unpack('<3f',cpu.mem_read(user+0xd0,12))), tuple(struct.unpack('<2I',cpu.mem_read(user+0xe4,8))))
    if (*result,[event[0] for event in events]) != want:raise AssertionError((location,destination,result,events))
    if events and any(position!=destination or cell!=(0,0) for _,position,cell in events):raise AssertionError(events)
    print(location, destination, *result, events)

# Independent boundary matrix around the original overlapping cell view. The
# expected model is limited to a valid original grid and finite coordinates.
values=(-1., 0., 39.5, 50., 50.5, 80., 159.5, 160.)
checked=0
for x in values:
    for z in values:
        cpu.mem_write(user, bytes(0x7000));cpu.mem_write(zone, bytes(0x1000));cpu.mem_write(map_object, bytes(0x100));cpu.mem_write(grid, bytes(0x10000));events.clear()
        put(map_object+4,160);put(zone+0x24,map_object,5,5,grid);put(grid,*(grid+0x1000+row*5*292 for row in range(5)))
        for row in range(5):
            for column in range(5):
                cpu.mem_write(grid+0x1000+(row*5+column)*292+0x114,struct.pack('<4f',column*40-10,column*40+50,row*40-10,row*40+50))
        cpu.mem_write(user+0xd0,struct.pack('<3f',20.,3.,20.));put(user+0xe4,0,0);put(user+0x5758,4)
        destination=(x,7.,z);bits=struct.unpack('<3I',struct.pack('<3f',*destination));put(stack+0x8000,stop,zone,*bits)
        cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,user);cpu.emu_start(0x416550,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original move did not return')
        safe=0<=x<160 and 0<=z<160;cross=safe and (x>50 or z>50)
        expected_cell=(int(x/40),int(z/40)) if cross else (0,0)
        expected_position=destination if safe else (20.,3.,20.)
        calls=['0x40f200','0x423560','0x41f350'] if cross else []
        actual=(cpu.reg_read(UC_X86_REG_EAX)&255,tuple(struct.unpack('<3f',cpu.mem_read(user+0xd0,12))),tuple(struct.unpack('<2I',cpu.mem_read(user+0xe4,8))),[event[0] for event in events])
        if actual!=(int(safe),expected_position,expected_cell,calls):raise AssertionError((destination,actual))
        if any(position!=destination or cell!=(0,0) for _,position,cell in events):raise AssertionError(events)
        checked+=1
print('finite boundary matrix passed:',checked)

# NaN is accepted by the original x87 comparison helpers. If the other axis
# forces a cell change, conversion of NaN produces an invalid cell and closes.
special=((float('nan'),20.,5,1,[]), (float('nan'),80.,5,0,['0x4d43a0']),
         (float('inf'),20.,5,0,[]), (80.,20.,1,0,['0x4d43a0']))
for x,z,width,success,calls in special:
    cpu.mem_write(user,bytes(0x7000));cpu.mem_write(zone,bytes(0x1000));cpu.mem_write(map_object,bytes(0x100));cpu.mem_write(grid,bytes(0x10000));events.clear()
    put(map_object+4,160);put(zone+0x24,map_object,width,5,grid);put(grid,*(grid+0x1000+row*5*292 for row in range(5)))
    cpu.mem_write(grid+0x1000+0x114,struct.pack('<4f',-10.,50.,-10.,50.))
    cpu.mem_write(user+0xd0,struct.pack('<3f',20.,3.,20.));put(user+0xe4,0,0);put(user+0x5758,4)
    destination=(x,7.,z);bits=struct.unpack('<3I',struct.pack('<3f',*destination));put(stack+0x8000,stop,zone,*bits)
    cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,user);cpu.emu_start(0x416550,stop,count=100000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original special move did not return')
    position=bytes(cpu.mem_read(user+0xd0,12));expected=struct.pack('<3f',*(destination if x!=float('inf') else (20.,3.,20.)))
    if cpu.reg_read(UC_X86_REG_EAX)&255!=success or position!=expected or [event[0] for event in events]!=calls:
        raise AssertionError((destination,width,position.hex(),events))
print('nonfinite/inconsistent-grid cases passed:',len(special))

# Reenter the real movement routine from the ported first notification.
cpu.mem_write(user,bytes(0x7000));cpu.mem_write(zone,bytes(0x1000));cpu.mem_write(map_object,bytes(0x100));cpu.mem_write(grid,bytes(0x10000));events.clear()
put(map_object+4,160);put(zone+0x24,map_object,5,5,grid);put(grid,*(grid+0x1000+row*5*292 for row in range(5)))
for row in range(5):
    for column in range(5):cpu.mem_write(grid+0x1000+(row*5+column)*292+0x114,struct.pack('<4f',column*40-10,column*40+50,row*40-10,row*40+50))
cpu.mem_write(user+0xd0,struct.pack('<3f',20.,3.,20.));put(user+0xe4,0,0);put(user+0x5758,4)
put(stack+0x8000,stop,zone,*struct.unpack('<3I',struct.pack('<3f',65.,7.,20.)))
cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,user);nested=True
cpu.emu_start(0x416550,stop,count=100000)
actual=(cpu.reg_read(UC_X86_REG_EIP),cpu.reg_read(UC_X86_REG_EAX)&255,tuple(struct.unpack('<3f',cpu.mem_read(user+0xd0,12))),tuple(struct.unpack('<2I',cpu.mem_read(user+0xe4,8))))
if actual!=(stop,1,(120.,9.,20.),(1,0)):raise AssertionError(('nested move',actual,events))
print('nested movement position/public-cell:',actual[2:])
