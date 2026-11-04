#!/usr/bin/env python3
"""Complete original live reloads; record SQL calls, cleanup and field publication."""
import random
import struct
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack=0x20000000;game=0x30000000;db=0x32000000;mobs=0x33000000;skills=0x34000000;counter=0x35000000;stop=0x10000000
    for at in (stack,db,mobs,skills,counter,stop):cpu.mem_map(at,0x10000)
    cpu.mem_map(game,0xb00000)
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    state={};maps={0:{},1:{}}
    def event(v):state['events'].append(v)
    def hook(c,at,size,_):
        sp=c.reg_read(UC_X86_REG_ESP);mode=state['mode']
        if at==stop+16:event('P');c.reg_write(UC_X86_REG_EAX,0 if mode==1 else 1)
        elif at==0x4cb4e0:event('S')
        elif at==0x4d3840:event('C');c.reg_write(UC_X86_REG_EAX,0 if mode==2 else 1)
        elif at==0x4d3c40:event('A');c.reg_write(UC_X86_REG_EAX,0 if mode==3 else db)
        elif at==0x4e2050:
            query=bytes(c.mem_read(get(sp+4),100)).split(b'\0')[0]
            expected=b'EXEC usp_Read_MobSkills_R2 7' if state['kind'] else b'EXEC usp_Read_Mobs_R2 7'
            if query!=expected:raise RuntimeError(f'Wrong reload query: {query!r}')
            event('Q7');c.reg_write(UC_X86_REG_EAX,0xffff if mode==4 else 0)
        elif at==0x4e2090:
            state['row']+=1;r=state['row'];n=len(state['rows']);value=(-1 if mode==10 and r==0 or mode==11 and r==1 else 0 if r<n else 100);event('F'+str(value));c.reg_write(UC_X86_REG_EAX,value&0xffff)
        elif at==0x4e15b0:
            col,n,kind,dest=struct.unpack('<4I',c.mem_read(sp+4,16));kind=kind if kind<0x80000000 else kind-0x100000000;r=state['row'];event('G'+str(col))
            if col==1:state['base']=dest
            offset=dest-state['base'];maps[state['kind']][col]=(offset,n,kind)
            failed=(mode in (7,10) and col==1 or mode==8 and col==state['width']//2 or mode==9 and col==state['width'] or mode==11 and r==1 or mode==16 and r==1 and col==state['width']//2)
            if failed:c.reg_write(UC_X86_REG_EAX,0xffff);return
            value=state['rows'][r][col-1]
            if value is not None:
                if kind==1:c.mem_write(dest,b'R\0')
                else:
                    width={4:4,5:2,-6:1}[kind];c.mem_write(dest,(value&((1<<(8*width))-1)).to_bytes(width,'little'))
            c.reg_write(UC_X86_REG_EAX,0)
        elif at==0x4d3d00:event('B')
        elif at==0x4d33e0:event('D')
        elif at==0x4d35e0:
            event('R')
            if mode in (13,17):
                p=state['target'];cpu.mem_write(p+38,bytes([cpu.mem_read(p+38,1)[0]^255]));put(counter,97)
        elif at==0x4d9510:event('E')
    ports=((stop+16,24),(0x4cb4e0,0),(0x4d3840,28),(0x4d3c40,0),(0x4e2050,4),(0x4e2090,0),(0x4e15b0,20),(0x4d3d00,4),(0x4d33e0,0),(0x4d35e0,0),(0x4d9510,0),(0x505316,0))
    for at,n in ports:
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
    put(0x5210ec,stop+16)
    def written(c,access,at,n,value,_):
        p=state.get('target',0)
        if p<=at<p+state.get('size',0):state['writes'].append(f'{at-p}:{n}')
    cpu.hook_add(UC_HOOK_MEM_WRITE,written)
    def canonical(raw,kind):
        raw=bytearray(raw)
        if kind==0:
            for i in range(3):
                off=128+i*20+16;p=struct.unpack_from('<I',raw,off)[0];struct.pack_into('<I',raw,off,(p-skills)//160 if p else 0)
        return bytes(raw)
    def call(kind):
        state.update(row=-1,events=[],writes=[]);put(stack+0xf000,stop,game,7,*([counter] if kind else []));cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_ECX,counter);cpu.reg_write(UC_X86_REG_FPCW,0x27f)
        cpu.emu_start(0x4c50c0 if kind else 0x4c20c0,stop,count=1000000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original reload did not return')
        return cpu.reg_read(UC_X86_REG_EAX)&255
    rng=random.Random(0x4c50c0)
    print(f'# ps_game.exe SHA256 {digest}; complete ReLoadConstMob 004c20c0 / ReLoadConstMobSkill 004c50c0, real GetMobSkillInfo 004027e0. Ports: profile/parse/pool/SQL/log/cookie. Raw pointers canonicalized to stable skill-slot tokens.')
    for case in range(432):
        kind=case%2;mode=case//2%18;width=82 if kind else 69;size=160 if kind else 268;p=skills+160 if kind else mobs+268
        cpu.mem_write(game,bytes(0x100));put(game+0xa81804,9,mobs);put(game+0xa8181c,9,skills)
        links=[]
        for i in range(1,9):
            at=skills+i*160;cpu.mem_write(at,bytes(160));sid=0 if i==8 and case%3==0 else i;effect=(1,2,3,4)[i%4];detail=(40,41,42)[(case+i)%3];cpu.mem_write(at,struct.pack('<H',sid));cpu.mem_write(at+49,bytes([effect]));cpu.mem_write(at+50,struct.pack('<H',detail));links.append((i,sid,effect,detail))
        raw=bytearray(rng.randbytes(size));struct.pack_into('<H',raw,0,1)
        if not kind:
            for i in range(3):struct.pack_into('<I',raw,144+i*20,skills+(i+1)*160 if (case+i)%2 else 0)
        cpu.mem_write(p,bytes(raw))
        rows=[]
        for r in range(2 if mode in (6,16) else 0 if mode in (5,17) else 1):
            row=[rng.randrange(256) for _ in range(width)];row[0]=1
            if kind:row[16]=(case//3)%5;row[17]=(40,41,42)[case%3];row[37]=(0,1,65,65535)[case//4%4]
            else:
                row[7]=(0,1000,1001,2000,2001,4294967295)[case//4%6]
                for i in range(3):row[45+i*8]=(0,1,2,3,4,5,8,99)[(case+i)%8];row[52+i*8]=(0,1,2,128,255)[(case//3+i)%5]
            if mode==12:
                for i in range(3,width,3):row[i]=None
            rows.append(row)
        initial=(0xfffffffe if mode==14 else case*13)&0xffffffff
        state.update(kind=kind,mode=0 if mode==15 else mode,width=width,size=size,target=p,rows=rows)
        if mode==15:
            put(counter,0);call(kind);raw=bytearray(cpu.mem_read(p,size))
        before=canonical(cpu.mem_read(p,size),kind);put(counter,initial);success=call(kind)
        print('S',case,kind,mode,initial,before.hex())
        for values in links:print('L',*values)
        for row in rows:print('V',*('N' if v is None else v for v in row))
        print('O',success,get(counter),canonical(cpu.mem_read(p,size),kind).hex(),'|',' '.join(state['writes']),'|',' '.join(state['events']))
    for kind,columns in maps.items():
        for col,(off,n,ctype) in sorted(columns.items()):print('# MAP',kind,col,off,n,ctype)
if __name__=='__main__':main()
