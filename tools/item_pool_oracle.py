#!/usr/bin/env python3
"""Original CItem pool, object IDs, partial initialization and retained craft bytes."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game

SCALARS=(0x14,0x6c,0x70,0x74,0x7c,0x80,8,12,16,0x38,0x3c)


def main():
    cpu,digest=load_game()
    stack,manager,items,definitions,stop=(0x20000000,0x30000000,0x31000000,0x32000000,0x10000000)
    for address in (stack,manager,items,definitions,stop):
        cpu.mem_map(address,0x10000)
    cpu.mem_write(stop+0x100,b'\xc2\x04\x00');cpu.mem_write(stop+0x200,b'\xc2\x08\x00')
    for iat in (0x5210cc,0x5210d0,0x5210f8):
        cpu.mem_write(iat,struct.pack('<I',stop+0x100))
    cpu.mem_write(0x5210c8,struct.pack('<I',stop+0x200));cpu.mem_write(0x503a3f,b'\xc3')
    qualities=(0,1,12345,65535)
    for i,quality in enumerate(qualities):
        cpu.mem_write(definitions+i*256+0x4c,struct.pack('<H',quality))
    allocated=[];events=[];state={};available=set()

    def words(address,count=1):
        return struct.unpack('<'+'I'*count,cpu.mem_read(address,4*count))

    def write(address,*values):
        cpu.mem_write(address,struct.pack('<'+'I'*len(values),*[value&0xffffffff for value in values]))

    def index(pointer):
        return allocated.index(pointer) if pointer else -1

    def hook(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x503a3f:
            if words(sp+4)[0]!=160:
                raise RuntimeError('Unexpected CItem allocation size')
            events.append('N'+str(int(state['fail'])))
            if state['fail']:
                machine.reg_write(UC_X86_REG_EAX,0)
            else:
                pointer=items+len(allocated)*256;allocated.append(pointer);cpu.mem_write(pointer,bytes(160));machine.reg_write(UC_X86_REG_EAX,pointer)
        elif address==stop+0x100 and words(sp)[0]==0x40a537:
            pointer=words(sp+4)[0]
            if pointer!=manager+4:
                raise RuntimeError('Wrong item-specific ID counter')
            serial=(words(pointer)[0]+1)&0xffffffff;write(pointer,serial);machine.reg_write(UC_X86_REG_EAX,serial);events.append('I'+str(serial))
    for address in (0x503a3f,stop+0x100):
        cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)

    def call(entry,args=(),regs=()):
        write(stack+0x8000,stop,*args);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000)
        for reg,value in regs:
            cpu.reg_write(reg,value)
        cpu.emu_start(entry,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:
            raise RuntimeError('Original item pool operation did not return')
        return cpu.reg_read(UC_X86_REG_EAX)
    rng=random.Random(0x40a510)
    def mutation(handle,identity=None):
        values=[rng.getrandbits(32) for _ in SCALARS]
        values[0]=rng.choice((0,2,77,0xffffffff)) if identity is None else identity
        # All persisted craft buffers carry a terminating NUL, with nonzero tail bytes.
        craft=bytearray(rng.randrange(1,256) for _ in range(21));craft[rng.randrange(21)]=0
        return ['M',handle,*values,rng.randrange(5),*[rng.randrange(256) for _ in range(3)],rng.randrange(65536),*[rng.randrange(256) for _ in range(6)],
                craft.hex(),rng.getrandbits(32),rng.randrange(256),rng.randrange(256),*[rng.randrange(65536) for _ in range(11)]]
    print(f'# ps_game.exe SHA256 {digest}; CObjectMgr ctor 0040a0e0/CreateItem 0040a510/FreeItem 0040a590, SSyncHeap<CItem>::Alloc 0040adb0, CItem::Init 004b9f30/004b9f80; original pool and resets, raw allocation/critical/interlocked ports isolated')
    print('# command; result serial freshAttempts availableCount allocatedCount; each allocation(available,11 DWORDs,definitionTag,type/typeID/count,quality,6 gems,craft21hex,madeAt,makeType,dropType,11 craft additions); events')
    for step in range(640):
        phase=step%80;regime=step//80
        if phase==0:
            command=['N',(1,0xfffffffe,0xffffffff,0x7fffffff)[regime%4]]
        elif phase==1:
            command=['A',0]
        elif phase==2:
            command=mutation(0,identity=words(allocated[0]+0x14)[0])
        elif phase==3:
            command=['F',0]
        elif phase==4:
            command=['A',1]  # Reuse succeeds even when fresh allocation would fail.
        elif phase==5:
            command=['A',1]  # Empty-pool failure increments attempts only.
        elif phase==6:
            command=['A',0]
        elif phase==7:
            command=['F',0]
        elif phase==8:
            command=['F',1]
        elif phase in (9,10):
            command=['A',0]
        else:
            active=[i for i in range(len(allocated)) if i not in available]
            op=rng.choice(('A','A','F','M','R','V')) if active else 'A'
            if op=='A':
                command=['A',rng.randrange(2)]
            elif op=='F':
                command=['F',rng.choice([-1,*active])]
            elif op=='M':
                command=mutation(rng.choice(active))
            elif op=='R':
                command=['R',rng.choice(active)]
            else:
                candidates=[i for i in active if words(allocated[i]+0x30)[0]]
                command=['V',rng.choice(candidates),rng.randrange(256),rng.randrange(256),rng.randrange(256),rng.getrandbits(32),rng.randrange(256)] if candidates else mutation(rng.choice(active))
        events.clear();result=-1;op=command[0]
        if op=='N':
            allocated.clear();available.clear();cpu.mem_write(manager,bytes(0x1000));cpu.mem_write(items,bytes(0x10000));call(0x40a0e0,regs=((UC_X86_REG_ESI,manager),));write(manager+4,command[1])
        elif op=='A':
            state['fail']=bool(command[1]);pointer=call(0x40a510,regs=((UC_X86_REG_EAX,manager),));result=index(pointer);available.discard(result)
        elif op=='F':
            pointer=allocated[command[1]] if command[1]>=0 else 0;call(0x40a590,regs=((UC_X86_REG_ESI,manager),(UC_X86_REG_EDI,pointer)))
            if pointer:
                available.add(command[1])
        elif op=='M':
            _,handle,*data=command;pointer=allocated[handle]
            for offset,value in zip(SCALARS,data[:11]):
                write(pointer+offset,value)
            definition,kind,type_id,count,quality,*rest=data[11:];gems=rest[:6];craft,made_at,make_type,drop_type,*additions=rest[6:]
            write(pointer+0x30,definitions+(definition-1)*256 if definition else 0);cpu.mem_write(pointer+0x40,bytes((kind,type_id,count)));cpu.mem_write(pointer+0x44,struct.pack('<H',quality));cpu.mem_write(pointer+0x46,bytes(gems))
            cpu.mem_write(pointer+0x4c,bytes.fromhex(craft));write(pointer+0x64,made_at);cpu.mem_write(pointer+0x68,bytes([make_type]));cpu.mem_write(pointer+0x78,bytes([drop_type]));cpu.mem_write(pointer+0x84,struct.pack('<11H',*additions))
        elif op=='R':
            call(0x4b9f30,regs=((UC_X86_REG_EAX,allocated[command[1]]),))
        else:
            _,handle,*args=command;call(0x4b9f80,args,((UC_X86_REG_EAX,allocated[handle]),))
        snapshots=[]
        for handle,pointer in enumerate(allocated):
            definition=words(pointer+0x30)[0]
            snapshots.extend((int(handle in available),*[words(pointer+offset)[0] for offset in SCALARS],(definition-definitions)//256+1 if definition else 0,
                *cpu.mem_read(pointer+0x40,3),struct.unpack('<H',cpu.mem_read(pointer+0x44,2))[0],*cpu.mem_read(pointer+0x46,6),bytes(cpu.mem_read(pointer+0x4c,21)).hex(),words(pointer+0x64)[0],cpu.mem_read(pointer+0x68,1)[0],cpu.mem_read(pointer+0x78,1)[0],*struct.unpack('<11H',cpu.mem_read(pointer+0x84,22))))
        print(*command,result,words(manager+4)[0],words(manager+0x88)[0],words(manager+0x8c)[0],len(allocated),*snapshots,len(events),*events,sep='\t')


if __name__=='__main__':
    main()
