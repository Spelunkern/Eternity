#!/usr/bin/env python3
"""Complete original StatusNextAttack/SendAttackStart/Stop, including reentry."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();root=0x30000000;cpu.mem_map(root,0x20000)
    user,stack,stop=root,root+0x10000,root+0x1f000
    offsets=(0x142c,0x1274,0x143c,0x1440,0x1528,0x152c,0x1530,0x1534,0x1450)
    events=[];nested=False
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def snapshot():return ','.join(str(get(user+x)) for x in offsets)
    def event(tag,extra=''):events.append(tag+':'+snapshot()+(','+extra if extra else ''))
    def hook(c,at,size,data):
        nonlocal nested
        if at==0x482717 and nested:
            event('n');nested=False
        if at!=0x4d4d20:return
        esp=c.reg_read(UC_X86_REG_ESP);ret=get(esp);packet=bytes(cpu.mem_read(get(esp+4),get(esp+8))).hex();event('P',packet)
        if mutation&1:put(user+0x1274,0);put(user+0x142c,1)
        if mutation&2:put(user+0x1528,1,42,2,99)
        if mutation&4:put(user+0x143c,0,99)
        if mutation&8:put(user+0x1530,0)
        if mutation&16:
            put(user+0x142c,3);put(user+0x1528,7);event('N');nested=True
            # Actual nested StatusNextAttack(force=true); its RET 4 also
            # consumes this send port's stack arguments before SendAttackStart
            # resumes at its original return address. No nested emu_start.
            put(esp+4,ret,1);c.reg_write(UC_X86_REG_ESP,esp+4);c.reg_write(UC_X86_REG_ESI,user);c.reg_write(UC_X86_REG_EIP,0x48c970)
        else:
            c.reg_write(UC_X86_REG_ESP,esp+12);c.reg_write(UC_X86_REG_EIP,ret)
    cpu.hook_add(UC_HOOK_CODE,hook)
    rng=random.Random(0x48c970)
    print(f'# ps_game.exe SHA256 {digest}; complete StatusNextAttack 0048c970, SendAttackStart 004826f0, StatusAttackStop 00489ed0 and security cookie; only self transport is a port. Nested forced stops execute original instructions.')
    print('# force mutation mode recovery previous phase target_type target_id backup_type backup_id resume; final events')
    modes=(0,1,2,3,4,0xffffffff,0x80000000,0x7fffffff)
    for case in range(2048):
        force=case%2;mutation=(case//2)%32;mode=modes[(case//64)%8]
        recovery=rng.choice((0,1,2,3,0xffffffff,0x80000000,0x7fffffff));previous=rng.choice((0,1,0xffffffff));phase=rng.choice((0,1,2,99))
        kind=rng.choice((0,1,1,2,4));identity=rng.choice((0,42,42,77,0xffffffff));backup_kind=rng.choice((0,1,2,0xffffffff));backup_id=rng.choice((0,42,99,0xffffffff));resume=rng.getrandbits(32)
        cpu.mem_write(user,b'\xa5'*0x2000);put(user+0xdc,42)
        values=(mode,recovery,previous,phase,kind,identity,backup_kind,backup_id,resume)
        for at,value in zip(offsets,values):put(user+at,value)
        events=[];nested=False;put(stack,stop,force);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ESI,user)
        cpu.emu_start(0x48c970,stop,count=10000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop or cpu.reg_read(UC_X86_REG_ESP)!=stack+8 or nested:raise RuntimeError('StatusNextAttack did not return balanced')
        print(force,mutation,*values,snapshot(),'|'.join(events) or '-',sep='\t')


if __name__=='__main__':main()
