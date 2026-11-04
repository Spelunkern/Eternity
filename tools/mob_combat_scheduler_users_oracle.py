#!/usr/bin/env python3
"""Complete UpdateAttack normal-user paths with real IsTeam and stop/timing."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_ECX,UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();root=0x30000000;cpu.mem_map(root,0x40000)
    user,peer,zone,definition,stack,stop=[root+x for x in (0,0x8000,0x18000,0x19000,0x30000,0x3f000)]
    events=[]
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def snapshot():
        offsets=(0x142c,0x1274,0x1528,0x152c,0x143c,0x1440,0x1450)
        values=[get(user+x) for x in offsets]+[cpu.mem_read(user+0x1448,1)[0],get(user+0x12ec),get(peer+0xdc),get(peer+0x142c),*cpu.mem_read(peer+0x5774,2),cpu.mem_read(peer+0x1354,1)[0]]
        return ','.join(map(str,values))
    def event(tag,arg=''):events.append(tag+':'+snapshot()+(','+arg if arg else ''))
    def hook(c,at,size,data):
        pop,result=0,None
        if at==0x48a460:event('Q');result=4
        elif at==0x489560:event('T');pop=4
        elif at==0x415f30:
            event('F',str(get(c.reg_read(UC_X86_REG_ESP)+4)));pop=4;result=peer if found else 0
            if mutation&1:put(user+0x152c,42);put(user+0x12ec,9)
            if mutation&32:put(peer+0x142c,0);cpu.mem_write(peer+0x5774,b'\x01\x01');cpu.mem_write(peer+0x1354,b'\0')
        elif at==0x44bdf0:event('D');return  # actual IsTeam executes
        elif at==0x44bf90:
            event('R');pop=4;result=in_range
            if mutation&2:put(peer+0x142c,1);put(user+0x12ec,0)
        elif at==0x456870:
            event('A');pop=4
            if mutation&4:put(peer+0x142c,1);put(user+0x142c,0);put(user+0x12ec,9)
        elif at==0x48a010:
            event('C')
            if mutation&8:put(user+0x1450,1234);cpu.mem_write(user+0x1448,b'\x07')
            return
        elif at==0x4d4d20:
            sp=c.reg_read(UC_X86_REG_ESP);event('P',bytes(cpu.mem_read(get(sp+4),get(sp+8))).hex());pop=8
            if mutation&16:put(user+0x1450,9);cpu.mem_write(user+0x1448,b'\x08')
        else:return
        sp=c.reg_read(UC_X86_REG_ESP);ret=get(sp);c.reg_write(UC_X86_REG_ESP,sp+4+pop)
        if result is not None:c.reg_write(UC_X86_REG_EAX,result)
        c.reg_write(UC_X86_REG_EIP,ret)
    cpu.hook_add(UC_HOOK_CODE,hook);rng=random.Random(0x48cce0)
    print(f'# ps_game.exe SHA256 {digest}; complete UpdateAttack 0048ca50 normal-user paths with actual IsTeam 0044bdf0/StatusAttackStop/GetAttackSpeedNow/CancelMove/cookie; effect/lookup/range/attack/transport ports observed')
    print('# requested target_id found target_mode visible attackable shape country peer_country map_mode relation range mutation speed now resume recovery; bool final events')
    for case in range(2048):
        requested=rng.choice((0,42,77,0xffffffff));identity=42 if case%13==0 else requested
        found=rng.randrange(2);peer_mode=rng.choice((0,1,2));visible=rng.randrange(2);attackable=rng.randrange(2);shape=rng.choice((0,13));country=rng.randrange(2);peer_country=rng.randrange(2);map_mode=rng.randrange(2);relation=rng.randrange(5)
        in_range=rng.randrange(2);mutation=rng.randrange(64);speed=rng.choice((0,4,9,0xffffffff));now=rng.choice((100,0xfffffff0,0xffffffff));resume=0 if case%11 else ((now+1)&0xffffffff);recovery=rng.choice((0,1,0xffffffff))
        if case%3==0:
            requested=77;identity=77;found=1;peer_mode=0;visible=1;attackable=1;shape=0;country=0;peer_country=1;map_mode=0;relation=0;resume=0
        cpu.mem_write(user,bytes(0x6000));cpu.mem_write(peer,bytes(0x6000));cpu.mem_write(zone,bytes(0x2000))
        put(user+0xdc,42);put(peer+0xdc,identity);put(user+0xe0,zone);put(zone+0x24,definition);put(definition+0x7c,map_mode)
        put(user+0x142c,2);put(user+0x1274,recovery);put(user+0x1528,1,requested);put(user+0x143c,55,1);put(user+0x1450,resume);put(user+0x12ec,speed);put(user+0x1d8,1);cpu.mem_write(user+0x1448,b'\xa5')
        put(peer+0x142c,peer_mode);cpu.mem_write(peer+0x5774,bytes((visible,attackable)));cpu.mem_write(peer+0x1354,bytes((shape,)));cpu.mem_write(user+0x12d,bytes((country,)));cpu.mem_write(peer+0x12d,bytes((peer_country,)))
        if relation==1:put(user+0x54cc,4);put(user+0x54d4,identity)
        elif relation in (2,4):
            put(user+0x1748,1);put(peer+0x1748,2);put(user+0x54e4,2);put(peer+0x54e4,1 if relation==2 else 9);put(user+0x54fc,1);put(peer+0x54fc,1)
        elif relation==3:put(peer+0x54cc,4);put(peer+0x54d4,42)
        events=[];put(stack,stop,now);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ECX,user)
        cpu.emu_start(0x48ca50,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop or cpu.reg_read(UC_X86_REG_ESP)!=stack+8:raise RuntimeError('normal-user polling did not return balanced')
        print(requested,identity,found,peer_mode,visible,attackable,shape,country,peer_country,map_mode,relation,in_range,mutation,speed,now,resume,recovery,cpu.reg_read(UC_X86_REG_EAX)&255,snapshot(),'|'.join(events) or '-',sep='\t')


if __name__=='__main__':main()
