#!/usr/bin/env python3
"""Run both complete original loaders with positional SQL ports and real skill lookup."""
import argparse
import random
from pathlib import Path
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_game


def synthetic_scenarios():
    rng=random.Random(0x4c1720)
    for case in range(256):
        skillmax=8;mobmax=4;skills=[];mobs=[]
        for i in range(1,9):
            if (case+i)%11==0:continue
            row=[0]*82;row[0]=i;row[1]=rng.randrange(256);row[16]=(0,1,2,3,4,255)[(case+i)%6];row[17]=(0,40,41,42,43,65535)[(case//3+i)%6];row[37]=(0,1,65,65535,-1,None)[(case//7+i)%6];skills.append(row)
        if case%4==0:skills.append(skills[0].copy());skills[-1][37]=1234
        for i in range(1,5):
            row=[0]*69;row[0]=i;row[2]=1;row[4]=(0,1,3,9,10,99,100,255)[(case+i)%8];row[8]=100;row[44]=(0,1,60,61,255,None)[(case+i)%6]
            for j in range(3):
                c=45+j*8;row[c:c+8]=[(0,1,2,3,4,5,6,7,8,10,65535)[(case+i+j)%11],rng.choice((0,-1,1,2147483647)),rng.choice((0,60,61,255)),rng.randrange(65536),rng.randrange(65536),(0,1,2,4,5,255)[(case+i+j)%6],rng.choice((0,1,15,16,255)),(0,1,2,128,255,None)[(case//5+i+j)%6]]
            mobs.append(row)
        if case%5==0:mobs.append(mobs[0].copy());mobs[-1][44]=7
        yield case,mobmax,skillmax,skills,mobs


def sql_scenario(path):
    skills=[];mobs=[];header=None
    for line in path.read_text().splitlines():
        if not line or line.startswith('#'):continue
        parts=line.split()
        if parts[0]=='S':header=list(map(int,parts[1:]));continue
        if parts[0] in ('K','M'):
            (skills if parts[0]=='K' else mobs).append([None if x=='N' else int(x) for x in parts[1:]])
        elif parts[0]!='E':raise RuntimeError('Unexpected SQL snapshot operation')
    if header is None or len(header)!=3:raise RuntimeError('Missing SQL MaxID snapshot')
    if any(row[c] is not None and (row[c]&65535)==header[2]+1 for row in mobs for c in (45,53,61)):
        raise RuntimeError('Restored row references the original undefined one-past skill slot')
    yield *header,skills,mobs


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sql-rows',type=Path,help='Ignored positional snapshot from read-only native SQL test')
    options=parser.parse_args()
    scenarios=sql_scenario(options.sql_rows) if options.sql_rows else synthetic_scenarios()
    cpu,digest=load_game();stack=0x20000000;game=0x30000000;database=0x32000000;mobdata=0x33000000;skilldata=0x34000000;mob=0x35000000;stop=0x10000000
    for at in (stack,database,mob,stop):cpu.mem_map(at,0x10000)
    for at in (mobdata,skilldata):cpu.mem_map(at,0x200000)
    cpu.mem_map(game,0xb00000)
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    state={};calls={}
    def hook(c,at,size,_):
        sp=c.reg_read(UC_X86_REG_ESP)
        if at==0x4d3c40:c.reg_write(UC_X86_REG_EAX,database)
        elif at==0x503ae3:c.reg_write(UC_X86_REG_EAX,skilldata if state['service']=='skill' else mobdata)
        elif at==0x4e2050:state.update(query=state['query']+1,row=-1);c.reg_write(UC_X86_REG_EAX,0)
        elif at==0x4e2090:
            state['row']+=1;n=1 if state['query']==0 else len(state['rows']);c.reg_write(UC_X86_REG_EAX,0 if state['row']<n else 100)
        elif at==0x4e15b0:
            column,n,kind,dest=struct.unpack('<4I',c.mem_read(sp+4,16));kind=kind if kind<0x80000000 else kind-0x100000000
            if state['query']==0:v=state['max']
            else:
                v=state['rows'][state['row']][column-1]
                key=(state['service'],column);shape=(n,kind)
                if key in calls and calls[key]!=shape:raise RuntimeError('SQL binding shape changed')
                calls[key]=shape
            if v is not None:
                if kind==1:c.mem_write(dest,b'Binding\0')
                else:
                    width={4:4,5:2,-6:1}[kind];c.mem_write(dest,(v&((1<<(width*8))-1)).to_bytes(width,'little'))
            c.reg_write(UC_X86_REG_EAX,0)
        elif at==0x504697:c.reg_write(UC_X86_REG_EAX,0)
        elif at==stop+16:c.reg_write(UC_X86_REG_EAX,123)
        elif at in (0x4a9130,0x4b4c20):c.reg_write(UC_X86_REG_EAX,0)
    for at,n in ((0x4d3c40,0),(0x4d3d00,4),(0x503ae3,0),(0x4e13f0,0),(0x4e2050,4),(0x4e2090,0),(0x4e15b0,20),(0x4d9510,0),(0x505316,0),(0x48ec70,4),(0x48f600,0),(0x504697,0),(stop+16,0),(0x4a9130,4),(0x4b4c20,0)):
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
    put(0x5210e4,stop+16)
    def call(at,reg,args=()):
        cpu.mem_write(stack,bytes(0x10000));put(stack+0xf000,stop,*args);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_FPCW,0x27f)
        for r,v in reg:cpu.reg_write(r,v)
        cpu.emu_start(at,stop,count=100000000 if options.sql_rows else 2000000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError(f'Original loader/Init did not return {at:08x}')
        return cpu.reg_read(UC_X86_REG_EAX)&255
    def read(at,width):return int.from_bytes(cpu.mem_read(at,width),'little')
    print(f'# ps_game.exe SHA256 {digest}; complete LoadConstMobSkill 004c0be0, LoadConstMob 004c1720, GetMobSkillInfo 004027e0, CMob::Init 00490040/SetAttack 004902f0. SQL/allocator/cookie/clock/random/path/AI ports. No skill lookup stub.')
    print('# S scenario mob_max skill_max; K full 82-column skill row; M full 69-column mob row; D mob_id ai range country then three normalized attack projections; I Init definition retention/timer-zero assertions; E end scenario. N=NULL; labels deliberately absent.')
    for case,mobmax,skillmax,skills,mobs in scenarios:
        if (mobmax+1)*268>0x200000 or (skillmax+1)*160>0x200000:raise RuntimeError('Original oracle allocation mapping exceeded')
        print('S',case,mobmax,skillmax)
        for row in skills:print('K',*('N' if v is None else v for v in row))
        for row in mobs:print('M',*('N' if v is None else v for v in row))
        state.update(service='skill',max=skillmax,rows=skills,query=-1,row=-1)
        if not call(0x4c0be0,((UC_X86_REG_EBX,game),)):raise RuntimeError('Original skill loader failed')
        if get(game+0xa8181c)!=skillmax+1 or get(game+0xa81820)!=skilldata:raise RuntimeError('Skill allocation binding differs')
        for i in range(1,skillmax+1):
            p=skilldata+i*160;print('P',i,read(p,2),read(p+49,1),read(p+50,2),read(p+74,2),read(p+2,1))
        state.update(service='mob',max=mobmax,rows=mobs,query=-1,row=-1)
        if not call(0x4c1720,((UC_X86_REG_ECX,game),)):raise RuntimeError('Original mob loader failed')
        checked=0
        for i in range(1,mobmax+1):
            p=mobdata+i*268
            if read(p,2)==0:continue
            checked+=1;out=[read(p,2),read(p+38,1),read(p+105,1),get(p+120)]
            for j in range(3):
                a=p+128+j*20;pointer=get(a+16);skillid=(pointer-skilldata)//160 if pointer else 0
                out += [read(a,2),read(a+2,1),get(a+4),read(a+8,1),read(a+10,2),read(a+12,2),read(a+14,1),read(a+15,1),skillid,read(pointer+74,2) if pointer else 0,read(pointer+2,1) if pointer else 0]
            print('D',*out)
            before=bytes(cpu.mem_read(p,268));cpu.mem_write(mob,bytes(3380));put(mob+0xa8,p);put(mob+0x1b8,111,222,333)
            call(0x490040,((UC_X86_REG_ESI,mob),),(i,0))
            if get(mob+0xa8)!=p or bytes(cpu.mem_read(p,268))!=before or any(get(mob+0x1b8+k*4) for k in range(3)):raise RuntimeError('Original Init changed definition bindings/timer reset differs')
        print('I',checked);print('E')
    for (service,col),shape in sorted(calls.items()):print('# SQL',service,col,*shape)
if __name__=='__main__':main()
