#!/usr/bin/env python3
"""Original duel attack exit, wager drops and log header over two real actor layouts."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game

ACTOR_FIELDS = (0x54cc, 0x54d0, 0x54d4, 0x1528, 0x152c, 0x15b8)


def main():
    cpu, digest = load_game()
    stack, users, items, roots, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x10000000
    for address in (stack, items, roots, stop):
        cpu.mem_map(address, 0x10000)
    cpu.mem_map(users, 0x20000)
    cpu.mem_write(stop+0x100, b'\xc2\x04\x00')
    cpu.mem_write(stop+0x200, b'\xc2\x04\x00')
    for iat in (0x5210cc, 0x5210d0):
        cpu.mem_write(iat, struct.pack('<I', stop+0x100))
    cpu.mem_write(0x5210dc, struct.pack('<I', stop+0x200))
    for address, cleanup in ((0x415f30, 4), (0x40a510, 0), (0x504697, 0), (0x417320, 8), (0x4d4d20, 8), (0x4d4e50, 8)):
        cpu.mem_write(address, b'\xc2'+struct.pack('<H', cleanup))
    trace, state = [], {}
    rng = random.Random(0x47c750)

    def words(address, count=1):
        return struct.unpack('<'+'I'*count, cpu.mem_read(address, count*4))

    def actor_id(pointer):
        if pointer not in (users, users+0x10000):
            raise RuntimeError('Unexpected actor pointer')
        return 9001+(pointer-users)//0x10000

    def item_state(pointer):
        return [*cpu.mem_read(pointer+0x40, 3), struct.unpack('<H', cpu.mem_read(pointer+0x44, 2))[0], cpu.mem_read(pointer+0x68, 1)[0],
                int(words(pointer+0x30)[0]!=0), *words(pointer+0x70, 2), cpu.mem_read(pointer+0x78, 1)[0], *words(pointer+0x7c, 2), *words(pointer+8, 3)]

    def hook(machine, address, size, context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x415f30:
            identity=words(sp+4)[0];trace.append('Q'+str(identity))
            target=users if identity==9001 else users+0x10000 if identity==9002 and state['available'] else 0
            machine.reg_write(UC_X86_REG_EAX, target)
        elif address==0x40a510:
            trace.append('A'+str(state['allocate']));machine.reg_write(UC_X86_REG_EAX, items if state['allocate'] else 0)
        elif address==0x504697:
            index=state['random_index'];value=state['randoms'][index];state['random_index']+=1
            trace.append('R'+str(value));machine.reg_write(UC_X86_REG_EAX, value&0xffffffff)
        elif address==0x417320:
            pointer=machine.reg_read(UC_X86_REG_EAX);zone,notify=words(sp+4, 2)
            trace.append('E'+','.join(map(str,[(pointer-items)//256, zone, notify, *item_state(pointer)])))
            machine.reg_write(UC_X86_REG_EAX, 0)  # The caller ignores failure.
        elif address==0x4b8aa0:
            actor=machine.reg_read(UC_X86_REG_ESI)-0x1538;trace.append('H'+str(actor_id(actor)))
        elif address in (0x4d4d20, 0x4d4e50):
            pointer,length=words(sp+4, 2);data=bytes(cpu.mem_read(pointer,length)).hex()
            trace.append(('S'+str(actor_id(machine.reg_read(UC_X86_REG_ECX)))+':' if address==0x4d4d20 else 'G')+data)
        elif address==stop+0x200:
            pointer=words(sp+4)[0];year=state['year'];trace.append('T')
            cpu.mem_write(pointer,struct.pack('<8H',year,11,0,2,13,47,59,0))
    for address in (0x415f30, 0x40a510, 0x504697, 0x417320, 0x4b8aa0, 0x4d4d20, 0x4d4e50, stop+0x200):
        cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
    print(f'# ps_game.exe SHA256 {digest}; PvPChkLeaveByAttack 0047c750, ItemDropPvP 0047c990, GetDropPosRand 0048a270, SetGameLogMain 0045b0d0; nested original cleanup, random positioning, log/calendar code')
    print('# op state peer available allocate year fill; two wagers: money,8 item indexes; 72 random values; final actors (5 duel/target DWORDs,money,8 indexes),9 item states (14 fields), event count and events; op0 appends header hex')
    for scenario in range(768):
        op=0 if scenario<128 else 1 if scenario<384 else 2
        mode=rng.choice((-0x80000000,-1,0,1,2,3,4,5,6,0x7fffffff));peer=rng.choice((0,9001,9002,9999));available=rng.randrange(2);allocate=rng.randrange(2)
        if op==2 and scenario<512:
            mode=(scenario-384)%8;peer=9002;available=1
        year=rng.choice((1999,2000,2026,2063,2064));fill=rng.choice((0,85,170))
        money=[rng.choice((0,1,10000,0xffffffff)) for _ in range(2)]
        slots=[[rng.choice((0,0,1+i)) for i in range(8)] for _ in range(2)]
        # Same pool allocation can be referenced repeatedly by original wager fields.
        if scenario%8==0:
            slots[0]=[1]*8
        randoms=[rng.choice((0,999,1000,32767,-1,-2147483648,rng.randrange(32768))) for _ in range(72)]
        state.update(available=available,allocate=allocate,year=year,randoms=randoms,random_index=0)
        cpu.mem_write(users,bytes(0x20000));cpu.mem_write(items,bytes(0x10000));cpu.mem_write(roots,bytes(0x10000));cpu.mem_write(stack,bytes(0x10000))
        for index in range(2):
            actor=users+index*0x10000;root=roots+index*0x100;damage=root+32
            for offset,value in zip(ACTOR_FIELDS,(mode if index==0 else 4,2+index,peer if index==0 else 9001,1+index,999+index,money[index])):
                cpu.mem_write(actor+offset,struct.pack('<I',value&0xffffffff))
            for i,slot in enumerate(slots[index]):
                cpu.mem_write(actor+0x15bc+i*4,struct.pack('<I',items+slot*256 if slot else 0))
            for offset,value in ((0xdc,9001+index),(0xe0,17+index),(0x578c,12345+index),(0x128,23456+index),(0x13c,0xffff0000+index)):
                cpu.mem_write(actor+offset,struct.pack('<I',value))
            cpu.mem_write(actor+0x136,struct.pack('<H',123+index));cpu.mem_write(actor+0x164,struct.pack('<H',7+index))
            cpu.mem_write(actor+0xd0,struct.pack('<3f',10000.25+index,-30.5+index,-9000.75+index))
            cpu.mem_write(actor+0x5794,(b'long-account-name\0' if index==0 else b'ab\0'));cpu.mem_write(actor+0x188,(b'LongCharacterName\0' if index==0 else b'Cd\0'))
            cpu.mem_write(root,struct.pack('<II',root,root));cpu.mem_write(actor+0xa94,struct.pack('<II',root,root))
            cpu.mem_write(damage,struct.pack('<II',damage,damage));cpu.mem_write(actor+0x1538,struct.pack('<II',damage,damage))
        for index in range(9):
            pointer=items+index*256
            cpu.mem_write(pointer+0x40,bytes((10+index,20+index,30+index)));cpu.mem_write(pointer+0x44,struct.pack('<H',40+index));cpu.mem_write(pointer+0x68,bytes([65+index]))
            cpu.mem_write(pointer+0x30,struct.pack('<I',0x44556677))
            cpu.mem_write(pointer+0x70,struct.pack('<II',88,77));cpu.mem_write(pointer+0x78,b'\x09');cpu.mem_write(pointer+0x7c,struct.pack('<II',66,55))
            cpu.mem_write(pointer+8,struct.pack('<3f',1,2,3))
        trace.clear();cpu.reg_write(UC_X86_REG_FPCW,0x27f);cpu.mem_write(stack+0x8000,struct.pack('<II',stop,users if op!=1 else users+0x10000))
        cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,users);cpu.reg_write(UC_X86_REG_EDI,users);cpu.reg_write(UC_X86_REG_ESI,roots+0x1000)
        if op==0:
            cpu.mem_write(roots+0x1000,bytes([fill])*100)
        cpu.emu_start((0x45b0d0,0x47c990,0x47c750)[op],stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:
            raise RuntimeError('Original duel operation did not return')
        final=[]
        for index in range(2):
            actor=users+index*0x10000;final += [words(actor+offset)[0] for offset in ACTOR_FIELDS]
            final += [((words(actor+0x15bc+i*4)[0]-items)//256 if words(actor+0x15bc+i*4)[0] else 0) for i in range(8)]
        for index in range(9):
            final += item_state(items+index*256)
        print(op,mode,peer,available,allocate,year,fill,*[v for index in range(2) for v in (money[index],*slots[index])],*randoms,*final,len(trace),*trace,
              *([bytes(cpu.mem_read(roots+0x1000,100)).hex()] if op==0 else []),sep='\t')


if __name__=='__main__':
    main()
