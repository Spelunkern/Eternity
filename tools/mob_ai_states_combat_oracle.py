#!/usr/bin/env python3
"""Execute original CMob::WhileCombat 004b55c0; observe required world/Lua ports."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
cpu,digest=load_game()
base=0x30000000
cpu.mem_map(base,0x20000)
mob,definition,user,stack,stop=[base+x for x in (0,0x2000,0x4000,0x18000,0x1f000)]
offsets=[0x24c,0x248,0x240,0x290,0x284,0xcb4,0x224,0x28c,0x12c,0xcd0,0x2b4]
context={}
def put(at,*values):cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
def ret(cleanup=0,result=None):
    sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+cleanup)
    if result is not None:cpu.reg_write(UC_X86_REG_EAX,result)
def emit(tag,*values):context['trace'].append(tag+':'+','.join(str(v) for v in values))
def hook(machine,address,size,data):
    sp=machine.reg_read(UC_X86_REG_ESP)
    if address==stop+16:
        emit('T')
        if context['mutation']==1:put(mob+0x24c,1)
        if context['mutation']==2:
            put(mob+0x24c,0);put(mob+0x248,0);put(mob+0x290,0);put(mob+0xcb4,context['now'])
        ret(result=context['now'])
    elif address==0x415f30:
        assert machine.reg_read(UC_X86_REG_EAX)==get(mob+0x60)
        emit('F',get(sp+4))
        if context['mutation']==3:put(mob+0x290,1);put(mob+0x284,1);put(mob+0xcb4,0xffffffff)
        ret(4,user if context['found'] else 0)
    elif address==0x4b66f0:
        assert machine.reg_read(UC_X86_REG_EAX)==mob and get(sp+8)==user
        emit('A',get(sp+4))
        if context['mutation']==4:put(mob+0x24c,1);put(mob+0x2b4,100)
        ret(8,context['success'])
    elif address==0x4a1580:
        assert get(sp+4)==mob;emit('V')
        if context['mutation']==5:put(mob+0x12c,0x76543210)
        ret(4)
    elif address==0x4b62c0:
        assert machine.reg_read(UC_X86_REG_EBX)==mob
        emit('R',machine.reg_read(UC_X86_REG_EAX));ret()
    elif address==0x4b1210:
        name=bytes(machine.mem_read(machine.reg_read(UC_X86_REG_EBX),32)).split(b'\0')[0].decode()
        assert get(sp+4)==mob and machine.reg_read(UC_X86_REG_EDI)==get(mob+0x27c)
        assert name=='WhileCombat', name
        emit('L',get(sp+8),get(sp+12),get(sp+16))
        if context['mutation']==6:put(mob+0x2b4,123);put(mob+0x290,0)
        ret(16)
cpu.hook_add(UC_HOOK_CODE,hook)
put(0x5210e4,stop+16)
print(f'# ps_game.exe SHA256 {digest}; original CMob::WhileCombat 004b55c0; tick, FindUser, AttackAIExp, RemApplySkillVisionFix, SetReTarget and LuaCall are observed ports; x87 precision 53 bits (Windows default).')
print('# case mutation now max_hp found life success input[11] trace final[11]')
for case in range(1536):
    now=[0,1,0xfffffff0,0xffffffff,100][case%5]
    sleep=[0,0,1,0xffffffff][case%4]
    stun=[0,0,0xffffffff,1][(case//4)%4]
    speed=[0,1,6,7,255,256,0xffffffff,0x80000000][(case//3)%8]
    only=[0,0,1,0xffffffff][(case//7)%4]
    lua=[0,1,0xffffffff][(case//11)%3]
    deadline=[0,now,(now-1)&0xffffffff,(now+1)&0xffffffff,0xffffffff][(case//13)%5]
    hp=[0,1,2,3,99,100,0x7fffffff,0x80000000,0xffffffff][(case//17)%9]
    maximum=[0,1,3,7,100,0x7fffffff,0xffffffff][(case//19)%7]
    found=(case//5)%2;life=[0,1,2,0xffffffff][(case//23)%4];success=(case//29)%2
    mutation=(case//31)%7
    values=[sleep,stun,speed,only,lua,deadline,hp,0xffffffff-case,0x80000000+case,777,888]
    for offset,value in zip(offsets,values):put(mob+offset,value)
    put(definition+0x30,maximum);put(mob+0xa8,definition);put(mob+0x27c,base+0x6000);put(mob+0x60,base+0x8000);put(user+0x142c,life)
    context.update(trace=[],now=now,mutation=mutation,found=found,success=success)
    cpu.reg_write(UC_X86_REG_FPCW,0x27f);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ECX,mob);put(stack,stop)
    cpu.emu_start(0x4b55c0,stop,count=2000)
    assert cpu.reg_read(UC_X86_REG_EIP)==stop and cpu.reg_read(UC_X86_REG_ESP)==stack+4
    print(case,mutation,now,maximum,found,life,success,*values,';'.join(context['trace']),*[get(mob+x) for x in offsets])
