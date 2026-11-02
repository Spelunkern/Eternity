#!/usr/bin/env python3
"""Original complete detached-item admission, stacking, group notices, logs and ground fallback."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,user,item,info,zone,cell,roots,parties,old,manager,donor,stop=(0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000,0x35000000,0x36000000,0x37000000,0x38000000,0x39000000,0x10000000)
    for address in (0,stack,user,item,info,zone,cell,roots,parties,old,manager,donor,stop):
        cpu.mem_map(address,0x10000)
    cpu.mem_write(stop+0x100,b'\xc2\x04\x00');cpu.mem_write(stop+0x200,b'\xc2\x04\x00');cpu.mem_write(stop+0x300,b'\xc3')
    for iat in (0x5210cc,0x5210d0):
        cpu.mem_write(iat,struct.pack('<I',stop+0x100))
    cpu.mem_write(0x5210dc,struct.pack('<I',stop+0x200));cpu.mem_write(0x5210e4,struct.pack('<I',stop+0x300));cpu.mem_write(0x546f3c,struct.pack('<I',manager))
    for address,cleanup in ((0x4d4f10,8),(0x4d4d20,8),(0x4d4e50,8),(0x445700,8),(0x415f30,4),(0x410320,0),(0x418ca0,4),(0x4208c0,12)):
        cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup))
    events=[];state={};rng=random.Random(0x45df40)
    def words(address,count=1):
        return struct.unpack('<'+'I'*count,cpu.mem_read(address,4*count))
    def write(address,*values):
        cpu.mem_write(address,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    def party(kind):
        return parties+kind*0x1000 if kind>=0 else 0
    def hook(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==stop+0x100:
            argument=words(sp+4)[0];return_address=words(sp)[0]
            if argument in (parties+0x118,parties+0x1118):
                token=(argument-parties-0x118)//0x1000+1
                events.append(('L' if return_address==0x45e0a7 else 'U')+str(token))
        elif address==stop+0x200:
            events.append('T');cpu.mem_write(words(sp+4)[0],struct.pack('<8H',2026,11,0,2,13,47,59,0))
        elif address==stop+0x300:
            events.append('K');machine.reg_write(UC_X86_REG_EAX,100)
        elif address in (0x4d4f10,0x4d4d20,0x4d4e50):
            pointer,length=words(sp+4,2);events.append({0x4d4f10:'B',0x4d4d20:'S',0x4d4e50:'G'}[address]+bytes(cpu.mem_read(pointer,length)).hex())
            if address==0x4d4f10 and not state['mutated']:
                state['mutated']=True
                if state['change']!=-2:
                    write(user+0x173c,party(state['change']))
        elif address==0x445700:
            pointer=machine.reg_read(UC_X86_REG_ECX);length=machine.reg_read(UC_X86_REG_EAX)
            if words(sp+4,2)!=(user,0x42c80000):
                raise RuntimeError('Wrong party loot range/sender')
            events.append('P'+bytes(cpu.mem_read(pointer,length)).hex())
        elif address in (0x410320,0x415f30):
            identity=words(machine.reg_read(UC_X86_REG_EBX))[0] if address==0x410320 else words(sp+4)[0];events.append(f'Q{int(address==0x415f30)},{identity}')
            if address==0x410320:
                output=machine.reg_read(UC_X86_REG_EAX);write(roots+0x5010,donor);write(output,roots+0x5000 if state['donor'] else roots+0x6000)
            else:
                machine.reg_write(UC_X86_REG_EAX,donor if state['donor'] else 0)
        elif address==0x45e73f:
            events.append('F')
        elif address==0x417320:
            events.append('R');state['returned']=True
        elif address==0x418ca0:
            state['indexed']=True
        elif address==0x4208c0:
            owner,pointer,length=words(sp+4,3)
            if owner!=zone or not state['indexed'] or words(item+0x18)[0]!=zone:
                raise RuntimeError('Inventory remainder broadcast preceded admission')
            events.append('V'+bytes(cpu.mem_read(pointer,length)).hex())
    for address in (stop+0x100,stop+0x200,stop+0x300,0x4d4f10,0x4d4d20,0x4d4e50,0x445700,0x410320,0x415f30,0x45e73f,0x417320,0x418ca0,0x4208c0):
        cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
    def bits(value):
        return struct.unpack('<I',struct.pack('<f',value))[0]
    print(f'# ps_game.exe SHA256 {digest}; complete CUser::ItemGet(CItem*) 0045df40, actual stack/placement/packet/log/fallback code; party transport, lookup, DB/log IO and clock isolated')
    print('# bags stackMax incomingCount quality UID madeAt makeType gemsHex craftHex dropType dropID initialParty changeParty donorPresent xBits; sparse input rows(bag,slot,kind,count); final receiptKind count poolAvailable bound expiry; sparse final rows(bag,slot,type,typeID,count,UID,quality,gemsHex,craftHex); events')
    for case in range(640):
        bags=rng.choice((-1,0,1,2,3,9));maximum=rng.choice((0,1,2,10,99,255));amount=rng.choice((0,1,2,5,10,99,255));quality=rng.randrange(65536);uid=0x1122334455667788;made_at=rng.getrandbits(32);make_type=rng.choice((0,65,85,255));gems=bytes(rng.randrange(256) for _ in range(6));craft=rng.choice((b'',b'x',b'ABCDEFGHIJKLMNOPQRST',b'\x81\xa1'))
        drop_type=rng.randrange(5);drop_id=0x12345678;initial_party=rng.choice((-1,0,1));change=rng.choice((-2,-1,0,1));donor_present=rng.randrange(2);x=rng.choice((10.25,200.0));rows=[]
        # Dense inventories exercise partial admission and ground return, including bag 9.
        dense=case%4 in (0,1)
        for bag in range(0,max(bags,1)+1):
            for slot in range(24):
                if dense or rng.randrange(4)==0:
                    kind=rng.choice((1,1,2));count=rng.choice((0,1,max(maximum-1,0),maximum,255));rows.append((bag,slot,kind,count))
        if case%8==0:
            bags=1;maximum=10;amount=25;rows=[(1,slot,1,9) for slot in range(24)]+[(0,0,1,0)]
        if case%8==1:
            bags=1;maximum=10;amount=25;rows=[(1,slot,1,9) for slot in range(23)]+[(0,0,1,0)]
        for address in (stack,user,item,info,zone,cell,roots,parties,old,manager,donor):
            cpu.mem_write(address,bytes(0x10000))
        write(user+0x1304,bags);write(user+0x578c,12345);write(user+0xdc,9001);write(user+0x128,23456);write(user+0x13c,0xffff0000);write(user+0xe0,zone);write(user+0xd0,bits(10.25),bits(-1.5),bits(10.75));cpu.mem_write(user+0x136,struct.pack('<H',123));cpu.mem_write(user+0x164,struct.pack('<H',7));cpu.mem_write(user+0x5794,b'inventory-account\0');cpu.mem_write(user+0x188,b'InventoryCharacter\0')
        for kind in (0,1):
            cpu.mem_write(party(kind)+0x114,bytes([kind]))
        write(user+0x173c,party(initial_party));cpu.mem_write(donor+0x188,b'Dropper\0')
        write(info,5001);cpu.mem_write(info+4,b'Inventory item\0');cpu.mem_write(info+0x4b,bytes([maximum]));write(item+0x30,info);write(item+0x14,2);write(item+0x38,uid&0xffffffff,uid>>32);cpu.mem_write(item+0x40,bytes((5,1,amount)));cpu.mem_write(item+0x44,struct.pack('<H',quality));cpu.mem_write(item+0x46,gems);cpu.mem_write(item+0x4c,craft+b'\0');write(item+0x64,made_at);cpu.mem_write(item+0x68,bytes([make_type]));cpu.mem_write(item+0x78,bytes([drop_type]));write(item+0x7c,drop_id);write(item+8,bits(x),bits(-1.5),bits(10.75));write(item+0x2c,item+8)
        for index,(bag,slot,kind,count) in enumerate(rows):
            pointer=old+index*256;cpu.mem_write(pointer+0x40,bytes((5 if kind==1 else 6,1 if kind==1 else 2,count)));write(user+0x1c4+(bag*24+slot)*4,pointer)
        write(zone+0x28,5,5,zone+0x1000);write(zone+0x1000,cell);write(zone+0x7c,roots+0x6000)
        for offset,root in ((0x84,roots+0x100),(0x194,roots+0x200)):
            write(root,root,root);write((cell if offset==0x84 else zone)+offset,root,root)
        state.update(change=change,mutated=False,donor=donor_present,returned=False,indexed=False);events.clear();write(stack+0x8000,stop,item);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ECX,user);cpu.reg_write(UC_X86_REG_FPCW,0x27f)
        cpu.emu_start(0x45df40,stop,count=200000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:
            raise RuntimeError('Original inventory admission did not return')
        final=[]
        for bag in range(10):
            for slot in range(24):
                pointer=words(user+0x1c4+(bag*24+slot)*4)[0]
                if pointer:
                    name=bytes(cpu.mem_read(pointer+0x4c,21)).split(b'\0',1)[0]
                    final.append((bag,slot,*cpu.mem_read(pointer+0x40,3),struct.unpack('<Q',cpu.mem_read(pointer+0x38,8))[0],struct.unpack('<H',cpu.mem_read(pointer+0x44,2))[0],bytes(cpu.mem_read(pointer+0x46,6)).hex(),name.hex() or '-'))
        free=words(manager+0x8c)[0];kind=3 if state['returned'] else 1 if free else 2
        print(bags,maximum,amount,quality,uid,made_at,make_type,gems.hex(),craft.hex() or '-',drop_type,drop_id,initial_party,change,donor_present,bits(x),len(rows),*[v for row in rows for v in row],kind,cpu.mem_read(item+0x42,1)[0],free,int(words(item+0x18)[0]!=0),words(item+0x6c)[0],len(final),*[v for row in final for v in row],len(events),*events,sep='\t')


if __name__=='__main__':
    main()
