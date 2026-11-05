#!/usr/bin/env python3
"""Original mob-to-user normal AttackDamage, stopping before conditional mob logging."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game

c, digest = load_game()
base = 0x30000000
c.mem_map(base, 0x30000)
mob, user, info, entry, damage, stack = (base+x for x in (0,0x4000,0x8000,0x9000,0xa000,0x10000))
def put(a, v): c.mem_write(a, struct.pack('<I', v & 0xffffffff))
def word(a, v): c.mem_write(a, struct.pack('<H', v))
def flt(a, v): c.mem_write(a, struct.pack('<f', v))
def get(a): return struct.unpack('<I', c.mem_read(a, 4))[0]
state = {}
def hook(machine, address, size, context):
    if address == 0x504697:
        sp = machine.reg_read(UC_X86_REG_ESP)
        machine.reg_write(UC_X86_REG_EAX, state['rolls'][state['draws']])
        state['draws'] += 1
        machine.reg_write(UC_X86_REG_EIP, get(sp))
        machine.reg_write(UC_X86_REG_ESP, sp+4)
c.hook_add(UC_HOOK_CODE, hook, begin=0x504697, end=0x504697)
rng = random.Random(0x4a2920)
print('# ps_game.exe SHA256', digest, 'CMob::AttackDamage 004a2920..004a3026; CRT random intercepted')
print('# kind moblevel userlevel base variance multiplier attack defense attribute wisdom critical userwisdom rolls[5] | critical damage draws')
for i in range(2048):
    kind=i%4; ml=rng.choice((0,1,5,30,60,100,65535)); ul=rng.choice((0,1,5,30,60,100,65535))
    attack=rng.choice((-100,0,1,50,1000,32767,2147483647,-2147483648)); defense=rng.choice((-100,0,1,50,1000,32767,2147483647,-2147483648))
    raw=rng.choice((0,1,50,1000,65535)); variance=rng.choice((0,1,5,100,65535)); multiplier=rng.choice((0.0,0.5,1.0,1.5,10.0))
    attribute=rng.randrange(11); wisdom=rng.choice((0,1,10,100,65535)); critical=rng.choice((-100,0,5,50,99,100,1000,2147483647,-2147483648)); uw=rng.choice((-100,0,1,100,1000,2147483647,-2147483648)); rolls=[rng.randrange(32768) for _ in range(5)]
    for a,n in ((mob,0x1000),(user,0x2000),(info,0x1000),(entry,0x1000),(damage,0x1000)): c.mem_write(a,bytes(n))
    put(mob+0xa8,info);word(info+0x22,ml);word(info+0x3c,wisdom);word(user+0x136,ul)
    word(entry,kind);word(entry+10,raw);word(entry+12,variance);flt(mob+0x268,multiplier)
    if kind<3:
        put(mob+0x13c+28*kind,attack);put(mob+0x148+28*kind,critical);put(user+0x13c4+44*kind,defense)
    put(user+0x1284,attribute);put(user+0x1234,uw)
    sp=stack+0x8000;put(sp,0);put(sp+4,damage)
    c.reg_write(UC_X86_REG_ESP,sp);c.reg_write(UC_X86_REG_EAX,entry);c.reg_write(UC_X86_REG_EBX,user);c.reg_write(UC_X86_REG_ECX,mob)
    c.reg_write(UC_X86_REG_FPCW,0x27f);c.reg_write(UC_X86_REG_FPSW,0);c.reg_write(UC_X86_REG_FPTAG,0xffff)
    state.update(rolls=rolls,draws=0)
    c.emu_start(0x4a2920,0x4a3026,count=10000)
    result=struct.unpack('<i',c.mem_read(damage,4))[0]; crit=c.mem_read(sp+4,1)[0]
    print(kind,ml,ul,raw,variance,multiplier,attack,defense,attribute,wisdom,critical,uw,*rolls,crit,result,state['draws'],sep='\t')
