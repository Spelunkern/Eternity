#!/usr/bin/env python3
"""Probe original 0041f350 cell notifications using isolated callback ports."""
import struct
import sys
from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_INVALID
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game

cpu,digest=load_game()
stop,stack,user,zone,grid,buffers=0x10000000,0x20000000,0x30000000,0x31000000,0x32000000,0x33000000
for addr in (stop,stack,user,zone,grid,buffers):cpu.mem_map(addr,0x10000)
def put(addr,*values):cpu.mem_write(addr,struct.pack('<'+'I'*len(values),*values))
def get(addr):return struct.unpack('<I',cpu.mem_read(addr,4))[0]
for address,cleanup in ((0x4df9f0,0),(0x4df070,0),(0x4243d0,4),(0x424130,4),(0x423aa0,0)):
    cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup))
put(0x5211f0,stop+0x100);cpu.mem_write(stop+0x100,b'\xb8\x01\x00\x00\x00\xc2\x04\x00')
events=[];packets={};allocations=0;fail_mask=0;mutate_shape=False
def hook(m,address,size,_):
    global allocations
    sp=m.reg_read(UC_X86_REG_ESP)
    if address==0x4df9f0:
        pointer,length=struct.unpack('<2I',m.mem_read(sp+4,8));index=allocations;allocations+=1
        if fail_mask & (1<<index):m.reg_write(UC_X86_REG_EAX,0);return
        buf=buffers+index*256
        m.mem_write(buf,bytes(128));put(buf+8,1);packets[buf]=bytes(m.mem_read(pointer,length));m.reg_write(UC_X86_REG_EAX,buf)
    elif address in (0x4243d0,0x424130,0x423aa0):
        cell=m.reg_read(UC_X86_REG_ESI if address==0x424130 else UC_X86_REG_EDI);index,remainder=divmod(cell-grid-0x1000,292)
        if remainder or not 0<=index<100:raise RuntimeError(('invalid cell',hex(cell)))
        packet=packets[get(sp+4)][:2].hex() if address==0x4243d0 else ''
        events.append((hex(address),index%10,index//10,packet))
        if address==0x423aa0 and mutate_shape:
            m.mem_write(user+0x5774,b'\x01');m.mem_write(user+0x1354,b'\x07')
cpu.hook_add(UC_HOOK_CODE,hook)
def invalid(machine,access,address,size,value,_):
    raise RuntimeError(('unmapped access',hex(machine.reg_read(UC_X86_REG_EIP)),hex(address)))
cpu.hook_add(UC_HOOK_MEM_INVALID,invalid)
put(zone+0x28,10,10,grid);put(grid,*(grid+0x1000+z*10*292 for z in range(10)));put(user+0xdc,42)
print('# ps_game.exe',digest,'0041f350; allocator/broadcast/cell-info/reference ports isolated')
coordinates=(0,1,3,8,9)
cells=[(x,z) for z in coordinates for x in coordinates]
fixture=open(sys.argv[1],'w') if len(sys.argv)==2 else None
if len(sys.argv)>2:raise SystemExit('usage: vertical_user_view_probe.py [fixture]')
if fixture:fixture.write('# original 0041f350: old x z, new x z, leave cells, enter cells\n')
for old,new in ((old,new) for old in cells for new in cells):
    events.clear();packets.clear();allocations=0;cpu.mem_write(buffers,bytes(0x10000))
    put(stack+0x8000,stop,zone,old[0],user);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000)
    cpu.reg_write(UC_X86_REG_ECX,new[0]);cpu.reg_write(UC_X86_REG_EAX,new[1]);cpu.reg_write(UC_X86_REG_EDX,old[1])
    try:cpu.emu_start(0x41f350,stop,count=300000)
    except Exception as error:raise RuntimeError(('view probe failed',hex(cpu.reg_read(UC_X86_REG_EIP)),events)) from error
    if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original view move did not return')
    def rectangle(cell):
        return [(x,z) for z in range(max(0,cell[1]-2),min(9,cell[1]+2)+1) for x in range(max(0,cell[0]-2),min(9,cell[0]+2)+1)]
    old_cells=rectangle(old);new_cells=rectangle(new)
    leaving=[cell for cell in old_cells if cell not in new_cells];entering=[cell for cell in new_cells if cell not in old_cells]
    expected=[]
    for x,z in leaving:expected.extend([('0x4243d0',x,z,'0202'),('0x424130',x,z,'')])
    for x,z in entering:expected.extend([('0x4243d0',x,z,'0102'),('0x4243d0',x,z,'1c05'),('0x423aa0',x,z,'')])
    for x,z in entering:expected.append(('0x4243d0',x,z,'1d05'))
    if events!=expected or allocations!=4:raise AssertionError((old,new,allocations,events,expected))
    if fixture:
        actual_leave=[(x,z) for address,x,z,_ in events if address=='0x424130']
        actual_enter=[(x,z) for address,x,z,_ in events if address=='0x423aa0']
        encode=lambda values:','.join(f'{x}:{z}' for x,z in values) or '-'
        fixture.write(f'{old[0]} {old[1]} {new[0]} {new[1]} {encode(actual_leave)} {encode(actual_enter)}\n')
if fixture:fixture.close()
print('clipped rectangle transition traces passed:',len(cells)**2)

for fail_mask in range(16):
    old,new=(3,3),(4,3);events.clear();packets.clear();allocations=0;cpu.mem_write(buffers,bytes(0x10000))
    put(stack+0x8000,stop,zone,old[0],user);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000)
    cpu.reg_write(UC_X86_REG_ECX,new[0]);cpu.reg_write(UC_X86_REG_EAX,new[1]);cpu.reg_write(UC_X86_REG_EDX,old[1])
    cpu.emu_start(0x41f350,stop,count=300000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original allocation-failure probe did not return')
    expected=[]
    if not fail_mask&1:
        for z in range(1,6):expected.extend([('0x4243d0',1,z,'0202'),('0x424130',1,z,'')])
    if not fail_mask&6:
        for z in range(1,6):expected.extend([('0x4243d0',6,z,'0102'),('0x4243d0',6,z,'1c05'),('0x423aa0',6,z,'')])
    if not fail_mask&8:
        for z in range(1,6):expected.append(('0x4243d0',6,z,'1d05'))
    if allocations!=4 or events!=expected:raise AssertionError((fail_mask,allocations,events,expected))
print('buffer failure combinations passed:',16)

# Shape suppression and payload depend on live user fields after the enter pass.
fail_mask=0
variants=((0,0,0,0,0,False,13),(1,0,0,0,0,False,None),
          (1,0,7,0,0,False,7),(1,2,0,4,5,False,9),(1,2,0,0,0,False,None),(1,2,0,255,1,False,0),
          (0,0,0,0,0,True,7))
for flag,mode,shape,left,right,mutate_shape,expected in variants:
    cpu.mem_write(user,bytes(0x7000));put(user+0xdc,42);cpu.mem_write(user+0x5774,bytes([flag]));cpu.mem_write(user+0x1354,bytes([shape]));put(user+0x1460,mode);put(user+0x1468,left,right)
    events.clear();packets.clear();allocations=0;cpu.mem_write(buffers,bytes(0x10000))
    put(stack+0x8000,stop,zone,3,user);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000)
    cpu.reg_write(UC_X86_REG_ECX,4);cpu.reg_write(UC_X86_REG_EAX,3);cpu.reg_write(UC_X86_REG_EDX,3)
    cpu.emu_start(0x41f350,stop,count=300000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original shape probe did not return')
    shapes=[packet for packet in packets.values() if packet[:2]==b'\x1d\x05']
    if (not shapes if expected is None else len(shapes)==1 and len(shapes[0])==7 and shapes[0][6]==expected) is False:
        raise AssertionError((flag,mode,shape,left,right,mutate_shape,expected,shapes))
print('shape state and late mutation cases passed:',len(variants))

mutate_shape=False;fail_mask=0
for permission,mode in ((10,3),(11,0)):
    cpu.mem_write(user,bytes(0x7000));put(user+0xdc,42);cpu.mem_write(user+0x5768,bytes([permission]));cpu.mem_write(user+0x166,struct.pack('<H',0x1234))
    cpu.mem_write(user+0xd0,struct.pack('<3f',12.5,-3.,99.));put(user+0x1748,0xfffffff9);put(user+0x1460,mode);put(user+0x1470,0x12345678)
    events.clear();packets.clear();allocations=0;cpu.mem_write(buffers,bytes(0x10000));put(stack+0x8000,stop,zone,3,user)
    cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ECX,4);cpu.reg_write(UC_X86_REG_EAX,3);cpu.reg_write(UC_X86_REG_EDX,3)
    cpu.emu_start(0x41f350,stop,count=300000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original appearance probe did not return')
    appearance=[packet for packet in packets.values() if packet[:2]==b'\x01\x02']
    expected=struct.pack('<HIBHfffII',0x201,42,2 if permission<=10 else 0,0x1234,12.5,-3.,99.,0xfffffff9,0x12345678 if mode==3 else 0)
    if appearance!=[expected]:raise AssertionError((permission,mode,appearance,expected))
print('appearance packet cases passed:',2)

speed_cases=((0,0,0,0,0,0,0,0,0),(12,8,0,0,0,0,0,9,4),
             (0xffffffff,0xffffffff,0,0,0,0,0,0,0),(3,2,1,0,0,0,0,255,255),
             (3,2,0,1,0,0,0,255,255),(3,2,0,0,1,0,0,255,255),
             (3,2,0,0,0,1,0,255,0),(3,2,0,0,0,0,1,3,255))
for attack,move,block_a,block_b,casting,fixed,restricted,expected_attack,expected_move in speed_cases:
    cpu.mem_write(user,bytes(0x7000));put(user+0xdc,42);put(user+0x12ec,attack,move);put(user+0x1340,block_a,block_b,fixed);put(user+0x1428,casting);put(user+0x5778,restricted)
    events.clear();packets.clear();allocations=0;cpu.mem_write(buffers,bytes(0x10000));put(stack+0x8000,stop,zone,3,user)
    cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ECX,4);cpu.reg_write(UC_X86_REG_EAX,3);cpu.reg_write(UC_X86_REG_EDX,3)
    cpu.emu_start(0x41f350,stop,count=300000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original speed probe did not return')
    speed=[packet for packet in packets.values() if packet[:2]==b'\x1c\x05'];expected=struct.pack('<HIBB',0x51c,42,expected_attack,expected_move)
    if speed!=[expected]:raise AssertionError((attack,move,block_a,block_b,casting,fixed,restricted,speed,expected))
print('view speed packet cases passed:',len(speed_cases))
