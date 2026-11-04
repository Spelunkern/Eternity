#!/usr/bin/env python3
"""Execute actual mob construction/pool/recycling and random seeding; requires Unicorn."""
import struct
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_INVALID
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,manager,heap,stop=0x20000000,0x30000000,0x31000000,0x10000000
    for at,n in ((0,0x10000),(stack,0x10000),(manager,0x10000),(heap,0x400000),(stop,0x10000)):cpu.mem_map(at,n)
    context={}
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    for at,cleanup in ((0x503a3f,0),(0x503ba1,0),(0x5046b9,0),(0x4f55b0,0),(stop+0x100,4),(stop+0x200,8),(stop+0x300,4)):
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup))
    for iat in (0x5210cc,0x5210d0):put(iat,stop+0x100)
    put(0x5210c8,stop+0x200);put(0x5210f8,stop+0x300)
    def hook(machine,address,size,_):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x503a3f:
            size=get(sp+4)
            if size==3380:
                context['events']+=b'N';context['attempts']+=1
                if context['fail']:
                    machine.reg_write(UC_X86_REG_EAX,0);return
            at=context['heap'];context['heap']+=(size+15)&~15;machine.mem_write(at,b'\xa5'*size);machine.reg_write(UC_X86_REG_EAX,at)
            if size==3380:context['objects'].append(at)
        elif address==0x5046b9:
            context['events']+=b'T';context['times']+=1;machine.reg_write(UC_X86_REG_EAX,context['time'])
        elif address==stop+0x300:
            at=get(sp+4);value=(get(at)+1)&0xffffffff;put(at,value);context['events']+=b'I';machine.reg_write(UC_X86_REG_EAX,value)
        elif address==0x4953f0:context['events']+=b'C' # Actual empty compiled FSM trees are cleared.
        elif address==0x492b90:context['events']+=b'S' # Actual empty stack/hash structures are cleared.
        elif address==0x4f55b0:context['events']+=b'L'+struct.pack('<I',get(sp+4))
    cpu.hook_add(UC_HOOK_CODE,hook)
    def invalid(machine,access,address,size,value,_):raise RuntimeError(f'Invalid pool access {address:08x} at {machine.reg_read(UC_X86_REG_EIP):08x}')
    cpu.hook_add(UC_HOOK_MEM_INVALID,invalid)
    def call(at):
        put(stack+0x8000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.emu_start(at,stop,count=1000000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original pool did not return')
        return cpu.reg_read(UC_X86_REG_EAX)
    def ident(at):return 255 if not at else context['objects'].index(at)
    def snapshot():
        out=struct.pack('<4I',get(manager+8),get(manager+0x40),get(manager+0x44),len(context['objects']))
        first=get(manager+0x3c);order=[]
        while first:
            order.append(ident(first));nxt=get(first+0x4c);first=nxt-0x48 if nxt else 0
            if len(order)>len(context['objects']):raise RuntimeError('Original pool cycle')
        out+=bytes([len(order),*order])
        for at in context['objects']:
            out+=bytes(cpu.mem_read(at+0x5c,4))+bytes(cpu.mem_read(at+0x124,8))+bytes(cpu.mem_read(at+0x50,12))
            out+=bytes(cpu.mem_read(at+0x44,4))+bytes(cpu.mem_read(at+0x40,4))+bytes(cpu.mem_read(at+0x284,4))+bytes(cpu.mem_read(at+0x290,4))+bytes(cpu.mem_read(at+0x27c,4))
            out+=bytes(cpu.mem_read(at+0x2bc,20))+bytes(cpu.mem_read(at+0xc7c,8))
            damage=at+0x1f0;head=get(damage+4);node=get(head+4);out+=bytes(cpu.mem_read(damage,1))+struct.pack('<I',get(damage+0x24))
            while node!=head:
                out+=bytes(cpu.mem_read(node+8,1))+bytes(cpu.mem_read(node+12,16));node=get(node+4)
        return out
    print(f'# ps_game.exe SHA256 {digest}; CreateMob 0040a5e0, FreeMob 0040a630, Alloc 0040ab80, CMob constructor 0048fd50, ReleaseLuaFSM 004b5260, actual empty FSM cleanup, SRandom seeding and embedded damage Add/Reset. Ports: allocator failure, locks, time, interlocked increment, Lua close. Configured retained fields are seeded explicitly.')
    print('# B scenario counter; A fail time expected; C index config expected; F index expected; G index adds damage, H index resets damage. Expected: returned object index BYTE (255 none), event length DWORD/events, counter/construction/free/objects DWORDs, free-list order, per-object retained state/random and embedded damage projections.')
    for scenario in range(128):
        cpu.mem_write(manager,bytes(0x1000));cpu.mem_write(heap,bytes(0x40000));counter=(0,0xfffffffe,0xffffffff,scenario*31)[scenario%4];put(manager+8,counter);put(0x3ed353c,0,0,0)
        context.update(heap=heap,objects=[],events=b'',time=0,times=0,attempts=0,fail=False)
        print('B',scenario,counter,sep='\t')
        active=[]
        operations=[('A',1),('A',0),('A',0),('C',0),('C',1),('F',0),('F',1),('A',1),('A',0),('A',int(scenario%2==0)),('C',0),('F',0),('A',1),('F',1),('F',0)]
        operations[3:3]=[('G',0),('G',1)]
        operations += [('H',0),('H',1)]
        for step,op in enumerate(operations):
            context['events']=b'';result=255;args=list(op)
            if op[0]=='A':
                context['fail']=bool(op[1]);context['time']=(scenario*0x112233+step*17)&0xffffffff;args.append(context['time']);cpu.reg_write(UC_X86_REG_EAX,manager);at=call(0x40a5e0);result=ident(at)
                if at:
                    # Untouched constructor bytes are deliberately known allocator
                    # fill, rather than assumptions about uninitialized source fields.
                    active.append(result)
            elif op[0] in ('G','H'):
                damage=context['objects'][op[1]]+0x1f0;put(stack+0x8000,stop,111,0x7fffffff,100,7);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ESI,damage);cpu.reg_write(UC_X86_REG_EBX,1)
                cpu.emu_start(0x4b8dd0 if op[0]=='G' else 0x4b8d10,stop,count=100000)
                if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Embedded mob damage operation did not return')
            elif op[0]=='C':
                index=op[1];at=context['objects'][index];config=(scenario+step)%8;args.append(config)
                put(at+0x124,0x12340000+config,0x87650000+config);cpu.mem_write(at+0x50,struct.pack('<3f',config+0.25,-3.5,config+1.75))
                put(at+0x44,config&1);put(at+0x40,0);put(at+0x284,(config>>1)&1);put(at+0x290,0x98760000+config);put(at+0x27c,0x70000000+index*16 if config&4 else 0)
            else:
                index=op[1];at=context['objects'][index];cpu.reg_write(UC_X86_REG_EAX,at);cpu.reg_write(UC_X86_REG_EBX,manager);call(0x40a630);active.remove(index)
            out=bytes([result])+struct.pack('<I',len(context['events']))+context['events']+snapshot();print(*args,out.hex(),sep='\t')


if __name__=='__main__':main()
