#!/usr/bin/env python3
"""Complete original CUser::Rebirth with explicit corpse/move/transport/log ports."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game

def main():
    c,digest=load_game();b=0x30000000;user=b;dead=b+0x10000;world=b+0x11000;maps=b+0x12000;dest=b+0x20000;stack=b+0x30000;stop=b+0x3f000
    c.mem_map(b,0x40000)
    def put(at,*v):c.mem_write(at,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
    def get(at):return struct.unpack('<I',c.mem_read(at,4))[0]
    def floats(at,*v):c.mem_write(at,struct.pack('<'+'f'*len(v),*v))
    def ret(n=0,result=None):
        sp=c.reg_read(UC_X86_REG_ESP);c.reg_write(UC_X86_REG_EIP,get(sp));c.reg_write(UC_X86_REG_ESP,sp+4+n)
        if result is not None:c.reg_write(UC_X86_REG_EAX,result)
    def shot():
        return (bytes(c.mem_read(user+0x142c,4))+bytes(c.mem_read(user+0x1274,4))+bytes(c.mem_read(user+0x1434,1))+
                bytes(c.mem_read(user+0x1238,12))+bytes(c.mem_read(user+0x164,4))+bytes(c.mem_read(user+0xd0,12))+
                bytes(c.mem_read(user+0x13c,4))+bytes([int(get(user+0xe0)!=0)])).hex()
    ctx={}
    names={0x4176c0:('F',4),0x417820:('D',4),0x416260:('M',20),0x4208c0:('V',12),0x45b0d0:('G',0),0x4d4e50:('L',8),0x482230:('C',12),0x4d4d20:('S',8),0x40d0c0:('W',20)}
    def mutate(name):
        selected={1:'F',2:'D',3:'M',4:'S',5:'V',6:'G',7:'L',8:'C'}
        if not ctx['mutated'] and (selected.get(ctx['mutation'])==name or (ctx['mutation']==3 and name=='W')):
            ctx['mutated']=True;put(user+0x142c,7);put(user+0x1274,-3);c.mem_write(user+0x1434,b'\x08');put(user+0x1238,65537+ctx['case'],7,9)
            c.mem_write(user+0x164,struct.pack('<HH',8,77));floats(user+0xd0,12,-3,8);put(user+0x13c,0xfedcba98);put(user+0xe0,0 if get(user+0xe0) else world)
    def hook(cpu,at,size,_):
        if at==stop+0x150:ctx['nested']=False;return
        if at==0x505316:ret();return
        if at not in names:return
        name,cleanup=names[at];sp=cpu.reg_read(UC_X86_REG_ESP);extra=''
        if name=='F':
            if get(sp+4)!=9001:raise RuntimeError('Wrong corpse user')
        elif name=='D':
            if get(sp+4)!=123:raise RuntimeError('Wrong corpse identity')
        elif name=='M':
            extra=str(get(sp+8)&65535)+':'+bytes(cpu.mem_read(sp+12,12)).hex()
        elif name in ('S','L'):
            extra=bytes(cpu.mem_read(get(sp+4),get(sp+8))).hex()
        elif name=='V':extra=bytes(cpu.mem_read(get(sp+8),get(sp+12))).hex()
        elif name=='C':
            hp,spv,mp=struct.unpack('<3I',cpu.mem_read(sp+4,12))
            extra=struct.pack('<3I',hp,mp,spv).hex()
        elif name=='W':extra=str(struct.unpack('<i',cpu.mem_read(sp+8,4))[0])+':'+bytes(cpu.mem_read(sp+12,12)).hex()
        ctx['events'].append(name+extra+'/'+shot())
        if name=='F' and ctx['reenter'] and not ctx['entered']:
            ctx['entered']=True;ctx['nested']=True;c.reg_write(UC_X86_REG_EIP,stop+0x100);return
        if name=='G':cpu.mem_write(cpu.reg_read(UC_X86_REG_ESI),bytes((i*3)&255 for i in range(74)))
        if name=='M' and ctx['moveok']:cpu.mem_write(user+0xd0,bytes(cpu.mem_read(sp+12,12)))
        if name=='W':
            cpu.mem_write(user+0x164,struct.pack('<HH',get(sp+8)&65535,5));cpu.mem_write(user+0xd0,bytes(cpu.mem_read(sp+12,12)));put(user+0xe0,0)
        mutate(name)
        ret(cleanup,dead if ctx['corpse'] and not ctx['nested'] else 0) if name=='F' else ret(cleanup,int(ctx['moveok']) if name=='M' else 0)
    c.hook_add(UC_HOOK_CODE,hook)
    put(0x546f38,world);put(world+8,10,maps);put(dead+0x18,123)
    print(f'# ps_game.exe SHA256 {digest}; COMPLETE CUser::Rebirth 00459ec0..0045a172; corpse/move/zone-leave/transport/SetGameLogMain/SendRecoverChange are required ports. Cookie shim only.')
    print('# case | final shared state/resources/location hex | complete ordered port/packet and pre-callback state trace')
    for case in range(1552):
        c.mem_write(user,bytes(0x6000));life=(1,1,1,1,0,-1,2,1)[case%8];corpse=(case//8)%2;restore=(case//16)%2;route=(case//32)%4;moveok=(case//128)%2;scene=(case//256)%2;mutation=(case//7)%9
        put(user+0x142c,life);put(user+0x1274,(-5,0,1,7)[case%4]);c.mem_write(user+0x1434,b'\x09');put(user+0xdc,9001);put(user+0xe0,world if scene else 0);put(user+0x13c,0x12345678)
        c.mem_write(user+0x164,struct.pack('<HH',7,3));floats(user+0xd0,1.25,2.5,3.75);put(user+0x1238,11,22,33);put(user+0x17c,65537,70000,0xffffffff);floats(dest,101.25,-2.5,77.75)
        target=(7,8,11,-1)[route];kind=case*257-512
        if case>=1536:
            life=1;corpse=1;restore=case%2;target=7 if case%4<2 else 8;moveok=1;scene=1;mutation=0
            put(user+0x142c,1);put(user+0xe0,world)
        # Nested complete original call, saving the outer FindDead registers.
        code=b'\x60'+b'\x68'+struct.pack('<I',0)+b'\x68'+struct.pack('<I',target&0xffffffff)+b'\x68'+struct.pack('<I',kind&0xffffffff)+b'\x68'+struct.pack('<I',dest)+b'\xb9'+struct.pack('<I',user)+b'\xb8'+struct.pack('<I',0x459ec0)+b'\xff\xd0'
        code+=b'\x90'*(0x50-len(code))+b'\x61\xb8'+struct.pack('<I',dead)+b'\xc2\x04\x00'
        c.mem_write(stop+0x100,code)
        ctx.update(case=case,corpse=corpse,moveok=moveok,mutation=mutation,mutated=False,events=[],reenter=case>=1536,entered=False,nested=False)
        put(stack,stop,dest,kind,target,restore);c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_ECX,user)
        c.emu_start(0x459ec0,stop,count=10000)
        if c.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original did not return')
        print(case,'|',shot(),'|',' '.join(ctx['events']))
if __name__=='__main__':main()
