#!/usr/bin/env python3
"""Execute CMob::SkillHeal, its self HealUser and real CMob::Heal."""
import itertools,struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
c,digest=load_game();stack,mob,user,skill,info,zone,stop=(0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000,0x35000000)
for a in (stack,mob,user,skill,info,zone,stop):c.mem_map(a,0x10000)
def put(a,*v):c.mem_write(a,struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v)))
def get(a):return struct.unpack('<I',c.mem_read(a,4))[0]
def word(a):return struct.unpack('<H',c.mem_read(a,2))[0]
state={}
def hook(m,a,size,ctx):
    sp=m.reg_read(UC_X86_REG_ESP)
    if a==0x4b8d10:state['trace']+='D'
    elif a==0x4a5a30:state['trace']+='C'
    elif a==0x4a5520:state['trace']+='R'
    elif a==0x421220:
        args=[get(sp+i) for i in range(4,36,4)];assert args[0]==zone and args[2:]==[21,mob+0x50,0x42700000,0,42,6]
        state['trace']+='P';state['packet']=bytes(c.mem_read(args[1],21)).hex()
for a,ret in ((0x4b8d10,b'\xc3'),(0x4a5a30,b'\xc2\x04\x00'),(0x4a5520,b'\xc2\x08\x00'),(0x421220,b'\xc2\x20\x00'),(0x505316,b'\xc3')):
    c.mem_write(a,ret);c.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
print('# ps_game.exe SHA256 '+digest+'; CMob::SkillHeal 004a54e0 / HealUser 004a5780 / Heal 004909e0; reset, remove-state, range and view intercepted')
print('# target detail hp mp sp heal_h heal_m heal_s wis zone | trace hp mp sp packet')
for target,detail,hp,mp,sp,hh,hm,hs,wis,has_zone in itertools.product((2,3),(40,41,42),(0,90,100),(0,49),(0,69),(0,20),(0,10),(0,10),(0,5),(0,1)):
    c.mem_write(mob,bytes(0x1000));c.mem_write(user,bytes(0x2000));c.mem_write(skill,bytes(0x100));c.mem_write(info,bytes(0x100));put(mob+0xa8,info);put(mob+0x5c,77);put(mob+0x60,zone if has_zone else 0);put(mob+0x130,2);put(mob+0x224,hp);c.mem_write(mob+0x228,struct.pack('<HH',mp,sp));put(info+0x30,100);c.mem_write(info+0x34,struct.pack('<HH',50,70));c.mem_write(info+0x3a,struct.pack('<H',wis));put(user+0xdc,42)
    c.mem_write(skill,struct.pack('<HB',123,4));c.mem_write(skill+0x32,struct.pack('<H',detail));c.mem_write(skill+0x56,bytes([target]));c.mem_write(skill+0x88,struct.pack('<HHH',hh,hm,hs));put(stack+0x8000,stop)
    state.update(trace='',packet='-');c.reg_write(UC_X86_REG_ESP,stack+0x8000);c.reg_write(UC_X86_REG_ECX,mob);c.reg_write(UC_X86_REG_EDX,user);c.reg_write(UC_X86_REG_ESI,skill)
    c.emu_start(0x4a54e0,stop,count=10000);assert c.reg_read(UC_X86_REG_EIP)==stop
    print(target,detail,hp,mp,sp,hh,hm,hs,wis,has_zone,state['trace'] or '-',get(mob+0x224),word(mob+0x228),word(mob+0x22a),state['packet'],sep='\t')
