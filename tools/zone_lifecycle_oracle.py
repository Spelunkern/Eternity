#!/usr/bin/env python3
"""Execute original EnterZone, ResetKeepTick, map checks and NProtect control.

Ghidra work/zone-map-skills and work/zone-entry-lifecycle. Auth provider,
RemApplySkill, party delivery, OS locks/clock and transport are boundary ports.
Original intrusive removals, pool release, query bytes and field writes execute.
"""
import itertools
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,user,nodes,defs,obj,auth,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000,0x10000000
    for at in (stack,user,nodes,defs,obj,auth,stop):cpu.mem_map(at,0x10000)
    def word(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def put(at,*values):cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    events=[];state={}
    def hook(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==stop+0x100:
            tick=state['tick'];state['tick']=(tick+7)&0xffffffff;events.append('TICK '+str(tick));machine.reg_write(UC_X86_REG_EAX,tick)
        elif address in (0x4d4d20,0x4d4f10):
            packet=bytes(cpu.mem_read(word(sp+4),word(sp+8)))
            if address==0x4d4f10:packet=b'DB '+packet
            else:packet=b'SELF '+packet
            events.append(packet[:packet.index(b' ')+1].decode()+packet[packet.index(b' ')+1:].hex())
        elif address==0x51c080:
            events.append('QUERY');put(auth+12,*state['query']);machine.reg_write(UC_X86_REG_EAX,state['error'])
        elif address==0x4d43a0:events.append('CLOSE '+str(word(sp+4)))
        elif address==0x4d9510:events.append('ERROR '+str(state['error']))
        elif address==0x4847c5:events.append('FREE '+str(word(machine.reg_read(UC_X86_REG_ESI)+8)))
        elif address==0x4829a0:
            events.append('PARTY '+str(word(sp+4)&65535)+' '+str(word(sp+8)&255))
    for at,cleanup in ((stop+0x100,0),(stop+0x120,4),(0x4d4d20,8),(0x4d4f10,8),(0x51c080,0),(0x4d43a0,8),(0x4d9510,0),(0x484c60,0),(0x4829a0,8)):
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
    cpu.hook_add(UC_HOOK_CODE,hook,begin=0x4847c5,end=0x4847c5)
    for iat,target in ((0x5210e4,stop+0x100),(0x5210cc,stop+0x120),(0x5210d0,stop+0x120)):put(iat,target)
    put(0x546f3c,obj);put(0x546f48,obj+0x1000)
    rng=random.Random(0x44ae30)
    print(f'# ps_game.exe SHA256 {digest}; EnterZone 0044ae30, ResetKeepTick 00486230, ChkApplySkillByMap 00484680, NProtectCSASend 0044b700; OS/auth/unapply/transport/party ports')
    for count,map_id,enabled,present,received,error,party in itertools.product((0,1,9,55,160),(1,18),(0,1),(0,1),(0,1),(0,7),(0,1)):
        cpu.mem_write(user,bytes(0x10000));cpu.mem_write(nodes,bytes(0x10000));cpu.mem_write(obj,bytes(0x10000));cpu.mem_write(stack,bytes(0x10000))
        tick=rng.choice((0,12345,0xfffffff0,0xfff24000));position=[rng.getrandbits(32) for _ in range(3)];query=[rng.getrandbits(32) for _ in range(4)]
        root=user+0x7000;put(user+0xa94,root,root);put(root,root,root);put(user+0xab4,count);put(user+0x578c,42);previous=root
        entries=[]
        for i in range(count):
            at=nodes+i*0x40;sid=(111,124,200)[i%3];level=(1,2,100)[(i//3)%3];detail=(0,61,81)[i%3]
            put(at,previous,root,1000+i,sid,level);put(previous+4,at);previous=at
            put(at+0x24,777);put(at+0x30,defs+i*0x100);cpu.mem_write(defs+i*0x100+0x32,struct.pack('<H',detail));entries.append((sid,level,detail))
        put(root,previous);put(user+0xd0,*position);put(user+0x1528,99);cpu.mem_write(user+0x1434,b'\x01');cpu.mem_write(user+0x164,struct.pack('<H',map_id));put(user+0x173c,party)
        cpu.mem_write(0x541f7c,bytes([enabled]));put(user+0x5520,auth if present else 0);cpu.mem_write(user+0x5518,bytes([received]));put(user+0x551c,77)
        state.update(tick=tick,error=error,query=query);events.clear();put(stack+0x8000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,user)
        cpu.emu_start(0x44ae30,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original lifecycle did not return')
        print('CASE',count,map_id,enabled,present,received,error,party,tick,*position,*query)
        for event in events:print('EVENT',event)
        print('STATE',cpu.mem_read(user+0x1434,1)[0],word(user+0x1528),*struct.unpack('<11I',cpu.mem_read(user+0x5494,44)),cpu.mem_read(user+0x5518,1)[0],word(user+0x551c))
        print('END')


if __name__=='__main__':main()
