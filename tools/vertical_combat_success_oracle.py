#!/usr/bin/env python3
"""Original CUser::AttackSuccess normal branches with deterministic CRT draws."""
import random,struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
c,digest=load_game();base=0x30000000;c.mem_map(base,0x20000)
u,m,info,stack,end=(base+x for x in (0,0x3000,0x5000,0x10000,0x18000))
def put(a,v):c.mem_write(a,struct.pack('<I',v&0xffffffff))
def word(a,v):c.mem_write(a,struct.pack('<H',v))
def get(a):return struct.unpack('<I',c.mem_read(a,4))[0]
def hook(c,a,size,_):
 global draws
 if a!=0x504697:return
 draws+=1;sp=c.reg_read(UC_X86_REG_ESP);c.reg_write(UC_X86_REG_EAX,roll);c.reg_write(UC_X86_REG_EIP,get(sp));c.reg_write(UC_X86_REG_ESP,sp+4)
c.hook_add(UC_HOOK_CODE,hook);rng=random.Random(0x44d010)
print('# SHA256',digest,'CUser::AttackSuccess 0044d010 null skill melee, ranged, magic; real GetStatus; CRT rand intercepted')
print('# kind unbeatable status user_level mob_level user_accuracy user_evasion mob_accuracy mob_evasion user_dex mob_dex roll result draws')
for i in range(8192):
 kind=i%3;unbeatable=int(i%31==0);status=3 if i%37==0 else i%3
 ul=rng.choice((0,1,10,30,60,100,65535));ml=rng.choice((0,1,10,30,60,100,65535))
 dex=rng.randrange(1,1000);mobdex=rng.randrange(1,1000)
 vals=[rng.choice((0,1,-1,10,50,100,1000,32767,-32768,2147483647,-2147483648,rng.randrange(-5000,5001))) for _ in range(4)]
 ua,ue,ma,me=vals;roll=rng.randrange(32768)
 c.mem_write(u,b'\0'*0x2000);c.mem_write(m,b'\0'*0x1000);put(m+0xa8,info);put(m+0x298,unbeatable);put(m+0x124,status);word(u+0x136,ul);word(info+0x22,ml)
 put(u+0x13b8+44*kind,ua);put(u+0x13c0+44*kind,ue);put(m+0x138+28*kind,ma);put(m+0x140+28*kind,me);put(u+0x122c,dex);word(info+0x3a,mobdex)
 sp=stack+0x800;put(sp,end);put(sp+4,kind);c.reg_write(UC_X86_REG_ESP,sp);c.reg_write(UC_X86_REG_ECX,m);c.reg_write(UC_X86_REG_EDI,u);c.reg_write(UC_X86_REG_EAX,0);c.reg_write(UC_X86_REG_FPCW,0x27f);c.reg_write(UC_X86_REG_FPSW,0);c.reg_write(UC_X86_REG_FPTAG,0xffff);draws=0
 c.emu_start(0x44d010,end,count=5000)
 print(kind,unbeatable,status,ul,ml,ua,ue,ma,me,dex,mobdex,roll,c.reg_read(UC_X86_REG_EAX)&255,draws,sep='\t')
