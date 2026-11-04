#!/usr/bin/env python3
"""Execute original keep/sleep owners, real list/pool, TimeDamage and status lookup."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EBP, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game

LAYOUTS = [(), (0x3d,), (0x42,), (99, 0x3d, 0x42), (0x3d, 0x42, 0x3d),
           (0x42, 0x3d, 99, 0x42, 0x3d), (99, 99), (0x3d, 0x3d, 0x42, 99)]


def main():
    cpu, digest = load_game();base = 0x30000000;cpu.mem_map(base, 0x40000)
    mob, head, respawn, definition, pool, stop = [base+x for x in (0, 0x5000, 0x6000, 0x7000, 0x10114, 0x3f000)]
    state = {}
    def pack(*values):return struct.pack('<'+'I'*len(values), *[v & 0xffffffff for v in values])
    def put(at,*values):cpu.mem_write(at,pack(*values))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def idx(at):return state['objects'].index(at)
    def seq(values):return ','.join(map(str,values)) or '-'
    def nodes(at,sentinel=0):
        out=[]
        while at!=sentinel:
            assert len(out)<20;out.append(idx(at));at=get(at+4)
        return out
    def snapshot():
        active=nodes(get(head+4),head);free=nodes(get(pool))
        assert len(active)==get(mob+0x1e4) and len(free)==get(pool+8)
        data=[get(mob+0x5c),get(mob+0x224),get(mob+0x24c),get(mob+0x124),get(mob+0x44),get(mob+0x40) if get(mob+0x44) else 0,get(mob+0x284)]
        payload=[':'.join(map(str,[i,*struct.unpack('<10I',cpu.mem_read(at+8,40))])) for i,at in enumerate(state['objects'])]
        return '|'.join([seq(data),seq(active),seq(free),';'.join(payload) or '-',bytes(cpu.mem_read(mob+0xcf4,64)).hex()])
    def log(tag):state['events'].append(tag+':'+snapshot())
    # Fixed recursive-clear trampoline: no generated-code reuse/cache ambiguity.
    trampoline=stop+128
    code=b'\x56\xbe'+pack(mob)
    code+=b'\xe8'+struct.pack('<i',0x4a0990-(trampoline+len(code)+5))+b'\x5e\xc2\x08\x00'
    cpu.mem_write(trampoline,code)
    for at,code in [(0x503a3f,b'\xc3'),(0x505316,b'\xc3'),(0x490ee0,b'\xc2\x18\x00'),
                    (0x4910f0,b'\xc2\x08\x00'),(0x421220,b'\xc2\x20\x00'),
                    (0x4906d0,b'\xc3'),(0x4a1810,b'\xc2\x0c\x00'),(stop+16,b'\xc2\x04\x00'),(stop+32,b'\xc2\x04\x00')]:cpu.mem_write(at,code)
    put(0x5210cc,stop+16);put(0x5210d0,stop+32)
    def hook(machine,address,size,data):
        esp=machine.reg_read(UC_X86_REG_ESP)
        if address==stop+16:
            if get(esp+4)==mob+0x1cc:state['depth']+=1
        elif address==stop+32:
            if get(esp+4)==mob+0x1cc:state['depth']-=1;assert state['depth']>=0
        elif address==0x503a3f:
            at=base+0x20000+len(state['objects'])*64;state['objects'].append(at);cpu.mem_write(at,bytes(52));machine.reg_write(UC_X86_REG_EAX,at)
        elif address==0x4a3be0:
            at=machine.reg_read(UC_X86_REG_EBP);log('T'+str(idx(at))+pack(get(esp+4)).hex())
        elif address==0x490ee0:
            at=machine.reg_read(UC_X86_REG_EBP);i=idx(at);state['current']=i
            assert get(esp+4)==mob and [get(esp+x) for x in (16,20,24)]==[0,0,0]
            values=bytes(cpu.mem_read(machine.reg_read(UC_X86_REG_EAX),12))
            log('P'+str(i)+pack(get(esp+8),get(esp+12)).hex()+values.hex())
            if state['mode']==3 and state['hits']==0:
                put(mob+0x44,0);put(mob+0x124,2);put(at+0xc,0xffff1234,0xffffff80);put(at+0x28,9,123)
            killed=state['mode'] in (1,2) and state['hits']==0
            state['hits']+=1;machine.reg_write(UC_X86_REG_EAX,int(killed))
        elif address==0x4910f0:
            log('X'+str(state['current'])+pack(get(esp+4),get(esp+8)).hex())
            if state['mode']==1:put(mob+0x44,0);put(mob+0x124,2)
            machine.reg_write(UC_X86_REG_EIP,trampoline)
        elif address==0x4a1810:
            assert state['depth']>=1 and get(esp+4)==6
            log('L'+str(idx(machine.reg_read(UC_X86_REG_EBX))))
        elif address==0x4906d0:
            assert state['depth']==1 and get(mob+0x24c)==0;log('S')
        elif address==0x421220:
            assert state['depth']==1 and get(esp+12)==15
            assert [get(esp+x) for x in (20,24,28,32)]==[0x42700000,0,0,6]
            log('V'+bytes(cpu.mem_read(get(esp+8),15)).hex())
    cpu.hook_add(UC_HOOK_CODE,hook)
    def call(address,esi=0,edi=0,ecx=0,args=()):
        put(base+0x3e000,stop,*args);cpu.reg_write(UC_X86_REG_ESP,base+0x3e000)
        cpu.reg_write(UC_X86_REG_ESI,esi);cpu.reg_write(UC_X86_REG_EDI,edi);cpu.reg_write(UC_X86_REG_ECX,ecx);cpu.reg_write(UC_X86_REG_FPCW,0x27f)
        cpu.emu_start(address,stop,count=100000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
    print(f'# ps_game.exe SHA256 {digest}; real ChkKeepSkill 004a10d0, RemApplySkillSleep 004a16c0, ClearApplySkill 004a0990, TimeDamage 004a3be0/CRT, GetStatus 0041d760, lists and shared pool. Observed required combat/death/log/view/speed/lock/allocation ports; death port executes real recursive Clear.')
    print('# kind case now sleep_bits stack_mode callback_mode flags events final_snapshot; snapshot mob id/hp/sleep/status/stack_active/active_count/lua | active | free | ten-word node payloads | full previous-name buffers. T calculation, P damage, X death, L log, V packet, S speed.')
    for kind,count in [('K',512),('S',128)]:
        for case in range(count):
            cpu.mem_write(base,bytes(0x3e000));state.update(objects=[],events=[],depth=0,hits=0,current=0,mode=(case//32)%4 if kind=='K' else 0)
            now=[0,1,2999,3000,3001,0xffffffff,0x80000000,9000][(case//11)%8]
            sleep=[0,1,0xffffffff][(case//7)%3];stack=(case//8)%4;flags=(case//128)%4 if kind=='K' else (case//32)%4
            put(0x546f3c,base+0x10000);put(mob+0x1c4,head,head);put(head,head,head)
            put(mob+0x5c,7);put(mob+0x224,1000+case);cpu.mem_write(mob+0x228,struct.pack('<2H',100,50));put(mob+0x24c,sleep)
            put(mob+0xa8,definition);put(definition+0x30,2000+case);cpu.mem_write(definition+0x34,struct.pack('<2H',200,150))
            put(mob+0x124,2 if stack in (1,2) else 0)
            if stack in (2,3):
                put(mob+0x44,1)
                if stack==3:
                    put(mob+0x40,1);put(mob+0x34,base+0x8000,1);put(base+0x8000,base+0x8100);put(base+0x8100,base+0x8200);put(base+0x8228,2)
            put(mob+0x284,flags&1);put(mob+0xc84,respawn if flags&2 else 0);cpu.mem_write(respawn+0x1c,b'\x01')
            put(mob+0x60,base+0x9000 if case%2 else 0)
            cpu.mem_write(mob+0xcf4,b'U'*32+b'S'*32);cpu.mem_write(mob+0xcf4,b'BeforeUser\0');cpu.mem_write(mob+0xd14,b'BeforeSkill\0')
            for i,detail in enumerate(LAYOUTS[case%8]):
                call(0x40b180,esi=pool);at=cpu.reg_read(UC_X86_REG_EAX);info=base+0x30000+i*256
                put(at+8,i+1,100+i,i%3+1,50+i,60+i,70+i,0,100*i+case%17,1,42+i);put(at+48,info)
                cpu.mem_write(info+3,('Skill'+str(i)).encode()+b'\0');cpu.mem_write(info+0x32,struct.pack('<H',detail))
                cpu.mem_write(info+0x6e,bytes([case%6]));cpu.mem_write(info+0x70,struct.pack('<3H',1,5,33))
                call(0x486200,esi=mob+0x1c4,edi=at)
            state['events'].clear()
            if kind=='K':call(0x4a10d0,ecx=mob,args=(now,))
            else:call(0x4a16c0,args=(mob,))
            assert state['depth']==0
            print(kind,case,now,sleep,stack,state['mode'],flags,';'.join(state['events']) or '-',snapshot(),sep='\t')


if __name__=='__main__':main()
