#!/usr/bin/env python3
"""Execute original cell broadcast; send callback can unlink its current user."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game

cpu,digest=load_game();stop,stack,cell,head,users,buffer=0x10000000,0x20000000,0x30000000,0x31000000,0x32000000,0x33000000
for address in (stop,stack,cell,head,users,buffer):cpu.mem_map(address,0x10000)
def put(address,*values):cpu.mem_write(address,struct.pack('<'+'I'*len(values),*values))
def get(address):return struct.unpack('<I',cpu.mem_read(address,4))[0]
for address in (0x5210cc,0x5210d0):put(address,stop+0x100)
cpu.mem_write(stop+0x100,b'\xc2\x04\x00');cpu.mem_write(0x4d4cf0,b'\xc2\x04\x00')
subjects=[users+i*0x1000 for i in range(4)];events=[];action='none';outer_sp=0;skip_outer=False
def unlink(subject):
    prev,next_node=get(subject+8),get(subject+12)
    put(prev+4,next_node);put(next_node,prev);put(subject+8,0,0)
def append(subject):
    sentinel=head+8;tail=get(sentinel);put(subject+8,tail,sentinel);put(tail+4,subject+8);put(sentinel,subject+8)
def hook(m,address,size,_):
    global outer_sp,skip_outer
    if address==stop+0x200:
        m.reg_write(UC_X86_REG_ESP,outer_sp);m.reg_write(UC_X86_REG_EIP,0x4d4cf0);skip_outer=True;return
    if address!=0x4d4cf0:return
    if skip_outer:skip_outer=False;return
    subject=m.reg_read(UC_X86_REG_ECX);events.append(subjects.index(subject))
    if len(events)==1:
        # Emulate the pointer writes of real SSyncList<CUser>::Remove 0040f225;
        # append uses 0040f188..1a0; no destruction/concurrent worker modeled.
        if action=='remove_current':unlink(subject)
        elif action=='remove_next':unlink(subjects[1])
        elif action=='remove_tail':unlink(subjects[2])
        elif action=='append':append(subjects[3])
        elif action=='reinsert':unlink(subject);append(subject)
        elif action.startswith('nested'):
            outer_sp=m.reg_read(UC_X86_REG_ESP);put(outer_sp-8,stop+0x200,buffer)
            m.reg_write(UC_X86_REG_ESP,outer_sp-8);m.reg_write(UC_X86_REG_EDI,cell);m.reg_write(UC_X86_REG_EIP,0x4243d0)
    elif len(events)==2:
        if action=='nested_remove_current':unlink(subject)
        elif action=='nested_remove_next':unlink(subjects[1])
cpu.hook_add(UC_HOOK_CODE,hook)
cases={'none':[0,1,2],'remove_current':[0],'remove_next':[0,2],'remove_tail':[0,1],
       'append':[0,1,2,3],'reinsert':[0],'nested':[0,0,1,2,1,2],
       'nested_remove_current':[0,0],'nested_remove_next':[0,0,2,2]}
for action,expected in cases.items():
    events.clear();cpu.mem_write(cell,bytes(0x100));cpu.mem_write(head,bytes(0x100));cpu.mem_write(users,bytes(0x4000))
    put(cell,head);nodes=[head+8]+[subject+8 for subject in subjects[:3]]
    for i,node in enumerate(nodes):put(node,nodes[(i-1)%len(nodes)],nodes[(i+1)%len(nodes)])
    put(stack+0x8000,stop,buffer);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EDI,cell)
    cpu.emu_start(0x4243d0,stop,count=10000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original broadcast did not return')
    if events!=expected:raise AssertionError((action,events,expected))
    print(action,'recipient order:',events)
print('ps_game.exe',digest,'004243d0; lock/send ported, unlink/append pointer writes modeled')
