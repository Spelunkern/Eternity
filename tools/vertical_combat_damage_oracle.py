#!/usr/bin/env python3
"""Original normal CUser::AttackDamage with real CriticalSuccess/Distance2 and observed CRT rand."""
import random,struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
c,digest=load_game();base=0x30000000;c.mem_map(base,0x30000)
u,m,info,vector,stack,end=(base+x for x in (0,0x4000,0x6000,0x7000,0x10000,0x20000))
def put(a,v):c.mem_write(a,struct.pack('<I',v&0xffffffff))
def word(a,v):c.mem_write(a,struct.pack('<H',v))
def flt(a,v):c.mem_write(a,struct.pack('<f',v))
def get(a):return struct.unpack('<I',c.mem_read(a,4))[0]
def hook(c,a,size,_):
 global draws,distances
 if a==0x41b1f0:distances+=1;return
 if a!=0x504697:return
 sp=c.reg_read(UC_X86_REG_ESP);c.reg_write(UC_X86_REG_EAX,rolls[draws]);draws+=1;c.reg_write(UC_X86_REG_EIP,get(sp));c.reg_write(UC_X86_REG_ESP,sp+4)
c.hook_add(UC_HOOK_CODE,hook);rng=random.Random(0x44d540)
print('# SHA256',digest,'CUser::AttackDamage 0044d540 normal melee/ranged; real CriticalSuccess and Distance2; CRT rand intercepted')
print('# kind userlvl moblvl attack variance wisdom critical mobluck defense attackelement defenseelement shield distance rolls[5] critical damage draws distance_calls')
for i in range(8192):
 kind=i%2;ul=rng.choice((1,5,30,60,100,65535));ml=rng.choice((1,5,30,60,100,65535));attack=rng.choice((0,1,5,25,100,1000,32767,2147483647,-1,-100));variance=rng.choice((0,1,5,100,1000,-1));wis=rng.choice((0,1,10,100,1000,-1,-100,-2147483648));crit=rng.choice((-100,0,1,5,10,50,99,100,500,2147483647,-2147483648));luck=rng.choice((-100,0,1,10,100,1000,2147483647,-2147483648));defense=rng.choice((-100,0,1,20,100,1000,32767,2147483647,-2147483648));ae=rng.randrange(11);de=rng.randrange(11);shield=rng.randrange(2);distance=rng.choice((0.0,1.0,2.99,3.0,3.01,10.0));rolls=[rng.randrange(32768) for _ in range(5)]
 c.mem_write(u,b'\0'*0x2000);c.mem_write(m,b'\0'*0x1000);c.mem_write(info,b'\0'*0x1000);c.mem_write(vector,b'\0'*16);put(m+0xa8,info);word(u+0x136,ul);word(info+0x22,ml);put(u+0x13bc+44*kind,attack);put(u+0x12dc,variance);put(u+0x1234,wis);put(u+0x13c8+44*kind,crit);put(m+0x234,luck);put(m+0x144+28*kind,defense);put(u+0x1280,ae);put(m+0x238,de);put(u+0x1330,shield);flt(u+0xd0,0);flt(u+0xd8,0);flt(m+0x50,distance);flt(m+0x58,0)
 sp=stack+0x800;put(sp,end);put(sp+4,kind);put(sp+8,0);c.reg_write(UC_X86_REG_ESP,sp);c.reg_write(UC_X86_REG_EAX,m);c.reg_write(UC_X86_REG_ECX,u);c.reg_write(UC_X86_REG_ESI,vector);c.reg_write(UC_X86_REG_FPCW,0x27f);c.reg_write(UC_X86_REG_FPSW,0);c.reg_write(UC_X86_REG_FPTAG,0xffff);draws=0;distances=0
 c.emu_start(0x44d540,end,count=30000)
 print(kind,ul,ml,attack,variance,wis,crit,luck,defense,ae,de,shield,distance,*rolls,c.reg_read(UC_X86_REG_EAX)&255,struct.unpack('<i',c.mem_read(vector,4))[0],draws,distances,sep='\t')
