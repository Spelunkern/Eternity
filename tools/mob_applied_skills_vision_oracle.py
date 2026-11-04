#!/usr/bin/env python3
"""Execute original vision cleanup, persistent shared pool and live definition reads."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game

LAYOUTS=[(),(0x48,),(0x42,),(0x48,0x42,0x48),(99,0x48,0x40,0x48),
         (0x48,0x48,0x48),(99,0x48,99,0x42,0x48,99),(0x48,99,0x48,99,0x48,99,0x42)]


def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x40000)
    mob,other,head,other_head,respawn,pool,infos,stop=[base+x for x in (0,0x2000,0x5000,0x5100,0x6000,0x10114,0x30000,0x3f000)]
    state={};core_offsets=(0x2b8,0x128,0x12c,0x124,0xcbc,0x224,0x248,0x244,0x24c,0x284)
    def pack(*v):return struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v])
    def put(at,*v):cpu.mem_write(at,pack(*v))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def idx(at):return state['objects'].index(at)
    def seq(v):return ','.join(map(str,v)) or '-'
    def nodes(at,sentinel=0):
        out=[]
        while at!=sentinel:assert len(out)<30;out.append(idx(at));at=get(at+4)
        return out
    def core():return (b''.join(bytes(cpu.mem_read(mob+x,4)) for x in core_offsets)+pack(int(get(mob+0xc84)!=0),cpu.mem_read(respawn+0x1c,1)[0])).hex()
    def snapshot():
        first=nodes(get(head+4),head);second=nodes(get(other_head+4),other_head);free=nodes(get(pool))
        assert len(first)==get(mob+0x1e4) and len(second)==get(other+0x1e4) and len(free)==get(pool+8)
        payload=[':'.join(map(str,[i,*struct.unpack('<10I',cpu.mem_read(at+8,40)),(get(at+48)-infos)//256])) for i,at in enumerate(state['objects'])]
        details=[struct.unpack('<H',cpu.mem_read(infos+i*256+0x32,2))[0] for i in range(10)]
        return '|'.join([core(),str(get(pool+4)),seq(first),seq(second),seq(free),';'.join(payload),seq(details)])
    for address,code in [(0x503a3f,b'\xc3'),(0x4a1810,b'\xc2\x0c\x00'),(stop+16,b'\xc2\x04\x00'),(stop+32,b'\xc2\x04\x00')]:cpu.mem_write(address,code)
    put(0x5210cc,stop+16);put(0x5210d0,stop+32)
    def hook(machine,address,size,data):
        esp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x503a3f:
            assert state['seeding'],'Expected retained shared-pool reuse'
            at=base+0x20000+len(state['objects'])*64;state['objects'].append(at);cpu.mem_write(at,bytes(52));machine.reg_write(UC_X86_REG_EAX,at)
        elif address==stop+16:
            if get(esp+4)==mob+0x1cc:
                assert not state.get('vision_call',False) or get(mob+0x2b8)==0;state['depth']+=1;state['entries']+=1
        elif address==stop+32:
            if get(esp+4)==mob+0x1cc:state['depth']-=1;assert state['depth']>=0
        elif address==0x4a1810:
            at=machine.reg_read(UC_X86_REG_EBX);assert state['depth']==1 and get(esp+4)==6
            assert get(esp+8)==get(at+48)+3 and get(esp+12)==get(at+16)
            assert machine.reg_read(UC_X86_REG_EDI)==mob+0xcf4
            assert idx(at) in nodes(get(head+4),head) and idx(at) not in nodes(get(pool))
            state['events'].append('L'+str(idx(at))+':'+snapshot())
            if state['mutate'] and not state['mutated']:
                state['mutated']=True;put(mob+0x2b8,0x11223344);put(mob+0x284,get(mob+0x284)^1)
                cpu.mem_write(infos+(state['count']-1)*256+0x32,struct.pack('<H',0x48))
        elif address in (0x4a0da0,0x4906d0,0x4902f0):raise RuntimeError('Unexpected effect reversal/speed/attack call')
    cpu.hook_add(UC_HOOK_CODE,hook)
    def call(address,esi=0,edi=0,args=()):
        put(base+0x3e000,stop,*args);cpu.reg_write(UC_X86_REG_ESP,base+0x3e000);cpu.reg_write(UC_X86_REG_ESI,esi);cpu.reg_write(UC_X86_REG_EDI,edi)
        cpu.emu_start(address,stop,count=100000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
    def add(owner,definition,ident,instance):
        call(0x40b180,esi=pool);at=cpu.reg_read(UC_X86_REG_EAX)
        put(at+8,instance,ident,definition%3+1,0x80000000+instance,60+definition,70+definition,0xffffffff,definition*100,definition%3,42 if definition==0 else 999+definition)
        put(at+48,infos+definition*256);call(0x486200,esi=owner+0x1c4,edi=at)
    print(f'# ps_game.exe SHA256 {digest}; complete RemApplySkillVisionFix 004a1580, real Alloc/AddBack/Clear and shared pool/list. Required log/allocation/lock ports observed; effect-removal/speed/attack calls forbidden. Log mutation is explicit, not claimed as original SendLogBothMob behavior.')
    print('# S case flags initial12wordcore; V log_events snapshot; F vision snapshot; R other-owner raw reuse snapshot. Snapshot core|attempts|primary|other|free|ten-word payload+definition|live details. First log mutation restores vision, toggles Lua and makes the last definition type 0x48.')
    for case in range(512):
        cpu.mem_write(base,bytes(0x3e000));layout=LAYOUTS[case%8];flags=(case//32)%8
        state.update(objects=[],seeding=True,depth=0,entries=0,events=[],count=len(layout),mutate=bool(case&256),mutated=False)
        put(0x546f3c,base+0x10000);put(mob+0x1c4,head,head);put(head,head,head);put(other+0x1c4,other_head,other_head);put(other_head,other_head,other_head)
        put(mob+0x2b8,[0,42,0xffffffff,0x80000000][(case//8)%4]);put(mob+0x128,case%3,0xcafef00d);put(mob+0x124,2 if case&1 else 0);put(mob+0xcbc,0xabcdef01);put(mob+0x224,0 if case&2 else 1000)
        put(mob+0x248,2);put(mob+0x244,3);put(mob+0x24c,4);put(mob+0x284,flags&1);put(mob+0xc84,respawn if flags&2 else 0);cpu.mem_write(respawn+0x1c,bytes([bool(flags&4)]))
        for i,detail in [(9,99),(8,0x48),*enumerate(layout)]:
            info=infos+i*256;cpu.mem_write(info,struct.pack('<HB',900 if i==9 else 800 if i==8 else 100+i,i%3+1));cpu.mem_write(info+3,('Def'+str(i)).encode()+b'\0');cpu.mem_write(info+0x31,bytes([i%4]));cpu.mem_write(info+0x32,struct.pack('<H',detail))
        for i in range(3):add(other,9,900,9000+i)
        call(0x4a0990,esi=other)
        for i in range(len(layout)):add(mob,i,100+i,1000+i)
        add(other,8,800,3000);state['seeding']=False;initial=core()
        print('S',case,flags,initial,sep='\t')
        for op in ('V','V','F','V','R','V'):
            state['events'].clear();before=get(mob+0x2b8);entries=state['entries']
            if op=='V':
                state['vision_call']=True;call(0x4a1580,args=(mob,));state['vision_call']=False;assert state['entries']-entries==(1 if before else 0)
                print('V',';'.join(state['events']) or '-',snapshot(),sep='\t')
            elif op=='F':put(mob+0x2b8,0xabcdef01);print('F',0xabcdef01,snapshot(),sep='\t')
            else:
                call(0x40b180,esi=pool);at=cpu.reg_read(UC_X86_REG_EAX);call(0x486200,esi=other+0x1c4,edi=at);print('R',snapshot(),sep='\t')
            assert state['depth']==0


if __name__=='__main__':main()
