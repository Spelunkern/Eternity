#!/usr/bin/env python3
"""Execute entire original PacketParty normal paths, retained sequences and nested accept."""
import struct
from original_oracle import load_game
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP


def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x100000);cpu.mem_map(0,0x10000);cpu.mem_map(0x31000000,0x10000)
    manager=0x547688;parties=[base+0x1000+i*0x1000 for i in range(4)];users=[base+0x10000+i*0x6000 for i in range(8)]
    packet,stack,stop=base+0x70000,base+0xf0000,base+0xff000
    init,destroy,lock,unlock,increment,tick=[stop+n for n in (16,32,48,64,80,96)]
    ctx={}
    def put(at,v):cpu.mem_write(at,struct.pack('<I',v&0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def pid(at):return parties.index(at)+1 if at in parties else 0
    def uid(at):return users.index(at)+100 if at in users else 0
    def zone(at):return 1 if at==base+0x60000 else 2 if at==base+0x61000 else 0
    def entries():
        head=get(manager+0x44)
        if not head:return []
        def walk(n):return [] if not n or n==head else walk(get(n))+[(get(n+12),pid(get(n+16)))]+walk(get(n+8))
        return walk(get(head+4))
    def state():
        values=[get(manager),get(manager+8),get(manager+12)];pairs=entries();values+=[len(pairs)]+[v for pair in pairs for v in pair]
        for p in parties:
            values += [get(p+n) for n in (8,12,16,0x104,0x108,0x10c,0x110)]+[cpu.mem_read(p+0x114,1)[0]]
            values += [get(p+0x14+i*8) for i in range(30)]+[uid(get(p+0x18+i*8)) for i in range(30)]
        b=struct.pack('<'+'I'*len(values),*values)
        for u in users:
            values=[get(u+0xdc),pid(get(u+0x173c)),get(u+0x1740),get(u+0x5834),get(u+0x15e0),cpu.mem_read(u+0x12d,1)[0],get(u+0x5758),zone(get(u+0xe0)),struct.unpack('<H',cpu.mem_read(u+0x164,2))[0],cpu.mem_read(u+0x1744,1)[0]]
            b+=struct.pack('<10I',*values)+bytes(cpu.mem_read(u+0x188,21))
        return b
    def event(tag,payload=b''):
        if not ctx.get('record'):return
        h=14695981039346656037
        for v in state():h=((h^v)*1099511628211)&0xffffffffffffffff
        ctx['events']+=tag.encode()+struct.pack('<QI',h,len(payload))+payload
    def lock_id(at):
        if at==manager+0x10:return 0,0
        if at==manager+0x28:return 1,0
        for i,p in enumerate(parties):
            if at==p+0x118:return 2,i+1
        for i,u in enumerate(users):
            if at==u+0xa9c:return 3,i+100
        raise RuntimeError(f'Unknown lock {at:x}')
    ports={init:8,destroy:4,lock:4,unlock:4,increment:4,tick:0,0x503a3f:0,0x503ba1:0,0x415f30:4,0x40d840:4,0x4455d0:0,0x4d4d20:8,0x4208c0:12,0x45b0d0:0,0x4d4e50:8,0x505316:0,0x445a30:4,0x446510:4,0x447120:4,0x4d43a0:8}
    for at,n in ports.items():cpu.mem_write(at,b'\xc2'+struct.pack('<H',n))
    for at,target in ((0x5210c8,init),(0x5210d8,destroy),(0x5210cc,lock),(0x5210d0,unlock),(0x5210f8,increment),(0x5210e4,tick)):put(at,target)
    for i,u in enumerate(users):
        code=0x31000000+i*0x100;nested_packet=0x31004000+i*0x100
        cpu.mem_write(nested_packet,b'\x03\x0b\x00')
        prefix=b'\x9c\x60\x68'+struct.pack('<I',u)+b'\xb9'+struct.pack('<I',nested_packet)
        cpu.mem_write(code,prefix+b'\xe8'+struct.pack('<I',(0x4676c0-(code+len(prefix)+5))&0xffffffff)+b'\x61\x9d\xc2\x08\x00')
    def lookup_mutation():
        if not ctx.get('record') or ctx['mutated']:return
        root=users[ctx['root']]
        if ctx['mutation']==1:put(root+0x173c,0);ctx['mutated']=True
        elif ctx['mutation']==3:put(root+0x1740,900);put(root+0x5834,123);ctx['mutated']=True
    def hook(machine,at,size,data):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if at in (init,destroy,lock,unlock):
            k,i=lock_id(get(sp+4));event({init:'I',destroy:'D',lock:'[',unlock:']'}[at],struct.pack('<2I',k,i))
        elif at==increment:
            p=get(sp+4);n=(get(p)+1)&0xffffffff;put(p,n);machine.reg_write(UC_X86_REG_EAX,n)
        elif at==0x503a3f:
            n=get(sp+4)
            if n==304:
                event('O')
                if ctx['alloc_fail'] or ctx['raw']>=4:p=0
                else:p=parties[ctx['raw']];ctx['raw']+=1
            else:p=base+0xc0000+ctx['nodes']*0x100;ctx['nodes']+=1
            machine.reg_write(UC_X86_REG_EAX,p)
        elif at==tick:
            n=(ctx['clock']+ctx['ticks']*17)&0xffffffff;ctx['ticks']+=1;event('K',struct.pack('<I',n))
            if ctx.get('record') and ctx['mutation']==2 and not ctx['mutated']:
                u=users[ctx['root']];put(u+0xdc,get(u+0xdc)+1000);cpu.mem_write(u+0x188,b'tick-name-0000000000\0');ctx['mutated']=True
            machine.reg_write(UC_X86_REG_EAX,n)
        elif at==0x415f30:
            z=zone(machine.reg_read(UC_X86_REG_EAX));requested=get(sp+4);event('Z',struct.pack('<2I',z,requested));lookup_mutation();i=requested-100
            p=users[i] if not ctx['missing'] and 0<=i<8 and zone(get(users[i]+0xe0))==z else 0
            machine.reg_write(UC_X86_REG_EAX,p)
        elif at==0x40d840:
            p=machine.reg_read(UC_X86_REG_EAX);name=bytearray()
            while cpu.mem_read(p,1)!=b'\0':name+=cpu.mem_read(p,1);p+=1
            event('N',bytes(name));lookup_mutation();found=0
            if not ctx['missing']:
                for u in users:
                    if bytes(cpu.mem_read(u+0x188,21)).split(b'\0')[0]==bytes(name):found=u;break
            machine.reg_write(UC_X86_REG_EAX,found)
        elif at==0x4455d0:event('P',bytes(machine.mem_read(machine.reg_read(UC_X86_REG_ECX),machine.reg_read(UC_X86_REG_EAX))))
        elif at==0x4d4d20:
            u=machine.reg_read(UC_X86_REG_ECX);payload=bytes(machine.mem_read(get(sp+4),get(sp+8)));event('U',struct.pack('<I',uid(u))+payload)
            if ctx.get('record') and ctx['mutation']==4 and not ctx['nested'] and payload[:2] in (b'\x02\x0b',b'\x09\x0b'):
                ctx['nested']=True;event('V',struct.pack('<I',uid(u)));machine.reg_write(UC_X86_REG_EIP,0x31000000+users.index(u)*0x100)
        elif at==0x4208c0:event('S',bytes(machine.mem_read(get(sp+8),get(sp+12))))
        elif at==0x45b0d0:
            i=uid(machine.reg_read(UC_X86_REG_EDI));event('F',struct.pack('<I',i));cpu.mem_write(machine.reg_read(UC_X86_REG_ESI),bytes((i+j)&255 for j in range(74)))
        elif at==0x4d4e50:event('L',bytes(machine.mem_read(get(sp+4),get(sp+8))))
        elif at==0x445a30:
            event('R',struct.pack('<2I',pid(machine.reg_read(UC_X86_REG_ECX)),uid(get(sp+4))));machine.reg_write(UC_X86_REG_EAX,ctx['raid_success'])
        elif at==0x446510:event('r',struct.pack('<2I',pid(machine.reg_read(UC_X86_REG_ECX)),uid(get(sp+4))))
        elif at==0x447120:event('X',struct.pack('<2I',pid(get(sp+4)),uid(machine.reg_read(UC_X86_REG_ECX))))
        elif at==0x4d43a0:event('C',struct.pack('<2I',get(sp+4),get(sp+8)))
    cpu.hook_add(UC_HOOK_CODE,hook)
    def call(at,args=(),ecx=0,esi=0):
        put(stack,stop)
        for i,v in enumerate(args):put(stack+4+i*4,v)
        for r,v in ((UC_X86_REG_ESP,stack),(UC_X86_REG_ECX,ecx),(UC_X86_REG_ESI,esi)):cpu.reg_write(r,v)
        cpu.emu_start(at,stop,count=1000000)
        assert cpu.reg_read(UC_X86_REG_EIP)==stop, "Original request did not return"
    print(f'# ps_game.exe SHA256 {digest}; entire PacketParty 004676c0 normal request paths; actual lock/leader/find/change-leader, registry create/map, normal entry/leave, experience and GetPartyType bodies. Ports: lookup, OS, raw memory, transport/log, missing ExpelUser and raid enter/leave. V is literal nested PacketParty accept via saved-register trampoline, not a modeled join.')
    print('# S case profile flags mutation missing alloc_fail raid_success clock; T actor packet_hex; R final_packet_hex state_hex events_hex; Z')
    opcodes=(0xb02,0xb03,0xb05,0xb0d,0xb09,0xb07,0xb06,0xb0c,0xb08)
    for case in range(1024):
        cpu.mem_write(base,bytes(0xef000));cpu.mem_write(manager,bytes(84));cpu.mem_write(0,bytes(0x10000))
        profile=(case//9)%9;flags=(0,1,2,4,8,16,32,64,128,256)[(case//81)%10];mutation=(case//29)%5
        missing=(case//17)%11==0;alloc_fail=(case//23)%13==0;raid_success=(case//31)%2;clock=(0,0xfffffff0,0x7fffffff,2000)[case%4]
        if mutation==4 and case%9 in (0,4):profile=(0,1,4)[(case//9)%3];flags=0;missing=False;alloc_fail=False
        ctx.update(record=False,events=b'',raw=2,nodes=0,alloc_fail=alloc_fail,clock=clock,ticks=0,mutation=mutation,mutated=False,nested=False,missing=missing,raid_success=raid_success,root=0)
        for i,u in enumerate(users):
            put(u+0xdc,100+i);cpu.mem_write(u+0x136,struct.pack('<H',10+i));cpu.mem_write(u+0x134,bytes([i%6]));cpu.mem_write(u+0x188,f'user-{i:015}'.encode()+b'\0');cpu.mem_write(u+0x1744,b'\x09');put(u+0xe0,base+0x60000);put(u+0x5758,4)
            for off,v in zip((0x17c,0x1238,0x184,0x1240,0x180,0x123c,0x164),(100+i,200+i,300+i,400+i,500+i,600+i,1)):cpu.mem_write(u+off,struct.pack('<H',v))
            cpu.mem_write(u+0xd0,struct.pack('<3f',1.25+i,-2.5,3.75));head=base+0x80000+i*0x1000;put(u+0xa94,head);put(head+4,head)
        call(0x4473f0,(manager,))
        for i in range(2):call(0x444c60,esi=parties[i]);put(parties[i]+8,10+i*10)
        members=[]
        if profile in (1,2,5,6):members=[0,2]
        elif profile==3:members=list(range(7))
        elif profile==4:members=[0,2,3,4,5,6]
        elif profile==7:members=[0,1]
        if members or profile==8:
            p=parties[0];put(p+0x10,len(members));put(p+0xc,1 if profile==2 else 0);put(p+0x10c,60);cpu.mem_write(p+0x114,bytes([int(profile==5)]))
            for j,i in enumerate(members):put(p+0x18+j*8,users[i]);put(users[i]+0x173c,p)
            if profile==8:put(users[0]+0x173c,p)
            call(0x447780,(p+8,p),esi=manager+0x28)
        if profile==6:
            p=parties[1];put(p+0x10,1);put(p+0x18,users[1]);put(users[1]+0x173c,p);call(0x447780,(p+8,p),esi=manager+0x28)
        if flags&1:put(users[0]+0x15e0,1)
        if flags&2:put(users[1]+0x15e0,2)
        if flags&4:cpu.mem_write(users[1]+0x12d,b'\x01')
        if flags&8:put(users[1]+0x5758,0)
        if flags&16:put(users[1]+0x1740,900)
        if flags&128:put(users[1]+0xe0,base+0x61000)
        print('S',case,profile,flags,mutation,int(missing),int(alloc_fail),raid_success,clock)
        op=opcodes[case%len(opcodes)];root=1 if op==0xb03 else 0
        if op==0xb03:
            put(users[1]+0x1740,0 if flags&32 else 100);put(users[1]+0x5834,0xfffffff0)
            data=struct.pack('<HB',op,int(bool(flags&64)))
        elif op==0xb09:data=struct.pack('<H',op)+f'user-{0 if flags&256 else 1:015}'.encode()+b'X'
        elif op in (0xb02,0xb06,0xb07):data=struct.pack('<HI',op,(100 if flags&256 else 101) if op==0xb02 else 102)
        else:data=struct.pack('<H',op)
        ctx.update(record=True,events=b'',root=root,mutated=False,nested=False)
        cpu.mem_write(packet,data);call(0x4676c0,(users[root],),ecx=packet)
        print('T',root,data.hex());print('R',bytes(cpu.mem_read(packet,len(data))).hex(),state().hex(),ctx['events'].hex() or '-')
        if ctx['nested']:
            assert get(users[1]+0x173c)!=0, 'Nested accept did not actually join'
            data=struct.pack('<H',0xb05);ctx.update(events=b'',root=1)
            cpu.mem_write(packet,data);call(0x4676c0,(users[1],),ecx=packet)
            print('T',1,data.hex());print('R',bytes(cpu.mem_read(packet,len(data))).hex(),state().hex(),ctx['events'].hex() or '-')
        print('Z')

if __name__=='__main__':main()
