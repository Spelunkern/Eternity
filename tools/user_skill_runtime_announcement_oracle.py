#!/usr/bin/env python3
"""Complete original SendUseSkill, real packet/cookie/zone guard, explicit view port."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game

def main():
    c,digest=load_game();base=0x30000000;c.mem_map(base,0x80000)
    user,info,zone,stack,stop=base,base+0x10000,base+0x20000,base+0x60000,base+0x7f000;state={}
    def put(p,*v):c.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(p):return struct.unpack('<I',c.mem_read(p,4))[0]
    def hook(cpu,at,size,data):
        if at==0x44f4c0:
            sp=cpu.reg_read(UC_X86_REG_ESP)
            if get(sp+12)!=0 or get(sp+16)!=1:raise RuntimeError('chain hit flags')
            state['hits']+=1;cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+20)
        elif at==0x421220:
            sp=cpu.reg_read(UC_X86_REG_ESP);args=[get(sp+4*i) for i in range(1,9)]
            z,p,length,pos,radius,ex1,ex2,channel=args
            if (z,length,pos,radius,channel)!=(zone,20,user+0xd0,0x42700000,5):raise RuntimeError('view ABI')
            state['packet']=bytes(cpu.mem_read(p,length)).hex();state['exclusions']=(ex1,ex2);state['sends']+=1
            # Reentry changes all source inputs. Emitted frame must stay captured.
            put(user+0xe0,0);put(user+0xdc,0x76543210);c.mem_write(info,struct.pack('<HB',0xabcd,0xfe));c.mem_write(info+0x58,b'\x03')
            cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+36)
    c.hook_add(UC_HOOK_CODE,hook)
    print(f'# ps_game.exe SHA256 {digest}; WHOLE CUser::SendUseSkill0044e900, only PSendView00421220 ported; all 8 view arguments checked; callback mutates source ID/zone and definition IDs.')
    print('# case in_zone source_id target_id skill_id skill_level reason mob_argument packet exclude_source exclude_target sends')
    flags=(0,1,2,255,256,257,0xffffffff,0x80000000);ids=(0,42,0xffffffff,0x80000001);reasons=(0,4,6,255)
    for n in range(2048):
        in_zone=n%2;source_id=ids[(n//2)%4];target_id=ids[(n//8)%4];skill_id=(0,11,65535,32768)[(n//32)%4];level=(0,2,255,128)[(n//128)%4];reason=reasons[(n//16)%4];flag=flags[(n//256)%8]
        c.mem_write(user,bytes(0x6000));put(user+0xdc,source_id,zone if in_zone else 0);c.mem_write(info,struct.pack('<HB',skill_id,level));state.update(packet='-',exclusions=(0,0),sends=0)
        put(stack,stop,user,target_id,flag);c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_ECX,info);c.reg_write(UC_X86_REG_EDX,0xaabbcc00|reason)
        c.emu_start(0x44e900,stop,count=100000)
        if c.reg_read(UC_X86_REG_EIP)!=stop or c.reg_read(UC_X86_REG_ESP)!=stack+16:raise RuntimeError('unbalanced complete original')
        print(n,in_zone,source_id,target_id,skill_id,level,reason,flag,state['packet'],*state['exclusions'],state['sends'],sep='\t')
    # Complete existing resolved attack -> strike -> actual announcement chain.
    target,skill=base+0x30000,base+0x40000
    for in_zone in (0,1):
        c.mem_write(user,bytes(0x6000));c.mem_write(target,bytes(0x6000));c.mem_write(info,bytes(160));c.mem_write(skill,bytes(64))
        put(user+0xdc,42,zone if in_zone else 0);put(target+0xdc,77);put(skill+12,60000,99);put(skill+48,info)
        c.mem_write(info,struct.pack('<HB',11,2));c.mem_write(info+0x31,b'\x01');c.mem_write(info+0x32,struct.pack('<H',22));c.mem_write(info+0x4a,struct.pack('<H',7));c.mem_write(info+0x56,b'\x03');c.mem_write(info+0x58,b'\x01')
        state.update(packet='-',exclusions=(0,0),sends=0,hits=0)
        put(stack,stop);c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_EAX,user);c.reg_write(UC_X86_REG_ECX,skill);c.reg_write(UC_X86_REG_EDI,target);c.reg_write(UC_X86_REG_EDX,100)
        c.emu_start(0x451c80,stop,count=100000)
        if c.reg_read(UC_X86_REG_EIP)!=stop or c.reg_read(UC_X86_REG_ESP)!=stack+4:raise RuntimeError('unbalanced complete attack chain')
        print('# chain',in_zone,get(skill+0x1c),state['hits'],state['packet'],*state['exclusions'],state['sends'],sep='\t')
if __name__=='__main__':main()
