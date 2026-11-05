#!/usr/bin/env python3
"""Guild-creation cleanup inside CUser::LeaveZone 0044b19c..0044b1eb."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_ESP
from original_oracle import load_game
cpu,digest=load_game();stop,stack,user,peer,creation=(0x10000000,0x20000000,0x30000000,0x31000000,0x32000000)
for p in (stop,stack,user,peer,creation):cpu.mem_map(p,0x10000)
def put(p,*v):cpu.mem_write(p,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
def get(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
for p,n in ((0x433430,0),(0x432a50,0),(0x505316,0)):cpu.mem_write(p,b'\xc2'+struct.pack('<H',n))
events=[]
def hook(m,p,size,data):
 if p==0x433430:
  n=m.reg_read(UC_X86_REG_EAX);addr=m.reg_read(UC_X86_REG_ECX);events.append(('broadcast',bytes(m.mem_read(addr,n))))
 elif p==0x432a50:events.append(('release',m.reg_read(UC_X86_REG_EAX)))
cpu.hook_add(UC_HOOK_CODE,hook)
for present in (False,True):
 for phase in (0,1,2):
  for subject_slot in ('leader','member','absent'):
   cpu.mem_write(user,bytes(0x7000));cpu.mem_write(peer,bytes(0x7000));cpu.mem_write(creation,bytes(0x1000));cpu.mem_write(stack,b'\xa5'*0x10000);events.clear()
   put(user+0x175c,creation if present else 0);put(peer+0x175c,creation);put(creation+0x2c,user if subject_slot=='leader' else peer);put(creation+0x4c,user if subject_slot=='member' else peer);cpu.mem_write(creation+8,bytes([phase]));cpu.mem_write(creation+9,b'Guild\0');
   cpu.reg_write(UC_X86_REG_ESI,user);cpu.reg_write(UC_X86_REG_EBX,0);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.emu_start(0x44b19c,0x44b1eb,count=10000)
   expected=[]
   if present:
    if phase:expected.append(('broadcast',b'\x01\x0d\x07'+b'\xa5'*5+b'Guild\0'+b'\xa5'*84))
    else:expected.append(('broadcast',b'\x03\x0d\x03'))
    expected.append(('release',creation if subject_slot=='absent' else 0))
   assert events==expected,(present,phase,subject_slot,events,expected)
   if present:assert get(user+0x175c)==(creation if subject_slot=='absent' else 0) and get(peer+0x175c)==0
print('18 original guild creation cleanup cases passed; ps_game.exe SHA256',digest)
