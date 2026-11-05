#!/usr/bin/env python3
"""Trace ReturnHome blocked-home recovery with real map and path resets."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_FPCW
from original_oracle import load_game
cpu,digest=load_game();stack,mob,definition,zone,map_object,tiles,trail,path,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000,0x35000000,0x36000000,0x10000000
for at in (stack,mob,definition,zone,map_object,tiles,trail,path,stop):cpu.mem_map(at,0x10000)
def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
def ret():
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4)
state={}
def hook(machine,at,size,context):
 if at==0x492310:
  state['events'].append('F');put(mob+0x124,100)
  if state['mutate']:cpu.mem_write(mob+0x50,struct.pack('<3f',26,5,20))
  ret()
 elif at==0x48ec70:state['events'].append('P')
 elif at==0x48f600:state['events'].append('T')
 elif at==0x4169d0:raise RuntimeError('Blocked-home recovery unexpectedly moved')
for at in (0x492310,0x48ec70,0x48f600,0x4169d0):cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
cpu.mem_write(stop+16,b'\xc2\x04\x00');put(0x5210cc,stop+16);put(0x5210d0,stop+16)
cpu.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x1000)+b'\xc3')
print('# ps_game.exe SHA256 '+digest+'; CMob::ReturnHome 004a7670 blocked-home recovery, EnableLine 0042ab80, ResetPath 0048ec70 and ChasePathList::Reset 0048f600; ResetFSM/locks intercepted')
print('# immediate pais status trail mutate | status x_bits y_bits z_bits home_x_bits home_y_bits home_z_bits found cursor trail trace')
for immediate,pais,status,has_trail,mutate in ((1,1,3,1,0),(0,1,3,1,0),(1,1,2,1,1),(0,1,2,1,1),(1,0,2,0,0),(0,0,3,0,0)):
 for at in (mob,definition,zone,map_object,tiles,trail,path):cpu.mem_write(at,bytes(0x10000))
 put(0x3ed34b8,0,0,0);put(0x3ed34dc,0,0,0);put(mob+0xa8,definition);put(mob+0x60,zone);put(zone+0x24,map_object);put(map_object+4,32,tiles);put(mob+0x23c,2);put(mob+0x124,status)
 cpu.mem_write(mob+0x250,bytes([pais]));cpu.mem_write(mob+0x50,struct.pack('<3f',25,0,20));cpu.mem_write(mob+0xc88,struct.pack('<3f',10,0,20));put(mob+0x84,path,path,path);put(path+0x1c,0)
 if has_trail:put(trail,23,20,0);put(mob+0xa4,trail)
 for x in (10,24 if immediate else 15):
  tile=20*32+x;old=cpu.mem_read(tiles+tile//8,1)[0];cpu.mem_write(tiles+tile//8,bytes([old|(1<<(tile%8))]))
 state.update(events=[],mutate=mutate);put(stack+0x8000,stop,mob,100);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,mob+0xc88);cpu.reg_write(UC_X86_REG_FPCW,0x27f);cpu.emu_start(0x4a7670,stop,count=1000000)
 if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('ReturnHome blocked-home branch failed')
 print(immediate,pais,status,has_trail,mutate,get(mob+0x124),get(mob+0x50),get(mob+0x54),get(mob+0x58),get(mob+0xc88),get(mob+0xc8c),get(mob+0xc90),int(cpu.mem_read(mob+0xa0,1)[0]!=0),int(get(mob+0x8c)!=0),int(get(mob+0xa4)!=0),''.join(state['events']),sep='\t')
