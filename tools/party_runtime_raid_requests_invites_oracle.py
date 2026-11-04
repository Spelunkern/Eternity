#!/usr/bin/env python3
"""Execute entire original PacketParty raid entry/leave, registry and request/expulsion compositions."""
import struct
import sys
from original_oracle import load_game
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP


def main():
    if len(sys.argv)!=1:raise SystemExit("This invitation matrix observes send boundaries; pooled transport has its separate recovered oracle.")
    live_send=False
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x400000);cpu.mem_map(0,0x10000);cpu.mem_map(0x31000000,0x10000);cpu.mem_map(0x32000000,0x10000)
    manager=0x547688;parties=[base+0x1000+i*0x1000 for i in range(4)];users=[base+0x10000+i*0x6000 for i in range(32)]
    packet,stack,stop=base+0x240000,base+0x3f0000,base+0x3ff000
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
            values += [get(p+n) for n in (8,12,16,0x104,0x108,0x10c,0x110)]+[cpu.mem_read(p+0x114,1)[0],cpu.mem_read(p+0x115,1)[0]]
            values += [get(p+0x14+i*8) for i in range(30)]+[uid(get(p+0x18+i*8)) for i in range(30)]
        b=struct.pack('<'+'I'*len(values),*values)
        for index,u in enumerate(users):
            head=get(u+0xa94);cursor=get(u+0xa98);size=65 if index>=30 else index%4
            position=size if cursor==head else (cursor-head-0x100)//0x40 if head+0x100<=cursor<head+0x100+size*0x40 else 0
            values=[get(u+0xdc),pid(get(u+0x173c)),get(u+0x1740),get(u+0x5834),get(u+0x15e0),cpu.mem_read(u+0x12d,1)[0],get(u+0x5758),zone(get(u+0xe0)),struct.unpack('<H',cpu.mem_read(u+0x164,2))[0],cpu.mem_read(u+0x1744,1)[0],position,int(bool(get(u+0x175c)))]
            b+=struct.pack('<12I',*values)+bytes(cpu.mem_read(u+0x188,21))
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
    ports={init:8,destroy:4,lock:4,unlock:4,increment:4,tick:0,0x503a3f:0,0x503ba1:0,0x415f30:4,0x40d840:4,0x4455d0:0,0x4d4d20:8,0x4208c0:12,0x45b0d0:0,0x4d4e50:8,0x505316:0,0x4d43a0:8}
    pools={0x4df710:32,0x4df7b0:128,0x4df850:2048}
    buffer,frame,agent,worker=base+0x260000,base+0x261000,base+0x250000,base+0x251000
    decrement=stop+112
    if live_send:
        ports.pop(0x4455d0);ports.pop(0x4d4d20)
        ports.update({decrement:4,0x4dc1b0:8,0x4df070:0,**{p:0 for p in pools}})
        put(0x5211f0,decrement)
    for at,n in ports.items():cpu.mem_write(at,b'\xc2'+struct.pack('<H',n))
    for at,target in ((0x5210c8,init),(0x5210d8,destroy),(0x5210cc,lock),(0x5210d0,unlock),(0x5210f8,increment),(0x5210e4,tick)):put(at,target)
    for i,u in enumerate(users):
        code=0x31000000+i*0x100;nested_packet=0x31004000+i*0x100
        cpu.mem_write(nested_packet,b'\x15\x0b\x00')
        prefix=b'\x9c\x60\x68'+struct.pack('<I',u)+b'\xb9'+struct.pack('<I',nested_packet)
        cpu.mem_write(code,prefix+b'\xe8'+struct.pack('<I',(0x4676c0-(code+len(prefix)+5))&0xffffffff)+b'\x61\x9d\xc2\x08\x00')
    # Actual nested PacketParty leave, preserving the interrupted original ABI.
    # CParty::Send uses RET 0; CZone::SendView uses RET 12.
    for cleanup,offset in ((0,0x8000),(12,0xa000)):
        for i,u in enumerate(users[:8]):
            code=0x31000000+offset+i*0x100;leave_packet=0x31006000+i*0x100
            cpu.mem_write(leave_packet,b'\x05\x0b')
            prefix=b'\x9c\x60\x68'+struct.pack('<I',u)+b'\xb9'+struct.pack('<I',leave_packet)
            cpu.mem_write(code,prefix+b'\xe8'+struct.pack('<I',(0x4676c0-(code+len(prefix)+5))&0xffffffff)+b'\x61\x9d\xc2'+struct.pack('<H',cleanup))
    def nested_leave(machine,cleanup):
        if not ctx.get('record') or ctx['nested']:return
        selected=next((i for i in (2,0) if get(users[i]+0x173c)),None)
        if selected is None:return
        ctx['nested']=True;event('W',struct.pack('<I',100+selected))
        machine.reg_write(UC_X86_REG_EIP,0x31000000+(0xa000 if cleanup else 0x8000)+selected*0x100)
    for i,u in enumerate(users[:2]):
        code=0x3100c000+i*0x100;data=0x3100e000+i*0x100
        cpu.mem_write(data,b'\x0f\x0b\xd2\x04\x00\x00')
        prefix=b'\x9c\x60\x68'+struct.pack('<I',u)+b'\xb9'+struct.pack('<I',data)
        cpu.mem_write(code,prefix+b'\xe8'+struct.pack('<I',(0x4676c0-(code+len(prefix)+5))&0xffffffff)+b'\x61\x9d\xc2\x00\x00')
    for i,u in enumerate(users):
        code=0x32000000+i*0x100;data=0x32004000+i*0x100
        cpu.mem_write(data,struct.pack('<HII',0xb12,0,29))
        prefix=b'\x9c\x60\x68'+struct.pack('<I',u)+b'\xb9'+struct.pack('<I',data)
        cpu.mem_write(code,prefix+b'\xe8'+struct.pack('<I',(0x4676c0-(code+len(prefix)+5))&0xffffffff)+b'\x61\x9d\xc2\x00\x00')
    def lookup_mutation():
        if not ctx.get('record') or ctx['mutated']:return
        root=users[ctx['root']]
        if ctx['mutation']==1:put(root+0x173c,0);ctx['mutated']=True
        elif ctx['mutation']==3:put(root+0x1740,900);put(root+0x5834,123);ctx['mutated']=True
    def hook(machine,at,size,data):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if at in (0x44670a,0x44724d,0x446ac1):
            user=machine.reg_read(UC_X86_REG_ESI if at==0x44670a else UC_X86_REG_EDI)
            if get(user+0x175c):
                event('G',struct.pack('<I',uid(user)));put(user+0x175c,0)
                if at==0x446ac1:machine.reg_write(UC_X86_REG_EBX,unlock)
                machine.reg_write(UC_X86_REG_EIP,{0x44670a:0x446806,0x44724d:0x447325,0x446ac1:0x446bae}[at])
        elif at in (init,destroy,lock,unlock):
            k,i=lock_id(get(sp+4));event({init:'I',destroy:'D',lock:'[',unlock:']'}[at],struct.pack('<2I',k,i))
        elif live_send and at in pools:
            event('A',struct.pack('<I',pools[at]));machine.reg_write(UC_X86_REG_EAX,0 if ctx['pool_fail'] else buffer)
        elif live_send and at==decrement:
            address=get(sp+4);n=(get(address)-1)&0xffffffff;put(address,n);machine.reg_write(UC_X86_REG_EAX,n)
        elif live_send and at==0x4dc1b0:
            user=get(sp+4);assert get(sp+8)==buffer
            wire=bytes(cpu.mem_read(frame,struct.unpack('<H',cpu.mem_read(frame,2))[0]))
            event('E',struct.pack('<2I',uid(user),get(buffer+8))+wire)
            put(buffer+8,get(buffer+8)-1)
            if get(buffer+8)==0:event('B',struct.pack('<I',0))
        elif live_send and at==0x4df070:
            assert get(sp+4)==buffer and get(buffer+8)==0;event('B',struct.pack('<I',0))
        elif at==increment:
            p=get(sp+4);n=(get(p)+1)&0xffffffff;put(p,n);machine.reg_write(UC_X86_REG_EAX,n)
        elif at==0x503a3f:
            n=get(sp+4)
            if n==304:
                event('O')
                if ctx['alloc_fail'] or ctx['raw']>=4:p=0
                else:p=parties[ctx['raw']];ctx['raw']+=1
            else:p=base+0x2c0000+ctx['nodes']*0x100;ctx['nodes']+=1
            machine.reg_write(UC_X86_REG_EAX,p)
        elif at==tick:
            n=(ctx['clock']+ctx['ticks']*17)&0xffffffff;ctx['ticks']+=1;event('K',struct.pack('<I',n))
            if ctx.get('record') and ctx['mutation']==2 and not ctx['mutated']:
                u=users[ctx['root']];put(u+0xdc,get(u+0xdc)+1000);cpu.mem_write(u+0x188,b'tick-name-0000000000\0');ctx['mutated']=True
            machine.reg_write(UC_X86_REG_EAX,n)
        elif at==0x415f30:
            z=zone(machine.reg_read(UC_X86_REG_EAX));requested=get(sp+4);event('Z',struct.pack('<2I',z,requested));lookup_mutation();i=requested-100
            p=users[i] if not ctx['missing'] and 0<=i<32 and zone(get(users[i]+0xe0))==z else 0
            machine.reg_write(UC_X86_REG_EAX,p)
        elif at==0x40d840:
            p=machine.reg_read(UC_X86_REG_EAX);name=bytearray()
            while cpu.mem_read(p,1)!=b'\0':name+=cpu.mem_read(p,1);p+=1
            event('N',bytes(name));lookup_mutation();found=0
            if not ctx['missing']:
                for u in users:
                    if bytes(cpu.mem_read(u+0x188,21)).split(b'\0')[0]==bytes(name):found=u;break
            machine.reg_write(UC_X86_REG_EAX,found)
        elif at==0x4455d0:
            payload=bytes(machine.mem_read(machine.reg_read(UC_X86_REG_ECX),machine.reg_read(UC_X86_REG_EAX)));event('P',payload)
            if ctx['mutation']==9 and not ctx['nested'] and payload[:2] in (b'\x12\x0b',b'\x13\x0b',b'\x17\x0b'):
                p=get(users[ctx['root']]+0x173c);selected=get(p+0x18+get(p+12)*8) if p else 0
                if selected in users:
                    ctx['nested']=True;event('W',struct.pack('<I',uid(selected)));machine.reg_write(UC_X86_REG_EIP,0x32000000+users.index(selected)*0x100)
            if ctx['mutation']==7 and not ctx['nested'] and payload[:2] in (b'\x0e\x0b',b'\x16\x0b'):
                ctx['nested']=True;event('W',struct.pack('<I',100+ctx['root']));machine.reg_write(UC_X86_REG_EIP,0x3100c000+ctx['root']*0x100)
            if ctx['mutation']==6 and payload[:2]==b'\x06\x0b':nested_leave(machine,0)
        elif at==0x4d4d20:
            u=machine.reg_read(UC_X86_REG_ECX);payload=bytes(machine.mem_read(get(sp+4),get(sp+8)));event('U',struct.pack('<I',uid(u))+payload)
            if ctx.get('record') and ctx['mutation']==4 and not ctx['nested'] and payload[:2] in (b'\x14\x0b',b'\x18\x0b'):
                ctx['nested']=True;event('V',struct.pack('<I',uid(u)));machine.reg_write(UC_X86_REG_EIP,0x31000000+users.index(u)*0x100)
        elif at==0x4208c0:
            event('S',bytes(machine.mem_read(get(sp+8),get(sp+12))))
            if ctx['mutation']==5:nested_leave(machine,12)
        elif at==0x45b0d0:
            i=uid(machine.reg_read(UC_X86_REG_EDI));event('F',struct.pack('<I',i));cpu.mem_write(machine.reg_read(UC_X86_REG_ESI),bytes((i+j)&255 for j in range(74)))
        elif at==0x4d4e50:event('L',bytes(machine.mem_read(get(sp+4),get(sp+8))))
        elif at==0x4d43a0:event('C',struct.pack('<2I',get(sp+4),get(sp+8)))
    cpu.hook_add(UC_HOOK_CODE,hook)
    def call(at,args=(),ecx=0,esi=0):
        put(stack,stop)
        for i,v in enumerate(args):put(stack+4+i*4,v)
        for r,v in ((UC_X86_REG_ESP,stack),(UC_X86_REG_ECX,ecx),(UC_X86_REG_ESI,esi)):cpu.reg_write(r,v)
        cpu.emu_start(at,stop,count=1000000)
        assert cpu.reg_read(UC_X86_REG_EIP)==stop, "Original request did not return"
    print(f'# ps_game.exe SHA256 {digest}; entire PacketParty 004676c0 raid ID/name invitation and acceptance branches; actual IsPartyBoth/SubBoth, CreateParty/EnterUser/raid and normal membership/BreakParty; real PacketParty/ExpelUser/normal Leave/registry/map/GetPartyType. Only OS/raw allocator/lookup/transport/log and explicit guild cleanup block ports (44670a..446806,44724d..447325,446ac1..446bae). No raid membership stubs. W/V are actual nested requests.')
    print('# Transport:', 'actual original Send/AllocBuffer/connection/agent; pool/worker completion are observed providers' if live_send else 'observed party/direct send boundaries')
    print('# S case count flags mutation missing alloc_fail unused clock; J/L/X actor placeholder; T actor packet; R return_or_packet state events; Z')
    for case in range(512):
        cpu.mem_write(base,bytes(0x3ef000));cpu.mem_write(manager,bytes(84));cpu.mem_write(0,bytes(0x10000))
        count=(0,1,2,3,6,7,29,30)[case%8];source=case%2
        flags=((case//8)%16)|(32 if case%5==0 else 0)|(128 if case%11==0 else 0)|(256 if case%13==0 else 0)|(512 if case%17==0 else 0)|(1024 if case%19==0 else 0)|(2048 if case%23==0 else 0)|(4096 if case%7==0 else 0)|(8192 if case%41==0 else 0)|(16384 if case%3==0 else 0)|(65536 if case%31==0 else 0)|(131072 if case%37==0 else 0)|(262144 if source else 0)
        mutation=(case//11)%5
        if flags&32:flags&=~1
        alloc_fail=case%29==0;missing=case%43==0
        if mutation==4:count=(0,2,3,6)[case%4];flags=262144 if source else 0;source=0;flags=0;alloc_fail=False;missing=False
        if case>=496:count=(3,6)[case%2];source=0;flags=8192;mutation=0;alloc_fail=False;missing=False
        clock=(0,0xfffffff0,0x7fffffff,2000)[case%4]
        if live_send and mutation==6:mutation=0
        if mutation in (5,6):flags&=~1;count=max(count,4)
        ctx.update(pool_fail=case%13==0,record=False,events=b'',raw=2,nodes=0,alloc_fail=alloc_fail,clock=clock,ticks=0,mutation=mutation,mutated=False,nested=False,missing=missing,raid_success=False,root=source)
        put(buffer+4,frame);put(agent+0x16c,worker)
        for i,u in enumerate(users):
            if live_send:put(u+0x68,(-1,0,5)[i%3]);put(u+0xc8,agent)
            put(u+0xdc,100+i);cpu.mem_write(u+0x136,struct.pack('<H',10+i));cpu.mem_write(u+0x134,bytes([i%6]));cpu.mem_write(u+0x188,f'user-{i:015}'.encode()+b'\0');cpu.mem_write(u+0x1744,b'\x09');put(u+0xe0,base+0x60000);put(u+0x5758,4);put(u+0x175c,base+0x235000 if flags&16 and i>=30 else 0)
            for off,v in zip((0x17c,0x1238,0x184,0x1240,0x180,0x123c,0x164),(100+i,200+i,300+i,400+i,500+i,600+i,1)):cpu.mem_write(u+off,struct.pack('<H',v))
            cpu.mem_write(u+0xd0,struct.pack('<3f',1.25+i,-2.5,3.75));head=base+0x150000+i*0x4000;put(u+0xa94,head);put(head+4,head)
            effects=65 if i>=30 else (i%4)
            if effects:
                put(head+4,head+0x100)
                for j in range(effects):
                    node=head+0x100+j*0x40;put(node+4,head if j+1==effects else node+0x40)
                    cpu.mem_write(node+12,struct.pack('<H',1000+j));cpu.mem_write(node+16,bytes([j%255]));put(node+32,clock+1500+j*1000)
                    definition=base+0x230000+(0x100 if j%3==0 else 0);put(node+48,definition);cpu.mem_write(definition+0x97,bytes([int(j%3==0)]))
        call(0x4473f0,(manager,))
        for i in range(2):call(0x444c60,esi=parties[i]);put(parties[i]+8,10+i*10)
        p=parties[0];put(p+16,count);put(p+12,count-1 if flags&4 and count else 0);put(p+0x110,count//2 if count else 1);put(p+0x108,17);put(p+0x104,2);put(p+0x10c,80);cpu.mem_write(p+0x114,bytes([0 if flags&32 else 1,flags&8!=0]))
        for i in range(count):
            put(p+0x14+i*8,0xffff if flags&2 else (i*7)%30)
            if not (flags&1 and i%5==2):put(p+0x18+i*8,users[i]);put(users[i]+0x173c,p)
        if not get(p+0x18+get(p+0x110)*8):
            put(p+0x110,next((i for i in range(count) if get(p+0x18+i*8) and i!=get(p+12)),0))
        call(0x447780,(p+8,p),esi=manager+0x28)
        if flags&128:cpu.mem_write(users[31]+0x12d,b'\x01')
        if flags&256:put(users[source]+0x15e0,1)
        if flags&512:put(users[31]+0x15e0,1)
        if flags&1024:put(users[31]+0x1740,900);put(users[31]+0x5834,123)
        if flags&16384:put(users[31]+0x1740,100+source);put(users[31]+0x5834,0xfffffff0)
        if flags&2048:
            put(parties[1]+16,1);put(parties[1]+0x18,users[31]);put(users[31]+0x173c,parties[1]);call(0x447780,(parties[1]+8,parties[1]),esi=manager+0x28)
        if flags&8192 and count>=3:
            position=next((i for i in range(count) if i not in (source,get(p+12),get(p+0x110))),None)
            if position is not None:
                previous=get(p+0x18+position*8)
                if previous:put(previous+0x173c,0)
                put(p+0x18+position*8,users[31]);put(users[31]+0x173c,p)
        if flags&65536:put(users[31]+0xe0,base+0x61000);put(users[31]+0x5758,0)
        print('S',case,count,flags,mutation,int(missing),int(alloc_fail),0,clock)
        ctx.update(record=True,events=b'')
        op=0xb14 if (case//2)%2==0 else 0xb18
        target=source if flags&131072 else 31
        invitation=struct.pack('<HI',op,100+target) if op==0xb14 else struct.pack('<H',op)+f'user-{target:015}'.encode()+b'X'
        commands=[('T',source,invitation),('T',31,struct.pack('<HB',0xb15,int(bool(flags&4096)))),('T',31,struct.pack('<HB',0xb15,0)),('B',source,struct.pack('<H',0xb16))]
        for tag,actor,data in commands:
            if tag=='B':
                current=get(users[source]+0x173c);selected=get(current+0x18+get(current+12)*8) if current else 0;actor=users.index(selected) if selected in users else source;tag='T'
            ctx.update(events=b'',root=actor)
            cpu.mem_write(packet,data);call(0x4676c0,(users[actor],),ecx=packet);result=bytes(cpu.mem_read(packet,len(data)))
            if mutation==4 and data[:2] in (b'\x14\x0b',b'\x18\x0b'):assert ctx['nested'] and get(users[31]+0x173c)!=0, 'Original nested raid acceptance did not actually join'
            print(tag,actor,data.hex());print('R',result.hex(),state().hex(),ctx['events'].hex() or '-')
        if mutation==4:assert ctx['nested'] and get(users[31]+0x1740)==0, 'Literal nested raid acceptance was not executed'
        print('Z')

if __name__=='__main__':main()
