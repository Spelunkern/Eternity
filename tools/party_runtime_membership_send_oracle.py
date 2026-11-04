#!/usr/bin/env python3
"""Execute original party fan-out, allocation/copy, connection guard and ref ownership."""
import struct
from original_oracle import load_game
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESI, UC_X86_REG_ESP


def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x100000)
    party,buffer,frame,payload,agent,worker,stack,stop=[base+n for n in (0,0x30000,0x31000,0x40000,0x50000,0x51000,0xf0000,0xff000)]
    users=[base+0x10000+i*0x1000 for i in range(7)]
    lock,unlock,increment,decrement=[stop+n for n in (16,32,48,64)]
    def put(at,value):cpu.mem_write(at,struct.pack('<I',value&0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def identity(at):return users.index(at)+100 if at in users else 0
    context={}
    def state():
        values=[get(party+0x10),cpu.mem_read(party+0x114,1)[0],get(buffer+8),get(buffer+12),int(context['freed'])]
        values += [identity(get(party+0x18+i*8)) for i in range(7)]
        values += [get(u+0x68) for u in users]
        return struct.pack('<19I',*values)
    def event(tag,data=b''):context['events']+=tag.encode()+state()+struct.pack('<I',len(data))+data
    pools={0x4df710:32,0x4df7b0:128,0x4df850:2048}
    for at,cleanup in {lock:4,unlock:4,increment:4,decrement:4,0x4dc1b0:8,0x4df070:0,**{p:0 for p in pools}}.items():cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup))
    for at,target in ((0x5210cc,lock),(0x5210d0,unlock),(0x5210f8,increment),(0x5211f0,decrement)):put(at,target)
    def hook(machine,at,size,data):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if at in pools:
            event('A',struct.pack('<I',pools[at]));machine.reg_write(UC_X86_REG_EAX,buffer if context['allocated'] else 0)
        elif at in (increment,decrement):
            address=get(sp+4);value=(get(address)+(1 if at==increment else -1))&0xffffffff;put(address,value);machine.reg_write(UC_X86_REG_EAX,value)
        elif at==lock:
            event('[')
            if context['mutate_input']:cpu.mem_write(payload,bytes(context['length']))
        elif at==unlock:event(']')
        elif at==0x4dc1b0:
            user=get(sp+4);assert get(sp+8)==buffer
            event('E',struct.pack('<I',identity(user)));context['queues']+=1
            if context['queues']==1:
                index=get(sp+12);assert index<7
                mode=context['mutation']
                if mode==1:put(party+0x10,0)
                elif mode==2:put(party+0x10,7)
                elif mode==3 and index+1<7:put(party+0x18+(index+1)*8,users[6]);put(users[6]+0x68,0)
                elif mode==4 and index+1<7:put(party+0x18+(index+1)*8,0)
                elif mode==5:cpu.mem_write(party+0x114,b'\x02')
                elif mode==6 and index+1<7:
                    next_user=get(party+0x18+(index+1)*8)
                    if next_user:put(next_user+0x68,-1)
            if context['completion']==1 or (context['completion']==2 and context['queues']%2==1):put(buffer+8,get(buffer+8)-1)
        elif at==0x4df070:
            assert get(sp+4)==buffer and get(buffer+8)==0
            context['freed']=True;event('B')
    cpu.hook_add(UC_HOOK_CODE,hook)
    print(f'# ps_game.exe SHA256 {digest}; real CParty::Send 004455d0, AllocBuffer 004df9f0, SConnection::Send 004d4cf0 and SAgent::SendRequest 004d8950. Ports: pool allocation/free, worker SendPost, locks and atomic OS intrinsics; queue callbacks explicitly model completion reference releases.')
    print('# kind count slot_mask allocated completion mutation mutate_input sockets[7] payload_hex; R state_hex frame_hex events_hex')
    lengths=(0,1,2,6,30,31,126,127,335,2014,2046)
    for case in range(1024):
        cpu.mem_write(base,bytes(0xfd000));kind=(0,1,2,255)[case%4];count=(-1,0,1,2,3,7)[(case//4)%6]
        mask=(127,0,(case*37)&127)[(case//9)%3];allocated=(case//5)%4!=0;completion=(case//11)%3;mutation=(case//13)%7;mutate_input=(case//23)%2
        sockets=[(-1,0,5)[(case//7+i)%3] for i in range(7)]
        data=bytes((case+i*17)&255 for i in range(lengths[(case//17)%len(lengths)]))
        context.update(events=b'',freed=False,allocated=allocated,completion=completion,mutation=mutation,mutate_input=mutate_input,length=len(data),queues=0)
        put(party+0x10,count);cpu.mem_write(party+0x114,bytes([kind]));put(buffer+4,frame);put(buffer+8,17);put(buffer+12,19)
        put(agent+0x16c,worker)
        for i,user in enumerate(users):put(party+0x18+i*8,user if (mask>>i)&1 else 0);put(user+0x68,sockets[i]);put(user+0xc8,agent)
        cpu.mem_write(payload,data);put(stack,stop);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ESI,party);cpu.reg_write(UC_X86_REG_ECX,payload);cpu.reg_write(UC_X86_REG_EAX,len(data))
        cpu.emu_start(0x4455d0,stop,count=100000)
        wire=bytes(cpu.mem_read(frame,len(data)+2)) if allocated else b''
        print(kind,count,mask,int(allocated),completion,mutation,mutate_input,*sockets,data.hex() or '-')
        print('R',state().hex(),wire.hex() or '-',context['events'].hex())

if __name__=='__main__':main()
