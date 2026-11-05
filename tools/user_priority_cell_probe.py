#!/usr/bin/env python3
"""Original CCell::PSendRange 00424780; locks/send and sqrt CRT ported."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_game
cpu,digest=load_game();stack,cell,head,user,position,buffer,stop=(0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000,0x10000000)
for p in (stack,cell,head,user,position,buffer,stop):cpu.mem_map(p,0x10000)
def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
for p in (0x5210cc,0x5210d0):put(p,stop+0x100)
cpu.mem_write(stop+0x100,b'\xc2\x04\x00');cpu.mem_write(0x5048a4,b'\xdd\x44\x24\x04\xd9\xfa\xc3')
for p,n in ((0x4d4cf0,4),(0x4bd6f0,8)):cpu.mem_write(p,b'\xc2'+struct.pack('<H',n))
cpu.reg_write(UC_X86_REG_FPCW,0x27f);events=[]
def hook(m,p,size,data):
 if p in (0x4d4cf0,0x4bd6f0):
  sp=m.reg_read(UC_X86_REG_ESP);args=struct.unpack('<'+'I'*(1 if p==0x4d4cf0 else 2),m.mem_read(sp+4,4 if p==0x4d4cf0 else 8));events.append((p,args))
cpu.hook_add(UC_HOOK_CODE,hook);put(cell,head);put(head+8,user+8,user+8);put(user+8,head+8,head+8);cpu.mem_write(position,struct.pack('<3f',100.,0.,100.))
for x in (19.9,20.,20.1,100.,179.9,180.,180.1,300.):
 for target in (42,43,0):
  cpu.mem_write(user+0xd0,struct.pack('<3f',x,9.,100.));put(user+0xdc,target);events.clear()
  put(stack+0x8000,stop,position,struct.unpack('<I',struct.pack('<f',80.))[0],42,0,7)
  cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,buffer);cpu.reg_write(UC_X86_REG_EBX,cell)
  cpu.emu_start(0x424780,stop,count=10000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
  actual_x=struct.unpack('<f',struct.pack('<f',x))[0];distance=struct.unpack('<f',struct.pack('<f',abs(100.-actual_x)))[0]
  expected=[(0x4d4cf0,(buffer,))] if target in (42,0) else [(0x4bd6f0,(7,struct.unpack('<I',struct.pack('<f',distance))[0]))] if distance<=80 else []
  assert events==expected,(x,target,events,expected)
print('24 original prioritized cell recipient cases passed; ps_game.exe SHA256',digest)
# Ring uses the same immediate IDs and includes both 80 and 120 boundaries.
for x in (-20.1,-20.,-19.9,19.9,20.,20.1,100.,180.,220.,220.1):
 for target in (42,43,0):
  cpu.mem_write(user+0xd0,struct.pack('<3f',x,9.,100.));put(user+0xdc,target);events.clear()
  put(stack+0x8000,stop,position,struct.unpack('<I',struct.pack('<f',80.))[0],struct.unpack('<I',struct.pack('<f',120.))[0],42,0,7)
  cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,buffer);cpu.reg_write(UC_X86_REG_EBX,cell)
  cpu.emu_start(0x424880,stop,count=10000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
  actual_x=struct.unpack('<f',struct.pack('<f',x))[0];distance=struct.unpack('<f',struct.pack('<f',abs(100.-actual_x)))[0]
  expected=[(0x4d4cf0,(buffer,))] if target in (42,0) else [(0x4bd6f0,(7,struct.unpack('<I',struct.pack('<f',distance))[0]))] if 80<=distance<=120 else []
  assert events==expected,(x,target,events,expected)
print('30 original prioritized ring recipient cases passed')
