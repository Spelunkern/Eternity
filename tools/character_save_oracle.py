#!/usr/bin/env python3
"""Execute original complete DB/user save orchestration with verified phase ports.

Ghidra: work/save-owner and work/save-coordinator. SQL phase, max-grow, purchase,
recovery, ACK and social bodies are ports. The former bodies have dedicated
fixtures; native social persistence remains pending.
Original allocation/error/lock/transaction/retry/exit branches execute unchanged.
"""
import itertools
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_original


def main():
    cpu,digest=load_original('ps_dbAgent');stack,user,db,data,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
    for at in (stack,user,db,data,stop):cpu.mem_map(at,0x10000)
    def word(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def put(at,*values):cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    state={};events=[]
    def port(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP);cfg=state['configs'][max(0,state['attempt']-1)]
        if address==0x423190:
            state['attempt']+=1;state['flag']=word(sp+16);events.append('ATTEMPT '+str(int(bool(word(sp+8))))+' '+str(int(bool(word(sp+12)))))
        elif address==0x4200c0:events.append('GROW');machine.reg_write(UC_X86_REG_EAX,cfg[0])
        elif address==0x43ccc0:events.append('ALLOC');machine.reg_write(UC_X86_REG_EAX,db if cfg[1] else 0)
        elif address==0x43e730:events.append('BEGIN');machine.reg_write(UC_X86_REG_EAX,0 if cfg[2] else 0xffffffff)
        elif address==0x43e7f0:events.append('COMMIT');machine.reg_write(UC_X86_REG_EAX,0 if cfg[4] else 0xffffffff)
        elif address==0x43e850:events.append('ROLLBACK');machine.reg_write(UC_X86_REG_EAX,0 if cfg[5] else 0xffffffff)
        elif address==0x43cd80:events.append('FREE')
        elif address==0x432d80:events.append('LOG')
        elif address==0x409560:events.append('ACK')
        elif address==0x424ce0:events.append('RECOVERY');machine.reg_write(UC_X86_REG_EAX,1)
        elif address==0x41ed10:events.append('PURCHASES');machine.reg_write(UC_X86_REG_EAX,state['purchase_ok'])
        elif address==0x4261e0:events.append('FRIENDS')
        elif address==0x4266e0:events.append('BLOCKS')
        elif address==0x431a60:events.append('QUEUE_SAVE' if word(sp+4)==8 else 'QUEUE_EXIT')
        elif address==0x402f70:events.append('EXIT')
        elif address==0x4230b0:events.append('LEAVE')
        elif address==stop+0x100:
            at=word(sp+4)
            if at==user+0x8804:events.append('LOCK')
            elif at==user+0x8794:events.append('PENDING')
        elif address==stop+0x120:
            if word(sp+4)==user+0x8804:events.append('UNLOCK')
        elif address==0x42328e:
            events.append('PHASES '+str(int(bool(word(sp+0x223c)))))
            if cfg[3]:
                if cfg[3]!=3:cpu.mem_write(db+20,b'HYT00\0' if cfg[3]==2 else b'42000\0')
                machine.reg_write(UC_X86_REG_EIP,0x424887)
            else:machine.reg_write(UC_X86_REG_EDI,db);machine.reg_write(UC_X86_REG_EIP,0x424a81)
    for at,cleanup in ((0x4200c0,0),(0x43ccc0,0),(0x43e730,0),(0x43e7f0,0),(0x43e850,0),(0x43cd80,4),(0x432d80,0),(0x409560,0),(0x424ce0,0),(0x41ed10,0),(0x4261e0,0),(0x4266e0,0),(0x431a60,4),(0x402f70,0),(0x4230b0,0),(stop+0x100,4),(stop+0x120,4)):
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,port,begin=at,end=at)
    for at in (0x423190,0x42328e):cpu.hook_add(UC_HOOK_CODE,port,begin=at,end=at)
    put(0x4600d4,stop+0x100);put(0x4600d8,stop+0x120);cpu.mem_write(stop+0x140,b'\xb8\x7b\x00\x00\x00\xc3');put(0x4600dc,stop+0x140)
    rng=random.Random(0x423190)
    print(f'# ps_dbAgent.exe SHA256 {digest}; DBCharacter::SaveCharacter 00423190, CUser::SaveCharacter 00404f30, SaveCharacterTick 00405000; verified SQL/cache/recovery/purchase/social bodies are ports')
    configs=list(itertools.product((0,3,-1),(False,True),(False,True),(0,1,2,3),(False,True),(False,True)))
    cases=[]
    for cfg in configs:
        for recover,sticky,old_timeout,present in itertools.product((False,True),(False,True),(False,True),(False,True)):
            cases.append(('DB',False,True,recover,sticky,old_timeout,present,False,True,[cfg,cfg]))
    for periodic,quick,in_game,pending,purchase_ok in itertools.product((False,True),repeat=5):
        for _ in range(64):cases.append(('USER',periodic,quick,in_game,False,rng.choice((False,True)),True,pending,purchase_ok,[rng.choice(configs),rng.choice(configs)]))
    for shutdown,pending,purchase_ok in itertools.product((False,True),repeat=3):
        for _ in range(256):cases.append(('SHUTDOWN' if shutdown else 'EXIT',False,True,True,shutdown,rng.choice((False,True)),True,pending,purchase_ok,[rng.choice(configs),rng.choice(configs)]))
    for kind,periodic,quick,active_or_recover,sticky,old_timeout,present,pending,purchase_ok,pair in cases:
        cpu.mem_write(user,bytes(0x10000));cpu.mem_write(stack,bytes(0x10000));put(user+0x60,530001);put(user+0x8790,int(pending));put(user+0x87f4,2 if active_or_recover else 1);cpu.mem_write(db+20,b'HYT00\0' if old_timeout else b'00000\0');events.clear();state.update(configs=pair,attempt=0,flag=data,purchase_ok=int(purchase_ok));put(data,int(sticky));sp=stack+0x8000
        if kind=='DB':put(sp,stop,user if present else 0,int(quick),int(active_or_recover),data);address=0x423190
        else:
            put(sp,stop,int(quick));cpu.reg_write(UC_X86_REG_EAX,user)
            address={'EXIT':0x4050b0,'SHUTDOWN':0x405140}.get(kind,0x405000 if periodic else 0x404f30)
        cpu.reg_write(UC_X86_REG_ESP,sp);cpu.emu_start(address,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original save owner did not return')
        print('CASE',kind,int(periodic),int(quick),int(active_or_recover),int(sticky),int(old_timeout),int(present),int(pending),int(purchase_ok),*[int(v) for cfg in pair for v in cfg])
        # Exit methods return void. Their synthetic result reports successful
        # character saving, established by the success-only FRIENDS call.
        returned=int('FRIENDS' in events) if kind in ('EXIT','SHUTDOWN') else cpu.reg_read(UC_X86_REG_EAX)&255
        print('RETURN',returned,int(bool(word(state['flag']))),state['attempt'])
        for event in events:print('EVENT',event)
        print('END')
    bounds=(0,1,899999,900000,0x7fffffff,0x80000000,0xfff2445f,0xfff24460,0xfffffffe,0xffffffff)
    ticks=list(itertools.product((0,1,2,3),bounds,bounds,bounds))
    for _ in range(1024):ticks.append((rng.randrange(4),*[rng.getrandbits(32) for _ in range(3)]))
    for status,now,nxt,last in ticks:
        put(user+0x87f4,status);put(user+0x87fc,nxt);put(0x470894,last);put(stack+0x8000,stop);events.clear()
        cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ECX,user);cpu.reg_write(UC_X86_REG_EAX,now)
        cpu.emu_start(0x403d90,stop,count=1000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original save tick did not return')
        print('TICK',status,now,nxt,last,word(user+0x87fc),word(0x470894),len(events))
        if events and events!=['QUEUE_SAVE']:raise RuntimeError('Unexpected original tick effects')


if __name__=='__main__':main()
