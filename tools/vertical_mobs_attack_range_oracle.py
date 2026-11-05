#!/usr/bin/env python3
"""Trace CMob::EnableAttackRange using the original CRT sqrt."""
import math,struct
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_game
cpu,digest=load_game();stack,mob,user,definition,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
for address in (stack,mob,user,definition,stop):cpu.mem_map(address,0x10000)
def put(a,*v):cpu.mem_write(a,struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v)))
def bits(f):return struct.unpack('<I',struct.pack('<f',f))[0]
cpu.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x1000)+b'\xc3')
print('# ps_game.exe SHA256 '+digest+'; CMob::EnableAttackRange 004a19c0; real CRT sqrt, FPCW 027f')
print('# mob_x mob_z user_x user_z size range | allowed (float values as DWORD bits)')
cases=[(0,0,x,z,size,range_) for x,z,size,range_ in ((0,0,0,0),(0.29999,0,0,0),(0.3,0,0,0),(0.30001,0,0,0),(3,4,0,5),(5.29999,0,0,5),(5.3,0,0,5),(5.30001,0,0,5),(3,4,2,2),(0,0,0,-1),(1,1,255,255),(math.nan,0,1,1),(math.inf,0,1,1),(1e20,1e20,1,1))]+[(1.25,-2.5,-1.75,1.5,0,5),(1.25,-2.5,-1.75,1.5,0,4),(0,0,0,0,255,0x7fffffff)]
for mx,mz,ux,uz,size,range_ in cases:
    cpu.mem_write(mob,bytes(0x1000));cpu.mem_write(user,bytes(0x1000));cpu.mem_write(definition,bytes(0x1000));put(mob+0xa8,definition);put(mob+0x50,bits(mx));put(mob+0x58,bits(mz));put(user+0xd0,bits(ux));put(user+0xd8,bits(uz));cpu.mem_write(definition+0x3f,bytes([size]));put(stack+0x8000,stop,range_);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ESI,mob);cpu.reg_write(UC_X86_REG_EAX,user);cpu.reg_write(UC_X86_REG_FPCW,0x27f);cpu.emu_start(0x4a19c0,stop,count=100000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('EnableAttackRange did not return')
    print(bits(mx),bits(mz),bits(ux),bits(uz),size,range_,cpu.reg_read(UC_X86_REG_EAX)&255,sep='\t')
