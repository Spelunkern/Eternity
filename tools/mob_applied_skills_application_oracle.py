#!/usr/bin/env python3
"""Execute sustained original AddApplySkill/refresh/replacement and native-facing ports."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EBP, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game

DEFINITIONS = [(100,1,3,0x40,1), (100,1,3,0x41,3), (100,2,3,0x41,4294967),
               (100,1,3,0x42,2), (100,4,3,0x43,2), (100,4,3,0x48,0),
               (101,1,3,0x44,1), (101,2,3,0x48,1), (102,1,2,0x40,1),
               (103,0,3,0x3e,1), (103,3,3,0x43,1), (104,255,3,0x42,1),
               (104,1,3,0x44,1), (105,1,3,0x48,1), (105,1,3,0x48,9),
               (106,1,3,0x40,1), (106,1,3,0x40,2)]


def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x40000)
    mob,head,respawn,mob_info,manager,infos,stop=[base+x for x in (0,0x5000,0x6000,0x7000,0x10000,0x30000,0x3f000)]
    pool=manager+0x114;state={}
    fields=[0x230,0x22c,0x234,0x240,0x23c,*range(0x188,0x1b8,4),0x134,0x150,0x16c,
            0x248,0x244,0x24c,0x238,0x2b8,0x224,0x124,0x12c,0xcbc,0x128,0x284]
    def pack(*v):return struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v])
    def put(at,*v):cpu.mem_write(at,pack(*v))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def index(at):return state['objects'].index(at)
    def seq(v):return ','.join(map(str,v)) or '-'
    def nodes(at,sentinel=0):
        out=[]
        while at!=sentinel:assert len(out)<40;out.append(index(at));at=get(at+4)
        return out
    def core():return b''.join(bytes(cpu.mem_read(mob+x,4)) for x in fields).hex()
    def snapshot():
        active=nodes(get(head+4),head);free=nodes(get(pool));assert len(active)==get(mob+0x1e4) and len(free)==get(pool+8)
        payload=[]
        for i,at in enumerate(state['objects']):
            definition=(get(at+48)-infos)//256 if get(at+48) else 255
            payload.append(':'.join(map(str,[i,*struct.unpack('<10I',cpu.mem_read(at+8,40)),definition])))
        return '|'.join([core(),str(get(manager+0x10)),str(get(pool+4)),seq(active),seq(free),';'.join(payload) or '-',seq([get(infos+i*256+0x5c) for i in range(len(DEFINITIONS))])])
    def event(tag):state['events'].append(tag+':'+snapshot())
    for address,code in [(0x503a3f,b'\xc3'),(0x505316,b'\xc3'),(0x4902f0,b'\xc3'),(0x4906d0,b'\xc3'),
                         (0x4a1810,b'\xc2\x0c\x00'),(stop+16,b'\xc2\x04\x00'),(stop+32,b'\xc2\x04\x00'),(stop+48,b'\xc3')]:cpu.mem_write(address,code)
    put(0x5210cc,stop+16);put(0x5210d0,stop+32);put(0x5210e4,stop+48)
    cpu.mem_write(stop+64,bytes.fromhex('8b4c2404b801000000f00fc10140c20400'));put(0x5210f8,stop+64)
    def hook(machine,address,size,data):
        esp=machine.reg_read(UC_X86_REG_ESP)
        if address==stop+16:
            if get(esp+4)==mob+0x1cc:state['depth']+=1
        elif address==stop+32:
            if get(esp+4)==mob+0x1cc:state['depth']-=1;assert state['depth']>=0
        elif address==0x503a3f:
            at=0 if state['fail'] else base+0x20000+len(state['objects'])*64
            if at:state['objects'].append(at);cpu.mem_write(at,b'\xa5'*52)
            machine.reg_write(UC_X86_REG_EAX,at)
        elif address==stop+48:
            assert state['depth']==1;event('T')
            if state['case']&1:
                at=infos+state['incoming']*256+0x5c;put(at,get(at)+1)
            machine.reg_write(UC_X86_REG_EAX,(state['clock']+state['ticks']*17)&0xffffffff);state['ticks']+=1
        elif address in (0x4902f0,0x4906d0):
            assert state['depth']==1;event('B' if address==0x4902f0 else 'S')
            if state['case']&2:put(mob+0x284,get(mob+0x284)^1)
        elif address==0x4a1810:
            assert state['depth']>=1
            at=machine.reg_read(UC_X86_REG_EBX if state['op']=='C' else UC_X86_REG_EBP)
            action=get(esp+4);assert get(esp+8)==get(at+48)+3 and get(esp+12)==get(at+16)
            active=nodes(get(head+4),head)
            assert (index(at) in active)==(action==6)
            event('L'+str(action)+','+str(index(at)))
    cpu.hook_add(UC_HOOK_CODE,hook)
    def call(address,esi=0,eax=0,args=()):
        put(base+0x3e000,stop,*args);cpu.reg_write(UC_X86_REG_ESP,base+0x3e000)
        cpu.reg_write(UC_X86_REG_ESI,esi);cpu.reg_write(UC_X86_REG_EAX,eax)
        cpu.emu_start(address,stop,count=100000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
    print(f'# ps_game.exe SHA256 {digest}; real AddApplySkill 004a0aa0, AddApplySkillDebuff 004a0ca0, RemApplySkill 004a0da0 non-damage branches, SetSkillAbility 004a1420, manager creation/list/pool/GetStatus. Observed required clock/log/rebuild/speed/allocation/lock ports; locked xadd for InterlockedIncrement.')
    print('# S case flags stack_mode clock counter initial31wordhex; A definition attacker_type/id fail events snapshot; C events snapshot. Clock modes mutate incoming keep seconds; effect callbacks may toggle live Lua log gate. Snapshot core|counter|attempts|active|free|ten-word payload+definition|live keep seconds.')
    boundaries=[0,1,0xffffffff,0x7fffffff,0x80000000,65535,0xffff0000]
    for case in range(64):
        cpu.mem_write(base,bytes(0x3e000));flags=(case//4)%4;stack=(case//16)%4;clock=[0,0xfffffffe,0x80000000,0x7fffffff][case%4];counter=[0,0xfffffffe,0xffffffff,0x80000000][(case//4)%4]
        state.update(objects=[],events=[],depth=0,fail=False,case=case,clock=clock,ticks=0,incoming=0,op='A')
        put(0x546f3c,manager);put(manager+0x10,counter);put(mob+0x1c4,head,head);put(head,head,head)
        for i,at in enumerate(fields):put(mob+at,boundaries[(case+i)%7])
        put(mob+0x124,2 if stack in (1,2) else 0);put(mob+0x284,flags&1)
        if stack in (2,3):
            put(mob+0x44,1)
            if stack==3:put(mob+0x40,1);put(mob+0x34,base+0x8000,1);put(base+0x8000,base+0x8100);put(base+0x8100,base+0x8200);put(base+0x8228,2)
        put(mob+0xc84,respawn if flags&2 else 0);cpu.mem_write(respawn+0x1c,b'\x01');put(mob+0xa8,mob_info);cpu.mem_write(mob_info+0x40,b'\x07')
        for i,(ident,level,effect,detail,keep) in enumerate(DEFINITIONS):
            at=infos+i*256;cpu.mem_write(at,struct.pack('<HB',ident,level));cpu.mem_write(at+3,('Def'+str(i)).encode()+b'\0');cpu.mem_write(at+0x31,bytes([effect]));cpu.mem_write(at+0x32,struct.pack('<H',detail));put(at+0x5c,keep)
            abilities=((0,0),(16,65535),(32,65535)) if i==9 else (((20,1),(27,32768),(31,65535)) if i==10 else ((7,1),(8,2),(9,3)))
            for j,(kind,value) in enumerate(abilities):cpu.mem_write(at+0x7c+j*4,struct.pack('<BBH',kind,0,value))
        print('S',case,flags,stack,clock,counter,core(),sep='\t')
        operations=[(8,1,42,0),(0,1,42,1),(0,1,42,0),(1,2,43,1),(2,1,44,1),(3,1,45,0),(4,1,46,1),(4,1,46,0),(5,2,0,1),(5,2,777,1),(6,1,48,0),(7,1,999,1),(9,1,50,0),(10,1,51,1),(11,1,52,0),(12,1,53,0),(13,1,54,0),(14,2,888,1),None,(15,1,55,1),(16,2,56,1),None,(15,1,57,1),None]
        for op in operations:
            state['events'].clear()
            if op is None:
                state['op']='C';call(0x4a0990,esi=mob);print('C',';'.join(state['events']) or '-',snapshot(),sep='\t')
            else:
                state['op']='A';state['incoming']=op[0];state['fail']=bool(op[3]);call(0x4a0aa0,eax=infos+op[0]*256,args=(mob,op[1],op[2]));print('A',*op,';'.join(state['events']) or '-',snapshot(),sep='\t')
            assert state['depth']==0


if __name__=='__main__':main()
