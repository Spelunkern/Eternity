#!/usr/bin/env python3
"""Execute original item quest links/flags and pickup authorization through real zone removal."""
import hashlib
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game, ROOT

FIXTURE={(0,1):([9],[7]),(1,0):([8],[2]),(16,1):([2,2],[]),(27,1):([],[0,2,2,65535,7]),(30,3):([7],[2,7]),(255,255):([65535],[65535])}
FLAG_KEYS=((5,1),(16,1),(21,1),(45,1),(30,1),(30,2),(30,3),(30,4))


def main():
    cpu,digest=load_game();stack,user,item,zone,cell,roots,info,party,file,stop=(0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000,0x35000000,0x36000000,0x37000000,0x10000000)
    for address in (stack,user,item,zone,cell,roots,info,party,file,stop):
        cpu.mem_map(address,0x10000)
    game,heap,matrix=0x50000000,0x60000000,0x3ed35d0;cpu.mem_map(game,0xc00000);cpu.mem_map(heap,0x1000000)
    cpu.mem_write(stop+0x100,b'\xc2\x04\x00');cpu.mem_write(stop+0x200,b'\xc3')
    for iat in (0x5210cc,0x5210d0):
        cpu.mem_write(iat,struct.pack('<I',stop+0x100))
    cpu.mem_write(0x5210e4,struct.pack('<I',stop+0x200));cpu.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x1000)+b'\xc3')
    for address,cleanup in ((0x503ae3,0),(0x510785,0),(0x419010,0),(0x417f90,4),(0x4208c0,12)):
        cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup))
    state={};events=[]
    def words(address,count=1):
        return struct.unpack('<'+'I'*count,cpu.mem_read(address,count*4))
    def write(address,*values):
        cpu.mem_write(address,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    def hook(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x503ae3:
            amount=words(sp+4)[0];pointer=state['heap'];state['heap']+=(amount+15)&~15;machine.reg_write(UC_X86_REG_EAX,pointer)
        elif address==0x510785:
            fd,pointer,amount=words(sp+4,3);data=state['data'][state['offset']:state['offset']+amount]
            if fd!=12 or len(data)!=amount:
                raise RuntimeError('Item link read exceeded input')
            if amount:
                cpu.mem_write(pointer,data)
            state['offset']+=amount;machine.reg_write(UC_X86_REG_EAX,amount)
        elif address==0x419010:
            output=machine.reg_read(UC_X86_REG_EAX);write(roots+0xf010,item);write(output,roots+0xf000 if state['present'] else roots+0xe000)
        elif address==0x417f90:
            state['present']=False
        elif address==stop+0x200:
            events.append('T');machine.reg_write(UC_X86_REG_EAX,state['tick'])
        elif address==0x48a3f0:
            events.append('P')
        elif address==0x41b1f0:
            events.append('D')
        elif address==0x47e4b0:
            events.append('Q'+str(words(sp+4)[0]))
        elif address==0x4208c0:
            owner,pointer,length=words(sp+4,3)
            if owner!=zone or state['present'] or words(item+0x18)[0] or machine.reg_read(UC_X86_REG_ECX) or machine.reg_read(UC_X86_REG_EAX):
                raise RuntimeError('Pickup broadcast did not follow ordinary removal')
            events.append('V'+bytes(cpu.mem_read(pointer,length)).hex())
    for address in (0x503ae3,0x510785,0x419010,0x417f90,stop+0x200,0x48a3f0,0x41b1f0,0x47e4b0,0x4208c0):
        cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
    def call(entry,args=(),regs=(),timeout=120000000):
        write(stack+0x8000,stop,*args);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_FPCW,0x27f)
        for reg,value in regs:
            cpu.reg_write(reg,value)
        cpu.emu_start(entry,stop,timeout=timeout)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:
            raise RuntimeError(f'Original item pick/link operation did not return at {cpu.reg_read(UC_X86_REG_EIP):08x}')
        return cpu.reg_read(UC_X86_REG_EAX)
    def load(data,offset=0):
        cpu.mem_write(matrix,bytes(0x100000));state.update(data=data,offset=offset,heap=heap);write(file,12)
        if call(0x4c9d90,(file,),((UC_X86_REG_EAX,matrix),))&255!=1:
            raise RuntimeError('Original item link loader failed')
        canonical=bytearray()
        for i in range(65536):
            for at in (matrix+i*16,matrix+i*16+8):
                count,pointer=words(at,2);canonical+=struct.pack('<I',count)
                if count:
                    canonical+=bytes(cpu.mem_read(pointer,count*2))
        value=0xcbf29ce484222325
        for byte in canonical:
            value=((value^byte)*0x100000001b3)&0xffffffffffffffff
        cpu.mem_write(game,bytes(0xc00000))
        for kind,type_id in FLAG_KEYS:
            cpu.mem_write(game+0x1804+(kind*256+type_id)*168+0x24,bytes((kind,type_id)))
        call(0x4bf400,(game,))
        flags=[cpu.mem_read(game+0x1804+(kind*256+type_id)*168+0x9c,1)[0] for kind,type_id in FLAG_KEYS]
        for kind in (-1,0,1,16,27,30,255,256):
            for type_id in (-1,0,1,3,255,256):
                pointer=call(0x4152f0,(matrix,),((UC_X86_REG_EAX,kind&0xffffffff),(UC_X86_REG_ECX,type_id&0xffffffff)))
                expected=matrix+(kind*256+type_id)*16 if 0<kind<256 and 0<type_id<256 else 0
                if pointer!=expected:
                    raise RuntimeError('Original quest-link lookup bounds changed')
        return state['offset'],value,flags
    print(f'# ps_game.exe SHA256 {digest}; ItemQuestData load/lookup, CalcQuestItem 004bf400, PickEnable 004ba240, GetPickItem 004174a0 with actual party/quest/distance/zone removal code')
    print('# A distribution SHA,NPC prefix,consumed,link hash,8 flags; L sparse fixture records,consumed,hash,8 flags; P op checks present tick party itemOwner itemParty expiry itemType defPresent realType questStart defType defID initialError item/user XYZbits activeQuestCount IDs; result,error,bound,indexed,cellCount,zoneCount,eventCount/events')
    data=(ROOT/'.local/server-data/NpcQuest.SData').read_bytes();sha=hashlib.sha256(data).hexdigest()
    npc_row=next(line.split() for line in (ROOT/'tests/data/npcs-oracle.tsv').read_text().splitlines() if line.startswith('A\t'))
    if sha!=npc_row[1]:
        raise RuntimeError('Original NPC prefix evidence does not match server data')
    prefix=int(npc_row[2]);consumed,value,flags=load(data,prefix);print('A',sha,prefix,consumed,value,*flags,sep='\t')
    data=bytearray()
    for kind in range(256):
        for type_id in range(256):
            for group in FIXTURE.get((kind,type_id),([],[])):
                data+=struct.pack('<I',len(group))+struct.pack('<'+'H'*len(group),*group)
    consumed,value,flags=load(bytes(data));print('L',len(FIXTURE),*[v for (kind,type_id),groups in FIXTURE.items() for v in (kind,type_id,*[v for group in groups for v in (len(group),*group)])],consumed,value,*flags,sep='\t')
    # Set up a single native zone/cell list for each independent authorization case.
    rng=random.Random(0x4174a0)
    positions=((0,0,0),(6,9999,8),(10,0,0),(10.000001,0,0),(100,0,100),(-10,0,0),(0,-9999,0),(float('nan'),0,0))
    for case in range(1536):
        op=0 if case<512 else 1;checks=rng.randrange(2);present=rng.randrange(2) if case%5==0 else 1;tick=rng.choice((0,29999,30000,30001,60000,0xfffffff0,0xffffffff));party_id=rng.choice((0,9,77));owner=rng.choice((0,9001,9002));item_party=rng.choice((0,9,77));expiry=rng.choice((0,1,30000,60000,0xfffffff0,0xffffffff))
        item_type=rng.choice((5,26,27));definition=rng.randrange(2);real_type=rng.choice((0,26,27));quest_start=rng.randrange(2);def_type=rng.choice((0,16,27,30,255));def_id=rng.choice((0,1,3,255));initial=rng.choice((-1,0,2,3,4,123456))
        ipos=rng.choice(positions);upos=(0,rng.choice((-10,0,10000)),0);active=[rng.choice((0,2,7,42,65535)) for _ in range(rng.randrange(7))]
        # Concentrate full pickup cases on the linked quest-item branch as well as random gates.
        if case>=1024:
            checks=1;present=1;owner=0;item_type=27;definition=1;real_type=27;quest_start=0;def_type,def_id=rng.choice(((27,1),(27,3),(30,3),(0,1),(255,255)));ipos=rng.choice(positions[:4])
        bits=[struct.unpack('<I',struct.pack('<f',v))[0] for v in (*ipos,*upos)]
        for address in (user,item,zone,cell,roots,info,party):
            cpu.mem_write(address,bytes(0x10000))
        write(user+0xdc,9001);write(user+0xd0,*bits[3:]);write(party+8,party_id)
        if party_id:
            write(user+0x173c,party)
        root=roots+0x100;write(root,root,root);write(user+0x11f4,root,root)
        for i,identity in enumerate(active):
            node=roots+0x1000+i*32;last=words(root)[0];write(node,last,root,identity);write(last+4,node);write(root,node)
        write(zone+0x100,roots+0xe000);write(zone+0x30,zone+0x1000);write(zone+0x1000,cell);write(item+8,*bits[:3],9009);write(item+0x6c,expiry,owner,item_party);cpu.mem_write(item+0x40,bytes([item_type]))
        write(item+0x30,info if definition else 0);cpu.mem_write(info+0x24,bytes((def_type,def_id)));write(info+0xa0,real_type);cpu.mem_write(info+0x9c,bytes([quest_start]));cpu.mem_write(0x541af8,bytes([checks]))
        zone_root,cell_root=roots+0x200,roots+0x300;write(zone_root,zone_root,zone_root);write(cell_root,cell_root,cell_root);write(zone+0x194,zone_root,zone_root);write(cell+0x84,cell_root,cell_root)
        if present:
            write(item+0x18,zone,0,0);write(item,cell_root,cell_root);write(cell_root,item,item);write(item+0x24,zone_root,zone_root,item+8);write(zone_root,item+0x24,item+0x24);write(zone+0x1b4,1);write(cell+0xa4,1)
        state.update(present=bool(present),tick=tick);write(roots+0x400,initial);events.clear()
        result=call(0x4ba240,(9001,party_id),((UC_X86_REG_ESI,item),))&255 if op==0 else int(call(0x4174a0,(zone,user,9009,roots+0x400))!=0)
        print('P',op,checks,present,tick,party_id,owner,item_party,expiry,item_type,definition,real_type,quest_start,def_type,def_id,initial,*bits,len(active),*active,
              result,words(roots+0x400)[0],int(words(item+0x18)[0]!=0),int(state['present']),words(cell+0xa4)[0],words(zone+0x1b4)[0],len(events),*events,sep='\t')


if __name__=='__main__':
    main()
