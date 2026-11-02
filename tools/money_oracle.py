#!/usr/bin/env python3
"""Original money bounds, saturation, receipt packets/logs and detached-money-item credit."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_EDX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,user,item,manager,stop=(0x20000000,0x30000000,0x31000000,0x32000000,0x10000000)
    for address in (0,stack,user,item,manager,stop):
        cpu.mem_map(address,0x10000)
    cpu.mem_write(stop+0x100,b'\xc2\x04\x00');cpu.mem_write(stop+0x200,b'\xc2\x04\x00')
    for iat in (0x5210cc,0x5210d0):
        cpu.mem_write(iat,struct.pack('<I',stop+0x100))
    cpu.mem_write(0x5210dc,struct.pack('<I',stop+0x200));cpu.mem_write(0x546f3c,struct.pack('<I',manager))
    for address in (0x4d4f10,0x4d4d20,0x4d4e50):
        cpu.mem_write(address,b'\xc2\x08\x00')
    events=[];state={};rng=random.Random(0x45ec70)
    def words(address,count=1):
        return struct.unpack('<'+'I'*count,cpu.mem_read(address,4*count))
    def write(address,*values):
        cpu.mem_write(address,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    def hook(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==stop+0x200:
            events.append('T');cpu.mem_write(words(sp+4)[0],struct.pack('<8H',state['year'],11,0,2,13,47,59,0))
        elif address==0x40a590:
            if machine.reg_read(UC_X86_REG_EDI)!=item:
                raise RuntimeError('Money ItemGet freed the wrong allocation')
            events.append('F')
        else:
            pointer,length=words(sp+4,2);data=bytes(cpu.mem_read(pointer,length))
            route={0x4d4f10:'B',0x4d4d20:'S',0x4d4e50:'G'}[address];events.append(route+data.hex())
            if route=='S':
                if length!=16 or data[:2]!=b'\x05\x02':
                    raise RuntimeError('Unexpected money receipt packet')
                state['accepted']=struct.unpack_from('<I',data,7)[0]
    for address in (stop+0x200,0x40a590,0x4d4f10,0x4d4d20,0x4d4e50):
        cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
    print(f'# ps_game.exe SHA256 {digest}; ChkAddableMoney 00438460, IsvaildMoney 0044a040, ChkAddMoneyGet 0045b230, ChkAddMoney 00478360, SendDBMoney 0046c700, ItemGetMoney 0045ec70, ItemGet(CItem*) money branch 0045df40..0045e085')
    print('# op money amount accountID year itemTypeID itemCount; result finalMoney finalAmount accepted poolAvailable; eventCount events(B DB status,S self,T local time,G log,F free)')
    boundaries=(0,1,65535,65536,0x7fffffff,0x80000000,0xfffffffe,0xffffffff)
    for op,entry in enumerate((0x438460,0x44a040,0x45b230,0x478360,0x46c700,0x45ec70,0x45df40)):
        for case in range(256):
            current=boundaries[case//8%8] if case<64 else rng.getrandbits(32)
            amount=boundaries[case%8] if case<64 else rng.choice((0,1,0xffffffff,0xffffffff-current,rng.getrandbits(32)))
            account=rng.choice((0,12345,0xffffffff));year=rng.choice((2000,2026,2063,2064));type_id=rng.randrange(256);count=rng.randrange(256)
            state.update(year=year,accepted=0);cpu.mem_write(stack,bytes(0x10000));cpu.mem_write(user,bytes(0x10000));cpu.mem_write(item,bytes(0x10000));cpu.mem_write(manager,bytes(0x10000))
            write(user+0x140,current);write(user+0x578c,account);write(user+0x128,23456);write(user+0x13c,0xffff0000);write(user+0xd0,*struct.unpack('<3I',struct.pack('<3f',10.25,-1.5,100000.0)))
            cpu.mem_write(user+0x136,struct.pack('<H',123));cpu.mem_write(user+0x164,struct.pack('<H',7));cpu.mem_write(user+0x5794,b'money-account\0');cpu.mem_write(user+0x188,b'MoneyCharacter\0')
            cpu.mem_write(item+0x40,bytes((26,type_id,count)));write(item+0x80,amount);write(item+0x200,amount);write(item+0x14,2)
            events.clear();args=(amount,) if op in (0,1) else (item,) if op==6 else ()
            write(stack+0x8000,stop,*args);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000)
            for reg,value in ((UC_X86_REG_EAX,user),(UC_X86_REG_ECX,amount if op==5 else user),(UC_X86_REG_EDX,user if op==5 else amount),(UC_X86_REG_ESI,user),(UC_X86_REG_EDI,item+0x200)):
                cpu.reg_write(reg,value)
            cpu.emu_start(entry,stop,count=100000)
            if cpu.reg_read(UC_X86_REG_EIP)!=stop:
                raise RuntimeError('Original money function did not return')
            result=cpu.reg_read(UC_X86_REG_EAX)&255 if op in (0,1) else 0
            print(op,current,amount,account,year,type_id,count,result,words(user+0x140)[0],words(item+0x200)[0],state['accepted'],words(manager+0x8c)[0],len(events),*events,sep='\t')


if __name__=='__main__':
    main()
