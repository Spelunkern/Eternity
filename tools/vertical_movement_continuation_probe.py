#!/usr/bin/env python3
"""Original PacketPC success prefix, movement-view send ported."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stop,stack,user,packet=(0x10000000,0x20000000,0x30000000,0x31000000)
for p in (stop,stack,user,packet):cpu.mem_map(p,0x10000)
def put(p,*values):cpu.mem_write(p,struct.pack('<'+'I'*len(values),*values))
def get(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
put(0x5210e4,stop+0x100);cpu.mem_write(stop+0x100,b'\xc3')
for p,cleanup in ((0x489ed0,0),(0x48a090,0),(0x4bd950,12)):cpu.mem_write(p,b'\xc2'+struct.pack('<H',cleanup))
events=[]
def hook(m,address,size,data):
    if address==stop+0x100:m.reg_write(UC_X86_REG_EAX,0x12345678)
    elif address in (0x489ed0,0x48a090):events.append(('status',address))
    elif address==0x4bd950:
        sp=m.reg_read(UC_X86_REG_ESP);p,n,arg=struct.unpack('<3I',m.mem_read(sp+4,12));events.append(('send',bytes(m.mem_read(p,n)),m.reg_read(UC_X86_REG_EBX),arg))
cpu.hook_add(UC_HOOK_CODE,hook)
for motion in (0,1,0x7f,0x80,0xff):
 for attack in (0,1,128):
  for vehicle in (0,1,2,3):
   cpu.mem_write(user,bytes(0x7000));cpu.mem_write(packet,bytes(0x100));events.clear()
   put(user+0xdc,0x87654321);put(user+0x1460,vehicle,0xabcdef01);cpu.mem_write(user+0x1448,bytes([attack]))
   cpu.mem_write(packet,struct.pack('<HBHIII',0x501,motion,0xa55a,0x3f800000,0xc0200000,0x7fc00001))
   cpu.reg_write(UC_X86_REG_EDI,user);cpu.reg_write(UC_X86_REG_EBP,packet);cpu.reg_write(UC_X86_REG_EBX,1);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000)
   cpu.emu_start(0x46a49d,0x46a558,count=10000);assert cpu.reg_read(UC_X86_REG_EIP)==0x46a558
   final_motion=motion|0x80 if attack else motion
   expected=struct.pack('<HIBHIII',0x501,0x87654321,final_motion,0xa55a,0x3f800000,0xc0200000,0x7fc00001)
   assert events==[('status',0x489ed0 if motion&0x80 else 0x48a090),('send',expected,7,0)],events
   assert get(user+0x57cc)==0x12345678 and cpu.mem_read(user+0x166,2)==b'\x5a\xa5' and cpu.mem_read(user+0x1435,1)==bytes([final_motion])
   assert get(user+0x1460)==(0 if vehicle==1 else vehicle) and get(user+0x1464)==(0 if vehicle==1 else 0xabcdef01)
print('60 original movement continuation cases passed; ps_game.exe SHA256',digest)
