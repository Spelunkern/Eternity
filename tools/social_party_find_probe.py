#!/usr/bin/env python3
"""Execute ps_game PacketComm 220c with controlled live zone-map and sends."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESP
from original_oracle import load_game

cpu, digest = load_game()
stack, user, zone, nodes, peers, stop = 0x20000000, 0x30000000, 0x32000000, 0x33000000, 0x34000000, 0x10000000
for address, size in ((stack, 0x10000), (user, 0x10000), (zone, 0x10000), (nodes, 0x10000), (peers, 0x300000), (stop, 0x10000)):
    cpu.mem_map(address, size)
def put(address, value): cpu.mem_write(address, struct.pack('<I', value))
def get(address): return struct.unpack('<I', cpu.mem_read(address, 4))[0]
def byte(address, value): cpu.mem_write(address, bytes([value]))
def word(address, value): cpu.mem_write(address, struct.pack('<H', value))
events = []
def hook(machine, address, size, context):
    sp = machine.reg_read(UC_X86_REG_ESP)
    if address == 0x4d4d20:
        events.append((machine.reg_read(UC_X86_REG_ECX), bytes(machine.mem_read(get(sp+4), get(sp+8)))))
    elif address == 0x411c50:
        cursor = machine.reg_read(UC_X86_REG_EDX)
        put(cursor, get(get(cursor)))
for address, pop in ((0x4d4d20, 8), (0x411c50, 0), (stop+0x100, 4)):
    cpu.mem_write(address, b'\xc2'+struct.pack('<H', pop))
    cpu.hook_add(UC_HOOK_CODE, hook, begin=address, end=address)
put(0x5210cc, stop+0x100); put(0x5210d0, stop+0x100)

def run(entries, level=20, country=1, party=0, old_flag=0):
    events.clear();cpu.mem_write(user, bytes(0x10000));cpu.mem_write(zone, bytes(0x10000));cpu.mem_write(nodes, bytes(0x10000))
    put(user+0xe0, zone);put(user+0x173c, party);byte(user+0x1744, old_flag);byte(user+0x12d, country);word(user+0x136, level)
    head = nodes;put(zone+0x7c, head);put(head, head+0x20 if entries else head)
    for i, entry in enumerate(entries):
        node = head+0x20+i*0x20;peer = peers+i*0x10000
        cpu.mem_write(peer, bytes(0x10000));put(node, head+0x20+(i+1)*0x20 if i+1<len(entries) else head)
        actual = user if entry.get('self') else peer;put(node+0x10, actual)
        if actual != user:
            byte(peer+0x1744, entry.get('seeking', 1));byte(peer+0x12d, entry.get('country', 1));word(peer+0x136, entry.get('level', 20));byte(peer+0x134, entry.get('job', 3))
            name=entry.get('name', b'Peer\0'+bytes(16));assert len(name)==21;cpu.mem_write(peer+0x188, name)
    cpu.reg_write(UC_X86_REG_ESP, stack+0xf000);cpu.reg_write(UC_X86_REG_EBP, user)
    cpu.emu_start(0x4760ed, 0x476262, count=1000000)
    return cpu.mem_read(user+0x1744,1)[0], [p for target,p in events if target==user]

flag, packets=run([]);assert flag==1 and packets==[b'\x0c\x22\x01']
flag, packets=run([],party=1,old_flag=7);assert flag==7 and packets==[b'\x0c\x22\x01']
raw=b'X\0'+b'R'*19
entries=[{'self':True},{'seeking':0},{'country':2},{'level':14},{'level':26},{'level':15,'name':raw,'job':8},{'level':25,'job':9}]
flag, packets=run(entries);assert flag==1 and len(packets)==2 and packets[1]==b'\x0d\x22\x02'+bytes([15,8])+raw+bytes([25,9])+b'Peer\0'+bytes(16)
flag, packets=run([{'level':i,'job':i} for i in range(35)],level=15);assert len(packets)==2 and packets[1][2]==11
flag, packets=run([{'level':256,'job':1}],level=256);assert packets[1][3:5]==b'\0\x01'
flag, packets=run([{} for _ in range(35)]);assert packets[1][2]==30 and len(packets[1])==3+30*23
print('6 original party-find cases passed; ps_game.exe SHA256',digest)
