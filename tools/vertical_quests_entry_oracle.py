#!/usr/bin/env python3
"""Execute original active/finished quest entry batching and lock ordering."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EDI, UC_X86_REG_ESI,
                               UC_X86_REG_ESP, UC_X86_REG_EIP)
from original_oracle import load_game

cpu, digest = load_game()
base = 0x30000000
cpu.mem_map(base, 0x40000)
user, head, nodes, definition, stack, stop = (base, base+0x7000, base+0x10000,
                                                base+0x20000, base+0x3d000, base+0x3f000)
state = {}

def put(at, value):
    cpu.mem_write(at, struct.pack('<I', value & 0xffffffff))

def get(at):
    return struct.unpack('<I', cpu.mem_read(at, 4))[0]

def hook(machine, at, size, data):
    sp=machine.reg_read(UC_X86_REG_ESP)
    if at==stop+0x100:
        assert get(sp+4)==user+(0x11fc if state['active'] else 0x11d0)
        assert not state['locked'];state['locked']=True;state['events'].append('L')
    elif at==stop+0x120:
        assert get(sp+4)==user+(0x11fc if state['active'] else 0x11d0)
        assert state['locked'];state['locked']=False;state['events'].append('U')
    elif at==stop+0x140:
        state['events'].append('T');machine.reg_write(UC_X86_REG_EAX,100)
    elif at==0x4d4d20:
        packet=bytes(machine.mem_read(get(sp+4),get(sp+8)))
        op,count=struct.unpack_from('<HB',packet)
        width=7 if state['active'] else 3
        assert op==(0x901 if state['active'] else 0x906) and len(packet)==3+count*width
        ids=[struct.unpack_from('<H',packet,3+i*width)[0] for i in range(count)]
        assert ids==list(range(state['next'],state['next']+count))
        for i in range(count):
            offset=3+i*width
            if state['active']:
                assert packet[offset+2:offset+7]==bytes((1,0,1,2,3))
            else:
                assert packet[offset+2]==1
        state['next']+=count
        state['events'].append(('S' if state['locked'] else 's',count))

for at,cleanup in ((stop+0x100,4),(stop+0x120,4),(stop+0x140,0),(0x4d4d20,8),(0x505316,0)):
    cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup))
put(0x5210cc,stop+0x100);put(0x5210d0,stop+0x120);put(0x5210e4,stop+0x140)
cpu.hook_add(UC_HOOK_CODE,hook)
print('# ps_game.exe SHA256',digest,'SendCharacterQuest 004835f0 / SendCharacterQuestFinish 00483730')
for active in (True,False):
    for count in (0,1,254,255,256,510):
        cpu.mem_write(base,bytes(0x30000));put(user+(0x11f4 if active else 0x11c8),head)
        put(head+4,nodes if count else head);put(definition+0x128,1)
        for i in range(count):
            node=nodes+i*64;put(node+4,nodes+(i+1)*64 if i+1<count else head)
            put(node+8,i+1);put(node+0x14,60100);put(node+0x20,definition)
            cpu.mem_write(node+0xc,b'\x01');cpu.mem_write(node+0x1c,b'\x01\x02\x03')
        put(stack,stop);state.update(active=active,locked=False,events=[],next=1)
        for reg,value in ((UC_X86_REG_ESP,stack),(UC_X86_REG_EDI,user),(UC_X86_REG_ESI,user)):
            cpu.reg_write(reg,value)
        cpu.emu_start(0x4835f0 if active else 0x483730,stop,count=500000)
        assert cpu.reg_read(UC_X86_REG_EIP)==stop and not state['locked']
        expected=(['T'] if active else [])+['L']+[('S',255)]*(count//255)+['U']
        if count%255:expected.append(('s',count%255))
        assert state['events']==expected and state['next']==count+1,(active,count,state['events'],expected)
        print('active' if active else 'finished',count,state['events'])
