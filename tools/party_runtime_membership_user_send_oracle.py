#!/usr/bin/env python3
"""Execute original direct roster send, allocation/copy and transfer to worker."""
import struct
from original_oracle import load_game
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP


def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x100000)
    user,buffer,frame,payload,agent,worker,stack,stop=[base+n for n in (0,0x30000,0x31000,0x40000,0x50000,0x51000,0xf0000,0xff000)]
    close=stop+16
    def put(at,value):cpu.mem_write(at,struct.pack('<I',value&0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    context={}
    def state():return struct.pack('<6I',get(user+0x18),get(user+0x1c),get(user+0x68),get(buffer+8),get(buffer+12),int(context['freed']))
    def event(tag,data=b''):context['events']+=tag.encode()+state()+struct.pack('<I',len(data))+data
    pools={0x4df710:32,0x4df7b0:128,0x4df850:2048}
    for at,cleanup in {close:4,0x4dc1b0:8,**{p:0 for p in pools}}.items():cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup))
    put(0x5212f0,close)
    def hook(machine,at,size,data):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if at in pools:
            event('A',struct.pack('<I',pools[at]))
            mode=context['mutation']
            if mode in (1,4):put(user+0x68,-1 if mode==1 else 0)
            if mode in (2,4):cpu.mem_write(payload,bytes(context['length']))
            if mode==3:put(user+0x18,99);put(user+0x1c,77)
            context['used']=context['allocated'];machine.reg_write(UC_X86_REG_EAX,buffer if context['allocated'] else 0)
        elif at==close:
            event('C',struct.pack('<I',get(sp+4)))
            if context['close_mutation']:put(user+0x68,123);put(user+0x18,44);put(user+0x1c,55)
        elif at==0x4dc1b0:
            assert get(sp+4)==user and get(sp+8)==buffer
            length=struct.unpack('<H',cpu.mem_read(frame,2))[0]
            event('E',struct.pack('<I',get(buffer+8))+bytes(cpu.mem_read(frame,length)))
            if context['completion']:
                put(buffer+8,get(buffer+8)-1)
                if get(buffer+8)==0:context['freed']=True;event('B')
    cpu.hook_add(UC_HOOK_CODE,hook)
    print(f'# ps_game.exe SHA256 {digest}; entire SConnection::Send(void*,int) 004d4d20, real AllocBuffer 004df9f0 and SendRequest 004d8950. Ports: pool allocation, closesocket and worker SendPost with explicit completion/free model. Unsafe original payloads 2047/2048 excluded.')
    print('# socket error detail allocated completion mutation close_mutation payload_hex; R state_hex frame_hex events_hex')
    lengths=(0,1,30,31,126,127,2014,2046,2049,4096)
    for case in range(1024):
        cpu.mem_write(base,bytes(0xfd000));socket=(-1,0,5,-2,2147483647)[case%5];error=(0,7,0xffffffff)[(case//3)%3];detail=(0,5,0xffffffff)[(case//7)%3]
        allocated=(case//5)%3!=0;completion=(case//11)%2;mutation=(case//13)%5;close_mutation=(case//17)%2
        data=bytes((case+i*17)&255 for i in range(lengths[(case//19)%len(lengths)]))
        context.update(events=b'',freed=False,used=False,allocated=allocated,completion=completion,mutation=mutation,close_mutation=close_mutation,length=len(data))
        put(user+0x18,error);put(user+0x1c,detail);put(user+0x68,socket);put(user+0xc8,agent);put(agent+0x16c,worker);put(buffer+4,frame);put(buffer+8,17);put(buffer+12,19);cpu.mem_write(payload,data)
        put(stack,stop);put(stack+4,payload);put(stack+8,len(data));cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ECX,user)
        cpu.emu_start(0x4d4d20,stop,count=100000)
        wire=bytes(cpu.mem_read(frame,len(data)+2)) if context['used'] else b''
        print(socket,error,detail,int(allocated),completion,mutation,close_mutation,data.hex() or '-')
        print('R',state().hex(),wire.hex() or '-',context['events'].hex() or '-')

if __name__=='__main__':main()
