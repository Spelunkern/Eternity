#!/usr/bin/env python3
"""Execute persistent DB-agent inventory/bank cache mutations and actual deletion journals."""
import argparse
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_original

OPS=(0x701,0x702,0x703,0x704,0x705,0x706,0x707,0x708,0x709,0x70a,0x70b,0x70c,0x70d,0x711,0x715,0x716,0x717)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--status',action='store_true');args=parser.parse_args()
    ops=(0x617,0x618,0x619,0x701,0x704,0x705,0x706,0x707) if args.status else OPS
    cpu,digest=load_original('ps_dbAgent');stack,user,items,packet,deletes,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x10000000
    for address in (stack,user,items,packet,deletes,stop):cpu.mem_map(address,0x10000)
    cpu.mem_write(stop+0x100,b'\xc2\x04\x00')
    for iat in (0x4600d4,0x4600d8):cpu.mem_write(iat,struct.pack('<I',stop+0x100))
    for address in (0x443986,0x40e0d0,0x402f50):cpu.mem_write(address,b'\xc3')
    state={};rng=random.Random(0x408480)
    def word(address):return struct.unpack('<I',cpu.mem_read(address,4))[0]
    def write(address,*values):cpu.mem_write(address,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    def hook(machine,address,size,context):
        if address==0x443986:
            sp=machine.reg_read(UC_X86_REG_ESP)
            if word(sp+4)!=72:raise RuntimeError('Wrong cache allocation size')
            pointer=items+state['serial']*128 if state['allocate'] else 0
            if pointer:state['serial']+=1
            machine.reg_write(UC_X86_REG_EAX,pointer)
        elif address==0x40e0d0:
            pointer=deletes+state['journal_serial']*16;state['journal_serial']+=1;machine.reg_write(UC_X86_REG_EAX,pointer)
        else:state['disconnected']=True
    for address in (0x443986,0x40e0d0,0x402f50):cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
    def cached(pointer):
        uid=struct.unpack('<Q',cpu.mem_read(pointer+0x10,8))[0];ident=word(pointer+0x18);kind,tid,count=cpu.mem_read(pointer+0x1c,3);quality=struct.unpack('<H',cpu.mem_read(pointer+0x20,2))[0]
        return [uid,ident,kind,tid,count,quality,bytes(cpu.mem_read(pointer+0x22,6)).hex(),bytes(cpu.mem_read(pointer+0x28,21)).hex(),word(pointer+0x40),cpu.mem_read(pointer+0x44,1)[0],struct.unpack('<i',cpu.mem_read(pointer+8,4))[0],struct.unpack('<i',cpu.mem_read(pointer+12,4))[0]]
    def snapshot():
        result=[]
        for base,size in ((0xe0,144),(0x320,240)):
            rows=[(i,word(user+base+i*4)) for i in range(size) if word(user+base+i*4)];result.extend((len(rows),*[value for i,pointer in rows for value in (i,*cached(pointer))]))
        for offset in (0x86ac,0x86d8):
            root=word(user+offset);node=word(root+4);values=[]
            while node!=root:
                values.append(struct.unpack('<Q',cpu.mem_read(node+8,8))[0]);node=word(node+4)
                if len(values)>4096:raise RuntimeError('Journal did not terminate')
            result.extend((len(values),*values))
        live={word(user+base+i*4) for base,size in ((0xe0,144),(0x320,240)) for i in range(size)}-{0};free_count=word(0x470ac0)
        result.extend((word(user+0x78),word(0x470abc),free_count,state['serial']-len(live)-free_count))
        return result
    def existing(bank):return [i for i in range(240 if bank else 144) if word(user+(0x320 if bank else 0xe0)+i*4)]
    def pos(bank,prefer=True):
        current=existing(bank);return rng.choice(current) if current and prefer and rng.randrange(5)!=0 else rng.randrange(240 if bank else 144)
    def row_payload(index,bank):
        pointer=items+state['serial']*128;state['serial']+=1;write(user+(0x320 if bank else 0xe0)+index*4,pointer);uid=rng.choice((0,1,0x1122334455667788,rng.getrandbits(64)));ident=rng.getrandbits(32);kind,tid,count=rng.choice((5,16,30)),rng.choice((1,2)),rng.choice((0,1,254,255));quality=rng.randrange(65536);gems=bytes(rng.randrange(256) for _ in range(6));name=rng.choice((b'',b'01020304050607080903',b'\x81\xa1'));raw=(name+b'\0').ljust(21,b'\0');made=rng.getrandbits(32);make=rng.choice((0,85,255));save=rng.choice((-1,0,0,1,2,3))
        cpu.mem_write(pointer+0x10,struct.pack('<QI3BxH',uid,ident,kind,tid,count,quality));cpu.mem_write(pointer+0x22,gems);cpu.mem_write(pointer+0x28,raw);write(pointer+0x40,made);cpu.mem_write(pointer+0x44,bytes([make]));write(pointer+8,save,0)
        return [int(bank),index,*cached(pointer)]
    print(f'# ps_dbAgent.exe SHA256 {digest}; PacketUserItem 00408480 and status 00405b00 owned inventory/bank cases, actual AddDeleteItem 00405200 / AddDeleteItemBank 00405270, CopyString 00408450, Alloc 0040e320 and FreeItem 0040df10; raw allocation/disconnect isolated')
    print('# episode step initial money rowCount rows(bank index UID ID type typeID count quality gems rawCraft21 makeTime makeType saveType savedType); packetHex allocationAllowed; result; inventory and bank rows(index + same 12 fields), two FIFO UID journals, money attempts free stranded')
    for episode in range(32):
        cpu.mem_write(user,bytes(0x10000));cpu.mem_write(items,bytes(0x10000));cpu.mem_write(deletes,bytes(0x10000));cpu.mem_write(stack,bytes(0x10000));cpu.mem_write(0x470ab8,bytes(0x20));state.update(serial=0,journal_serial=0,allocate=True,disconnected=False)
        for index,offset in enumerate((0x86ac,0x86d8)):
            root=user+0x9000+index*32;write(root,root,root);write(user+offset,root)
        money=rng.getrandbits(32);write(user+0x78,money);loaded=[]
        for index in (range(144) if episode<4 else rng.sample(range(144),12)):
            loaded.append(row_payload(index,False))
        for index in rng.sample(range(240),12):loaded.append(row_payload(index,True))
        write(0x470abc,state['serial'])
        for step in range(64):
            op=ops[(episode+step)%len(ops)];amount=lambda:rng.choice((0,1,2,254,255));quality=lambda:rng.randrange(65536);balance=lambda:rng.getrandbits(32);source=pos(False);target=source if step%7==0 else pos(False);bank_source=pos(True);bank_target=bank_source if step%7==0 else pos(True);b,s=divmod(source,24);tb,ts=divmod(target,24)
            if not args.status and episode<4 and step==0:op=0x70e if episode%2==0 else 0x70f
            payload=b''
            if op==0x617:payload=bytes((b,s,amount()))
            elif op==0x618:payload=bytes((bank_source,amount()))
            elif op==0x619:payload=bytes((bank_source,))
            elif op==0x701:
                pointer=word(user+0xe0+target*4);kind,tid=cpu.mem_read(pointer+0x1c,2) if pointer else (5,1)
                if pointer and step%5==0:kind=(kind+1)%256
                payload=struct.pack('<QI5BH',rng.getrandbits(64),rng.getrandbits(32),tb,ts,kind,tid,amount(),quality())+bytes(rng.randrange(256) for _ in range(6))+bytes(rng.randrange(256) for _ in range(21))+struct.pack('<IB',balance(),amount())
            elif op in (0x702,0x707):payload=bytes((b,s,amount()))
            elif op==0x703:payload=bytes((b,s,amount(),tb,ts,amount()))
            elif op==0x704:payload=bytes((b,s,amount(),bank_target,amount()))
            elif op==0x705:payload=bytes((bank_source,amount(),tb,ts,amount()))
            elif op==0x706:payload=bytes((bank_source,amount(),bank_target,amount()))
            elif op==0x708:payload=bytes((b,s,rng.randrange(6),amount()))+struct.pack('<I',balance())
            elif op in (0x709,0x70a):payload=bytes((b,s))+struct.pack('<HI',quality(),balance())
            elif op in (0x70b,0x70d):
                entries=[(rng.randrange(24),quality()) for _ in range(rng.choice((0,1,8,24)))];payload=(struct.pack('<I',balance()) if op==0x70b else b'')+bytes([len(entries)])+b''.join(struct.pack('<BH',*entry) for entry in entries)
            elif op==0x70c:payload=struct.pack('<BH',rng.randrange(24),quality())
            elif op==0x711:payload=bytes((b,s))+bytes(rng.randrange(256) for _ in range(6))+struct.pack('<I',balance())
            elif op==0x715:payload=bytes((b,s,rng.randrange(6)))+struct.pack('<HI',quality(),balance())
            elif op in (0x716,0x717):payload=bytes((b,s))+(struct.pack('<I',balance()) if op==0x716 else b'')+bytes(rng.randrange(256) for _ in range(21))
            data=struct.pack('<HI',op,12345)+payload;cpu.mem_write(packet,data);cpu.mem_write(stack+0x8000,struct.pack('<II',stop,packet));cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,user);state.update(allocate=step%4!=0,disconnected=False)
            if op<0x700:
                cpu.mem_write(stack+0x8000,struct.pack('<I',stop));cpu.reg_write(UC_X86_REG_EAX,packet);cpu.reg_write(UC_X86_REG_ECX,user)
            cpu.emu_start(0x405b00 if op<0x700 else 0x408480,stop,count=100000)
            if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original inventory replica handler did not return')
            result=3 if state['disconnected'] else 0 if cpu.reg_read(UC_X86_REG_EAX)&255 else 1
            print(episode,step,int(step==0),money if step==0 else 0,len(loaded) if step==0 else 0,*[v for entry in loaded for v in entry] if step==0 else [],data.hex(),int(state['allocate']),result,*snapshot(),sep='\t')


if __name__=='__main__':main()
