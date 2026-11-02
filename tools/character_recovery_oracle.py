#!/usr/bin/env python3
"""Execute original complete character file recovery, its statements and mutations.

Ghidra: work/save-owner, work/recovery-log and preceding cache exports. Only raw
allocator, OS locks/time, transport and SLog output are substituted; CRT executes.
"""
import csv
import random
import re
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_original


def main():
    cpu,digest=load_original('ps_dbAgent');stack,user,items,aux,packet,log,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000,0x10000000
    for at in (0,stack,user,items,aux,packet,log,stop):cpu.mem_map(at,0x10000)
    def word(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def put(at,*values):cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[x&0xffffffff for x in values]))
    def string(at):
        result=bytearray()
        while (byte:=cpu.mem_read(at+len(result),1))!=b'\0':result.extend(byte)
        return result.decode('latin1')
    def call(at,end=stop):
        cpu.emu_start(at,end,count=3000000)
        if cpu.reg_read(UC_X86_REG_EIP)!=end:raise RuntimeError(f'Original recovery did not finish at {cpu.reg_read(UC_X86_REG_EIP):x}')
    state={};output=[]
    def raw(size):
        if size==72:at=items+state['item']*128;state['item']+=1;return at
        at=aux+state['aux'];state['aux']+=(size+15)&~15;return at
    def port(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x443986:machine.reg_write(UC_X86_REG_EAX,raw(word(sp+4)))
        elif address==0x42aa30:machine.mem_write(word(sp+4),b'2024-02-29 23:59:58\0')
        elif address==stop+0x120:machine.mem_write(word(sp+4),struct.pack('<8H',2024,2,4,29,23,59,58,0))
        elif address==0x432f30:
            fmt=string(word(sp+8));args=sp+12;result='';last=0
            for match in re.finditer(r'%[sud]',fmt):
                result+=fmt[last:match.start()];value=word(args);args+=4;result+=string(value) if match[0]=='%s' else str(value if match[0]=='%u' or value<0x80000000 else value-0x100000000);last=match.end()
            result+=fmt[last:];output.append(result);machine.reg_write(UC_X86_REG_EAX,state['io'])
        else:machine.reg_write(UC_X86_REG_EAX,1)
    for at,cleanup in ((0x443986,0),(0x42aa30,0),(0x432f30,0),(0x4327a0,8),(stop+0x120,4)):
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,port,begin=at,end=at)
    cpu.mem_write(stop+0x100,b'\xc2\x04\x00');cpu.mem_write(stop+0x140,b'\xb8\x7b\x00\x00\x00\xc3')
    for at in (0x4600d4,0x4600d8):put(at,stop+0x100)
    put(0x4600dc,stop+0x140);put(0x4600f4,stop+0x120)
    sp=stack+0x8000;put(sp,stop);cpu.reg_write(UC_X86_REG_ESP,sp);call(0x445779)
    def link(offset,nodes):
        root=word(user+offset);put(root,nodes[-1] if nodes else root,nodes[0] if nodes else root)
        for i,at in enumerate(nodes):put(at,nodes[i-1] if i else root,nodes[i+1] if i+1<len(nodes) else root)
        put(user+offset+32,len(nodes))
    def nodes(offset):
        root=word(user+offset);node=word(root+4);result=[]
        while node!=root:
            result.append(node);node=word(node+4)
            if len(result)>1024:raise RuntimeError('Original recovery list loop')
        return result
    def op(code,payload):
        p=struct.pack('<HI',code,state['account'])+payload;cpu.mem_write(packet,p);put(sp,stop,packet);cpu.reg_write(UC_X86_REG_ESP,sp)
        if code==0xe05:put(sp,stop,user,packet);call(0x406ca0)
        elif code<0x700:put(sp,stop);cpu.reg_write(UC_X86_REG_EAX,packet);cpu.reg_write(UC_X86_REG_ECX,user);call(0x405b00 if code<0x700 else 0x406c20)
        else:
            cpu.reg_write(UC_X86_REG_EAX,user);cpu.reg_write(UC_X86_REG_EBX,user);call(0x406100 if code<0x900 and code>=0x800 else 0x406590 if 0xa00<=code<0xb00 else 0x406c20 if 0xd00<=code<0xe00 else 0x408480)
        print('OP',p.hex())
    def record(q):
        if q.startswith('--'):return 'META '+q.encode('latin1').hex()
        match=re.fullmatch(r'\{\?=call (\w+)\((.*)\)\}|EXEC (\w+) (.*)',q)
        if not match:raise RuntimeError(q)
        proc,values=(match[1],match[2]) if match[1] else (match[3],match[4]);name=proc.lower();fields=next(csv.reader([values],delimiter=',',quotechar="'",skipinitialspace=True));encoded=[]
        text={'usp_save_char_product_item_e':(12,13),'usp_save_user_storedpointitems':(16,18,19),'usp_save_char_item_add_e':(14,16,17),'usp_save_user_storeditem_add_e':(14,16,17),'usp_save_char_item_mod_e':(11,),'usp_save_user_storeditem_mod_e':(12,),'usp_save_char_item_del_e':(1,),'usp_save_user_storeditem_del_e':(2,)}.get(name,())
        for i,value in enumerate(fields):
            coordinate=name=='usp_save_char_info_e' and 17<=i<=19 or name in ('usp_create_char_savepoint','usp_save_char_savepoint') and i>=3
            encoded.append(('n' if coordinate else 's')+value.encode('latin1').hex() if coordinate or i in text else 'i'+str(int(value)))
        return 'QUERY '+str(int(bool(match[1])))+' '+name+' '+' '.join(encoded)
    def snapshot():
        result=[cpu.mem_read(user+0x86a4,1)[0],cpu.mem_read(user+0x66,1)[0]]
        for base,count in ((0xe0,144),(0x320,240)):
            entries=[(i,word(word(user+base+4*i)+8)) for i in range(count) if word(user+base+4*i)];result.extend((len(entries),*[v for row in entries for v in row]))
        result.extend((len(nodes(0x86ac)),len(nodes(0x86d8)),word(user+0x86a8),cpu.mem_read(user+0x86a5,1)[0]))
        for offset in (0x3b14,0x3b40):
            current=nodes(offset);result.append(len(current))
            for at in current:result.extend((word(at+8),struct.unpack('<H',cpu.mem_read(at+12,2))[0]))
        deleted=nodes(0x8704);result.extend((len(deleted),*[struct.unpack('<H',cpu.mem_read(at+8,2))[0] for at in deleted]))
        result.extend(word(user+0x80a4+i*20) for i in range(4));result.append(word(user+0x6e0+239*48));point=word(user+0x8294+239*4);result.extend((int(bool(point)),word(point+8) if point else 0,word(user+0x8674)))
        print('STATE',*result)
    print(f'# ps_dbAgent.exe SHA256 {digest}; complete SaveToFile 00424ce0, max-grow helper 00425ee0; raw allocator/time/OS/SLog ports only; original CRT and all cache mutations')
    rng=random.Random(0x424ce0)
    for case in range(32):
        for at in (stack,user,items,aux):cpu.mem_write(at,bytes(0x10000))
        for at in (0x470ab8,0x470adc,0x470b00,0x470b48,0x470b6c,0x470b90):cpu.mem_write(at,bytes(12))
        state.update(item=0,aux=0,io=1,account=13001+case);account=state['account'];realm=rng.randrange(256);put(user+0x38,account,realm);cpu.mem_write(user+0x40,b'recovery_user\0');cpu.mem_write(user+0xc8,b'RecoveryHero\0');cpu.mem_write(user+0x60,bytes(rng.randrange(256) for _ in range(98)));put(user+0x60,520001+case);cpu.mem_write(user+0x66,bytes([case%4]));cpu.mem_write(user+0x68,bytes([case%3]));cpu.mem_write(user+0x6e,struct.pack('<H',rng.choice((0,29,30,65535))));cpu.mem_write(user+0x9c,struct.pack('<fff',1.0000001192092896,-0.0,-39.1234567));cpu.mem_write(user+0x86a4,b'\0\0')
        for i,offset in enumerate((0x33e0,0x3b14,0x3b40,0x8704,0x86ac,0x86d8)):
            root=user+0x9000+i*0x100;put(user+offset,root);put(root,root,root)
        for i in range(4):cpu.mem_write(user+0x80a8+i*20,b'\xff\xff')
        print('INIT',account,realm,bytes(cpu.mem_read(user+0x60,98)).hex())
        deletion=(0,1,39,40,41,81)[case%6]
        for bank in (False,True):
            for i in range(deletion+8):
                index=i if bank else 24+i;at=raw(72);put(user+(0x320 if bank else 0xe0)+index*4,at);uid=rng.choice((0,1,rng.getrandbits(64)));ident=rng.getrandbits(32);kind,tid,count=rng.randrange(256),rng.randrange(256),rng.randrange(256);quality=rng.randrange(65536);gems=bytes(rng.randrange(256) for _ in range(6));craft=rng.choice(('', '01020304050607080903','123'));made=rng.getrandbits(32);make=85;flag=0 if i<deletion else rng.choice((-1,0,1,2,3))
                put(at+8,flag,0);cpu.mem_write(at+16,struct.pack('<QI3BxH',uid,ident,kind,tid,count,quality));cpu.mem_write(at+34,gems);cpu.mem_write(at+40,craft.encode()+b'\0');put(at+64,made);cpu.mem_write(at+68,bytes([make]));print('ITEM',int(bank),index,uid,ident,kind,tid,count,quality,gems.hex(),craft or '-',made,make,flag)
            for i in range(deletion):op(0x619,bytes([i])) if bank else op(0x702,bytes(divmod(24+i,24))+b'\xff')
        learned=[];effects=[]
        for i in range(4):
            at=raw(16);ident,level,delay=rng.randrange(65536),rng.randrange(256),rng.randrange(65536);cpu.mem_write(at+8,struct.pack('<HBBI',ident,level,i,delay));put(user+0x3410+4*i,at);learned.extend((ident,level,i,delay))
        put(user+0x340c,4);put(user+0x86a8,4)
        for i in range(4):
            at=raw(16);ident,level,delay=rng.randrange(65536),rng.randrange(256),rng.choice((0,1,0xffffffff,rng.getrandbits(32)));cpu.mem_write(at+8,struct.pack('<HBBI',ident,level,0,delay));effects.append(at)
        link(0x33e0,effects);print('SKILLS',4,*learned,4,*[v for at in effects for v in (struct.unpack('<H',cpu.mem_read(at+8,2))[0],cpu.mem_read(at+10,1)[0],word(at+12))])
        if case%4==3:
            points=rng.randrange(65536);cpu.mem_write(user+0x8080,struct.pack('<H',points));put(sp,stop);cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_ESI,user);call(0x404c40);print('RESET',points)
        op(0x801,struct.pack('<HHB',44,32768,1))
        for finished in (False,True):
            rows=[]
            for i in range(3):
                at=raw(24);ident=20+i+(10 if finished else 0);delay=rng.randrange(65536);success=rng.randrange(256) if finished else 0;counters=bytes(rng.randrange(256) for _ in range(3));put(at+8,0 if finished else 2);cpu.mem_write(at+12,struct.pack('<HBxH',ident,success,delay)+counters);rows.append(at);print('QUEST',int(finished),ident,delay,success,*counters)
            link(0x3b40 if finished else 0x3b14,rows)
        op(0xa02,struct.pack('<HB',20,2));op(0xa01,struct.pack('<HH',21,65535));op(0xa02,struct.pack('<HB',22,1))
        op(0x609,bytes([2])+struct.pack('<BBBHBBBH',255,0,1,65535,0,255,5,32768))
        for i in range(4):
            payload=struct.pack('<BHIII',i,65535 if case%5==i else i+10,0x3c000000,0x80000000,0xbd200000);p=struct.pack('<HI',0xd02,account)+payload;cpu.mem_write(packet,p);put(sp,stop,packet);cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_ESI,packet);cpu.reg_write(UC_X86_REG_EDI,user);call(0x406c20);print('OP',p.hex())
        op(0x615,bytes((1,2,3,1)));op(0x61a,bytes([2])+struct.pack('<II',0,0xffffffff))
        cpu.mem_write(user+0x6e0+239*48+4,bytes([5,1,2]));print('PRODUCT',239,5001,2)
        point=raw(48);put(point+8,0);cpu.mem_write(point+28,bytes([16,1,0]));put(user+0x8294+239*4,point);print('POINT',239,16001,0)
        for code,slot in ((0x710,0),(0xe05,1)):
            payload=bytes((239,0,slot))+struct.pack('<QI3BH',rng.getrandbits(64),5001,5,1,3,100)+bytes(range(1,7))+b'01020304050607080903\0'+struct.pack('<IB',0x60442001,85);op(code,payload)
        for attempt in range(2):
            state['io']=attempt%2;output.clear();put(sp,stop,user);cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_ECX,log);call(0x424ce0);print('SAVE',state['io'],cpu.reg_read(UC_X86_REG_EAX));
            for q in output:print(record(q))
            snapshot();print('END')


if __name__=='__main__':main()
