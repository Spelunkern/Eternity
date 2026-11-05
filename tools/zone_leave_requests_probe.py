#!/usr/bin/env python3
"""CUser::LeaveZone vehicle/invitation branch 0044b04c..0044b19c."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_ESI,UC_X86_REG_ESP
from original_oracle import load_game
cpu,digest=load_game();stop,stack,user,peer,zone=(0x10000000,0x20000000,0x30000000,0x31000000,0x32000000)
for p in (stop,stack,user,peer,zone):cpu.mem_map(p,0x10000)
def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
def get(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
for p,n in ((0x415f30,4),(0x4d4d20,8)):cpu.mem_write(p,b'\xc2'+struct.pack('<H',n))
events=[];found=False
def hook(m,p,size,data):
 sp=m.reg_read(UC_X86_REG_ESP)
 if p==0x415f30:events.append(('find',get(sp+4)));m.reg_write(UC_X86_REG_EAX,peer if found else 0)
 elif p==0x4d4d20:events.append(('send',bytes(m.mem_read(get(sp+4),get(sp+8)))))
cpu.hook_add(UC_HOOK_CODE,hook)
count=0
for mode in (0,1,2,3):
 for peer_mode in (0,2):
  for found in (False,True):
   for active in (False,True):
    cpu.mem_write(user,bytes(0x7000));cpu.mem_write(peer,bytes(0x7000));events.clear();put(user+0xdc,42);put(user+0xe0,zone);put(user+0x1460,mode);put(peer+0x1460,peer_mode);put(user+0x1474,99 if active else 0,111);put(peer+0x1474,42,222)
    for off in (0x1740,0x1754,0x5478):put(user+off,99 if active else 0)
    for off in (0x5834,0x5838,0x583c):put(user+off,777)
    put(stack+0x8000,0,0);cpu.reg_write(UC_X86_REG_EBX,0);cpu.reg_write(UC_X86_REG_ESI,user);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.emu_start(0x44b04c,0x44b19c,count=10000)
    expected=[]
    if active and mode in (0,2):
     expected.append(('find',99))
     if found and mode==0 and peer_mode==2:expected.append(('send',b'\x1e\x02\x02'))
    if active:
     for packet in (b'\x03\x0b\x01\x2a\0\0\0',b'\x06\x0d\x00\x63\0\0\0',b'\x03\x22\x00'):
      expected.append(('find',99))
      if found:expected.append(('send',packet))
    assert events==expected,(mode,peer_mode,found,active,events,expected)
    assert get(user+0x1474)==(0 if active and mode in (0,2) else (99 if active else 0))
    assert get(user+0x1478)==(0 if active and mode==2 else 111)
    assert get(peer+0x1474)==(0 if active and found and (mode==2 or (mode==0 and peer_mode==2)) else 42)
    assert get(peer+0x1478)==(0 if active and found and mode==0 and peer_mode==2 else 222)
    for off,timer in ((0x1740,0x5834),(0x1754,0x5838),(0x5478,0x583c)):
     assert get(user+off)==0 and get(user+timer)==(0 if active else 777)
    count+=1
print(count,'original zone-leave request cases passed; ps_game.exe SHA256',digest)
