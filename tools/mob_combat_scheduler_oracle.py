#!/usr/bin/env python3
"""Complete UpdateAttack normal-mob paths with actual Stop/speed/timing bodies."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu, digest = load_game(); root=0x30000000;cpu.mem_map(root,0x30000)
    user,mob,stack,stop=root,root+0x8000,root+0x20000,root+0x2f000
    events=[]
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def snapshot():
        offsets=(0x142c,0x1274,0x1528,0x152c,0x143c,0x1440,0x1450)
        fields=[get(user+x) for x in offsets]+[cpu.mem_read(user+0x1448,1)[0],get(user+0x12ec),int(get(user+0x1d8)!=0),get(mob+0x124)]
        return ','.join(map(str,fields))+','+bytes(cpu.mem_read(mob+0xcf4,32)).hex()+','+bytes(cpu.mem_read(mob+0xd14,32)).hex()
    def event(tag,arg=None):events.append(tag+':'+snapshot()+((','+str(arg)) if arg is not None else ''))
    def hook(c,at,size,data):
        pop,result=0,None
        if at==0x48a460:event('Q');result=255 if blocked else 4
        elif at==0x489560:
            event('T');pop=4
            if mutation&1:put(user+0x1d8,0)
            if mutation&2:put(user+0x1440,3)
        elif at==0x44cd80:event('E',c.reg_read(UC_X86_REG_ECX));result=mob if found else 0
        elif at==0x44c030:event('R');pop=8;result=in_range
        elif at==0x44c610:event('B');result=both
        elif at==0x44ce10:
            event('A');pop=4;put(mob+0x124,post_status)
            if mutation&4:put(user+0x12ec,9);put(user+0x142c,1)
        elif at==0x48a010:
            event('C')
            if mutation&8:put(user+0x1450,1234);cpu.mem_write(user+0x1448,b'\x07')
            return  # actual CancleMove executes; its pending-move field is zero
        elif at==0x4d4d20:
            esp=c.reg_read(UC_X86_REG_ESP);packet=bytes(cpu.mem_read(get(esp+4),get(esp+8))).hex();event('P',packet);pop=8
            if mutation&16:put(user+0x1450,9);cpu.mem_write(user+0x1448,b'\x08')
        else:return
        esp=c.reg_read(UC_X86_REG_ESP);ret=get(esp);c.reg_write(UC_X86_REG_ESP,esp+4+pop)
        if result is not None:c.reg_write(UC_X86_REG_EAX,result)
        c.reg_write(UC_X86_REG_EIP,ret)
    cpu.hook_add(UC_HOOK_CODE,hook)
    rng=random.Random(0x48ca50)
    print(f'# ps_game.exe SHA256 {digest}; UpdateAttack 0048ca50 normal mob/skip/failure paths; actual StatusAttackStop 00489ed0, GetAttackSpeedNow 0048a4b0, CancleMove 0048a010 and security-cookie return; required attack/effect/lookup/range/send ports observed')
    print('# now resume mode next recovery kind speed weapon blocked found range both post_status mutation -> bool final_state events')
    for case in range(1536):
        now=rng.choice((0,100,0xfffffff0,0xffffffff));resume=rng.choice((0,now,(now+1)&0xffffffff))
        mode=rng.choice((0,1,2,2,2,4,0xffffffff));next_tick=rng.choice((0,1,1,1,3))
        recovery=rng.choice((0,1,3,0xffffffff));kind=rng.choice((0,1,2,2,2,4));speed=rng.choice((0,4,9,10,0xffffffff,0x80000000))
        weapon,blocked,found,in_range,both=[rng.randrange(2) for _ in range(5)]
        post_status=rng.choice((0,1,2));mutation=rng.randrange(32)
        if case%3==0:
            now=(100,0xfffffff0,0xffffffff)[(case//3)%3];resume=now;mode=2;next_tick=1;weapon=1;blocked=0;found=1;in_range=1;kind=2
            mutation=rng.choice((0,4,8,12,16,20,24,28))
        elif case%3==1:
            now=100;resume=100;mode=2;next_tick=1;kind=2
        cpu.mem_write(user,bytes(0x6000));cpu.mem_write(mob,bytes(0x1000))
        put(user+0xdc,42);put(user+0x142c,mode);put(user+0x1274,recovery);put(user+0x1528,kind,77 if kind==2 else 42)
        put(user+0x143c,55,next_tick);put(user+0x1450,resume);put(user+0x12ec,speed);put(user+0x1d8,weapon)
        cpu.mem_write(user+0x1448,b'\xa5');cpu.mem_write(user+0x188,b'Hero\0')
        cpu.mem_write(mob+0xcf4,b'\xa5'*32);cpu.mem_write(mob+0xd14,b'\x5a'*32)
        events=[];put(stack,stop,now);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ECX,user)
        cpu.emu_start(0x48ca50,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop or cpu.reg_read(UC_X86_REG_ESP)!=stack+8:raise RuntimeError('UpdateAttack did not return with balanced stack')
        print(now,resume,mode,next_tick,recovery,kind,speed,weapon,blocked,found,in_range,both,post_status,mutation,cpu.reg_read(UC_X86_REG_EAX)&255,snapshot(),'|'.join(events) or '-',sep='\t')

if __name__=='__main__':main()
