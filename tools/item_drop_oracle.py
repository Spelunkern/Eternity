#!/usr/bin/env python3
"""Original complete ItemDrop, whole/split identity, remaining-stack logging and failed admission."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
from equipment_oracle import definition
from equipment_remove_oracle import OFFSETS


def main():
    cpu,digest=load_game();stack,users,items,info,manager,party,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000,0x10000000
    for address in (0,stack,items,info,manager,party,stop):cpu.mem_map(address,0x10000)
    cpu.mem_map(users,0x20000);events=[];state={};rng=random.Random(0x45f260)
    def words(address,count=1):return struct.unpack('<'+'I'*count,cpu.mem_read(address,4*count))
    def write(address,*values):cpu.mem_write(address,struct.pack('<'+'I'*len(values),*[value&0xffffffff for value in values]))
    for address,cleanup in ((0x454280,0),(0x40a510,0),(0x457a50,0),(0x417320,8),(0x4d4f10,8),(0x4d4d20,8),(0x4d4e50,8),(0x421220,32),(0x4208c0,12),(0x445680,4),(0x415f30,4),(0x4d9510,0),(0x4b8aa0,0)):
        cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup))
    cpu.mem_write(stop+0x100,b'\xc2\x04\x00');cpu.mem_write(stop+0x200,b'\xc2\x04\x00')
    for iat in (0x5210cc,0x5210d0):write(iat,stop+0x100)
    write(0x5210dc,stop+0x200);write(0x546f3c,manager)
    for step in range(21):cpu.mem_write(0x542100+step*4,struct.pack('<HH',step*7,step*11))
    for gem in range(1,5):cpu.mem_write(0x6882ac+168*gem,definition(30,gem,True))
    for index,kind in enumerate((5,16,21,45,27)):
        address=info+index*256;cpu.mem_write(address,definition(16 if kind==27 else kind,1));cpu.mem_write(address+0x24,bytes((kind,1)));write(address+0xa0,27 if kind==27 else {5:5,16:16,21:21,45:1}[kind]);write(address,kind*1000+1);cpu.mem_write(address+4,b'Removal item\0')
    def hook(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address in (0x4d4f10,0x4d4d20,0x4d4e50):
            pointer,length=words(sp+4,2);prefix={0x4d4f10:'B',0x4d4d20:'S'+str(words(machine.reg_read(UC_X86_REG_ECX)+0xdc)[0])+':',0x4d4e50:'G'}[address]
            events.append(prefix+bytes(cpu.mem_read(pointer,length)).hex())
        elif address in (0x421220,0x4208c0):
            zone,pointer,length=words(sp+4,3)
            if zone!=party+0x1000:raise RuntimeError('Wrong removal zone route')
            events.append(('N' if address==0x421220 else 'C')+bytes(cpu.mem_read(pointer,length)).hex())
        elif address==0x445680:events.append('P'+bytes(cpu.mem_read(machine.reg_read(UC_X86_REG_ECX),machine.reg_read(UC_X86_REG_EAX))).hex())
        elif address==0x415f30:
            identity=words(sp+4)[0];events.append('Q'+str(identity));machine.reg_write(UC_X86_REG_EAX,users+0x10000 if state['available'] and identity==9002 else 0)
        elif address==0x40a510:
            events.append('A'+str(state['allocate']));machine.reg_write(UC_X86_REG_EAX,items+0x1000 if state['allocate'] else 0)
        elif address==0x457a50:
            events.append('U');machine.reg_write(UC_X86_REG_EAX,0x44332211);machine.reg_write(UC_X86_REG_EDX,0x88776655)
        elif address==0x417320:
            pointer=machine.reg_read(UC_X86_REG_EAX);zone,appearance=words(sp+4,2)
            if zone!=(party+0x1000 if state['bound'] else 0) or appearance!=1:raise RuntimeError('Wrong explicit drop admission')
            item_info=words(pointer+0x30)[0];craft=bytes(cpu.mem_read(pointer+0x4c,21)).split(b'\0',1)[0]
            fields=[words(pointer+0x14)[0],words(item_info)[0],struct.unpack('<Q',cpu.mem_read(pointer+0x38,8))[0],*cpu.mem_read(pointer+0x40,3),struct.unpack('<H',cpu.mem_read(pointer+0x44,2))[0],bytes(cpu.mem_read(pointer+0x46,6)).hex(),craft.hex() or '-',words(pointer+0x64)[0],cpu.mem_read(pointer+0x68,1)[0],*words(pointer+0x70,2),cpu.mem_read(pointer+0x78,1)[0],*words(pointer+0x7c,2),*words(pointer+8,3),*struct.unpack('<11H',cpu.mem_read(pointer+0x84,22))]
            events.append('E'+str(state['admission'])+':'+','.join(map(str,fields)));machine.reg_write(UC_X86_REG_EAX,state['admission'])
        elif address==0x40a590:raise RuntimeError('ItemDrop unexpectedly freed an allocation')
        elif address==0x4d9510:events.append('D'+str(struct.unpack('<i',cpu.mem_read(users+0x12fc,4))[0]))
        elif address==0x4b8aa0:events.append('X')
        elif address==stop+0x200:events.append('T');cpu.mem_write(words(sp+4)[0],struct.pack('<8H',2026,11,0,2,13,47,59,0))
    for address in (0x4d4f10,0x4d4d20,0x4d4e50,0x421220,0x4208c0,0x445680,0x415f30,0x40a590,0x40a510,0x457a50,0x417320,0x4d9510,0x4b8aa0,stop+0x200):cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
    print(f'# ps_game.exe SHA256 {digest}; complete ItemDrop 0045cd10; actual equipment/offer removal, visibility, remaining-stack log/DB payloads and new-item craft/time initialization. SetAttack arithmetic checked separately; allocation/UID/admission/transport/lookup/calendar ports isolated')
    print('# removal fixture inputs plus requested maximum allocate admission before rowCount; final state and events include complete drop payload at admission')
    for case in range(1536):
        op=0;bag=case%4;slot=rng.randrange(13 if bag==0 else 24);bags=3;query=5;query_id=1;bound=case//8%2;group=case%3-1;shape=case//3%2;init=case//6%2;trade=case//12%2;duel=case//24%2;available=case//48%2
        requested=rng.choice((-256,-1,0,1,2,10,255,256,0x7fffffff));maximum=rng.choice((0,1,2,10,255));allocate=case//16%2;admission=case//32%2
        rows=[]
        for index in range(int(case%7!=0)):
            b=index//2 if op==2 else bag;s=(index*3)%24 if op==2 else slot;kind=27 if case%11==0 else rng.choice((5,45) if b==0 and s==5 else (5,16,21,45));count=rng.choice((0,1,10,255));quality=rng.choice((0,100,501,1000,65535));tier=rng.randrange(3);saved=rng.choice((0,quality,65535));made=rng.getrandbits(32);make=rng.choice((0,85,255));gems=bytes(rng.randrange(6) for _ in range(6));craft=rng.choice(('short','01020304050607080903','01020304050607080951','99999999999999999970','0102030405060708xx99'));uid=0x1122334455667700+index
            rows.append((b,s,kind,count,quality,tier,saved,made,make,gems.hex(),craft,uid))
        if rows and case%5==0:requested=rows[0][3]
        for index,kind in enumerate((5,16,21,45,27)):
            cpu.mem_write(info+index*256+0x4b,bytes([maximum]))
        cpu.mem_write(users,bytes(0x20000));cpu.mem_write(stack,bytes(0x10000));cpu.mem_write(items,bytes(0x10000));cpu.mem_write(manager,bytes(0x1000));cpu.mem_write(party,bytes(0x2000))
        for offset in OFFSETS:write(users+offset,100000)
        write(users+0x1238,100000,100100,100200);write(users+0x136,1);write(users+0x1304,bags);write(users+0xdc,9001);write(users+0xe0,party+0x1000 if bound else 0,17,23);write(users+0x578c,12345);write(users+0x128,23456);write(users+0x13c,0xffff0000);cpu.mem_write(users+0x164,struct.pack('<H',7));cpu.mem_write(users+0x5794,b'removal-account\0');cpu.mem_write(users+0x188,b'RemovalCharacter\0');cpu.mem_write(users+0xd0,struct.pack('<3f',10.25,-1.5,10.75));write(users+0x173c,party if group>=0 else 0);cpu.mem_write(party+0x114,bytes([max(0,group)]));cpu.mem_write(users+0x1354,bytes([shape]));cpu.mem_write(users+0x57b5,bytes([init]));cpu.mem_write(users+0x1334,b'\x09');cpu.mem_write(users+0x1308,bytes([3]*20));cpu.mem_write(users+0x131c,bytes([7]*20));root=users+0x8000;write(root,root,root);write(users+0xa94,root);write(users+0x10000+0xdc,9002)
        cpu.mem_write(users+0x19d,bytes(13));cpu.mem_write(users+0x1aa,bytes(26))
        for index,(b,s,kind,count,quality,tier,saved,made,make,gems,craft,uid) in enumerate(rows):
            pointer=items+index*256;descriptor=info+(5,16,21,45,27).index(kind)*256;write(pointer+0x30,descriptor);write(pointer+0x14,2+index);cpu.mem_write(pointer+0x38,struct.pack('<Q',uid));cpu.mem_write(pointer+0x40,bytes((kind,1,count)));cpu.mem_write(pointer+0x44,struct.pack('<H',quality));cpu.mem_write(pointer+0x46,bytes.fromhex(gems));write(pointer+0x64,made);cpu.mem_write(pointer+0x68,bytes([make]));write(users+0x1c4+(b*24+s)*4,pointer)
            text=info+0x5000;cpu.mem_write(text,craft.encode()+b'\0');write(stack+0x8000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EDI,pointer);cpu.reg_write(UC_X86_REG_EAX,text);cpu.emu_start(0x4ba670,stop,count=10000)
            if b==0:cpu.mem_write(users+0x19d+s,bytes([tier]));cpu.mem_write(users+0x1aa+s*2,struct.pack('<H',saved))
        write(users+0x1568,3 if trade else 0,users+0x10000);write(users+0x1590,3 if duel else 0);write(users+0x54cc,5 if duel else 0);write(users+0x54d4,9002)
        for pointer in (users,users+0x10000):cpu.mem_write(pointer+0x158c,b'\x01');cpu.mem_write(pointer+0x15b4,b'\x02')
        cpu.mem_write(users+0x1574,bytes((bag,slot,1)));cpu.mem_write(users+0x159c,bytes((bag,slot,1)));state.update(available=available,allocate=allocate,admission=admission,bound=bound);write(items+0x1000+0x14,3);events.clear();write(stack+0x8000,stop,query if op==2 else bag,query_id)
        write(stack+0x8000,stop,bag,slot,requested);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ECX,users)
        cpu.emu_start(0x45cd10,stop,count=200000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original item drop did not return')
        values=[struct.unpack('<i',cpu.mem_read(users+offset,4))[0] if offset not in (0x17c,0x180,0x184,0x1280,0x1284) else words(users+offset)[0] for offset in OFFSETS]
        remaining=[]
        for b,s,*_ in rows:
            pointer=words(users+0x1c4+(b*24+s)*4)[0]
            if pointer:remaining.extend((b,s,struct.unpack('<Q',cpu.mem_read(pointer+0x38,8))[0],cpu.mem_read(pointer+0x42,1)[0],struct.unpack('<H',cpu.mem_read(pointer+0x44,2))[0]))
        ready=[cpu.mem_read(pointer+offset,1)[0] for pointer in (users,users+0x10000) for offset in (0x158c,0x15b4)]
        print(op,bag,slot,bags,query,query_id,bound,group,shape,init,trade,duel,available,requested,maximum,allocate,admission,len(rows),*[value for row in rows for value in row],*values,*words(users+0x1238,3),*cpu.mem_read(users+0x19d,13),*struct.unpack('<13H',cpu.mem_read(users+0x1aa,26)),len(remaining)//5,*remaining,words(manager+0x8c)[0],*ready,bytes(cpu.mem_read(users+0x1574,24)).hex(),bytes(cpu.mem_read(users+0x159c,24)).hex(),len(events),*events,sep='\t')


if __name__=='__main__':main()
