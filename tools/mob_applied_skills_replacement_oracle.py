#!/usr/bin/env python3
"""Execute full original replacement with real final damage math/list/pool coordination."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EBP, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x40000)
    mob,head,respawn,mob_info,manager,infos,stop=[base+x for x in (0,0x5000,0x6000,0x7000,0x10000,0x30000,0x3f000)]
    pool=manager+0x114;state={};orders=[(1,0,2),(0,1,2),(0,2,1)]
    def pack(*values):return struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values])
    def put(at,*values):cpu.mem_write(at,pack(*values))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def idx(at):return state['objects'].index(at)
    def seq(values):return ','.join(map(str,values)) or '-'
    def nodes(at,sentinel=0):
        out=[]
        while at!=sentinel:assert len(out)<20;out.append(idx(at));at=get(at+4)
        return out
    def core():
        words=[get(mob+x) for x in (0x5c,0x224)]+list(struct.unpack('<2H',cpu.mem_read(mob+0x228,4)))
        words += [get(mob+x) for x in (0x248,0x244,0x24c,0x238,0x2b8,0x124,0xcbc,0x128,0x12c,0x284)]
        active=get(mob+0x44)!=0;count=get(mob+0x40) if active else 0
        words += [int(get(mob+0x60)!=0),int(active),count,2 if count else 0]
        return pack(*words).hex()
    def snapshot():
        active=nodes(get(head+4),head);free=nodes(get(pool));assert len(active)==get(mob+0x1e4) and len(free)==get(pool+8)
        payload=[]
        for i,at in enumerate(state['objects']):payload.append(':'.join(map(str,[i,*struct.unpack('<10I',cpu.mem_read(at+8,40)),(get(at+48)-infos)//256])))
        return '|'.join([core(),str(get(manager+0x10)),str(get(pool+4)),seq(active),seq(free),';'.join(payload),bytes(cpu.mem_read(mob+0xcf4,64)).hex()])
    def event(tag):state['events'].append(tag+':'+snapshot())
    def selected_detached():
        n=state['selected'];assert n not in nodes(get(head+4),head) and n not in nodes(get(pool))
    # Two immutable trampolines implement declared death-port callback scripts.
    for nested in (0,1):
        start=stop+128+nested*128;code=b'\x56\x57\x53\x55\xbe'+pack(mob)
        code+=b'\xe8'+struct.pack('<i',0x4a0990-(start+len(code)+5))
        if nested:
            code+=b'\x68'+pack(777)+b'\x6a\x02\x68'+pack(mob)+b'\xb8'+pack(infos+4*256)
            code+=b'\xe8'+struct.pack('<i',0x4a0aa0-(start+len(code)+5))
        code+=b'\x5d\x5b\x5f\x5e\xc2\x08\x00';cpu.mem_write(start,code)
    for at,code in [(0x503a3f,b'\xc3'),(0x505316,b'\xc3'),(0x490ee0,b'\xc2\x18\x00'),(0x490cf0,b'\xc2\x18\x00'),
                    (0x4910f0,b'\xc2\x08\x00'),(0x421220,b'\xc2\x20\x00'),(0x4906d0,b'\xc3'),(0x4a1810,b'\xc2\x0c\x00'),
                    (stop+16,b'\xc2\x04\x00'),(stop+32,b'\xc2\x04\x00'),(stop+48,b'\xc3')]:cpu.mem_write(at,code)
    put(0x5210cc,stop+16);put(0x5210d0,stop+32);put(0x5210e4,stop+48)
    cpu.mem_write(stop+64,bytes.fromhex('8b4c2404b801000000f00fc10140c20400'));put(0x5210f8,stop+64)
    def hook(machine,address,size,data):
        esp=machine.reg_read(UC_X86_REG_ESP)
        if address==stop+16:
            if get(esp+4)==mob+0x1cc:state['depth']+=1
        elif address==stop+32:
            if get(esp+4)==mob+0x1cc:state['depth']-=1;assert state['depth']>=0
        elif address==0x503a3f:
            assert state['seeding'],'Replacement/reentrant reuse must not allocate'
            at=base+0x20000+len(state['objects'])*64;state['objects'].append(at);cpu.mem_write(at,bytes(52));machine.reg_write(UC_X86_REG_EAX,at)
        elif address==0x4a0da0:
            selected_detached();event('R')
        elif address==stop+48:
            event('T');machine.reg_write(UC_X86_REG_EAX,(state['clock']+17*state['ticks'])&0xffffffff);state['ticks']+=1
        elif address==0x4a3be0:
            selected_detached();event('Q'+pack(get(esp+4)).hex())
        elif address in (0x490ee0,0x490cf0):
            selected_detached();assert get(esp+4)==mob and [get(esp+x) for x in (16,20,24)]==[0,0,0]
            raw=machine.reg_read(UC_X86_REG_EAX);values=bytes(cpu.mem_read(raw,12)) if address==0x490ee0 else pack(raw)
            event(('P' if address==0x490ee0 else 'H')+pack(get(esp+8),get(esp+12)).hex()+values.hex())
            old=state['objects'][state['selected']]
            if state['case']&8:
                put(mob+0x284,get(mob+0x284)^1);put(mob+0x60,0 if get(mob+0x60) else base+0x9000)
                put(old+0xc,0xffff0abc,0x1234);put(old+0x28,5,0x87654321)
                if address==0x490ee0:put(raw,get(raw)^0xffff,get(raw+4)^0x80000000,get(raw+8)^0xffff0000)
            killed=bool(state['case']&2)
            if killed:put(mob+0x224,0);put(mob+0x44,0);put(mob+0x124,2)
            machine.reg_write(UC_X86_REG_EAX,int(killed))
        elif address==0x4910f0:
            selected_detached();event('X'+pack(get(esp+4),get(esp+8)).hex());machine.reg_write(UC_X86_REG_EIP,stop+128+(128 if state['case']&4 else 0))
        elif address==0x421220:
            selected_detached();assert get(esp+12)==15 and [get(esp+x) for x in (20,24,28,32)]==[0x42700000,0,0,6]
            event('V'+bytes(cpu.mem_read(get(esp+8),15)).hex())
        elif address==0x4906d0:event('S')
        elif address==0x4a1810:
            ret=get(esp);at=machine.reg_read(UC_X86_REG_EBX if ret==0x4a09f7 else UC_X86_REG_EBP)
            action=get(esp+4);assert get(esp+8)==get(at+48)+3 and get(esp+12)==get(at+16)
            assert (idx(at) in nodes(get(head+4),head))==(action==6)
            event('L'+str(action)+','+str(idx(at)))
    cpu.hook_add(UC_HOOK_CODE,hook)
    def call(address,eax=0,esi=0,edi=0,args=()):
        put(base+0x3e000,stop,*args);cpu.reg_write(UC_X86_REG_ESP,base+0x3e000);cpu.reg_write(UC_X86_REG_EAX,eax);cpu.reg_write(UC_X86_REG_ESI,esi);cpu.reg_write(UC_X86_REG_EDI,edi);cpu.reg_write(UC_X86_REG_FPCW,0x27f)
        cpu.emu_start(address,stop,count=100000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
    print(f'# ps_game.exe SHA256 {digest}; real AddApplySkill 004a0aa0 -> RemApplySkill 004a0da0 -> TimeDamage 004a3be0/CRT -> declared combat/death callbacks -> real ClearApplySkill 004a0990 and optional real nested AddApplySkill. Native comparison must compose these owners, not replay expected outputs.')
    print('# case clock initial_counter initial18wordcore events final_snapshot. Death port declares real Clear plus optional nested admission; it is not the recovered full Death reward body. Damage port projects death state before Death. Snapshots core|counter|attempts|active|free|10word payload+definition|names64. R reversal,T clock,Q calculation,P/H combat,X death,V result,S speed,L action/node.')
    boundaries=[0,1,1000,0xffffffff,0x80000000,0x7fffffff]
    for case in range(256):
        cpu.mem_write(base,bytes(0x3e000));state.update(case=case,objects=[],events=[],depth=0,seeding=True,ticks=0,clock=boundaries[(case//3)%6])
        flags=case//64;stack=(case//16)%4;counter=[0,0xfffffffe,0xffffffff,0x7fffffff][(case//7)%4]
        put(0x546f3c,manager);put(manager+0x10,counter);put(mob+0x1c4,head,head);put(head,head,head)
        put(mob+0x5c,7);put(mob+0x224,boundaries[(case//5)%6]);cpu.mem_write(mob+0x228,struct.pack('<2H',100,50));put(mob+0x248,1);put(mob+0x24c,1);put(mob+0x238,7);put(mob+0xcbc,123)
        put(mob+0x284,flags&1);put(mob+0xc84,respawn if flags&2 else 0);cpu.mem_write(respawn+0x1c,b'\x01');put(mob+0x60,base+0x9000 if case%3 else 0)
        put(mob+0xa8,mob_info);put(mob_info+0x30,2000);cpu.mem_write(mob_info+0x34,struct.pack('<2H',200,150));cpu.mem_write(mob_info+0x40,b'\x07')
        put(mob+0x124,2 if stack in (1,2) else 0)
        if stack in (2,3):
            put(mob+0x44,1)
            if stack==3:put(mob+0x40,1);put(mob+0x34,base+0x8000,1);put(base+0x8000,base+0x8100);put(base+0x8100,base+0x8200);put(base+0x8228,2)
        cpu.mem_write(mob+0xcf4,b'U'*32+b'S'*32);cpu.mem_write(mob+0xcf4,b'BeforeUser\0');cpu.mem_write(mob+0xd14,b'BeforeSkill\0')
        for role,(ident,level,detail) in enumerate([(200,1,0x42),(100,1,0x3f if case&1 else 0x3d),(201,1,0x40),(100,2,0x48 if case&8 else 0x40),(999,1,0x44)]):
            at=infos+role*256;cpu.mem_write(at,struct.pack('<HB',ident,level));cpu.mem_write(at+3,('Def'+str(role)).encode()+b'\0');cpu.mem_write(at+0x31,b'\x03');cpu.mem_write(at+0x32,struct.pack('<H',detail));put(at+0x5c,4294967 if role==3 else 2)
            cpu.mem_write(at+0x6e,bytes([(case//5)%6]));cpu.mem_write(at+0x70,struct.pack('<3H',1,3,5))
        for i,role in enumerate(orders[case%3]):
            call(0x40a780,eax=manager);at=cpu.reg_read(UC_X86_REG_EAX);put(at+0xc,(200,100,201)[role],1,0x80000000+i,60+i,70+i,9999,42*i+case%17,1,42+i);put(at+48,infos+role*256)
            call(0x486200,esi=mob+0x1c4,edi=at)
            if role==1:state['selected']=i
        state['seeding']=False;state['events'].clear();initial=core()
        call(0x4a0aa0,eax=infos+3*256,args=(mob,2,555));assert state['depth']==0
        print(case,state['clock'],counter,initial,';'.join(state['events']),snapshot(),sep='\t')


if __name__=='__main__':main()
