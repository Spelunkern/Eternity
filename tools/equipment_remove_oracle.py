#!/usr/bin/env python3
"""Execute ItemEquipmentRem and GemEquipmentRem; isolate effect/world notification ports."""
import random
import struct
import sys
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game
from equipment_oracle import definition

OFFSETS = (0x1220,0x1224,0x1230,0x1228,0x122c,0x1234,0x17c,0x180,0x184,0x12d8,0x12dc,0x12e0,0x12e4,0x12e8,0x12ec,0x12f0,0x12fc,0x1280,0x1284,0x1268,0x126c,0x1270,0x1250,0x1254,0x1258,*range(0x1288,0x12cc,4))


def main(actor=False):
    cpu,digest=load_game();stack,user,item,info,text,stop=(0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x10000000)
    for address in (0,stack,user,item,info,text,stop):
        cpu.mem_map(address,0x10000)
    cpu.mem_write(stop+0x100,b'\xc2\x04\x00')
    for iat in (0x5210cc,0x5210d0):
        cpu.mem_write(iat,struct.pack('<I',stop+0x100))
    for step in range(21):
        cpu.mem_write(0x542100+step*4,struct.pack('<HH',step*7,step*11))
    for gem in range(1,5):
        cpu.mem_write(0x6882ac+168*gem,definition(30,gem,True))
    boundaries=((0x4b8aa0,0),(0x445680,4),(0x421220,32),(0x4d4d20,8),(0x4d9510,0)) if actor else ((0x481990,0),(0x481aa0,0),(0x481ba0,0),(0x481f20,8),(0x4d9510,0))
    for address,cleanup in boundaries:
        cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup))
    events=[]
    def integer(offset):
        return struct.unpack('<i',cpu.mem_read(user+offset,4))[0]
    def hook(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if actor and address==0x4b8aa0:
            events.append('X')
        elif actor and address==0x445680:
            if struct.unpack('<I',cpu.mem_read(sp+4,4))[0]!=user:
                raise RuntimeError('Unexpected equipment group exclusion')
            events.append('P'+bytes(cpu.mem_read(machine.reg_read(UC_X86_REG_ECX),machine.reg_read(UC_X86_REG_EAX))).hex())
        elif actor and address in (0x421220,0x4d4d20):
            pointer,length=struct.unpack('<II',cpu.mem_read(sp+(8 if address==0x421220 else 4),8));events.append(('N' if address==0x421220 else 'S')+bytes(cpu.mem_read(pointer,length)).hex())
        elif address==0x4557ed:
            events.append('W'+','.join(str(integer(offset)) for offset in (0x12d8,0x12dc,0x17c,0x1280)))
        elif address==0x4d9510:
            events.append('D'+str(integer(0x12fc)))
        else:
            events.append({0x455ba9:'V',0x481990:'H',0x481aa0:'S',0x481ba0:'M',0x481f20:'R'}[address])
    for address in tuple(address for address,_ in boundaries) if actor else (0x4557ed,0x4d9510,0x455ba9,0x481990,0x481aa0,0x481ba0,0x481f20):
        cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
    rng=random.Random(0x4553c0)
    print(f'# ps_game.exe SHA256 {digest}; complete ItemEquipmentRem 004553c0, GemEquipmentRem 00456280; empty skill list; {"network/log IO isolated, actual maximum/recovery code" if actor else "max/recovery/log IO isolated"}')
    print('# slot type quality cachedTier transformed initializing seed craft gems[6]; optional actor extras(zone group currentHP currentMP currentSP); output attributes/resources/modifiers/elements/recovery/stateBlocks/tier; optional actor finalHP finalMP finalSP; eventCount events')
    for case in range(1024):
        slot=case%13;kind=rng.choice((5,45) if slot==5 else (5,16,21,45));quality=rng.choice((0,100,101,500,501,1000,65535));tier=case//13%3;shape=case//39%2;initializing=case//78%2
        seed=rng.choice((0,70,100000,0x7fffffff,0x80000000,0xffffffff));craft=rng.choice(('short','01020304050607080903','01020304050607080951','99999999999999999970','0102030405060708xx99'))
        gems=[rng.randrange(6) for _ in range(6)]
        cpu.mem_write(user,bytes(0x9000));cpu.mem_write(item,bytes(160));cpu.mem_write(info,definition(kind,1));cpu.mem_write(item+0x30,struct.pack('<I',info));cpu.mem_write(item+0x44,struct.pack('<H6B',quality,*gems));cpu.mem_write(text,craft.encode()+b'\0')
        for offset in OFFSETS:
            cpu.mem_write(user+offset,struct.pack('<I',seed))
        cpu.mem_write(user+0x19d+slot,bytes([tier]));cpu.mem_write(user+0x1354,bytes([shape]));cpu.mem_write(user+0x57b5,bytes([initializing]));cpu.mem_write(user+0x1334,b'\x09');cpu.mem_write(user+0x1308,bytes([3]*20));cpu.mem_write(user+0x131c,bytes([7]*20))
        extras=[]
        if actor:
            bound=case%2;group=case%3-1;current=(seed,(seed+100)&0xffffffff,(seed+200)&0xffffffff);extras=[bound,group,*current]
            cpu.mem_write(user+0xdc,struct.pack('<II',9001,text+0x2000 if bound else 0));cpu.mem_write(user+0x173c,struct.pack('<I',text+0x1000 if group>=0 else 0));cpu.mem_write(text+0x1114,bytes([max(0,group)]));cpu.mem_write(user+0x1238,struct.pack('<III',*current))
        root=user+0x8000;cpu.mem_write(root,struct.pack('<II',root,root));cpu.mem_write(user+0xa94,struct.pack('<I',root))
        cpu.mem_write(stack+0x8000,struct.pack('<I',stop));cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EDI,item);cpu.reg_write(UC_X86_REG_EAX,text);cpu.emu_start(0x4ba670,stop,count=10000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:
            raise RuntimeError('Craft decoding did not return')
        events.clear();cpu.mem_write(stack+0x8000,struct.pack('<II',stop,slot));cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ECX,user);cpu.reg_write(UC_X86_REG_EAX,item);cpu.emu_start(0x4553c0,stop,count=20000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:
            raise RuntimeError('Equipment removal did not return')
        values=[integer(offset) if offset not in (0x17c,0x180,0x184,0x1280,0x1284) else integer(offset)&0xffffffff for offset in OFFSETS]
        final=struct.unpack('<III',cpu.mem_read(user+0x1238,12)) if actor else ()
        print(slot,kind,quality,tier,shape,initializing,seed,craft,*gems,*extras,*values,cpu.mem_read(user+0x19d+slot,1)[0],*final,len(events),*events,sep='\t')


if __name__=='__main__':
    main(actor="--actor" in sys.argv)
