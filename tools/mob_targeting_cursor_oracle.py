#!/usr/bin/env python3
"""Original helper traversal with real Remove/AddBack/FindMob callback mutations."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    c,digest=load_game();b=0x20000000;stack=b;source=b+0x10000;user=b+0x20000;zone=b+0x30000;cell=b+0x40000;head=b+0x41000;data=b+0x50000;stop=b+0x70000;control=stop+0x1000;thunk=stop+0x2000
    c.mem_map(b,0x80000)
    def put(at,*v):c.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(at):return struct.unpack('<I',c.mem_read(at,4))[0]
    def mob(i):return data+(i-1)*0x1000
    c.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x3000)+b'\xc3')
    for at,code in ((stop+16,b'\xc2\x04\x00'),(stop+32,b'\xc2\x04\x00'),(stop+48,b'\xc2\x08\x00'),(stop+64,b'\xc3')):c.mem_write(at,code)
    put(0x5210cc,stop+16);put(0x5210d0,stop+32)
    c.mem_write(0x49fad0,b'\xff\x25'+struct.pack('<I',control))
    ctx={}
    def hook(cpu,at,size,_):
        sp=cpu.reg_read(UC_X86_REG_ESP)
        if at in (stop+16,stop+32):
            if get(sp+4)!=cell+0x34:raise RuntimeError('Wrong cell/list lock')
            ctx['events'].append('L' if at==stop+16 else 'U')
        elif at==0x49fad0:
            i=get(cpu.reg_read(UC_X86_REG_ECX)+0x5c)
            if get(sp+4)!=user or get(sp+8)!=0:raise RuntimeError('Wrong helper target call')
            ctx['events'].append('A'+str(i));put(control,thunk if ctx['calls']==ctx['trigger'] else stop+48);ctx['calls']+=1
        elif at==stop+64:
            p=cpu.reg_read(UC_X86_REG_EAX);ctx['events'].append('F'+str(get(p+0x5c) if p else 0))
    c.hook_add(UC_HOOK_CODE,hook)
    print(f'# ps_game.exe SHA256 {digest}; FindHelpNormal 0049ff70 / Normal2 004a0200 / Attack 004a0470; real SSyncList<CMob>::Remove 00424ac0, AddBack 00424a80 and CCell::FindMob 00423680. Ports: locks, CRT TLS, SetTarget(false) callback driver.')
    print('# kind count mode trigger fsm | acquisition/lock/find events | remaining list IDs; modes 0 none,1 current removal,2 next removal,3 previous removal,4 append,5 find tail,6 find miss,7 find current,8 remove/reappend current,9 remove next/find miss. Mutation once at zero-based acquisition trigger.')
    for scenario in range(450):
        kind=(2,3,5)[scenario%3];n=(scenario//3)%5+1;mode=(scenario//15)%10;fsm=(scenario//150)%3;trigger=(scenario//7)%n;current=trigger+1
        c.mem_write(source,bytes(0x1000));c.mem_write(zone,bytes(0x1000));c.mem_write(cell,bytes(0x2000));c.mem_write(data,bytes(0x7000))
        put(source+0x60,zone,0,0);put(source+0xa8,data+0x6000);put(data+0x6078,1);c.mem_write(data+0x6069,b'\xff');c.mem_write(source+0x220,struct.pack('<H',7))
        put(zone+0x130,1);put(zone+0x28,1,1,cell+0x800);put(cell+0x800,cell);put(cell+0x2c,head,head);put(cell+0x4c,n)
        put(head+0x48,mob(n)+0x48,mob(1)+0x48)
        put(user+0x142c,0);c.mem_write(user+0x12d,b'\0');c.mem_write(user+0x1366,b'\0');c.mem_write(user+0x5774,b'\1\1')
        for i in range(1,n+2):
            p=mob(i);info=p+0x400;put(p+0x48,head+0x48 if i==1 else mob(i-1)+0x48,head+0x48 if i==n else mob(i+1)+0x48);put(p+0x5c,i);put(p+0xa8,info);c.mem_write(p+0x220,struct.pack('<H',7));c.mem_write(info+0x26,b'\x01')
            if fsm:
                put(p+0x44,1);put(p+0x34,info+0x100);put(p+0x38,0,0,1 if fsm==1 else 0);put(info+0x100,info+0x110);put(info+0x110,info+0x120)
        put(mob(n+1)+0x48,0,0)
        code=bytearray(b'\x60') # pushad: preserve all original helper registers.
        def emit(v):code.extend(v)
        def call(at):emit(b'\xe8'+struct.pack('<i',at-(thunk+len(code)+5)))
        def remove(i):
            if not 1<=i<=n:return
            emit(b'\xbe'+struct.pack('<I',mob(i))+b'\xbf'+struct.pack('<I',cell+0x2c));call(0x424ac0)
        def add(i):emit(b'\xbb'+struct.pack('<I',mob(i))+b'\xbe'+struct.pack('<I',cell+0x2c));call(0x424a80)
        def find(i):emit(b'\xbf'+struct.pack('<I',cell)+b'\x68'+struct.pack('<I',i));call(0x423680);call(stop+64)
        if mode==1:remove(current)
        elif mode==2:remove(current+1)
        elif mode==3:remove(current-1)
        elif mode==4:add(n+1)
        elif mode==5:find(n)
        elif mode==6:find(99)
        elif mode==7:find(current)
        elif mode==8:remove(current);add(current)
        elif mode==9:remove(current+1);find(99)
        emit(b'\x61\xc2\x08\x00');c.mem_write(thunk,bytes(code));c.ctl_remove_cache(thunk,thunk+0x400)
        ctx.update(events=[],calls=0,trigger=trigger)
        put(stack+0xf000,stop,0 if kind==2 else source,user);c.reg_write(UC_X86_REG_ESP,stack+0xf000);c.reg_write(UC_X86_REG_ECX,source);c.reg_write(UC_X86_REG_FPCW,0x27f)
        c.emu_start({2:0x49ff70,3:0x4a0470,5:0x4a0200}[kind],stop,count=50000)
        if c.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original cursor traversal did not return')
        ids=[];p=get(head+0x4c)-0x48
        while p!=head:
            ids.append(get(p+0x5c));p=get(p+0x4c)-0x48
            if len(ids)>n+1:raise RuntimeError('Original list cycle')
        if len(ids)!=get(cell+0x4c):raise RuntimeError('Original list count differs')
        print(kind,n,mode,trigger,fsm,'|',' '.join(ctx['events']),'|',' '.join(map(str,ids)))
if __name__=='__main__':main()
