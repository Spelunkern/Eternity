#!/usr/bin/env python3
"""Execute ps_game's idle traversal and actual AddMob/RemoveMob list mutations."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game

cpu,digest=load_game()
stack,zone,mobs,heap,stop=0x20000000,0x30000000,0x31000000,0x33000000,0x10000000
for at in (stack,zone,mobs,heap,stop): cpu.mem_map(at,0x100000)
def put(at,*v): cpu.mem_write(at,struct.pack('<'+'I'*len(v),*v))
def get(at): return struct.unpack('<I',cpu.mem_read(at,4))[0]
def imm(n): return struct.pack('<I',n)
def rel(src,target): return b'\xe8'+struct.pack('<i',target-src-5)
# Only index and lock operations are stubbed. Real intrusive AddMob/RemoveMob run.
cpu.mem_write(0x417bb0,b'\xc2\x04\x00')
cpu.mem_write(0x4183a0,b'\xc2\x04\x00')
cpu.mem_write(0x421a80,b'\xc2\x08\x00')
cpu.mem_write(0x41d240,b'\xc2\x08\x00')
cpu.mem_write(stop+0x100,b'\xc2\x04\x00')
for at in (0x5210cc,0x5210d0): put(at,stop+0x100)
state={"serial":0}
def hook(machine,address,size,_):
    if address!=0x4a7e60:return
    sp=machine.reg_read(UC_X86_REG_ESP); ptr=get(sp+4)
    idx=(ptr-mobs)//0x2000
    if ptr!=mobs+idx*0x2000 or idx not in range(4): raise RuntimeError('unexpected mob')
    state['trace'].append(idx)
    if len(state['trace'])>40: raise RuntimeError('unexpected traversal loop')
    address=stop+0x200+state['serial']*0x100
    state['serial']+=1
    code=bytearray(b'\x57') # preserve frame EDI
    if not state['fired'] and idx==state['trigger']:
        state['fired']=True
        if state['op'] in ('remove','add'):
            target=mobs+state['target']*0x2000
            code+=b'\xb8'+imm(target)
            if state['op']=='remove': code+=b'\xbf'+imm(zone); dest=0x416790
            else: code+=b'\xb9'+imm(zone);dest=0x416700
            code+=rel(address+len(code),dest)
        elif state['op']=='nested':
            code+=rel(address+len(code),0x421a00)
    code+=b'\x5f\xc2\x08\x00'
    machine.mem_write(address,bytes(code));machine.reg_write(UC_X86_REG_EIP,address)
cpu.hook_add(UC_HOOK_CODE,hook)
print('# ps_game.exe SHA256 '+digest+'; CZone::UpdateMobIdle 00421a00 with real AddMob 00416700/RemoveMob 00416790; death/respawn, map index and locks stubbed')
print('# op trigger target | callback trace | final membership')
for op,trigger,target in [('none',0,0),('remove',0,0),('remove',1,1),('remove',0,1),('remove',1,0),('remove',2,1),('add',0,3),('add',2,3),('nested',0,0),('nested',1,0),('nested',2,0)]:
    cpu.mem_write(zone,bytes(0x1000));cpu.mem_write(mobs,bytes(0x10000));cpu.mem_write(heap,bytes(0x1000))
    head=heap;put(zone+0x13c,head,head);put(head,head,head)
    for i in range(4):
        m=mobs+i*0x2000;node=m+0x6c;put(node+8,m+0x50)
        if i<3:
            prev=head if i==0 else mobs+(i-1)*0x2000+0x6c
            nxt=head if i==2 else mobs+(i+1)*0x2000+0x6c
            put(node,prev,nxt)
    put(head,mobs+2*0x2000+0x6c,mobs+0x6c);put(zone+0x15c,3)
    state.update(trace=[],op=op,trigger=trigger,target=target,fired=False)
    put(stack+0x8000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ESI,zone);cpu.reg_write(UC_X86_REG_EDI,123)
    cpu.emu_start(0x421a00,stop,count=1000000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop: raise RuntimeError('did not return')
    node=get(head+4);members=[]
    while node!=head:
        members.append((node-mobs-0x6c)//0x2000);node=get(node+4)
        if len(members)>4:raise RuntimeError('bad list')
    print(op,trigger,target,','.join(map(str,state['trace'])),','.join(map(str,members)),sep='\t')
