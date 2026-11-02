#!/usr/bin/env python3
"""Execute original product/stored-point loads, receipts, packets, saves and ACK.

Ghidra: work/billing-replica, work/point-replica, work/character-save.
Only OS locks, allocator, SQL, DB pool, transport and diagnostic ports differ.
"""
import csv
import random
import re
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_original


def main():
    cpu,digest=load_original('ps_dbAgent');stack,user,items,points,data,db,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000,0x10000000
    for at in (0,stack,user,items,points,data,db,stop):cpu.mem_map(at,0x10000)
    def word(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def put(at,*values):cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[x&0xffffffff for x in values]))
    def string(at):
        result=bytearray()
        while (byte:=cpu.mem_read(at+len(result),1))!=b'\0':result.extend(byte)
        return result.decode('latin1')
    def call(at,end=stop):
        cpu.emu_start(at,end,count=1000000)
        if cpu.reg_read(UC_X86_REG_EIP) not in (end,0x424887):raise RuntimeError(f'Original point operation did not finish: {cpu.reg_read(UC_X86_REG_EIP):x}')
    state={};queries=[];sends=[]
    def port(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x443986:
            size=word(sp+4);kind='item' if size==72 else 'point' if size==48 else 'journal' if size==16 else None
            if kind is None:raise RuntimeError('Unexpected original point allocation')
            at=(items if kind=='item' else points if kind=='point' else data+0x1000)+state[kind+'_serial']*128 if state['allocate'] or kind=='journal' else 0
            if at:state[kind+'_serial']+=1
            machine.reg_write(UC_X86_REG_EAX,at)
        elif address==0x402f50:state['disconnect']=True
        elif address==0x43ccc0:machine.reg_write(UC_X86_REG_EAX,db)
        elif address==0x43f390:state['row']+=1;machine.reg_write(UC_X86_REG_EAX,0 if state['row']<len(state['rows']) else 100)
        elif address==0x43e8b0:
            col,size,target=word(sp+4),word(sp+8),word(sp+16);value=state['rows'][state['row']][col-1];machine.mem_write(target,value.to_bytes(size,'little'));machine.reg_write(UC_X86_REG_EAX,0)
        elif address==0x43f410:state['return']=word(sp+20);put(state['return'],1);machine.reg_write(UC_X86_REG_EAX,0)
        elif address==0x43f350:
            queries.append(string(word(sp+4)));failed=len(queries)-1==state['failure'];put(state.get('return',db+0x100),0xffffffff if failed and state['negative'] else 1);machine.reg_write(UC_X86_REG_EAX,0xffffffff if failed and not state['negative'] else 0)
        elif address==0x4327a0:sends.append(bytes(machine.mem_read(word(sp+4),word(sp+8))).hex());machine.reg_write(UC_X86_REG_EAX,1)
        else:machine.reg_write(UC_X86_REG_EAX,0)
    for address,cleanup in ((0x443986,0),(0x402f50,0),(0x43ccc0,0),(0x43cd80,4),(0x43e6f0,0),(0x43f410,28),(0x43f350,4),(0x43f390,0),(0x43e8b0,20),(0x43f3d0,0),(0x4327a0,8),(0x432d80,0)):
        cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,port,begin=address,end=address)
    cpu.mem_write(stop+0x100,b'\xc2\x04\x00')
    for at in (0x4600d4,0x4600d8):put(at,stop+0x100)
    sp=stack+0x8000;put(sp,stop);cpu.reg_write(UC_X86_REG_ESP,sp);call(0x445779)
    def receipt(at,product):
        bag,slot=cpu.mem_read(at+(7 if product else 12),2)
        uid=struct.unpack('<Q',cpu.mem_read(at+16,8))[0];ident=word(at+24);kind,tid,count=cpu.mem_read(at+28,3);quality=struct.unpack('<H',cpu.mem_read(at+32,2))[0];made=word(at+36);make=cpu.mem_read(at+40,1)[0]
        return [bag,slot,uid,ident,kind,tid,count,quality,made,make]
    def snapshot():
        product=[]
        for i in range(240):
            at=user+0x6e0+i*48
            if any(cpu.mem_read(at,48)):product.append([i,word(at),*cpu.mem_read(at+4,3),*receipt(at,True)])
        stored=[[i,word(at+8),*receipt(at,False)] for i in range(240) if (at:=word(user+0x8294+4*i))]
        inventory=[]
        for i in range(144):
            if not (at:=word(user+0xe0+4*i)):continue
            uid=struct.unpack('<Q',cpu.mem_read(at+16,8))[0];ident=word(at+24);kind,tid,count=cpu.mem_read(at+28,3);quality=struct.unpack('<H',cpu.mem_read(at+32,2))[0]
            inventory.append([i,word(at+8),word(at+12),uid,ident,kind,tid,count,quality,bytes(cpu.mem_read(at+34,6)).hex(),bytes(cpu.mem_read(at+40,21)).hex(),word(at+64),cpu.mem_read(at+68,1)[0]])
        print('STATE',len(product),*[v for r in product for v in r],len(stored),*[v for r in stored for v in r],len(inventory),*[v for r in inventory for v in r],word(0x470abc),word(0x470ac0),word(0x470b4c),word(0x470b50))
    def load(rows,product):
        state.update(rows=rows,row=-1,failure=-1,negative=False);queries.clear();put(sp,stop,user);cpu.reg_write(UC_X86_REG_ESP,sp)
        if product:call(0x41ead0)
        else:cpu.reg_write(UC_X86_REG_EBX,user);cpu.reg_write(UC_X86_REG_EBP,db);call(0x42293a,0x422a97)
        print('PRODUCTS' if product else 'POINTS',int(state['allocate']),len(rows),*[v for r in rows for v in r])
    def save(failure,negative=False):
        state.update(failure=failure,negative=negative);queries.clear();cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_EBP,user);put(sp+16,db)
        # Stop on the original fatal-save branch, before unrelated rollback/file I/O.
        call(0x423483,0x42366d)
        result=[]
        for q in queries:
            match=re.fullmatch(r'\{\?=call (\w+)\((.*)\)\}',q)
            if not match:raise RuntimeError(q)
            fields=next(csv.reader([match[2]],delimiter=',',quotechar="'",skipinitialspace=True));values=[]
            for value in fields:
                try:values.append('i'+str(int(value)))
                except ValueError:values.append('s'+value.encode('latin1').hex())
            result.append(match[1].lower()+' '+' '.join(values))
        return result,cpu.reg_read(UC_X86_REG_EIP)==0x424887
    cpu.hook_add(UC_HOOK_CODE,lambda m,a,s,c:m.emu_stop(),begin=0x424887,end=0x424887)
    print(f'# ps_dbAgent.exe SHA256 {digest}; product load 0041ead0, point load 0042293a..00422a97; transfers 0710/00408480 and 0e05/00406ca0; save 00423483..0042366d, full SaveSuccess 00409560; original CRT')
    rng=random.Random(0x423483)
    for episode in range(16):
        for at in (stack,user,items,points):cpu.mem_write(at,bytes(0x10000))
        for at in (0x470ab8,0x470b48,0x470b6c):cpu.mem_write(at,bytes(12))
        state.update(item_serial=0,point_serial=0,journal_serial=0,allocate=True,disconnect=False,return_value=0)
        account=12345+episode;server=rng.randrange(256);character=500001+episode;put(user+0x38,account,server);put(user+0x60,character);put(user+0x87d8,db)
        for i,offset in enumerate((0x3b14,0x3b40,0x8704,0x86ac,0x86d8)):
            root=user+0x9000+i*0x100;put(user+offset,root);put(root,root,root)
        print('INIT',account,server,character)
        for product in (True,False):load([(239,0xffffffff,0),(239,25001,2),(0,16001,255),(240,5001,1),(255,5001,1)]+[(i,rng.choice((5001,16001,30002,0x80000000,rng.getrandbits(32))),rng.randrange(256)) for i in range(1,9)],product)
        for step in range(64):
            state.update(allocate=step%5!=0,disconnect=False)
            if step%13==12:
                put(sp,stop);cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_EAX,user);call(0x409560);print('ACK')
            elif step%11==10:
                rows=[(rng.choice((0,1,2,3,4,5,6,7,8,239,240,255)),rng.getrandbits(32),rng.choice((0,1,255))) for _ in range(8)];load(rows,step%2==0)
            else:
                live=[i for i in range(144) if word(user+0xe0+4*i)];stored=[i for i in range(240) if word(user+0x8294+4*i)];op=0x702 if step%4==3 and live else 0xe05 if step%2 and stored else 0x710
                target=rng.choice(live) if live and step%9==0 else rng.randrange(144);bag,slot=divmod(target,24)
                if op==0x702:
                    index=rng.choice(live);bag,slot=divmod(index,24);payload=bytes((bag,slot,255))
                else:
                    source=rng.choice(stored) if op==0xe05 else rng.choice((0,1,2,3,4,5,6,7,8,239));stamp=rng.getrandbits(32);make=rng.choice((77,85,83));payload=bytes((source,bag,slot))+struct.pack('<QI3BH',rng.getrandbits(64),rng.getrandbits(32),rng.randrange(256),rng.randrange(256),rng.choice((0,1,255)),rng.randrange(65536))+bytes(rng.randrange(256) for _ in range(6))+bytes(rng.randrange(256) for _ in range(21))+struct.pack('<IB',stamp,make)
                p=struct.pack('<HI',op,account)+payload;cpu.mem_write(data,p)
                if op==0xe05:put(sp,stop,user,data);cpu.reg_write(UC_X86_REG_ESP,sp);call(0x406ca0)
                else:put(sp,stop,data);cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_EAX,user);call(0x408480)
                print('OP',int(state['allocate']),p.hex(),3 if state['disconnect'] else 0 if cpu.reg_read(UC_X86_REG_EAX)&255 else 1)
            snapshot();sends.clear()
            for address,reg in ((0x409a60,UC_X86_REG_ESI),(0x40ac20,UC_X86_REG_EDI)):
                put(sp,stop);cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(reg,user);before=len(sends);call(address);print('PACKET',sends[-1] if len(sends)>before else '-')
            plan,_=save(-1);failure=rng.choice((-1,rng.randrange(len(plan)))) if plan else -1;negative=bool(step%2);print('SAVE',failure,int(negative))
            for q in plan:print('PLAN',q)
            attempts,fatal=save(failure,negative)
            for q in attempts:print('ATTEMPT',q)
            print('RESULT',int(fatal));print('END')


if __name__=='__main__':main()
