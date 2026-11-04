#!/usr/bin/env python3
"""Execute original party constructor/reset, manager, heap, map and recursive leave."""
import struct
from original_oracle import load_game
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP


def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x100000);cpu.mem_map(0,0x10000)
    manager=0x547688;parties=[base+0x1000+i*0x1000 for i in range(6)];users=[base+0x10000+i*0x6000 for i in range(2)]
    stack,stop=base+0xf0000,base+0xff000
    init,destroy,lock,unlock,increment,tick=[stop+n for n in (16,32,48,64,80,96)]
    context={}
    def put(at,value):cpu.mem_write(at,struct.pack('<I',value&0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def party_id(at):return parties.index(at)+1 if at in parties else 0
    def user_id(at):return users.index(at)+100 if at in users else 0
    def entries():
        head=get(manager+0x44)
        if not head:return []
        def walk(node):
            if not node or node==head:return []
            return walk(get(node))+[(get(node+12),party_id(get(node+16)))]+walk(get(node+8))
        return walk(get(head+4))
    def state():
        values=[get(manager),party_id(get(manager+4)),get(manager+8),get(manager+12),context['freed']]
        pairs=entries();values += [len(pairs)]+[v for pair in pairs for v in pair]
        for p in parties:
            values += [get(p+n) for n in (8,12,16,0x104,0x108,0x10c,0x110)]
            values += [cpu.mem_read(p+0x114,1)[0],cpu.mem_read(p+0x115,1)[0],party_id(get(p)),party_id(get(p+4))]
            values += [get(p+0x14+i*8) for i in range(30)]
            values += [user_id(get(p+0x18+i*8)) for i in range(30)]
        for u in users:values += [party_id(get(u+0x173c)),cpu.mem_read(u+0x1744,1)[0]]
        return struct.pack('<'+'I'*len(values),*values)
    def event(tag,data=b''):
        value=14695981039346656037
        for byte in state():value=((value^byte)*1099511628211)&0xffffffffffffffff
        context['events']+=tag.encode()+struct.pack('<QI',value,len(data))+data
    def lock_identity(address):
        if address==manager+0x10:return 0,0
        if address==manager+0x28:return 1,0
        for i,p in enumerate(parties):
            if address==p+0x118:return 2,i+1
        for i,u in enumerate(users):
            if address==u+0xa9c:return 3,i+100
        raise RuntimeError(f'Unexpected lock {address:x}')
    ports={init:8,destroy:4,lock:4,unlock:4,increment:4,tick:0,0x503a3f:0,0x503ba1:0,0x4455d0:0,0x4d4d20:8,0x4208c0:12,0x45b0d0:0,0x4d4e50:8,0x505316:0}
    for at,cleanup in ports.items():cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup))
    for address,target in ((0x5210c8,init),(0x5210d8,destroy),(0x5210cc,lock),(0x5210d0,unlock),(0x5210f8,increment),(0x5210e4,tick)):put(address,target)
    def hook(machine,at,size,data):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if at in (init,destroy,lock,unlock):
            kind,identity=lock_identity(get(sp+4));values=[kind,identity]
            if at==init:values.append(get(sp+8))
            event({init:'I',destroy:'D',lock:'[',unlock:']'}[at],struct.pack('<'+'I'*len(values),*values))
        elif at==increment:
            address=get(sp+4);value=(get(address)+1)&0xffffffff;put(address,value);machine.reg_write(UC_X86_REG_EAX,value)
        elif at==0x503a3f:
            size=get(sp+4)
            if size==0x130:
                event('O');context['attempt']+=1
                if (context['fail'] and context['attempt']==1) or context['raw']>=6:address=0
                else:address=parties[context['raw']];context['raw']+=1
            else:
                assert size<=256,size
                address=base+0x80000+context['nodes']*0x100;context['nodes']+=1
            machine.reg_write(UC_X86_REG_EAX,address)
        elif at==0x503ba1:
            address=get(sp+4)
            if address in parties:
                identity=party_id(address);context['freed']|=1<<(identity-1);event('V',struct.pack('<I',identity))
        elif at==tick:event('K');machine.reg_write(UC_X86_REG_EAX,2000+context['case'])
        elif at==0x4455d0:event('P',bytes(machine.mem_read(machine.reg_read(UC_X86_REG_ECX),machine.reg_read(UC_X86_REG_EAX))))
        elif at==0x4d4d20:event('U',struct.pack('<I',user_id(machine.reg_read(UC_X86_REG_ECX)))+bytes(machine.mem_read(get(sp+4),get(sp+8))))
        elif at==0x4208c0:event('S',bytes(machine.mem_read(get(sp+8),get(sp+12))))
        elif at==0x45b0d0:
            uid=user_id(machine.reg_read(UC_X86_REG_EDI));event('F',struct.pack('<I',uid));cpu.mem_write(machine.reg_read(UC_X86_REG_ESI),bytes((uid+i)&255 for i in range(74)))
        elif at==0x4d4e50:event('L',bytes(machine.mem_read(get(sp+4),get(sp+8))))
    cpu.hook_add(UC_HOOK_CODE,hook)
    def call(address,args=(),eax=0,ebx=0,ecx=0,edi=0,esi=0):
        put(stack,stop)
        for i,value in enumerate(args):put(stack+4+i*4,value)
        for reg,value in ((UC_X86_REG_ESP,stack),(UC_X86_REG_EAX,eax),(UC_X86_REG_EBX,ebx),(UC_X86_REG_ECX,ecx),(UC_X86_REG_EDI,edi),(UC_X86_REG_ESI,esi)):cpu.reg_write(reg,value)
        cpu.emu_start(address,stop,count=1000000)
        return cpu.reg_read(UC_X86_REG_EAX)
    def operation(command,action):
        context['events']=b'';result=action();print(command);print('R',result,state().hex(),context['events'].hex() or '-')
        return result
    print(f'# ps_game.exe SHA256 {digest}; complete CParty ctor/reset, CPartyMgr ctor/create/remove/find/dtor, SSyncHeap Alloc/Release and real std::map insert/find/erase plus real normal membership and GetPartyType. Only OS locks/atomics/clock, raw CRT allocation, log context and transport are ports. External H/Q/K/V fixture projections seed retained heap/IDs; they are not claimed Free implementations. State snapshots are complete; event snapshots use FNV-1a64.')
    for case in range(64):
        cpu.mem_write(base,bytes(0xfd000));cpu.mem_write(manager,bytes(84));cpu.mem_write(0,bytes(0x10000))
        context.update(case=case,events=b'',freed=0,nodes=0,raw=2,attempt=0,fail=case%7==0)
        print('S',case,int(context['fail']))
        put(manager,9);put(manager+8,17);put(manager+12,19)
        for i,p in enumerate(parties):
            for off,value in ((8,(i+1)*100+case),(12,1),(16,2),(0x104,-7),(0x108,3),(0x10c,99),(0x110,9)):put(p+off,value)
            cpu.mem_write(p+0x114,b'\x01\x01');put(p,parties[i-1] if i else 0);put(p+4,parties[i+1] if i<5 else 0)
            for j in range(30):put(p+0x14+j*8,j+10+i);put(p+0x18+j*8,users[(j+i)%2])
        for i,u in enumerate(users):
            put(u+0xdc,100+i);cpu.mem_write(u+0x136,struct.pack('<H',20+i));cpu.mem_write(u+0x134,bytes([1+i]));cpu.mem_write(u+0x188,f'registry-{i:011}'.encode()+b'\0');cpu.mem_write(u+0x1744,b'\x09');put(u+0xe0,1 if (case+i)%2 else 0)
            for off,value in zip((0x17c,0x1238,0x184,0x1240,0x180,0x123c,0x164),(100+i,200+i,300+i,400+i,500+i,600+i,1)):cpu.mem_write(u+off,struct.pack('<H',value))
            cpu.mem_write(u+0xd0,struct.pack('<3f',1.25+i,-2.5,3.75));head=base+0x60000+i*0x1000;put(u+0xa94,head);put(head+4,head)
        operation('I',lambda:(call(0x4473f0,(manager,)),0)[1])
        operation('T 0',lambda:(call(0x444c60,esi=parties[0]),0)[1])
        operation('X 1',lambda:(call(0x444ca0,ecx=parties[1]),0)[1])
        counter=(1,0xfffffffe,0xffffffff,0)[case%4];count=case%3;id0=(0,77,0xffffffff,0)[(case//3)%4];id1=(0,77,88)[(case//5)%3]
        def seed():
            put(manager,counter);put(manager+4,parties[0] if count else 0);put(manager+8,7);put(manager+12,count);put(parties[0]+8,id0);put(parties[1]+8,id1);put(parties[0]+4,parties[1] if count==2 else 0);put(parties[1]+4,0);return 0
        operation(f'H {counter} {count} {id0} {id1}',seed)
        operation('R -1',lambda:(call(0x447540,eax=0,ecx=manager),0)[1])
        operation('C -1 1',lambda:party_id(call(0x4474b0,(0,1),ebx=manager)))
        first=operation('C 0 -7',lambda:party_id(call(0x4474b0,(users[0],0xfffffff9),ebx=manager)))
        if first:
            index=first-1;key=get(parties[index]+8)
            operation(f'F {key}',lambda:party_id(call(0x447560,(key,),eax=manager)))
            operation(f'J {index} 1',lambda:call(0x445fe0,(users[1],),ecx=parties[index])&255)
            operation(f'L {index} 0',lambda:call(0x446960,(users[0],),ecx=parties[index])&255)
            operation(f'F {key}',lambda:party_id(call(0x447560,(key,),eax=manager)))
            operation(f'R {index}',lambda:(call(0x447540,eax=parties[index],ecx=manager),0)[1])
        second=operation('C 0 2',lambda:party_id(call(0x4474b0,(users[0],2),ebx=manager)))
        if second:operation(f'R {second-1}',lambda:(call(0x447540,eax=parties[second-1],ecx=manager),0)[1])
        third=operation('C 1 1',lambda:party_id(call(0x4474b0,(users[1],1),ebx=manager)))
        if third:
            key=get(parties[third-1]+8)
            operation(f'K {(key-1)&0xffffffff}',lambda:(put(manager,(key-1)&0xffffffff),0)[1])
            fourth=operation('C 0 3',lambda:party_id(call(0x4474b0,(users[0],3),ebx=manager)))
            operation(f'F {key}',lambda:party_id(call(0x447560,(key,),eax=manager)))
            if fourth:operation(f'R {fourth-1}',lambda:(call(0x447540,eax=parties[fourth-1],ecx=manager),0)[1])
        operation('C 1 4',lambda:party_id(call(0x4474b0,(users[1],4),ebx=manager)))
        index=first-1 if first else 0
        def seed_free(tail=-1):
            put(manager+4,parties[index]);put(manager+12,1 if tail<0 else 2);put(parties[index]+4,0 if tail<0 else parties[tail])
            if tail>=0:put(parties[tail]+4,0)
            return 0
        operation(f'Q {index} -1',seed_free)
        operation('A',lambda:party_id(call(0x4475f0,esi=manager+4)))
        operation(f'Q {index} 1',lambda:seed_free(1))
        operation('D',lambda:(call(0x447450,(manager,)),0)[1])
        print('Z')

if __name__=='__main__':main()
