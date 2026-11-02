#!/usr/bin/env python3
"""Execute original world waiting lists and both zone-move entry owners.

Ghidra work/world-admission-owner, work/world-admission-lists, work/zone-admission.
Original intrusive list bodies/cursors and state transitions run. Packet bodies,
zone selection/admission, world registration, rank clock, transport and OS locks
are ports. A packet-body trampoline calls original WaitUser during list iteration.
"""
import itertools
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,base,world,zone,map_ptr,guild,data,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000,0x35000000,0x10000000
    for at in (stack,world,zone,map_ptr,guild,data,stop):cpu.mem_map(at,0x10000)
    cpu.mem_map(base,0x30000);users=[base+i*0x10000 for i in range(3)];events=[];state={}
    def word(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def put(at,*values):cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    def index(user):return users.index(user)
    def hook(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x466cc0:
            user=machine.reg_read(UC_X86_REG_ESI);i=index(user);events.append('PACKETS '+str(i))
            if i<2 and state['wait']&(1<<i):state['wait']&=~(1<<i);machine.reg_write(UC_X86_REG_EIP,stop+0x300)
        elif address==0x413df0:
            i=index(word(sp+12));events.append('SELECT '+str(i));machine.reg_write(UC_X86_REG_EAX,0 if state['fail']&(1<<i) else zone)
        elif address==0x40d450:events.append('REGISTER '+str(index(machine.reg_read(UC_X86_REG_EDI))))
        elif address==0x415f80:
            user=machine.reg_read(UC_X86_REG_EDI);events.append('ENTER '+str(index(user)))
            if state['success']:put(user+0xe0,word(sp+4))
            machine.reg_write(UC_X86_REG_EAX,state['success'])
        elif address==0x428d20:
            i=index(machine.reg_read(UC_X86_REG_EBX));events.append('WINDOW '+str(i))
            if state['open']:put(word(sp+8),0x1501010b)
            machine.reg_write(UC_X86_REG_EAX,state['open'])
        elif address==0x4d43a0:events.append('CLOSE '+str(index(machine.reg_read(UC_X86_REG_ECX)))+' '+str(word(sp+4)))
        elif address==0x4d9510:
            ret=word(sp)
            if ret==0x40cf4e:user=machine.reg_read(UC_X86_REG_ESI);kind=0
            elif ret==0x40c11d:user=machine.reg_read(UC_X86_REG_ESI);kind=3
            elif ret==0x40d097:user=machine.reg_read(UC_X86_REG_ESI);kind=2
            elif ret==0x44b8dd:user=machine.reg_read(UC_X86_REG_EBX);kind=1
            elif ret==0x44ba17:user=machine.reg_read(UC_X86_REG_EBX);kind=2
            else:raise RuntimeError(f'Unknown original admission diagnostic return {ret:08x}')
            events.append('DIAGNOSTIC '+str(index(user))+' '+str(kind))
        elif address in (stop+0x100,stop+0x120):
            if word(sp+4)==guild+0x578:
                user=machine.reg_read(UC_X86_REG_EBX);events.append(('GUILD_LOCK ' if address==stop+0x100 else 'GUILD_UNLOCK ')+str(index(user)))
                if address==stop+0x100 and state['vanish']:put(user+0x1758,0)
        elif address==stop+0x140:
            at=word(sp+4);value=(word(at)+1)&0xffffffff;put(at,value);events.append('GUILD_INCREMENT '+str(index(machine.reg_read(UC_X86_REG_EBX))));machine.reg_write(UC_X86_REG_EAX,value)
    for at,cleanup in ((0x466cc0,0),(0x413df0,12),(0x40d450,4),(0x415f80,4),(0x428d20,8),(0x4d43a0,8),(0x4d9510,0),(stop+0x100,4),(stop+0x120,4),(stop+0x140,4)):
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
    for iat,target in ((0x5210cc,stop+0x100),(0x5210d0,stop+0x120),(0x5210f8,stop+0x140)):put(iat,target)
    # pushad; mov eax,esi; call original WaitUser; popad; ret.
    trampoline=stop+0x300;cpu.mem_write(trampoline,b'\x60\x89\xf0\xe8'+struct.pack('<i',0x40ce70-(trampoline+8))+b'\x61\xc3')
    def init_list(at,root,items):
        put(at,root,root);put(root+8,root+8,root+8);put(at+0x20,len(items));previous=root+8
        for user in items:put(user+8,previous,root+8);put(previous+4,user+8);previous=user+8
        put(root+8,previous)
    def call(address,i=0):
        put(stack+0x8000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,users[i]);cpu.reg_write(UC_X86_REG_EBX,world);cpu.reg_write(UC_X86_REG_ECX,world)
        cpu.emu_start(address,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original world admission did not return')
    def snapshot():
        values=[word(world+0x120),word(world+0x14c),word(world+0x178),word(zone+0x54),word(guild+0xa0)]
        for user in users:values.extend((word(user+0x5758),word(user+0x57d8),word(user+0x57dc),int(bool(word(user+0xe0))),word(user+0x54c8),word(user+0x54c4),int(bool(word(user+0x1758)))))
        print('STATE',*values)
    rng=random.Random(0x40bf90);cases=[]
    for location,mode,permission,opened,has_guild,vanish in itertools.product((0,1,2,3,4),(0,3,5,6),(0,10,11,255),(False,True),(False,True),(False,True)):
        cases.append((location,rng.randrange(4),rng.randrange(4),rng.randrange(2),mode,permission,int(opened),3 if has_guild else 0,int(vanish)))
    for _ in range(384):cases.append((rng.choice((0,1,2,3,4,0xffffffff)),rng.randrange(4),rng.randrange(4),rng.randrange(2),rng.choice((0,1,2,3,4,5,6,7)),rng.randrange(256),rng.randrange(2),rng.choice((0,1,3)),rng.randrange(2)))
    print(f'# ps_game.exe SHA256 {digest}; WaitUser 0040ce70, UpdatePackets 0040bf90, CUser::ZoneEnterUserMove 0044b8a0, CWorld::ZoneEnterUserMove 0040d000; list bodies execute; selection/entry/clock/registration/OS/transport are ports')
    for location,fail,wait,success,mode,permission,opened,guild_mask,vanish in cases:
        cpu.mem_write(world,bytes(0x10000));cpu.mem_write(zone,bytes(0x10000));cpu.mem_write(base,bytes(0x30000));put(zone+0x24,map_ptr);put(map_ptr+0x1a4,mode);put(guild+0xa0,0xfffffffe)
        init_list(world+0x100,data,[users[2]]);init_list(world+0x12c,data+0x1000,users[:2]);init_list(world+0x158,data+0x2000,[]);init_list(zone+0x34,data+0x3000,[])
        for i,user in enumerate(users):
            put(user+0x5758,location if i==0 else 1 if i==1 else 0);put(user+0x57d8,7,8);put(user+0x578c,i+1);put(user+0x54c8,77);put(user+0x54c4,88);put(user+0x1758,guild if guild_mask&(1<<i) else 0);cpu.mem_write(user+0x5768,bytes([permission]));cpu.mem_write(user+0x164,struct.pack('<H',20))
        state.update(fail=fail,wait=wait,success=success,open=opened,vanish=vanish)
        print('CASE',location,fail,wait,success,mode,permission,opened,guild_mask,vanish)
        for op,i,address in (('WAIT',0,0x40ce70),('UPDATE',0,0x40bf90),('CLIENT',0,0x44b8a0),('WORLD',1,0x40d000),('CLIENT',0,0x44b8a0),('UPDATE',0,0x40bf90),('CLIENT',1,0x44b8a0)):
            events.clear();call(address,i);print('STEP',op,i)
            for event in events:print('EVENT',event)
            snapshot()
        print('END')


if __name__=='__main__':main()
