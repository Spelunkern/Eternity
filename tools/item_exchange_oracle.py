#!/usr/bin/env python3
"""Execute original item-reference removal and both readiness cancellations over three actors."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,users,stop=0x20000000,0x30000000,0x10000000
    cpu.mem_map(stack,0x10000);cpu.mem_map(users,0x30000);cpu.mem_map(stop,0x1000)
    for address,cleanup in ((0x415f30,4),(0x4d4d20,8)):
        cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup))
    events=[];state={};rng=random.Random(0x46f310)
    def write(address,*values):
        cpu.mem_write(address,struct.pack('<'+'I'*len(values),*[value&0xffffffff for value in values]))
    def word(address,signed=False):
        return struct.unpack('<i' if signed else '<I',cpu.mem_read(address,4))[0]
    def actor(index):
        return users+index*0x10000
    def snapshot():
        return bytes(cpu.mem_read(actor(i)+offset,1)[0] for i in range(3) for offset in (0x158c,0x15b4))+bytes(cpu.mem_read(users+offset+index*3,1)[0] for offset in (0x1574,0x159c) for index in range(8))
    def mutate():
        if state['mutated']:
            return
        state['mutated']=True;mode=state['mutation']
        if mode==1:
            write(users+0x54cc,0)
        elif mode==2:
            write(users+0x54cc,5);write(users+0x54d4,9003);write(users+0x1590,3)
        elif mode==3:
            write(users+0x156c,actor(2))
        elif mode==4:
            cpu.mem_write(users+0x1574+state['match']*3,b'\x4d')
        elif mode==5 and state['peer']>=0:
            cpu.mem_write(actor(state['peer'])+0x158c,b'\xff')
        elif mode==7:
            write(users+0x1590,0)
    def hook(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x4d4d20:
            pointer,length=struct.unpack('<II',cpu.mem_read(sp+4,8));target=machine.reg_read(UC_X86_REG_ECX)
            if target not in (actor(0),actor(1),actor(2)):
                raise RuntimeError('Unexpected exchange notification target')
            events.append('S'+str(9001+(target-users)//0x10000)+':'+bytes(cpu.mem_read(pointer,length)).hex()+':'+snapshot().hex());mutate()
        else:
            identity=word(sp+4);events.append('Q'+str(identity))
            if machine.reg_read(UC_X86_REG_EAX)!=5:
                raise RuntimeError('Exchange lookup did not use requester zone')
            if state['mutation']==6 and 0<=state['bag']<=255 and 0<=state['slot']<=255:
                cpu.mem_write(users+0x159c+7*3,bytes((state['bag'],state['slot'])))
            index=identity-9001;target=actor(index) if index==0 or index in (1,2) and state['available']&(1<<index) else 0
            machine.reg_write(UC_X86_REG_EAX,target)
    for address in (0x415f30,0x4d4d20):
        cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
    def read(index):
        pointer=actor(index);peer=word(pointer+0x156c)
        return [word(pointer+0x54cc,True),word(pointer+0x54d4),word(pointer+0x1568,True),-1 if not peer else (peer-users)//0x10000,word(pointer+0x1570),cpu.mem_read(pointer+0x158c,1)[0],bytes(cpu.mem_read(pointer+0x1574,24)).hex(),word(pointer+0x1590,True),word(pointer+0x1598),cpu.mem_read(pointer+0x15b4,1)[0],bytes(cpu.mem_read(pointer+0x159c,24)).hex()]
    print(f'# ps_game.exe SHA256 {digest}; complete ExchangeItemRemove 0046f310, ExchangeCancleReady 0046f450, ExchangeCancleReadyPvP 0047d0a0; actual nested readiness, lookups/transport isolated')
    print('# op bag slot mutation availability directPeer; 3 actors(state opponent tradeMode tradePeer money ready offersHex duelMode money ready offersHex); final 3 actors; eventCount events(with pre-send readiness/bag bytes)')
    for case in range(2048):
        op=0 if case<1536 else 1 if case<1792 else 2;bag=rng.choice((-1,0,1,9,255,256,0x7fffffff));slot=rng.choice((-1,0,1,23,255,256));mutation=case%8;available=rng.randrange(8);direct=rng.randrange(3)
        cpu.mem_write(users,bytes(0x30000));cpu.mem_write(stack,bytes(0x10000))
        for index in range(3):
            pointer=actor(index);write(pointer+0xdc,9001+index,5);write(pointer+0x54cc,rng.choice((-1,0,3,4,5,6)));write(pointer+0x54d4,rng.choice((0,9001,9002,9003,9999)))
            write(pointer+0x1568,rng.choice((-1,0,1,2,3,4)),0 if index and case%3 else actor(rng.randrange(3)),rng.getrandbits(32));write(pointer+0x1590,rng.choice((-1,0,1,2,3,4)),0x12345678,rng.getrandbits(32))
            for offset in (0x1574,0x159c):
                cpu.mem_write(pointer+offset,bytes(rng.randrange(256) for _ in range(24)))
            for offset in (0x158c,0x15b4):
                cpu.mem_write(pointer+offset,bytes([rng.choice((0,0,1,2,255))]))
        if case%5!=4:
            bag=(0,1,9,255)[case%4];slot=case%24;write(users+0x1568,3,actor(case%3));write(users+0x54cc,5);write(users+0x54d4,(9001,9002,9003,9999)[case%4]);write(users+0x1590,3)
            for offset in (0x1574,0x159c):
                for index in ((case%8,(case+1)%8) if case%4==0 else (case%8,)):
                    cpu.mem_write(users+offset+index*3,bytes((bag,slot,rng.randrange(256))))
        if case%5==0:
            write(users+0x156c,0)
        initial=[value for index in range(3) for value in read(index)];peer=initial[3]
        matching=[i for i in range(8) if bytes(cpu.mem_read(users+0x1574+i*3,2))==bytes((bag&255,slot&255))];match=matching[0] if matching else 0
        state.update(mutation=mutation,mutated=False,peer=peer,match=match,bag=bag,slot=slot,available=available);events.clear()
        cpu.reg_write(UC_X86_REG_EDI,users);cpu.reg_write(UC_X86_REG_ECX,users);cpu.reg_write(UC_X86_REG_ESI,actor(direct));cpu.mem_write(stack+0x8000,struct.pack('<III',stop,bag&0xffffffff,slot&0xffffffff));cpu.reg_write(UC_X86_REG_ESP,stack+0x8000)
        cpu.emu_start((0x46f310,0x46f450,0x47d0a0)[op],stop,count=5000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:
            raise RuntimeError('Original exchange reference operation did not return')
        print(op,bag,slot,mutation,available,direct,*initial,*[value for index in range(3) for value in read(index)],len(events),*events,sep='\t')


if __name__=='__main__':
    main()
