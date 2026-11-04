#!/usr/bin/env python3
"""Complete unpatched PvPChkLoseDeath; primitive owners/transports are ports."""
import struct
from original_oracle import load_game
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP

def main():
    c,digest=load_game();base=0x30000000;user=base;peer=base+0x8000;guild=base+0x10000;other=base+0x11000;stack=base+0x20000;stop=base+0x30000
    c.mem_map(0,0x1000);c.mem_map(base,0x40000)
    def put(at,n):c.mem_write(at,struct.pack('<I',n&0xffffffff))
    def get(at):return struct.unpack('<I',c.mem_read(at,4))[0]
    def ret(n=0):
        sp=c.reg_read(UC_X86_REG_ESP);c.reg_write(UC_X86_REG_EIP,get(sp));c.reg_write(UC_X86_REG_ESP,sp+4+n)
    ctx={};offsets=(0x54cc,0x54d0,0x1528,0x152c,0x54d4,0x154,0x158,0x578c,0x128,0x142c)
    def address(i):return user if i==0 or ctx['alias'] else peer
    def identity(at):
        if at==user:return 0
        if at==peer and not ctx['alias']:return 1
        raise RuntimeError('Wrong user identity')
    def token(at):
        if at==0:return 0
        if at==guild:return 1
        if at==other:return 2
        raise RuntimeError('Wrong guild identity')
    def shot():
        values=[get(address(i)+o) for i in range(2) for o in offsets]
        return (struct.pack('<20I',*values)+bytes([token(get(user+0x1758))])).hex()
    def mutate(stage):
        if ctx['mutated'] or ctx['ordinal']!=ctx['mutation']:return
        ctx['mutated']=True
        if stage=='K':put(user+0x1758,(0,guild,other)[ctx['change']]);return
        if stage=='U' and get(user+0x1758)==0:put(user+0x1758,other);return
        for i in range(2):
            a=address(i)
            for o,n in zip(offsets,(7,99,3,303,404,65534+i,100+i,700+i,800+i,9)):put(a+o,n)
            c.mem_write(a+0x188,(b'MutatedLoser\0' if i==0 else b'M\0'))
    hooks={0x47c990:('W',4),0x47c610:('C',4),0x4b8aa0:('R',0),0x4d4f10:('D',8),0x4d4d20:('S',8),0x45b0d0:('G',0),0x4d4e50:('L',8),stop+16:('K',4),stop+32:('U',4),0x438160:('V',8)}
    def hook(cpu,at,size,_):
        if at==0x505316:ret();return
        if at not in hooks:return
        stage,cleanup=hooks[at];sp=cpu.reg_read(UC_X86_REG_ESP);extra=''
        if stage=='W':extra=f'{identity(cpu.reg_read(UC_X86_REG_EAX))}:{identity(get(sp+4))}'
        elif stage=='C':extra=str(identity(get(sp+4)))
        elif stage=='R':extra=str(identity(cpu.reg_read(UC_X86_REG_ESI)-0x1538))
        elif stage=='G':extra=str(identity(cpu.reg_read(UC_X86_REG_EDI)))
        elif stage in ('D','S','L'):
            extra=bytes(cpu.mem_read(get(sp+4),get(sp+8))).hex()
            if stage=='S':extra=str(identity(cpu.reg_read(UC_X86_REG_ECX)))+':'+extra
        elif stage in ('K','U'):extra=str(token(get(sp+4)-0x578))
        elif stage=='V':
            if get(sp+4)!=user or get(sp+8)!=3:raise RuntimeError('Wrong guild leave subject/reason')
            extra=str(token(cpu.reg_read(UC_X86_REG_EDI)))+':3'
        ctx['events'].append(stage+extra+'/'+shot());ctx['ordinal']+=1
        if stage=='G':
            subject=int(extra);cpu.mem_write(cpu.reg_read(UC_X86_REG_ESI),bytes((i*3+subject)&255 for i in range(74)))
        if ctx['reenter'] and not ctx['entered'] and stage in ('W','K'):
            ctx['entered']=True;cpu.reg_write(UC_X86_REG_EIP,stop+0x100);return
        mutate(stage);ret(cleanup)
    c.hook_add(UC_HOOK_CODE,hook);put(0x5210cc,stop+16);put(0x5210d0,stop+32)
    print('# ps_game.exe SHA256 '+digest+'; complete PvPChkLoseDeath 0047b2c0; primitive drop/effect/reset/log/lock/leave/transports are ports; 16 actual nested calls; no instruction patches')
    print('# case | final two-user raw state and current guild token | ordered full-routine port/packet trace')
    for case in range(2064):
        ctx.update(alias=bool((case//11)%2),mutated=False,ordinal=0,mutation=(case//7)%17,change=(case//29)%3,events=[],reenter=case>=2048,entered=False)
        kind=(4,5,4,5,0,3,-1,-2147483648)[case%8]
        if case>=2048:kind=4 if case%2==0 else 5;ctx['mutation']=999
        c.mem_write(user,bytes(0x6000));c.mem_write(peer,bytes(0x6000));c.mem_write(stack-0x3000,bytes([case%251+1])*0x4000);put(0,0)
        for i,a in enumerate((user,peer)):
            count=(0,65534,65535,65536,0xffffffff,1)[(case//3+i)%6]
            for o,n in zip(offsets,(4+i,8+i,2+i,222+i,333+i,count,count,500+i,600+i,7+i)):put(a+o,n)
            c.mem_write(a+0x188,b'LongLoserName\0' if i==0 else b'W\0')
        put(user+0x1758,(0,guild,other)[(case//5)%3])
        if case>=2048:put(user+0x1758,guild)
        cleanup=4 # Reentered primitive is drop or EnterCriticalSection, both pop4.
        code=b'\x60\x68'+struct.pack('<I',address(1))+b'\x68'+struct.pack('<I',kind&0xffffffff)+b'\xbb'+struct.pack('<I',user)+b'\xb8'+struct.pack('<I',0x47b2c0)+b'\xff\xd0\x61\xc2'+struct.pack('<H',cleanup)
        c.mem_write(stop+0x100,code);c.ctl_remove_cache(stop+0x100,stop+0x200)
        put(stack,stop);put(stack+4,kind);put(stack+8,address(1));c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_EBX,user)
        c.emu_start(0x47b2c0,stop,count=20000)
        if c.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original routine did not return')
        print(case,'|',shot(),'|',' '.join(ctx['events']))
if __name__=='__main__':main()
