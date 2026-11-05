#!/usr/bin/env python3
"""Trace the common post-hit/miss attack deadline in the original executable."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_game
cpu,digest=load_game();stack,mob,user,definition,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
for address in (stack,mob,user,definition,stop):cpu.mem_map(address,0x10000)
def put(a,*v):cpu.mem_write(a,struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v)))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
def bits(f):return struct.unpack('<I',struct.pack('<f',f))[0]
def hook(machine,address,size,context):machine.reg_write(UC_X86_REG_EIP,0x4a235f)
cpu.hook_add(UC_HOOK_CODE,hook,begin=0x4a2056,end=0x4a2056);cpu.mem_write(0x505316,b'\xc3')
print('# ps_game.exe SHA256 '+digest+'; CMob::Attack 004a2000 common deadline at 004a235f; target-side body intercepted')
print('# now delay speed multiplier_bits | next_attack')
for now,delay,speed,multiplier in ((100,2000,3,1.0),(100,2000,0,2.0),(100,2000,6,0.5),(0xfffffffe,500,6,1.0),(100,2000,99,1.0),(100,2000,-1,1.0),(100,2000,3,0.0),(100,2000,3,-1.0),(100,16777217,3,3.0),(100,2000,3,float('nan'))):
 for at in (mob,user,definition):cpu.mem_write(at,bytes(0x10000))
 put(mob+0xa8,definition);put(mob+0x240,speed);put(mob+0x26c,bits(multiplier));put(definition+0x84,delay);put(stack+0x8000,stop,now)
 cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ECX,mob);cpu.reg_write(UC_X86_REG_EDX,user);cpu.reg_write(UC_X86_REG_FPCW,0x27f);cpu.emu_start(0x4a2000,stop,count=10000)
 if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Mob Attack deadline did not return')
 print(now,delay,speed,bits(multiplier),get(mob+0xcb4),sep='\t')
