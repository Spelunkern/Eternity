#!/usr/bin/env python3
"""Run the complete original LoadObelisk parser; compare linked-group configuration."""
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
        out=struct.pack('<I',count)
        for i in range(count):
            at=maps+i*496;n=get(at+28);out+=struct.pack('<I',n)
            for j in range(n):
                group=get(at+32)+j*9932;portal=get(group+20);portal_key=(portal-(maps+0x20000))//48+1 if portal else 0
                out+=bytes(cpu.mem_read(group,20))+struct.pack('<I',portal_key)+bytes(cpu.mem_read(group+24,4))
                for k in range(-1,get(group+24)):
                    spawn=group+(28 if k<0 else 328+k*300);out+=bytes(cpu.mem_read(spawn,88))+struct.pack('<16I',*(definition(get(spawn+88+x*4)) for x in range(16)))
                    out+=bytes(cpu.mem_read(spawn+152,16))+struct.pack('<I',definition(get(spawn+168)))+bytes(cpu.mem_read(spawn+172,4))+struct.pack('<I',int(get(spawn+176)!=0))+bytes(cpu.mem_read(spawn+180,4))+struct.pack('<I',int(get(spawn+184)==spawn))+bytes(cpu.mem_read(spawn+252,48))
                out+=bytes(cpu.mem_read(group+9928,4))
        return out
    def run(text,count):
        context.update(text=text.replace(b'\r\n',b'\n').split(b'\x1a')[0],position=0,heap=heap);cpu.mem_write(file,bytes(32));cpu.mem_write(maps,bytes(0x30000));put(world+8,count,maps)
        for i in range(count):
            at=maps+i*496;put(at+4,4000);portals=maps+0x20000+i*3*48;put(at+80,3,portals)
            for j,identifier in enumerate((1010,1010,1011)):put(portals+j*48,identifier)
        put(stack+0xf000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_FPCW,0x27f);
        try:cpu.emu_start(0x427300,stop,count=10000000)
        except Exception as error:raise RuntimeError(f"Parser failed at {cpu.reg_read(UC_X86_REG_EIP):08x}") from error
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original full parser did not return')
        if not cpu.reg_read(UC_X86_REG_EAX)&255:raise RuntimeError('Original full parser failed')
        return canonical(count)
    print(f'# ps_game.exe SHA256 {digest}; complete CMap::LoadObelisk 00427300, including original CRT token/numeric conversion, group copy/binding and constructors. Ports: file stream, zeroed allocation/TLS/CRT locks, cookie and exact temporary array reset.')
    cases=[b'[bothmob_1,0,2,30,1,1010]=(1.25,2.5,3.75),(1,7)\n[Mob]=(4,5,6,1),(1,2,3),2\n',b'[bothmob_1,0,0,0,0,999]=(1,2,3)\n[Mob]=(4,5,6,0.25),1,99\n',b'[bothmob_1,1,1,2,0,1011]=(1,2,3),1\n[RandomRespawn]=1,2,(4,5,6),(7,8,9),(10,11,12)\n']
    cases += [b'',b'// comment\n-- comment\n',b'[bothmob_9,0,1,2,2,1011]=(1,2,3),1',b'[bothmob_9,0,1,2,2,1011]=(1,2,3),1\n',
              b'[bothmob_1,0,2,3,0,1010]=(1,2)\n[Mob]=(9,8,7,6),1,2,3,4\n[bothmob_2,0,1,4,0,1011]=(5,6,7),2\n[Mob]=(1,2)\n[Mob]=(8,9,10,2),(7,2),3\n',
              b'[bothmob_1,0,1,2,9,1010]=(1,2,3),-1\n[bothmob_2,1,1,3,0,1011]=(4,5,6),2\n[bothmob_3,0,1,4,0,1000]=(7,8,9),1\n',
              b'[bothmob_1,0,1,2,0,1010]=(1,2,3),1\n[RandomRespawn]=2,3,(4,5)\n[RandomRespawn]=8\n[zone_1]\n[Obelisk_1]=(1,2,3),(1,2,3),0\n[Mob]=(4,5,6,1),(1,2,3),2\n',
              b'[bothmob_1,0,1,2,0,1010]=(1,2,3),1\n[Portal_1,0,1010]\n[Mob]=(4,5,6,1),1,2,3\n[bothmob_2,0,1,3,0,1010]=(4,5,6),2\n[Mob]=(4,5,6,1),1,2,3\n',
              b'[bothmob_4294967297,0,1,4294967295,0,1011]=(1.25,-2.5,3e2),7\n[Mob]=(4,5,6,-2.25),(4096,-1),-2\n',
              b'  [BoThMoB_1,{0,1,2,(0,1011)}]=(1,2,3),1\r\n\t[MoB]=(4,5,6,1),(1,2),1\r\n[RandomRespawn]=4294967295,2147483649,(1,2,3),(4,5,6)\r\n',
              b'[bothmob_1,0,1,2,0,1010]=(1,2,3),1\n'+b'[Mob]=(4,5,6,1),(1,2),99\n'*34,
              b'[bothmob_1,0,1,2,0,1010]=(1,2,3),1\n[Mob]=(nan,inf,0x10,1d2),(1,2),3\n',
              b'[bothmob_1,0,1,2,0,1010]=(1,2,3),1\n\x1a[bothmob_2,0,1,2,0,1010]=(1,2,3),1\n']
    for count in (1,3,15,16,99):
        n=min(count,15);text=f'[bothmob_1,0,{count},2,0,1011]=(1,2,3),'+','.join(str(i+1) for i in range(n))+'\n[Mob]=(4,5,6,1),'+','.join(str(i+1) for i in range(n+1))+',16\n';cases.append(text.encode())
    base=b'[bothmob_1,0,2,3,0,1011]=(1.25,2.5,3.75),(1,2)\n'
    header=b'[bothmob_9,0,2,30,1,1010]=(11,12,13),(3,4)'
    for cut in (10,12,14,16,19,22,28,31,35,39,42):cases.append(base+header[:cut]+b'\n'+b'[Mob]=(8,9,10,1),(1,2,3),2\n'+header+b'\n')
    for cut in (8,10,12,14,16,19,22,25):cases.append(base+b'[Mob]=(90,91,92,2),(3,4,5),7'[:cut]+b'\n[Mob]=(4,5,6,1),(1,2,3),3\n')
    cases += [b' '*8191+base,b'//'+b'x'*9000+b'\n'+base,base+b'[Unknown] foo\n[Mob]=(4,5,6,1),(1,2,3),2\n',base+b'[RandomRespawn]=1,2,(3,4,5)\n[RandomRespawn]=8,9,(10,11)\n']
    for text in cases:print('S',3,text.hex() if text else '-',run(text,3).hex(),sep='\t')
    text=(ROOT/'.local/server-data/Obelisk.ini').read_bytes();records=run(text,69);h=0xcbf29ce484222325
    for byte in records:h=((h^byte)*0x100000001b3)&0xffffffffffffffff
    print('A',69,hashlib.sha256(text).hexdigest(),len(records),h,sep='\t')
    at=4;counts=[]
    for map_id in range(69):
        n=struct.unpack_from('<I',records,at)[0];at+=4
        if n:counts.append((map_id,n))
        for _ in range(n):
            companions=struct.unpack_from('<I',records,at+24)[0];at+=28+(companions+1)*236+4
    print('# Distribution linked groups:',counts)



if __name__=='__main__':main()
