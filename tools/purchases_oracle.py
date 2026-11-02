#!/usr/bin/env python3
"""Execute complete original point-purchase handlers, queues and SaveBuyPointItem.

Ghidra: work/purchase-replica, work/point-replica, work/billing-replica,
work/purchase-db, work/purchase-db-end. Substitute allocator/OS/SQL/dispatch ports.
"""
import csv
import random
import re
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_original


def main():
    cpu,digest=load_original('ps_dbAgent');stack,user,items,aux,data,db,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000,0x10000000
    for at in (0,stack,user,items,aux,data,db,stop):cpu.mem_map(at,0x10000)
    def word(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def put(at,*values):cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    def string(at):
        result=bytearray()
        while (byte:=cpu.mem_read(at+len(result),1))!=b'\0':result.extend(byte)
        return result.decode('latin1')
    def call(at,end=stop):
        cpu.emu_start(at,end,count=2000000)
        if cpu.reg_read(UC_X86_REG_EIP)!=end:raise RuntimeError(f'Original purchase did not finish at {cpu.reg_read(UC_X86_REG_EIP):x}')
    def sql(q):
        m=re.fullmatch(r'(?:EXEC (\w+) (.*)|\{\?=call (\w+)\((.*)\)\})',q)
        if not m:raise RuntimeError(q)
        proc,values=(m[1],m[2]) if m[1] else (m[3],m[4]);result=[]
        for index,value in enumerate(next(csv.reader([values],delimiter=',',quotechar="'",skipinitialspace=True))):
            textual=index in ((14,16,17) if proc.lower()=='usp_save_char_item_add_e' else (3,4))
            result.append('s'+value.encode('latin1').hex() if textual else 'i'+str(int(value)))
        return proc.lower()+' '+' '.join(result)
    state={};events=[];actions=[];sends=[]
    def port(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x443986:
            size=word(sp+4)
            if size==72:
                valid=state['limit']<0 or state['fresh']<state['limit'];state['fresh']+=1;pointer=items+state['serial']*128 if valid else 0
                if pointer:state['serial']+=1
            else:pointer=aux+state['aux'];state['aux']+=(size+15)&~15
            machine.reg_write(UC_X86_REG_EAX,pointer)
        elif address==0x443c3e:pass
        elif address==0x431a60:actions.append(0 if word(sp+4)==18 else 1 if word(sp+4)==9 else 99)
        elif address==0x402f70:actions.append(2)
        elif address==0x4327a0:sends.append(bytes(machine.mem_read(word(sp+4),word(sp+8))).hex());machine.reg_write(UC_X86_REG_EAX,1)
        elif address==0x43ccc0:
            if not state['active']:
                state['group']+=1;state['mode']=state['modes'][state['group']];state['query_i']=0;pointer=0 if state['mode']==1 else db;state['active']=bool(pointer)
                cpu.mem_write(db+20,b'HYT00\0' if state['mode']==18 else b'00000\0');cpu.mem_write(db+0x1000+20,b'HYT00\0' if state['mode']==19 else b'00000\0');connection='I'
            else:pointer=0 if state['mode']==5 else db+0x1000;connection='P'
            events.append('ALLOC '+connection);machine.reg_write(UC_X86_REG_EAX,pointer)
        elif address==0x43cd80:
            pointer=word(sp+4);events.append('FREE '+('I' if pointer==db else 'P'))
            if pointer==db:state['active']=False
        elif address==0x432d80:events.append('LOG')
        else:
            pointer=machine.reg_read(UC_X86_REG_ECX);connection='I' if pointer==db else 'P';mode=state['mode'];failed=False
            if address==0x43e730:events.append('BEGIN '+connection);failed=mode==2
            elif address==0x43e7f0:events.append('COMMIT '+connection);failed=mode==12
            elif address==0x43e850:events.append('ROLLBACK '+connection);failed=mode in (13,15)
            elif address==0x43f410:
                events.append('BIND '+connection);state['return']=word(sp+20);put(state['return'],1);failed=mode in (6,16)
                if mode==16:cpu.mem_write(pointer+20,b'HYT00\0')
            elif address==0x43f3d0:
                events.append('MORE '+connection);failed=mode in (9,17)
                if mode==17:cpu.mem_write(pointer+20,b'HYT00\0')
            elif address==0x43f350:
                events.append('QUERY '+connection+' '+sql(string(word(sp+4))))
                if connection=='I':state['query_i']+=1;failed=mode in (3,4,13,15) and state['query_i']==1
                else:failed=mode in (7,8);put(state['return'],0xffffffff if mode in (10,18,19) else 0 if mode==11 else 1)
                if failed and mode in (4,8,15):cpu.mem_write(pointer+20,b'HYT00\0')
                elif failed:cpu.mem_write(pointer+20,b'42000\0')
            machine.reg_write(UC_X86_REG_EAX,0xffffffff if failed else 0)
    for address,cleanup in ((0x443986,0),(0x443c3e,0),(0x431a60,4),(0x402f70,0),(0x4327a0,8),(0x43ccc0,0),(0x43cd80,4),(0x432d80,0),(0x43e730,0),(0x43e7f0,0),(0x43e850,0),(0x43f410,28),(0x43f350,4),(0x43f3d0,0),(0x43e6f0,0)):
        cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,port,begin=address,end=address)
    cpu.mem_write(stop+0x100,b'\xc2\x04\x00')
    for at in (0x4600d4,0x4600d8):put(at,stop+0x100)
    sp=stack+0x8000;put(sp,stop);cpu.reg_write(UC_X86_REG_ESP,sp);call(0x445779)
    def item(pointer):
        uid=struct.unpack('<Q',cpu.mem_read(pointer+16,8))[0];ident=word(pointer+24);kind,tid,count=cpu.mem_read(pointer+28,3);quality=struct.unpack('<H',cpu.mem_read(pointer+32,2))[0]
        return [word(pointer+8),word(pointer+12),uid,ident,kind,tid,count,quality,bytes(cpu.mem_read(pointer+34,6)).hex(),bytes(cpu.mem_read(pointer+40,21)).hex(),word(pointer+64),cpu.mem_read(pointer+68,1)[0]]
    def snapshot():
        history=[[word(user+0x80f8+36*i),bytes(cpu.mem_read(user+0x80fc+36*i,21)).hex(),word(user+0x8114+36*i),word(user+0x8118+36*i)] for i in range(10)]
        head=word(user+0x878c);node=word(head);purchases=[];owned=0
        while node!=head:
            root=word(node+16);at=word(root);rows=[]
            while at!=root:rows.append(item(word(at+8)));at=word(at)
            purchases.append([word(node+8),len(rows),*[v for row in rows for v in row]]);owned+=len(rows);node=word(node)
            if len(purchases)>10:raise RuntimeError('Original purchase queue overflow')
        rows=[[i,*item(at)] for i in range(144) if (at:=word(user+0xe0+i*4))]
        print('STATE',*[v for row in history for v in row],len(purchases),*[v for row in purchases for v in row],len(rows),*[v for row in rows for v in row],word(0x470abc),word(0x470ac0),state['serial']-len(rows)-owned-word(0x470ac0))
    def packet():
        sends.clear();put(sp,stop,user);cpu.reg_write(UC_X86_REG_ESP,sp);call(0x40ab50);print('PACKET',sends[-1])
    rng=random.Random(0x41ed10)
    print(f'# ps_dbAgent.exe SHA256 {digest}; 0e02 PacketUserPoint 00406ca0, SaveBuyPointItem 0041ed10, full SaveSuccess 00409560, point-log packet 0040ab50; original CRT/list/pool code')
    for episode in range(32):
        for at in (stack,user,items,aux):cpu.mem_write(at,bytes(0x10000))
        for at in (0x470ab8,0x470b6c):cpu.mem_write(at,bytes(12))
        state.update(limit=-1,fresh=0,serial=0,aux=0,active=False);account=12001+episode;cash=rng.getrandbits(32);character=510001+episode;put(user+0x38,account);put(user+0x60,character);put(user+0x80f4,cash);put(user+0x87d8,db)
        for i,offset in enumerate((0x3b14,0x3b40,0x8704,0x86ac,0x86d8,0x878c)):
            root=user+0x9000+i*0x100;put(user+offset,root);put(root,root,root)
        print('INIT',account,character,cash)
        for step in range(48):
            events.clear();actions.clear();state['fresh']=0
            if step%7==6:
                n=word(user+0x8790);modes=[(episode+i)%20 if episode>=8 else (0 if step%14==6 else episode%20) for i in range(n)];state.update(modes=modes,group=-1,active=False);put(sp,stop,user);cpu.reg_write(UC_X86_REG_ESP,sp);call(0x41ed10);print('SAVE',n,*modes);print('RETURN',cpu.reg_read(UC_X86_REG_EAX));print('\n'.join('EVENT '+event for event in events))
            elif step%13==12:
                put(sp,stop);cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_EAX,user);call(0x409560);print('ACK')
            else:
                live=[i for i in range(144) if word(user+0xe0+4*i)];free=[i for i in range(144) if not word(user+0xe0+4*i)];state['limit']=rng.choice((-1,-1,-1,0,1,2,3));in_game=step%11!=10;put(user+0x87f4,2 if in_game else 1)
                if live and step%4==3:
                    op=0x702;bag,slot=divmod(rng.choice(live),24);payload=bytes((bag,slot,255))
                else:
                    op=0xe02;count=rng.choice((0,1,1,2,3));code=b'X'*21 if step%17==16 else (f'product_{episode}_{step}'.encode()+b'\0'+b'\xab'*21)[:21];made=rng.getrandbits(32);points=rng.getrandbits(32);payload=code+struct.pack('<IIBB',points,made,85,count);chosen=rng.sample(free,min(len(free),count))
                    for i in range(count):
                        position=chosen[i] if len(chosen)>i else rng.randrange(144)
                        if i and step%9==8:position=chosen[0]
                        elif live and step%10==9:position=rng.choice(live)
                        bag,slot=divmod(position,24);craft=rng.choice((b'01020304050607080903X',b'\0'+b'\xfe'*20,b'123\0'+b'\xfe'*17));payload+=struct.pack('<QI5BH',rng.getrandbits(64),rng.getrandbits(32),bag,slot,rng.randrange(256),rng.randrange(256),rng.choice((0,1,255)),rng.randrange(65536))+bytes(rng.randrange(256) for _ in range(6))+craft
                p=struct.pack('<HI',op,account)+payload;cpu.mem_write(data,p)
                if op==0xe02:put(sp,stop,user,data);cpu.reg_write(UC_X86_REG_ESP,sp);call(0x406ca0)
                else:put(sp,stop,data);cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_EAX,user);call(0x408480)
                print('OP',int(in_game),state['limit'],p.hex(),0 if cpu.reg_read(UC_X86_REG_EAX)&255 else 1,len(actions),*actions)
            snapshot();packet();print('END')


if __name__=='__main__':main()
