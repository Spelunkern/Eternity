#!/usr/bin/env python3
"""Run ReturnHome's real blocked-line A* search and failure reset."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_FPCW
from original_oracle import load_game
cpu,digest=load_game();stack,mob,definition,zone,map_object,tiles,heap,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000,0x42000000,0x10000000
for at in (stack,mob,definition,zone,map_object,tiles,stop):cpu.mem_map(at,0x10000)
cpu.mem_map(heap,0x1000000)
def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
def ret(cleanup=0,result=None):
 sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+cleanup)
 if result is not None:cpu.reg_write(UC_X86_REG_EAX,result)
state={}
def hook(machine,at,size,context):
 sp=machine.reg_read(UC_X86_REG_ESP)
 if at==0x503a3f:
  amount=get(sp+4);address=state['allocation'];state['allocation']+=(max(amount,1)+15)&~15
  if state['allocation']>=heap+0x1000000:raise RuntimeError('A* allocation exhausted')
  cpu.mem_write(address,bytes(amount));ret(result=address)
 elif at==0x4169d0:
  assert machine.reg_read(UC_X86_REG_EAX)==mob and get(sp+4)==zone and get(sp+20)==1
  state['moves']+=1;put(mob+0x50,get(sp+8),get(sp+12),get(sp+16));ret(20,1)
 elif at==0x492310:
  assert get(mob+0x124)==100;state['resets']+=1;ret()
for at in (0x503a3f,0x4169d0,0x492310):cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
cpu.mem_write(stop+16,b'\xc2\x04\x00');put(0x5210cc,stop+16);put(0x5210d0,stop+16)
cpu.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x1000)+b'\xc3')
print('# ps_game.exe SHA256 '+digest+'; CMob::ReturnHome 004a7670, CMap::EnableLine 0042ab80 and AStar::FindPath 0048eda0; allocation/locks/MoveMob/ResetFSM intercepted')
print('# wall_gap pais initial_status | moves resets x_bits z_bits home_x_bits home_z_bits next_chase status found cursor_x cursor_z')
for gap,pais,status in ((1,1,3),(0,1,3),(0,0,3),(0,1,2)):
 for at in (mob,definition,zone,map_object,tiles):cpu.mem_write(at,bytes(0x10000))
 put(0x3ed34b8,0,0,0);put(0x3ed34dc,0,0,0);put(mob+0xa8,definition);put(mob+0x60,zone);put(zone+0x24,map_object);put(map_object+4,32,tiles);put(mob+0x23c,2);put(mob+0x124,status);put(definition+0x64,500)
 cpu.mem_write(mob+0x250,bytes([pais]));cpu.mem_write(definition+0x68,b'\x02');cpu.mem_write(mob+0x50,struct.pack('<3f',25,0,25));cpu.mem_write(mob+0xc88,struct.pack('<3f',2,0,2))
 for z in range(32):
  if not gap or z!=10:
   index=z*32+12;current=cpu.mem_read(tiles+index//8,1)[0];cpu.mem_write(tiles+index//8,bytes([current|(1<<(index%8))]))
 state.update(allocation=heap,moves=0,resets=0);put(stack+0x8000,stop,mob,100);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,mob+0xc88);cpu.reg_write(UC_X86_REG_FPCW,0x27f);cpu.emu_start(0x4a7670,stop,count=100000000)
 if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('ReturnHome A* did not return')
 cursor=get(mob+0x8c);cx=get(cursor+4) if cursor else 0xffffffff;cz=get(cursor+8) if cursor else 0xffffffff
 print(gap,pais,status,state['moves'],state['resets'],get(mob+0x50),get(mob+0x58),get(mob+0xc88),get(mob+0xc90),get(mob+0xcb8),get(mob+0x124),int(cpu.mem_read(mob+0xa0,1)[0]!=0),cx,cz,sep='\t')
