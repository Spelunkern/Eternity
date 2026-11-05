#!/usr/bin/env python3
"""Execute mob AttackItemOption with selected and hit services intercepted."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_ESP
from original_oracle import load_game
c,digest=load_game();base=0x30000000;c.mem_map(base,0x80000)
user,weapon,mob,skills,stack,stop=base,base+0x10000,base+0x20000,base+0x30000,base+0x60000,base+0x7f000
jewel=0x6882ac+168
state={}
def put(p,*v):c.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
def get(p):return struct.unpack('<I',c.mem_read(p,4))[0]
def hook(cpu,at,size,data):
 if at not in (0x450e20,0x44fd90):return
 sp=cpu.reg_read(UC_X86_REG_ESP)
 if cpu.reg_read(UC_X86_REG_ECX)!=user or get(sp+4)!=mob or get(sp+8)!=skills+320:raise RuntimeError('wrong attack ABI')
 if at==0x44fd90 and [get(sp+12),get(sp+16)]!=[1,1]:raise RuntimeError('wrong hit flags')
 state['events']+='S' if at==0x450e20 else 'H';cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+(12 if at==0x450e20 else 20))
c.hook_add(UC_HOOK_CODE,hook)
print(f'# ps_game.exe SHA256 {digest}; CUser::AttackItemOption(CMob*) 0044c720; selected/hit callbacks intercepted')
print('# guard mob_status actor_mode weapon gem enabled skill_id effect guard_after events')
for guard in (0,1):
 for status in (0,2):
  for mode in (0,1):
   for equipped in (0,1):
    for enabled,skill_id,effect in ((0,2,1),(1,0,1),(1,16,1),(1,2,1),(1,2,3),(1,2,2)):
     c.mem_write(user,bytes(0x6000));c.mem_write(weapon,bytes(0x100));c.mem_write(mob,bytes(0x1000));c.mem_write(skills,bytes(640));c.mem_write(jewel,bytes(168))
     c.mem_write(user+0x57b4,bytes([guard]));put(user+0x142c,mode);put(user+0x1d8,weapon if equipped else 0);put(mob+0x124,status)
     c.mem_write(weapon+0x46,b'\x01');c.mem_write(jewel+0x24,bytes([enabled,enabled]));c.mem_write(jewel+0x46,bytes([skill_id]));c.mem_write(skills+320,struct.pack('<H',2));c.mem_write(skills+320+0x31,bytes([effect]));put(0xfcd2c4,4,skills)
     state['events']='';put(stack,stop,user);c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_EBX,mob)
     c.emu_start(0x44c720,stop,count=100000)
     if c.reg_read(UC_X86_REG_EIP)!=stop or c.reg_read(UC_X86_REG_ESP)!=stack+8:raise RuntimeError('unbalanced original')
     print(guard,status,mode,equipped,1,enabled,skill_id,effect,c.mem_read(user+0x57b4,1)[0],state['events'] or '-',sep='\t')
