#!/usr/bin/env python3
"""Complete polling with actual shape-effect unlink/release and move cancellation."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();root=0x30000000;cpu.mem_map(root,0x40000)
    user,head,mob,node,skill,pool,zone,stack,stop=[root+x for x in (0,0x9000,0xa000,0xc000,0xd000,0xe000,0x10000,0x30000,0x3f000)]
    events=[];backend=[];released=0
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def count():return 0 if get(head+4)==head else 1
    def snapshot():
        values=[get(user+x) for x in (0x142c,0x1274,0x1528,0x152c,0x143c,0x1440,0x1450)]+[cpu.mem_read(user+0x1448,1)[0],cpu.mem_read(user+0x1354,1)[0],get(user+0x1358),get(user+0x5818),get(user+0x581c),get(user+0x586c),*[get(user+0x1488+i*4) for i in range(11)],count(),released,int(get(user+0x1d8)!=0)]
        return ','.join(map(str,values))
    def event(tag,arg=''):events.append(tag+':'+snapshot()+(','+arg if arg else ''))
    def hook(c,at,size,data):
        nonlocal released
        pop,result=0,None
        if at in (0x4d4f10,0x4d4d20):
            sp=c.reg_read(UC_X86_REG_ESP);packet=bytes(cpu.mem_read(get(sp+4),get(sp+8)))
            pop=8
            if at==0x4d4f10:
                if count()!=0:raise RuntimeError('backend removal preceded unlink')
                backend.append(packet.hex())
            else:
                event('S',packet.hex())
                if packet[:2]==b'\x0e\x05':
                    if not backend:raise RuntimeError('client removal preceded backend removal')
                    if mutation&1:put(user+0x5818,2,1);put(user+0x586c,10)
                    if mutation&2:put(user+0x1d8,0)
        elif at==0x4208c0:
            sp=c.reg_read(UC_X86_REG_ESP);event('V',bytes(cpu.mem_read(get(sp+8),get(sp+12))).hex());pop=12
        elif at==0x4209a0:
            sp=c.reg_read(UC_X86_REG_ESP)
            if get(sp+4)!=zone or get(sp+16)!=user+0xd0 or get(sp+20)!=0x42700000:raise RuntimeError('wrong move-cancel route')
            event('M',bytes(cpu.mem_read(get(sp+8),get(sp+12))).hex());pop=20
            if mutation&4:put(user+0x1450,1234);cpu.mem_write(user+0x1448,b'\x07')
        elif at==0x40a7c0:
            released+=1;event('F',str(get(c.reg_read(UC_X86_REG_EDI)+8)));return # real shared pool release executes
        elif at==0x44cd80:event('E');result=mob
        elif at==0x44c030:event('R');pop=8;result=1
        elif at==0x44c610:event('B');result=0
        elif at==0x44ce10:event('A');pop=4;put(mob+0x124,2 if mutation&8 else 0)
        else:return
        sp=c.reg_read(UC_X86_REG_ESP);ret=get(sp);c.reg_write(UC_X86_REG_ESP,sp+4+pop)
        if result is not None:c.reg_write(UC_X86_REG_EAX,result)
        c.reg_write(UC_X86_REG_EIP,ret)
    cpu.hook_add(UC_HOOK_CODE,hook);cpu.mem_write(stop+16,b'\xc2\x04\x00');put(0x5210cc,stop+16);put(0x5210d0,stop+16);put(0x546f3c,pool)
    rng=random.Random(0x489560)
    print(f'# ps_game.exe SHA256 {digest}; complete UpdateAttack with actual RemApplySkillToMOB 00489560/unlink/SendSkillRemove/shape/run-mode/FreeSkill 0040a7c0 and CancelMove 0048a010; attack/transport/locks are ports. Original backend 0902 observed separately from native replica boundary.')
    print('# variant shape in_zone move_type pending slot mutation now -> bool final events backend09_or_dash')
    for case in range(768):
        variant=case%4;shape=rng.choice((0,13,100));in_zone=rng.randrange(2);move_type=rng.choice((0,1,2,3,0xffffffff));pending=rng.choice((0,1,0xffffffff));slot=rng.choice((0,10,11,0xffffffff));mutation=rng.randrange(16);now=rng.choice((100,0xfffffff0))
        cpu.mem_write(user,bytes(0x8000));cpu.mem_write(head,bytes(0x7000));cpu.mem_write(zone,bytes(0x1000))
        put(user+0xdc,42);put(user+0x578c,42);put(user+0xe0,zone if in_zone else 0,17,23);cpu.mem_write(user+0x5774,b'\x01')
        put(user+0x142c,2);put(user+0x1274,0);put(user+0x1528,2,77);put(user+0x143c,55,1);put(user+0x12ec,4);put(user+0x1d8,1);cpu.mem_write(user+0x1448,b'\xa5');cpu.mem_write(user+0x1354,bytes((shape,)));put(user+0x1358,321)
        put(user+0x5818,move_type,pending);put(user+0x586c,slot)
        for i in range(11):put(user+0x1488+i*4,0x11110000+i)
        put(head,head,head);put(user+0xa94,head,head);put(user+0xab4,int(variant!=0))
        if variant:
            identity=127 if variant==2 else 200;detail=90 if variant==2 else (88 if variant==1 else 82)
            data=bytearray(160);struct.pack_into('<HB',data,0,identity,1);struct.pack_into('<H',data,0x32,detail);struct.pack_into('<H',data,0x98,100 if variant==2 else 0);cpu.mem_write(skill,bytes(data))
            put(node,head,head,1000,identity,1);put(node+0x30,skill);put(head,node,node)
        events=[];backend=[];released=0;put(stack,stop,now);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ECX,user)
        cpu.emu_start(0x48ca50,stop,count=200000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop or cpu.reg_read(UC_X86_REG_ESP)!=stack+8:raise RuntimeError('actor polling did not return balanced')
        if get(pool+0x11c)!=released:raise RuntimeError('actual effect release count differs')
        print(variant,shape,in_zone,move_type,pending,slot,mutation,now,cpu.reg_read(UC_X86_REG_EAX)&255,snapshot(),'|'.join(events) or '-',','.join(backend) or '-',sep='\t')


if __name__=='__main__':main()
