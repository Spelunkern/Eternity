#!/usr/bin/env python3
"""Execute original whole-cell snapshots with NPCs; transport and locks are ports."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game

cpu, digest = load_game()
stack, cell, heap, npcs, user, stop = (0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x10000000)
zone,grid=0x34000000,0x35000000
for address in (stack,cell,heap,npcs,user,stop,zone,grid): cpu.mem_map(address,0x10000)
def put(address,*values): cpu.mem_write(address,struct.pack('<'+'I'*len(values),*values))
def get(address): return struct.unpack('<I',cpu.mem_read(address,4))[0]
for offset,links in ((0,8),(0x2c,0x48),(0x58,0),(0x84,0),(0xb0,0),(0xdc,0x28)):
    head=heap+offset*4;put(cell+offset,head);put(head+links,head+links,head+links)
for pointer in (0x5210cc,0x5210d0): put(pointer,stop+0x100)
cpu.mem_write(stop+0x100,b'\xc2\x04\x00')
cpu.mem_write(0x4d4d20,b'\xc2\x08\x00')
cpu.mem_write(0x505316,b'\xc3')
events=[]
reenter=False
nested=False
remove=False
trampoline=stop+0x200
# Save all registers; call the real enter routine, then return from the send port.
code=b"\x60\xbf"+struct.pack("<I",cell)+b"\xb9"+struct.pack("<I",user)
code+=b"\xe8"+struct.pack("<i",0x423aa0-(trampoline+len(code)+5))+b"\x61\xc2\x08\x00"
cpu.mem_write(trampoline,code)
remove_trampoline=stop+0x300
code=b"\x60\xb8"+struct.pack("<I",zone)+b"\xbe"+struct.pack("<I",npcs)
code+=b"\xe8"+struct.pack("<i",0x416e90-(remove_trampoline+len(code)+5))+b"\x61\xc2\x08\x00"
cpu.mem_write(remove_trampoline,code)
cpu.mem_write(0x416ca0,b"\xc3")
cpu.mem_write(0x4208c0,b"\xc2\x0c\x00")
put(zone+0x30,grid);put(grid,cell)
def hook(machine,address,size,_):
    global nested,remove
    if address==0x4d4d20:
        sp=machine.reg_read(UC_X86_REG_ESP);packet,length=struct.unpack('<2I',machine.mem_read(sp+4,8));events.append(bytes(machine.mem_read(packet,length)))
        if reenter: put(cell+0x5c,get(get(cell+0x58)))
        if nested:
            nested=False;machine.reg_write(UC_X86_REG_EIP,trampoline)
        elif remove:
            remove=False;machine.reg_write(UC_X86_REG_EIP,remove_trampoline)
cpu.hook_add(UC_HOOK_CODE,hook)
def call(address,enter):
    put(stack+0x8000,stop,*(() if enter else (user,)));cpu.reg_write(UC_X86_REG_ESP,stack+0x8000)
    cpu.reg_write(UC_X86_REG_EDI if enter else UC_X86_REG_ESI,cell)
    if enter: cpu.reg_write(UC_X86_REG_ECX,user)
    cpu.emu_start(address,stop,count=1000000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop: raise RuntimeError('original snapshot did not return')
head=get(cell+0x58)
for count in range(4):
    events.clear()
    nodes=[npcs+i*0x100 for i in range(count)]
    put(head, nodes[-1] if nodes else head, nodes[0] if nodes else head)
    expected=[]
    for i,node in enumerate(nodes):
        put(node,head if i==0 else nodes[i-1],head if i+1==count else nodes[i+1])
        put(node+8,0x3f800000+i,0x80000000+i,0x7f800000+i,0x12345678+i)
        put(node+0x34,0xabcdef10+i,0x76543210+i,0xfedcba98+i)
        expected.append(struct.pack('<HIBHIIIH',0xe01,0x12345678+i,(0xabcdef10+i)&255,(0x76543210+i)&65535,0x3f800000+i,0x80000000+i,0x7f800000+i,(0xfedcba98+i)&65535))
    call(0x423aa0,True)
    assert events==expected,(count,events,expected)
    events.clear();call(0x424130,False)
    assert events==[struct.pack('<HI',0xe02,0x12345678+i) for i in range(count)]
reenter=True
events.clear();call(0x423aa0,True)
assert len(events)==1, events
events.clear();call(0x424130,False)
assert len(events)==1, events
reenter=False
nested=True;events.clear();call(0x423aa0,True)
ids=[struct.unpack_from('<I',packet,2)[0] for packet in events]
assert ids==[0x12345678,0x12345678,0x12345679,0x1234567a,0x12345678,0x12345679,0x1234567a], ids
put(npcs+0x1c,0,0);put(cell+0x78,3)
remove=True;events.clear();call(0x423aa0,True)
ids=[struct.unpack_from('<I',packet,2)[0] for packet in events]
assert ids==[0x12345678,0x12345679,0x1234567a],ids
print(f'NPC snapshots: 4 original cases, 2 synthetic cursor cases a real nested snapshot and real current-NPC removal passed; ps_game.exe SHA256 {digest}')

# User-list entry speed packet ID bytes may depend on incoming stack contents.
# Fill the stack before each original call to expose any unwritten bytes.
user_head=get(cell);put(user_head+8,user+8,user+8);put(user+8,user_head+8,user_head+8)
put(cell+0x58,heap+0x58*4);npc_head=get(cell+0x58);put(npc_head,npc_head,npc_head)
put(user+0xdc,0x12345678)
for fill in (0x55,0xaa):
    cpu.mem_write(stack,bytes([fill])*0x10000);events.clear();call(0x423aa0,True)
    speed=next(packet for packet in events if packet[:2]==b"\x1c\x05")
    assert speed==bytes((0x1c,0x05,fill,fill,fill,fill,0,0)),speed.hex()
print('User cell-entry speed ID retains four prior stack bytes in both controlled cases')
# Real door-list block after all item packets, with two different byte values.
put(user_head+8,user_head+8,user_head+8)
door_head=get(cell+0xdc);doors=[npcs+0x1000,npcs+0x1100]
put(door_head+0x28,doors[-1]+0x28,doors[0]+0x28)
for i,door in enumerate(doors):
    put(door+0x28,(door_head if i==0 else doors[i-1])+0x28,(door_head if i+1==len(doors) else doors[i+1])+0x28)
    cpu.mem_write(door+0xc,bytes([0x12+i]));cpu.mem_write(door+0x30,bytes([i]))
events.clear();call(0x423aa0,True)
assert events==[bytes((3,4,0x12,0)),bytes((3,4,0x13,1))],events
events.clear();call(0x424130,False);assert not events
print('Door entry/leave snapshots: 2 original doors passed')
nested=True;events.clear();call(0x423aa0,True)
assert [p[2] for p in events]==[0x12,0x12,0x13,0x12,0x13],events
print('Door nested snapshot: original shared cursor restart passed')
