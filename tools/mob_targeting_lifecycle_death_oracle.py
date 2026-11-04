#!/usr/bin/env python3
"""Complete original DeathByNPC and SendDeath; real strncpy and ordered ports."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game

def main():
    c,digest=load_game();b=0x30000000;user=b;party=b+0x10000;other=b+0x11000;info=b+0x12000;stack=b+0x20000;stop=b+0x3f000
    c.mem_map(0,0x1000);c.mem_map(b,0x40000)
    def put(at,*v):c.mem_write(at,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
    def get(at):return struct.unpack('<I',c.mem_read(at,4))[0]
    def token(p):
        if p==0:return 0
        if p==party:return 1
        if p==other:return 2
        raise RuntimeError('Unexpected party identity')
    def shot():
        values=[get(user+x) for x in (0x142c,0x145c,0x58b0,0x58bc,0x1430,0xdc,0x581c,0x1454,0x1458,0x1238,0x123c,0x1240)]
        return (struct.pack('<12I',*values)+bytes([token(get(user+0x173c)),int(get(user+0xe0)!=0)])).hex()
    def ret(n=0,value=None):
        sp=c.reg_read(UC_X86_REG_ESP);c.reg_write(UC_X86_REG_EIP,get(sp));c.reg_write(UC_X86_REG_ESP,sp+4+n)
        if value is not None:c.reg_write(UC_X86_REG_EAX,value)
    ctx={}
    names={0x45b0d0:('G',0),0x41d640:('N',0),0x4d4e50:('L',8),0x47b530:('P',4),0x488000:('E',0),0x4b8aa0:('R',0),0x4584d0:('D',0),stop+16:('T',0),0x421120:('V',24),stop+32:('K',4),stop+48:('U',4),0x445680:('B',4)}
    def mutate(stage):
        selected=('-', 'G','N','L','P','E','R','D','T','V','K','B','U')[ctx['mutation']]
        if ctx['mutated'] or stage!=selected:return
        ctx['mutated']=True
        if stage=='K':
            put(user+0x173c,(0,party,other)[ctx['lock_change']]);return
        if stage=='U' and get(user+0x173c)==0:
            put(user+0x173c,other);return
        put(user+0x142c,7);put(user+0x145c,-1);put(user+0x581c,0xabcdef01);put(user+0x1454,99);put(user+0x1458,88);put(user+0x1238,101,202,303)
        put(user+0xe0,0 if get(user+0xe0) else party)
    def hook(cpu,at,size,_):
        if at==0x505316:ret();return
        if at not in names:return
        stage,pop=names[at];sp=cpu.reg_read(UC_X86_REG_ESP);extra=''
        if stage=='N':extra=str(cpu.reg_read(UC_X86_REG_ECX))+':'+str(cpu.reg_read(UC_X86_REG_EAX))
        elif stage=='L':extra=bytes(cpu.mem_read(get(sp+4),get(sp+8))).hex()
        elif stage=='V':
            if (get(sp+12),get(sp+16),get(sp+20),get(sp+24))!=(11,9001,0,5):raise RuntimeError('Wrong excluded death route')
            extra=bytes(cpu.mem_read(get(sp+8),get(sp+12))).hex()+':'+str(get(sp+16))
        elif stage in ('K','U'):extra=str(token(get(sp+4)-0x118))
        elif stage=='B':
            if get(sp+4)!=user:raise RuntimeError('Wrong party excluded subject identity')
            extra=str(token(cpu.reg_read(UC_X86_REG_EBX)))+':'+bytes(cpu.mem_read(cpu.reg_read(UC_X86_REG_ECX),cpu.reg_read(UC_X86_REG_EAX))).hex()+':9001'
        ctx['events'].append(stage+extra+'/'+shot())
        if stage=='G':
            # Port fills common prefix only; explicit caller stack seed retains tail.
            cpu.mem_write(cpu.reg_read(UC_X86_REG_ESI),bytes((i*3)&255 for i in range(74)))
        if stage=='G' and ctx['reenter'] and not ctx['entered']:
            ctx['entered']=True
            if ctx['force_reentry']:put(user+0x142c,0)
            c.reg_write(UC_X86_REG_EIP,stop+0x100);return
        mutate(stage)
        ret(pop,info if ctx['npc'] else 0) if stage=='N' else ret(pop,ctx['tick'] if stage=='T' else 0)
    c.hook_add(UC_HOOK_CODE,hook);put(0x5210e4,stop+16);put(0x5210cc,stop+32);put(0x5210d0,stop+48)
    print(f'# ps_game.exe SHA256 {digest}; COMPLETE DeathByNPC 00458fe0..004591ba and SendDeath 00458470; real CRT strncpy 00504c60. Log/NPC lookup/PvP/effects/damage/corpse/clock/recursive locks/party/transport are required ports.')
    print('# case | final lifecycle/shared/resources state | complete ordered ports, packets and callback-entry state')
    for case in range(2064):
        c.mem_write(user,bytes(0x6000));c.mem_write(stack-0x1000,bytes([case%251+1])*0x2000);put(0,0)
        life=(-1,0,1,2,3,0,0,0)[case%8];amulet=(-1,0,1,2,3,4,0x7fffffff)[(case//8)%7];noexp=(case//56)%3-1;group=(case//168)%3;scene=(case//504)%2
        if case>=2048:life=0;group=0;scene=1
        put(user+0x142c,life);put(user+0x145c,17);put(user+0x58b0,amulet);put(user+0x58bc,noexp);put(user+0x1430,55);put(user+0xdc,9001);put(user+0x581c,77);put(user+0x1454,88,99);put(user+0x1238,111,222,333);put(user+0x173c,(0,party,other)[group]);put(user+0xe0,party if scene else 0)
        name=(b'Guardian\0'+b'X'*23) if case%2 else bytes(65+i%26 for i in range(32));c.mem_write(info+20,name)
        ctx.update(case=case,npc=(case//3)%2,tick=(0xfffffff0+case*7)&0xffffffff,mutation=(case//5)%13,mutated=False,lock_change=(case//11)%3,events=[],reenter=case>=2048,entered=False,force_reentry=bool(case%2))
        if case>=2048:ctx['mutation']=0
        code=b'\x60\x68'+struct.pack('<I',0x10000+case)+b'\x68'+struct.pack('<I',case%13+1)+b'\xb9'+struct.pack('<I',user)+b'\xb8'+struct.pack('<I',0x458fe0)+b'\xff\xd0\x61\xc3'
        c.mem_write(stop+0x100,code)
        c.ctl_remove_cache(stop+0x100,stop+0x200) # Host-written thunk immediates change each case.
        put(stack,stop,case%13+1,0x10000+case);c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_ECX,user)
        c.emu_start(0x458fe0,stop,count=20000)
        if c.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original did not return')
        print(case,'|',shot(),'|',' '.join(ctx['events']))
if __name__=='__main__':main()
