#!/usr/bin/env python3
"""Execute original PacketPC movement-speed selector including GetMoveSpeedIndex."""
import struct
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_game
cpu,digest=load_game();stack,user=0x20000000,0x30000000
for p in (stack,user):cpu.mem_map(p,0x10000)
cpu.reg_write(UC_X86_REG_FPCW,0x27f)
def put(p,value):cpu.mem_write(p,struct.pack('<I',value&0xffffffff))
for vehicle in (0,1,2,3,4,0xffffffff):
 for speed in (-2147483648,-1,0,1,2,3,4,5,2147483647):
  cpu.mem_write(user,bytes(0x7000));put(user+0x1460,vehicle);put(user+0x12f0,speed)
  cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EDI,user)
  cpu.emu_start(0x46a36d,0x46a3a8,count=1000)
  assert cpu.reg_read(UC_X86_REG_EIP)==0x46a3a8
  bits=cpu.reg_read(UC_X86_REG_EAX);actual=struct.unpack('<f',struct.pack('<I',bits))[0]
  expected=8.0 if vehicle==2 else 10007.0 if vehicle==3 else 3.0+max(0,min(4,speed))
  assert actual==expected,(vehicle,speed,actual,expected)
print('54 original movement-speed selector cases passed; ps_game.exe SHA256',digest)
