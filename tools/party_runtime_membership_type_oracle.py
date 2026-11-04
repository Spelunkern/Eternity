#!/usr/bin/env python3
"""Execute original GetPartyType including recursive/migrating membership locks."""
import struct
from original_oracle import load_game
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP


def main():
    cpu, digest = load_game()
    base=0x30000000
    cpu.mem_map(base,0x100000)
    parties=[base,base+0x1000]
    user,other,stack,stop=base+0x10000,base+0x20000,base+0xf0000,base+0xff000
    lock,unlock=stop+16,stop+32
    def put(at,value):cpu.mem_write(at,struct.pack('<I',value & 0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def membership():
        at=get(user+0x173c)
        return 0 if not at else parties.index(at)+1
    def state():
        values=[membership(),*context['depth']]
        values += [get(p+0x10) for p in parties]
        values += [get(p+0xc) for p in parties]
        values += [int(get(p+0x18+get(p+0xc)*8)==user) for p in parties]
        return struct.pack('<9I',*values)
    cpu.mem_write(lock,b'\xc2\x04\x00');cpu.mem_write(unlock,b'\xc2\x04\x00')
    put(0x5210cc,lock);put(0x5210d0,unlock)
    context={}
    def hook(machine,at,size,data):
        if at not in (lock,unlock):return
        party=get(machine.reg_read(UC_X86_REG_ESP)+4)-0x118
        index=parties.index(party)
        context['depth'][index]+=1 if at==lock else -1
        assert min(context['depth'])>=0
        membership_value,target,count,leader,match=context['actions'][context['ordinal']]
        context['ordinal']+=1
        if membership_value!=255:put(user+0x173c,0 if not membership_value else parties[membership_value-1])
        if target:
            selected=parties[target-1]
            put(selected+0x10,count);put(selected+0xc,leader);put(selected+0x18+leader*8,user if match else other)
        context['events']+=struct.pack('<2I',index+1,int(at==lock))+state()
    cpu.hook_add(UC_HOOK_CODE,hook)
    print(f'# ps_game.exe SHA256 {digest}; unmodified CUser::GetPartyType 0048a330; only recursive critical-section ports replaced. Lock/unlock callbacks explicitly mutate membership/count/leader and preserve captured party lifetimes.')
    print('# initial_membership; per party count leader match kind; 4 callback actions membership(255 keep) target count leader match; result final_state_hex events_hex')
    counts=(-1,0,1,2,7,2147483647)
    for case in range(1024):
        cpu.mem_write(base,bytes(0xfd000))
        initial=case%3
        descriptors=[]
        for i,party in enumerate(parties):
            count=counts[(case//3+i)%len(counts)]
            leader=(case//9+i*3)%7
            match=(case//7+i)%2
            kind=(case//11+i)%3
            descriptors += [count,leader,match,kind]
            put(party+0x10,count);put(party+0xc,leader);cpu.mem_write(party+0x114,bytes([kind]))
            for slot in range(7):put(party+0x18+slot*8,user if slot==leader and match else other)
        put(user+0x173c,0 if not initial else parties[initial-1])
        actions=[]
        for step in range(4):
            mode=(case//(3*(4**step)))%4
            membership_value=(255,0,1,2)[mode]
            target=(case//(13+step*2)+step)%3
            count=counts[(case//(5+step)+step)%len(counts)]
            leader=(case//(7+step)+step)%7
            match=(case//(17+step)+step)%2
            actions.append((membership_value,target,count,leader,match))
        context.update(depth=[0,0],ordinal=0,events=b'',actions=actions)
        put(stack,stop);put(stack+4,user);cpu.reg_write(UC_X86_REG_ESP,stack)
        cpu.emu_start(0x48a330,stop,count=10000)
        assert context['depth']==[0,0]
        print(initial,*descriptors,*[value for action in actions for value in action],cpu.reg_read(UC_X86_REG_EAX),state().hex(),context['events'].hex() or '-')

if __name__=='__main__':main()
