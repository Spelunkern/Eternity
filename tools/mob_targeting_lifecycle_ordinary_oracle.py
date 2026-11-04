#!/usr/bin/env python3
"""Complete original Death ordinary/fallback routes (no player maximum).
No Death instructions patched; primitive owners, world lookup and transports are
ports. Explicit nonzero frame seed exposes failed-GetMax log provenance.
"""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game

def main():
    c,digest=load_game();b=0x30000000;user=b;peer=b+0x8000;mob=b+0x10000;info=b+0x12000;party=b+0x14000;other=b+0x15000;stack=b+0x20000;stop=b+0x3f000
    c.mem_map(0,0x1000);c.mem_map(b,0x40000)
    def put(at,*v):c.mem_write(at,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
    def get(at):return struct.unpack('<I',c.mem_read(at,4))[0]
    def token(p):
        if p==0:return 0
        if p==party:return 1
        if p==other:return 2
        raise RuntimeError('Unknown party identity')
    def ret(n=0,value=None):
        sp=c.reg_read(UC_X86_REG_ESP);c.reg_write(UC_X86_REG_EIP,get(sp));c.reg_write(UC_X86_REG_ESP,sp+4+n)
        if value is not None:c.reg_write(UC_X86_REG_EAX,value)
    def shot():
        values=[get(user+x) for x in (0x142c,0x145c,0x58b0,0x58bc,0x1430,0xdc,0x581c,0x1454,0x1458,0x1238,0x123c,0x1240,0x14c,0x150,0x154,0x158)]
        return (struct.pack('<16I',*values)+bytes([token(get(user+0x173c)),int(get(user+0xe0)!=0)])).hex()
    ctx={}
    names={0x44b3d0:('X',0),0x478520:('H',0),0x44b330:('A',0),0x4b8c40:('M',0),0x45b0d0:('G',0),0x416800:('O',4),0x415f30:('F',4),0x45a180:('I',8),0x4d4e50:('L',8),0x47b530:('P',4),0x488000:('E',0),0x4b8aa0:('R',0),0x4584d0:('D',0),stop+16:('T',0),0x421120:('V',24),stop+32:('K',4),stop+48:('U',4),0x445680:('B',4)}
    stages='-XHAMGOFILPERDTVKBU'
    def mutate(stage):
        if ctx['mutated'] or stages[ctx['mutation']]!=stage:return
        ctx['mutated']=True
        if stage=='K':put(user+0x173c,(0,party,other)[ctx['lock_change']]);return
        if stage=='U' and get(user+0x173c)==0:put(user+0x173c,other);return
        put(user+0x142c,7);put(user+0x145c,-1);put(user+0x581c,0xabcdef01);put(user+0x1454,99,88);put(user+0x1238,101,202,303);put(user+0x14c,7,8,9,10);put(user+0xe0,0 if get(user+0xe0) else party)
    def hook(cpu,at,size,_):
        if at==0x505316:ret();return
        if at not in names:return
        stage,cleanup=names[at];sp=cpu.reg_read(UC_X86_REG_ESP);extra=''
        if stage in ('O','F'):extra=str(get(sp+4))
        elif stage=='I':
            if get(sp+4)!=user:raise RuntimeError('Wrong drop victim')
            extra='1' if get(sp+8)==peer else '0'
        elif stage=='L':extra=bytes(cpu.mem_read(get(sp+4),get(sp+8))).hex()
        elif stage=='V':
            if (get(sp+12),get(sp+16),get(sp+20),get(sp+24))!=(11,9001,0,5):raise RuntimeError('Wrong death route')
            extra=bytes(cpu.mem_read(get(sp+8),get(sp+12))).hex()+':9001'
        elif stage in ('K','U'):extra=str(token(get(sp+4)-0x118))
        elif stage=='B':
            if get(sp+4)!=user:raise RuntimeError('Wrong party exclusion')
            extra=str(token(cpu.reg_read(UC_X86_REG_EBX)))+':'+bytes(cpu.mem_read(cpu.reg_read(UC_X86_REG_ECX),cpu.reg_read(UC_X86_REG_EAX))).hex()+':9001'
        ctx['events'].append(stage+extra+'/'+shot())
        if stage=='G':cpu.mem_write(cpu.reg_read(UC_X86_REG_ESI),bytes((i*3)&255 for i in range(74)))
        elif stage=='M' and ctx['maximum']:
            out=cpu.reg_read(UC_X86_REG_EBX);put(out,0,0,ctx['winner_kind'],123,999)
        if stage=='X' and ctx['reenter'] and not ctx['entered']:
            ctx['entered']=True
            if ctx['force_reentry']:put(user+0x142c,0)
            c.reg_write(UC_X86_REG_EIP,stop+0x100);return
        mutate(stage)
        value=None
        if stage=='M':value=int(ctx['maximum'])
        elif stage=='O':value=mob if ctx['found'] else 0
        elif stage=='F':value=peer if ctx['found'] else 0
        elif stage=='T':value=ctx['clock']
        ret(cleanup,value)
    c.hook_add(UC_HOOK_CODE,hook);put(0x5210e4,stop+16);put(0x5210cc,stop+32);put(0x5210d0,stop+48)
    print(f'# ps_game.exe SHA256 {digest}; COMPLETE ordinary/fallback Death 004586a0 and SendDeath 00458470. Player-maximum reward branch excluded explicitly. Primitive cleanup/GetMax/lookups/drop/log/clock/locks/transports are ports.')
    print('# case | final lifecycle/resource/kill state | ordered full routine port/packet/callback-entry trace')
    for case in range(1552):
        c.mem_write(user,bytes(0x6000));c.mem_write(stack-0x2000,bytes([case%251+1])*0x3000);put(0,0)
        life=(-1,0,1,2,3,0,0,0)[case%8];amulet=(-1,0,1,2,3,4,0x7fffffff)[(case//8)%7];noexp=(case//56)%3-1;group=(case//168)%3;scene=(case//504)%2
        if case>=1536:life=0
        put(user+0x142c,life);put(user+0x145c,17);put(user+0x58b0,amulet);put(user+0x58bc,noexp);put(user+0x1430,55);put(user+0xdc,9001);put(user+0x581c,77);put(user+0x1454,88,99);put(user+0x1238,111,222,333);put(user+0x14c,1,2,3,4);put(user+0x173c,(0,party,other)[group]);put(user+0xe0,party if scene else 0)
        c.mem_write(party+0x114,bytes([(case//7)%3]));c.mem_write(other+0x114,bytes([(case//11)%3]));c.mem_write(peer+0x188,b'OtherPlayer\0');put(mob+0xa8,info);c.mem_write(mob+0x220,struct.pack('<H',555));c.mem_write(info+2,b'Skeleton\0')
        maximum=(case//3)%2;winner_kind=(0,2,255)[(case//13)%3];kind=(1,2,-1)[(case//5)%3];source=9001 if (case//17)%2 else 999
        ctx.update(maximum=maximum,winner_kind=winner_kind,found=(case//19)%2,clock=(0xfffffff0+case*7)&0xffffffff,mutation=(case//23)%len(stages),lock_change=(case//29)%3,mutated=False,events=[],reenter=case>=1536,entered=False,force_reentry=bool(case%2))
        if case>=1536:ctx['mutation']=0
        code=b'\x60\x68'+struct.pack('<I',case%2)+b'\x68'+struct.pack('<I',source)+b'\x68'+struct.pack('<I',kind&0xffffffff)+b'\xb9'+struct.pack('<I',user)+b'\xb8'+struct.pack('<I',0x4586a0)+b'\xff\xd0\x61\xc3'
        c.mem_write(stop+0x100,code);c.ctl_remove_cache(stop+0x100,stop+0x200)
        put(stack,stop,kind,source,case%2);c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_ECX,user)
        c.emu_start(0x4586a0,stop,count=20000)
        if c.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original ordinary death did not return')
        print(case,'|',shot(),'|',' '.join(ctx['events']))
if __name__=='__main__':main()
