#!/usr/bin/env python3
"""Complete original UpdateAttack skill/cast paths, with actual continuation."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();root=0x30000000;cpu.mem_map(root,0x40000)
    user,mob,peer=root,root+0x8000,root+0xa000
    skills=[root+0x10000,root+0x10100];definitions=[root+0x11000,root+0x11100];stack=root+0x30000;stop=root+0x3f000
    events=[]
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def snapshot():
        offsets=(0x142c,0x1274,0x1528,0x152c,0x143c,0x1440,0x1450)
        values=[get(user+x) for x in offsets]+[cpu.mem_read(user+0x1448,1)[0],get(user+0x12ec),get(user+0x1444),get(mob+0x124),get(peer+0x142c)]
        return ','.join(map(str,values))+','+bytes(cpu.mem_read(mob+0xcf4,32)).hex()+','+bytes(cpu.mem_read(mob+0xd14,32)).hex()
    def event(tag,arg=''):events.append(tag+':'+snapshot()+(','+arg if arg else ''))
    def hook(c,at,size,data):
        pop,result=0,None
        if at==0x48a460:event('Q');result=4
        elif at==0x489560:event('T');pop=4
        elif at==0x44cd80:
            event('E',str(c.reg_read(UC_X86_REG_ECX)));result=mob if found else 0
            if mutation&1:put(user+0x1444,1)
        elif at==0x456780:
            sp=c.reg_read(UC_X86_REG_ESP);event('e',str(c.reg_read(UC_X86_REG_EBX))+','+str(get(sp+4)));pop=4;result=peer if found else 0
            if mutation&1:put(user+0x1444,1)
        elif at==0x44c610:
            event('B');result=both
            if mutation&2:put(user+0x1444,1)
        elif at in (0x44f2d0,0x450cf0,0x44f110,0x451c80):
            casting=at in (0x450cf0,0x451c80);to_user=at in (0x44f110,0x451c80)
            slot=c.reg_read(UC_X86_REG_ECX if casting else UC_X86_REG_EAX)
            # Casting user overload also receives CSkill in ECX; its actor
            # argument is EAX. Use overloads receive CSkill in EAX.
            number=skills.index(slot)
            target=c.reg_read(UC_X86_REG_EAX if casting else UC_X86_REG_ECX)
            now=c.reg_read(UC_X86_REG_EDX) if casting else get(c.reg_read(UC_X86_REG_ESP)+4)
            event(('a' if casting else 'u') if to_user else ('A' if casting else 'U'),f'{number},{now},{int(target!=0)}')
            pop=0 if at==0x451c80 else 4
            if mutation&4:put(user+0x142c,3);put(user+0x12ec,9)
            if mutation&8:put(user+0x142c,0);put(user+0x1528,1,42,2,77)
            put(mob+0x124,post_mob);put(peer+0x142c,post_user)
        elif at==0x48a010:
            event('C')
            if mutation&16:put(user+0x1450,1234);cpu.mem_write(user+0x1448,b'\x07')
            return
        elif at==0x4d4d20:
            sp=c.reg_read(UC_X86_REG_ESP);event('P',bytes(cpu.mem_read(get(sp+4),get(sp+8))).hex());pop=8
            if mutation&32:put(user+0x1274,0);put(user+0x1444,1)
        else:return
        sp=c.reg_read(UC_X86_REG_ESP);ret=get(sp);c.reg_write(UC_X86_REG_ESP,sp+4+pop)
        if result is not None:c.reg_write(UC_X86_REG_EAX,result)
        c.reg_write(UC_X86_REG_EIP,ret)
    cpu.hook_add(UC_HOOK_CODE,hook)
    for i,name in enumerate((b'Fire\0',b'Ice\0')):
        put(skills[i]+0x30,definitions[i]);cpu.mem_write(definitions[i]+3,name);cpu.mem_write(definitions[i]+0x32,struct.pack('<H',5+i))
    rng=random.Random(0x48cab9)
    print(f'# ps_game.exe SHA256 {digest}; complete UpdateAttack 0048ca50 skill/cast routes with actual SetPrevAttack/StatusNextAttack/SendAttackStart/Stop/GetAttackSpeedNow/CancelMove/cookie; effect/lookup/skill/transport ports observed')
    print('# mode kind present found both post_mob post_user mutation now resume previous recovery speed; bool final events')
    for case in range(2048):
        mode=2+case%2;kind=(0,1,2,4)[(case//2)%4];present=0 if case%17==0 else 1;found=rng.randrange(2);both=rng.randrange(2)
        post_mob=rng.choice((0,2));post_user=rng.randrange(2);mutation=rng.randrange(64);now=rng.choice((100,0xfffffff0,0xffffffff));resume=0 if case%11 else ((now+1)&0xffffffff)
        previous=rng.choice((0,55));recovery=rng.choice((0,1,3,0xffffffff));speed=rng.choice((0,4,9,0xffffffff))
        cpu.mem_write(user,bytes(0x6000));cpu.mem_write(mob,bytes(0x1000));cpu.mem_write(peer,bytes(0x2000))
        put(user+0xdc,42);put(peer+0xdc,77);put(user+0x142c,mode);put(user+0x1274,recovery);put(user+0x1528,kind,77);put(user+0x1530,2,77)
        put(user+0x143c,previous,2);put(user+0x1450,resume);put(user+0x12ec,speed);put(user+0xac4,*[s if present else 0 for s in skills]);put(user+0x1444,0)
        cpu.mem_write(user+0x188,b'Hero\0');cpu.mem_write(user+0x1448,b'\xa5');cpu.mem_write(mob+0xcf4,b'\xa5'*32);cpu.mem_write(mob+0xd14,b'\x5a'*32)
        events=[];put(stack,stop,now);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ECX,user)
        cpu.emu_start(0x48ca50,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop or cpu.reg_read(UC_X86_REG_ESP)!=stack+8:raise RuntimeError('UpdateAttack skill route did not return balanced')
        print(mode,kind,present,found,both,post_mob,post_user,mutation,now,resume,previous,recovery,speed,cpu.reg_read(UC_X86_REG_EAX)&255,snapshot(),'|'.join(events) or '-',sep='\t')


if __name__=='__main__':main()
