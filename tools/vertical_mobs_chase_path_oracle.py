#!/usr/bin/env python3
"""Trace Chase direct-line and retained-cursor selection with real map/path."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_FPCW
from original_oracle import load_game
cpu,digest=load_game();stack,mob,target,definition,zone,mapobj,tiles,node,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000,0x35000000,0x36000000,0x10000000
for address in (stack,mob,target,definition,zone,mapobj,tiles,node,stop):cpu.mem_map(address,0x10000)
def put(a,*v):cpu.mem_write(a,struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v)))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
def bits(f):return struct.unpack('<I',struct.pack('<f',f))[0]
def ret(machine,cleanup=0,result=None):
 sp=machine.reg_read(UC_X86_REG_ESP);machine.reg_write(UC_X86_REG_EIP,get(sp));machine.reg_write(UC_X86_REG_ESP,sp+4+cleanup)
 if result is not None:machine.reg_write(UC_X86_REG_EAX,result)
state={}
def hook(machine,address,size,context):
    if address==0x503a3f:state['alloc']+=1;ret(machine,result=node)
    elif address in (0x4a6fde,0x4a725f):
        state['route']='W' if address==0x4a6fde else 'B';state['x']=machine.reg_read(UC_X86_REG_ESI);state['z']=machine.reg_read(UC_X86_REG_EBX);machine.reg_write(UC_X86_REG_EAX,0);machine.reg_write(UC_X86_REG_ESP,stack+0x8014);machine.reg_write(UC_X86_REG_EIP,stop)
for address in (0x503a3f,0x4a6fde,0x4a725f):cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
cpu.mem_write(stop+16,b'\xc2\x04\x00');put(0x5210cc,stop+16);put(0x5210d0,stop+16);cpu.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x1000)+b'\xc3')
print('# ps_game.exe SHA256 '+digest+'; CMob::Chase 004a6de0, EnableLine 0042ab80 and SetPath 004a6b20; allocation/locks/continuations intercepted')
print('# mob_x mob_z target_x target_z blocked found cursor_x cursor_z | route selected_x selected_z found cursor alloc (float positions as bits)')
cases=((10,10,20,20,0,0,0,0),(10.8,10.2,20.9,20.7,0,0,0,0),(10,10,20,20,1,0,0,0),(10,10,20,20,1,1,15,12),(10,10,20,20,0,1,13,18))
for mx,mz,tx,tz,blocked,found,cx,cz in cases:
 for address in (mob,target,definition,zone,mapobj,tiles,node):cpu.mem_write(address,bytes(0x10000))
 put(0x3ed34b8,0,0,0);put(mob+0xa8,definition);put(mob+0x60,zone);put(zone+0x24,mapobj);put(mapobj+4,81,tiles);put(mob+0x50,bits(mx),0,bits(mz));put(mob+0xc88,bits(mx),0,bits(mz));put(target,bits(tx),0,bits(tz));put(mob+0x23c,2)
 if blocked:
  tile=15*81+15;cpu.mem_write(tiles+tile//8,bytes([1<<(tile%8)]))
 if found:put(mob+0x84,node,node,node);cpu.mem_write(mob+0xa0,b'\x01');put(node+4,cx,cz);put(node+0x1c,0)
 state.update(route='?',x=0,z=0,alloc=0);put(stack+0x8000,stop,mob,50,target,0);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_FPCW,0x27f);cpu.emu_start(0x4a6de0,stop,count=1000000)
 if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Chase path selection did not return')
 print(bits(mx),bits(mz),bits(tx),bits(tz),blocked,found,cx,cz,state['route'],state['x'],state['z'],int(cpu.mem_read(mob+0xa0,1)[0]!=0),int(get(mob+0x8c)!=0),state['alloc'],sep='\t')
