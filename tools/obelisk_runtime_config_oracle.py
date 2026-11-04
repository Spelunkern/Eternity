#!/usr/bin/env python3
"""Run full original LoadObelisk; observe obelisk materialization from parsed records."""
import hashlib
import struct
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_INVALID
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_game,ROOT


def main():
    cpu,digest=load_game();stack,world,maps,infos,heap,file,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x34000000,0x38000000,0x10000000
    for at,n in ((0,0x10000),(stack,0x10000),(world,0x10000),(maps,0x100000),(infos,0x200000),(heap,0x3000000),(file,0x10000),(stop,0x10000)):cpu.mem_map(at,n)
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    for at in (0x505316,0x50b152,0x50b21f,0x5043e5,0x504303,0x503ae3,0x503ba1,0x504444):cpu.mem_write(at,b'\xc3')
    cpu.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x1000)+b'\xc3')
    put(0x546f38,world);put(0xfcd2ac,4096,infos)
    for i in range(1,4096):cpu.mem_write(infos+i*268,struct.pack('<H',i));cpu.mem_write(infos+i*268+34,struct.pack('<H',0 if i%7==0 else 1));put(infos+i*268+48,1)
    context={}
    def hook(machine,address,size,_):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x5043e5:machine.reg_write(UC_X86_REG_EAX,file)
        elif address==0x504303:
            target,n,stream=struct.unpack('<3I',machine.mem_read(sp+4,12))
            if stream!=file:raise RuntimeError('Unexpected original stream')
            position=context['position'];text=context['text'];end=text.find(b'\n',position);end=len(text) if end<0 else end+1;end=min(end,position+n-1);line=text[position:end];context['position']=end
            if line:machine.mem_write(target,line+b'\0')
            if not line or (end==len(text) and not line.endswith(b'\n')):put(file+12,0x10)
            machine.reg_write(UC_X86_REG_EAX,target if line else 0)
        elif address==0x503ae3:
            n=get(sp+4);at=context['heap'];context['heap']+=(n+15)&~15;machine.mem_write(at,bytes(n));machine.reg_write(UC_X86_REG_EAX,at)
        elif address in (0x42733d,0x427349,0x427377):
            # Exact temporary-array reset, avoiding millions of REP callbacks.
            count=machine.reg_read(UC_X86_REG_ECX);at=machine.reg_read(UC_X86_REG_EDI);machine.mem_write(at,bytes(count*4));machine.reg_write(UC_X86_REG_EDI,at+count*4);machine.reg_write(UC_X86_REG_ECX,0);machine.reg_write(UC_X86_REG_EIP,address+2)
    for at in (0x5043e5,0x504303,0x503ae3,0x42733d,0x427349,0x427377):cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
    def invalid(machine,access,address,size,value,_):raise RuntimeError(f'Invalid parser access {address:08x} at {machine.reg_read(UC_X86_REG_EIP):08x}')
    cpu.hook_add(UC_HOOK_MEM_INVALID,invalid)
    def definition(pointer):return (pointer-infos)//268 if pointer else 0
    def canonical(count):
        out=[get(0x541b0c),get(0x5471bc),get(0x5471c0),get(0x5471c4)]
        for mi in range(count):
            at=maps+mi*496;n=get(at+0x1e8);out.append(n)
            for row in range(n):
                o=get(at+0x1ec)+row*0x70
                out.extend(get(o+x) for x in range(0,40,4))
                out.extend(definition(get(o+0x34+j*4)) for j in range(3))
                out.extend(get(o+x) for x in (0x40,0x44,0x48,0x4c))
                out.extend((cpu.mem_read(o+0x54,1)[0],get(o+0x64),int(get(o+0x6c)!=0)))
                for j in range(get(o+0x64)):
                    g=get(o+0x68)+j*0x58
                    out.extend(get(g+x) for x in range(0,24,4))
                    out.extend(definition(get(g+0x24+k*4)) for k in range(3))
                    out.extend(get(g+x) for x in (0x30,0x34,0x38,0x3c))
                    out.extend((cpu.mem_read(g+0x44,1)[0],int(get(g+0x48)==g)))
                    out.extend(int(get(get(g+0x54)+k*4)!=0) for k in range(get(g+0x14)))
        out.extend(cpu.mem_read(infos+i*268+0x7c,1)[0] for i in range(12))
        return out
    print(f'# ps_game.exe SHA256 {digest}; full LoadObelisk 00427300. Native comparison covers materialization from explicit parsed input, not native parsing. Required shared template marking is observed in actual source storage. Ports: file/heap/CRT/TLS/locks/cookie and exact temporary-array reset.')
    for scenario in range(64):
        text='';ids=(0,1,7,-1,4096,2,3,4)
        for zone_id in range(3):
            text+=f'[zone_{zone_id}]\n'
            for row in range((scenario//3+zone_id)%3):
                x=(-1,0,39.75,40,2)[(scenario+row)%5];z=-1 if scenario%11==0 else 1
                d=[ids[(scenario+row+k)%8] for k in range(3)];country=(scenario+zone_id+row)%5-1
                text+=f'[Obelisk_{row+1}]=({x},-2.5,{z}),({d[0]},{d[1]},{d[2]}),{country}\n'
                for j in range((scenario//5+row)%3):
                    radius=(-2.75,0.75,2.75,1073741824)[(scenario+j)%4];d=[ids[(scenario+j+k+1)%8] for k in range(3)];n=(scenario+j)%4
                    text+=f'[Mob]=({j+0.25},4,5,{radius}),({d[0]},{d[1]},{d[2]}),{n}\n'
        context.update(text=text.encode(),position=0,heap=heap);cpu.mem_write(file,bytes(32));cpu.mem_write(maps,bytes(0x30000));put(world+8,3,maps)
        for mi in range(3):put(maps+mi*496+4,40)
        for i in range(4096):cpu.mem_write(infos+i*268+0x7c,b'\x00')
        put(0x541b0c,(0,0xfffffffe)[scenario%2]);put(0x5471bc,scenario//9%3);put(0x5471c0,9,10)
        put(0xfcd2cc,0,0);put(0xfcd25c,1000);put(0xfd4e5c,-45)
        put(stack+0xf000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_FPCW,0x27f)
        cpu.emu_start(0x427300,stop,count=10000000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop or not cpu.reg_read(UC_X86_REG_EAX)&255:raise RuntimeError('Original full loader failed')
        print(scenario,' '.join(map(str,canonical(3))),sep='\t')

if __name__=='__main__':main()
