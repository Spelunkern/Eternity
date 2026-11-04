#!/usr/bin/env python3
"""Original Death solo region with complete GetExp/AddExpFromUser/LevelUpStatus.
Stop before Death common log/tail; attribution/drop, level-up notice and transports
are ports. No instruction patches. Synthetic status/experience tables are explicit.
"""
import struct
from original_oracle import load_game
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW

def main():
    c,digest=load_game();base=0x30000000;victim=base;user=base+0x8000;stack=base+0x20000;c.mem_map(base,0x30000);ctx={}
    def put(at,*v):c.mem_write(at,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
    def get(at):return struct.unpack('<I',c.mem_read(at,4))[0]
    def ret(n):
        sp=c.reg_read(UC_X86_REG_ESP);c.reg_write(UC_X86_REG_EIP,get(sp));c.reg_write(UC_X86_REG_ESP,sp+4+n)
    def hook(cpu,at,size,_):
        sp=cpu.reg_read(UC_X86_REG_ESP)
        if at in (0x4d4f10,0x4d4d20):
            p,n=struct.unpack('<II',cpu.mem_read(sp+4,8));ctx['events']+=bytes([ord('B') if at==0x4d4f10 else ord('C')])+struct.pack('<I',n)+bytes(cpu.mem_read(p,n));ret(8)
        elif at==0x48ad70:
            ctx['events']+=b'L'+struct.pack('<4I',cpu.reg_read(UC_X86_REG_ECX),cpu.reg_read(UC_X86_REG_EDX),get(user+0x13c),int.from_bytes(cpu.mem_read(user+0x136,2),'little'));ret(4)
        elif at==0x45b050:
            if cpu.reg_read(UC_X86_REG_ECX)!=user or cpu.reg_read(UC_X86_REG_ESI)!=victim:raise RuntimeError('Wrong KillPCFrom identities')
            ctx['events']+=b'F'+struct.pack('<II',get(user+0x13c),int.from_bytes(cpu.mem_read(user+0x136,2),'little'));ret(0)
        elif at==0x45a180:
            if (get(sp+4),get(sp+8))!=(victim,user):raise RuntimeError('Wrong ItemDropByUser identities')
            ctx['events']+=b'I'+struct.pack('<II',get(user+0x13c),int.from_bytes(cpu.mem_read(user+0x136,2),'little'));ret(8)
    c.hook_add(UC_HOOK_CODE,hook);c.mem_write(0x54c648,bytes(366*6))
    for grow in range(4):
        for level in range(61):put(0x54cedc+(grow*61+level)*4,level*1000)
    print('# ps_game.exe SHA256 '+digest+'; Death solo reward region 004588c3..00458934, complete CUser GetExp 004581b0/AddExpFromUser 00458240/LevelUpStatus 0048a5e0/SendDBExp 0046c6c0; no patches. Attribution/drop/level notice/transports are ports. x87 027f; zero status table, cumulative curve level*1000.')
    print('# case | expected ordered events | expected recipient core')
    for case in range(1024):
        c.mem_write(base,bytes(0x10000));c.mem_write(stack-0x2000,bytes(0x3000));grow=case%5
        vlevel=(0,1,10,20,29,60,65535)[(case//5)%7];level=(0,1,5,19,20,25,29,30,59,60,61,65535)[(case//7)%12]
        old=(0,999,59000,0xfffffff0)[(case//11)%4];double=(case//13)%2;half=(case//17)%2;bonus=(case//19)%6-1;source=(0,777,0xffffffff,123)[(case//23)%4]
        c.mem_write(victim+0x136,struct.pack('<H',vlevel));c.mem_write(user+0x136,struct.pack('<H',level));c.mem_write(user+0x12f,bytes([grow]))
        put(user+0x13c,old);put(user+0x578c,123);put(user+0x58d4,double);put(user+0x58d0,half);put(user+0x58b0,bonus);put(stack+0x168,source)
        c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_EDI,victim);c.reg_write(UC_X86_REG_EBP,user);c.reg_write(UC_X86_REG_FPCW,0x27f);ctx['events']=b''
        c.emu_start(0x4588c3,0x458cbe,count=1000000)
        if c.reg_read(UC_X86_REG_EIP)!=0x458cbe or c.reg_read(UC_X86_REG_FPCW)!=0x27f:raise RuntimeError('Solo reward region did not finish/control not restored')
        core=bytes(c.mem_read(user+0x136,10))+bytes(c.mem_read(user+0x170,24))+bytes(c.mem_read(user+0x1220,24))
        print(case,'|',ctx['events'].hex(),'|',core.hex())
if __name__=='__main__':main()
