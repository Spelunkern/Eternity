#!/usr/bin/env python3
"""Execute original SendUseSkill for the selected mob multi-hit call."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_EIP,UC_X86_REG_ESP
from original_oracle import load_game
c,digest=load_game();base=0x30000000;c.mem_map(base,0x80000)
user,skill,stack,stop=base,base+0x10000,base+0x60000,base+0x7f000
seen=[]
def put(p,*v):c.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
def get(p):return struct.unpack('<I',c.mem_read(p,4))[0]
def hook(cpu,at,size,data):
 if at==0x421220:
  sp=cpu.reg_read(UC_X86_REG_ESP);packet=get(sp+8);length=get(sp+12)
  args=[get(sp+i) for i in (4,12,20,24,28,32)]
  seen.append((bytes(cpu.mem_read(packet,length)).hex(),args))
  cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+36)
c.hook_add(UC_HOOK_CODE,hook)
print(f'# ps_game.exe SHA256 {digest}; CUser::SendUseSkill 0044e900, mob/user kind 4; zone PSendView intercepted')
print('# mob_target zoned actor target skill level packet')
for mob_target in (0,1):
 for zoned in (0,1):
  for actor,mob,identifier,level in ((42,77,11,2),(0x12345678,0x87654321,0xabcd,255)):
   c.mem_write(user,bytes(0x200));c.mem_write(skill,bytes(160));put(user+0xdc,actor);put(user+0xe0,base+0x30000 if zoned else 0)
   c.mem_write(skill,struct.pack('<HB',identifier,level));seen.clear();put(stack,stop,user,mob,mob_target)
   c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_ECX,skill);c.reg_write(UC_X86_REG_EDX,4)
   c.emu_start(0x44e900,stop,count=100000)
   if c.reg_read(UC_X86_REG_EIP)!=stop or c.reg_read(UC_X86_REG_ESP)!=stack+16:raise RuntimeError('unbalanced original')
   if zoned:
    packet,args=seen[0]
    if len(seen)!=1 or args!=[base+0x30000,20,0x42700000,actor,0 if mob_target else mob,5]:raise RuntimeError('wrong view ABI')
   else:
    if seen:raise RuntimeError('unexpected view')
    packet='-'
   print(mob_target,zoned,actor,mob,identifier,level,packet,sep='\t')
