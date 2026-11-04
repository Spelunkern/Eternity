#!/usr/bin/env python3
"""Complete UseSkill and instant resolved attack; real preparation state/packets."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game

def main():
    c,digest=load_game();base=0x30000000;c.mem_map(base,0x80000)
    user,target,skill,infos,zone,stack,stop=base,base+0x8000,base+0x10000,base+0x11000,base+0x20000,base+0x60000,base+0x7f000
    state={};events=[]
    def put(p,*v):c.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(p):return struct.unpack('<I',c.mem_read(p,4))[0]
    def snapshot():return ','.join(map(str,([get(user+x) for x in (0x142c,0x1274,0x143c,0x1440,0x1450)]+[get(skill+28),int(get(skill+48)==infos+0x100),c.mem_read(infos+0x48,1)[0],c.mem_read(infos+0x148,1)[0],get(skill+12),get(skill+16),get(user+0x12ec)])))
    def event(tag):events.append(tag+':'+snapshot())
    def hook(cpu,at,size,data):
        sp=cpu.reg_read(UC_X86_REG_ESP);pop=0;result=None
        if at in (0x44eb40,0x44ee50):
            if get(sp+4)!=state['now'] or cpu.reg_read(UC_X86_REG_EDI)!=skill:raise RuntimeError('wrong eligibility runtime/tick')
            event('E');pop=4;result=state['eligible']
            if state['mutation']&1:put(user+0x142c,0);put(user+0x1274,0xffffffff);put(user+0x143c,55);put(skill+48,infos+0x100)
        elif at in (0x451d90,0x450e20):
            event('A');pop=8 if at==0x450e20 else 0
            if state['mutation']&4:put(user+0x143c,55);put(user+0x142c,0);put(user+0x1274,0xffffffff);put(user+0x12ec,9);put(user+0x1450,777);put(skill+28,77);put(skill+48,infos+0x100)
        elif at==0x421220:
            z,ptr,length,pos,radius,id,excluded,channel=struct.unpack('<8I',cpu.mem_read(sp+4,32));wire=bytes(cpu.mem_read(ptr,length))
            if (z,length,pos,radius,id,excluded,channel)!=(zone,13,user+0xd0,0x42700000,42,0 if state['mob'] else 77,5):raise RuntimeError('wrong actual cast route')
            event('V'+wire.hex());pop=32
            if state['mutation']&2:
                captured=cpu.reg_read(UC_X86_REG_EBP);c.mem_write(captured+0x48,b'\x00');put(user+0x1450,1234);put(user+0x142c,4);put(skill+12,99,3);put(skill+48,infos+0x100)
        elif at==0x4d4d20:
            event('S'+bytes(cpu.mem_read(get(sp+4),get(sp+8))).hex());pop=8
            if state['mutation']&8:put(user+0x1274,0xffffffff);put(user+0x142c,1);put(user+0x143c,0);put(user+0x12ec,1);put(user+0x1450,444)
        else:return
        ret=get(sp);cpu.reg_write(UC_X86_REG_ESP,sp+4+pop)
        if result is not None:cpu.reg_write(UC_X86_REG_EAX,result)
        cpu.reg_write(UC_X86_REG_EIP,ret)
    c.hook_add(UC_HOOK_CODE,hook)
    print(f'# ps_game.exe SHA256 {digest}; WHOLE UseSkillUser0044f110/Mob0044f2d0, actual instant AttackUser00451c80/AttackMob00450cf0 and SendAttackStart004826f0. Eligibility/effects/view/self delivery are explicit ports. No eligibility/spending/damage body equivalence claim.')
    print('# case mob eligible ready alt_ready mode_bits previous recovery_bits speed_bits now in_zone mutation final events')
    for case in range(1024):
        c.mem_write(base,bytes(0x50000));mob=case%2;eligible=(case//2)%2;ready=(0,1,255)[(case//4)%3];alt_ready=0 if case%5 else 2;mode=(-1,0,1,2,3,4)[(case//12)%6];previous=(0,55)[(case//72)%2];recovery=(-1,0,1,3)[(case//144)%4];speed=(-1,0,4,9,10)[(case//7)%5];now=(100,0xfffffff0)[(case//360)%2];in_zone=int(case%7!=0);mutation=0 if case<576 else (case//3)%16;state=dict(now=now,eligible=eligible,mob=mob,mutation=mutation);events=[]
        put(user+0xdc,42);put(target+0xdc,77);put(target+0x5c,77);put(user+0xe0,zone if in_zone else 0);put(user+0x142c,mode);put(user+0x1274,recovery);put(user+0x143c,previous,2);put(user+0x1450,555);put(user+0x12ec,speed);put(skill+12,11,2);put(skill+28,999);put(skill+48,infos)
        for i,duration in enumerate((ready,alt_ready)):
            p=infos+i*0x100;c.mem_write(p+0x48,bytes((duration,)));c.mem_write(p+0x4a,struct.pack('<H',7+2*i));c.mem_write(p+0x31,b'\x01');c.mem_write(p+0x56,b'\x03')
        c.mem_write(stack-0x2000,bytes(0x2000));put(stack,stop,now);c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_ESI,user);c.reg_write(UC_X86_REG_EAX,skill);c.reg_write(UC_X86_REG_ECX,target)
        c.emu_start(0x44f2d0 if mob else 0x44f110,stop,count=200000)
        if c.reg_read(UC_X86_REG_EIP)!=stop or c.reg_read(UC_X86_REG_ESP)!=stack+8:raise RuntimeError('unbalanced full use function')
        print(case,mob,eligible,ready,alt_ready,mode&0xffffffff,previous,recovery&0xffffffff,speed&0xffffffff,now,in_zone,mutation,snapshot(),'|'.join(events) or '-',sep='\t')

if __name__=='__main__':main()
