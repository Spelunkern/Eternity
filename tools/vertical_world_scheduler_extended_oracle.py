#!/usr/bin/env python3
"""Original CWorldThread::Update prefix, including actual link mutation."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX,
    UC_X86_REG_ECX, UC_X86_REG_ESI, UC_X86_REG_EDI)
from original_oracle import load_game

cpu, digest = load_game()
b=0x30000000;cpu.mem_map(b,0x120000)
thread,head,wait_head,stack,stop,stub=(b,b+0x1000,b+0x1100,b+0x80000,b+0x90000,b+0xa0000)
zones=[b+0x10000+i*0x1000 for i in range(5)]
slots=(0x74,0x78,0x7c,0x80,0x84,0x88,0x94,0x8c,0x90,0xa4)
def put(at,*values):cpu.mem_write(at,struct.pack('<'+'I'*len(values),*(v&0xffffffff for v in values)))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
def ring(headptr, members):
    links=[z+8 for z in members];put(headptr,links[-1] if links else headptr,links[0] if links else headptr)
    for i,link in enumerate(links):put(link,links[i-1] if i else headptr,links[i+1] if i+1<len(links) else headptr,members[i])
def ident(address):return str(zones.index(address)) if address in zones else 'bad:'+hex(address)
def members(headptr):
    result=[];prior=headptr;node=get(headptr+4)
    while node!=headptr:
        if len(result)>=len(zones) or get(node)!=prior or get(node+8) not in zones:
            raise RuntimeError('corrupt original intrusive list')
        result.append(get(node+8));prior=node;node=get(node+4)
    if get(headptr)!=prior:raise RuntimeError('corrupt original list tail')
    return result
targets={0x4215e0:('packet','edi',0),0x421930:('user_attack','esi',0),0x421f80:('skill','edi',0),0x4221d0:('effect_tick','edi',4),0x4220a0:('effect','stack',8),0x421990:('mob_attack','esi',0),0x422390:('npc_attack','esi',0),0x421a00:('mob_idle','esi',0),0x4226b0:('trap','ecx',4),0x422400:('npc_idle','esi',0),0x421e20:('item','stack',8),0x422320:('quest','edi',0),0x420320:('world_send','eax',0),0x421730:('net_send','edi',0)}
registers={'edi':UC_X86_REG_EDI,'esi':UC_X86_REG_ESI,'ecx':UC_X86_REG_ECX,'eax':UC_X86_REG_EAX}
events=[];state={}
def build_mutation(kind):
    # Execute the original Remove / AddZone against the actual thread list. The
    # port is injected only at the selected packet callback, and saves all regs.
    address=stub+kind*0x100
    code=bytearray(b'\x60')
    def mov(op,v):code.extend(bytes([op])+struct.pack('<I',v))
    def call(dst):
        pc=address+len(code);code.extend(b'\xe8'+struct.pack('<i',dst-(pc+5)))
    if kind in (1,2):mov(0xbe,zones[kind-1]+8);mov(0xbf,thread+0x14);call(0x407fa0)
    elif kind==3:mov(0xb8,zones[4]);mov(0xbe,thread);call(0x4072b0)
    code.extend(b'\x61\xc3');cpu.mem_write(address,bytes(code))
def hook(c,at,size,data):
    if at==state.get('end'):c.emu_stop();return
    if at==0x407c30:
        events.append('kill_count:thread');sp=c.reg_read(UC_X86_REG_ESP);c.reg_write(UC_X86_REG_ESP,sp+4);c.reg_write(UC_X86_REG_EIP,get(sp));return
    if at==0x4073d0:events.append('promote:'+ident(get(c.reg_read(UC_X86_REG_EAX)+8)))
    if at in targets:
        name,source,pop=targets[at];sp=c.reg_read(UC_X86_REG_ESP)
        zone=get(sp+4) if source=='stack' else c.reg_read(registers[source]);events.append(name+':'+ident(zone))
        if name=='packet' and not state['mutated'] and state['mutation']:
            state['mutated']=True;c.reg_write(UC_X86_REG_EIP,stub+state['mutation']*0x100);return
        c.reg_write(UC_X86_REG_ESP,sp+4+pop);c.reg_write(UC_X86_REG_EIP,get(sp))
cpu.hook_add(UC_HOOK_CODE,hook)
for kind in (1,2,3):build_mutation(kind)
cpu.mem_write(stop+16,b'\xc2\x04\x00');cpu.mem_write(stop+32,b'\xc2\x04\x00');put(0x5210cc,stop+16);put(0x5210d0,stop+32)
print(f'# ps_game.exe SHA256 {digest}; original Update 00407390 through 00407c00 (max tick stops at 00407765); weather/rank/war/instance-expiry deadlines held not due; real RemoveFront 00408140 / Remove 00407fa0 / AddZone 004072b0; OS locks and zone bodies are observation ports')
print('# case now mask promote mutation relation deadlines_before deadlines_after active_count wait_count events')
case=0
for now in (0,1,500,1000,0x7fffffff,0xfffffff0,0xfffffffe,0xffffffff):
 for mask in (0,1,2,4,8,16,32,64,128,256,511,0x155,0x0aa):
  for promote in (0,1):
   for mutation in (0,1,2,3):
    for relation in (-1,0,1):
     cpu.mem_write(b,bytes(0x16000));put(thread+0x14,head,head);put(thread+0x40,wait_head,wait_head)
     ring(head,zones[:2]);ring(wait_head,zones[2:4]);
     for off in (0x98,0x9c,0xac,0xa0):put(thread+off,0xffffffff)
     put(zones[4]+16,zones[4]);put(thread+0x34,2);put(thread+0x60,2)
     for i,off in enumerate(slots):
         due=(mask>>i)&1 if i<9 else promote
         put(thread+off,(now+relation)&0xffffffff if due else 0xffffffff)
     before=','.join(str(get(thread+off)) for off in slots)
     put(stack,stop,thread,now);events.clear();state.update(mutation=mutation,mutated=False,end=0x407765 if now==0xffffffff else 0x407c00);cpu.reg_write(UC_X86_REG_ESP,stack)
     try:cpu.emu_start(0x407390,state['end'],count=100000)
     except Exception as e:raise RuntimeError(f'case {case} pc={cpu.reg_read(UC_X86_REG_EIP):x} {e}') from e
     if cpu.reg_read(UC_X86_REG_EIP)!=state['end']:raise RuntimeError(f'case {case} did not reach end')
     active=members(head);waiting=members(wait_head)
     if len(active)!=get(thread+0x34) or len(waiting)!=get(thread+0x60) or set(active)&set(waiting):raise RuntimeError(f'case {case} list count/ownership mismatch')
     after=','.join(str(get(thread+off)) for off in slots)
     print(case,now,mask,promote,mutation,relation,before,after,get(thread+0x34),get(thread+0x60),','.join(events),sep='\t');case+=1
