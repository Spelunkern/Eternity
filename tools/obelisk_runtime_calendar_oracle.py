#!/usr/bin/env python3
"""Original calendar bodies with Wine OLE response fixtures at OS import ports."""
import contextlib
import datetime
import os
from pathlib import Path
import random
import struct
import subprocess
import sys
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP,
                              UC_X86_REG_EIP, UC_X86_REG_FPCW)
from original_oracle import ROOT, load_game
from parallel_lock import resource_lock

PROBE = r'''
#include <windows.h>
#include <oleauto.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    unsigned y,m,d,h,n,s;
    while(scanf("%u %u %u %u %u %u",&y,&m,&d,&h,&n,&s)==6) {
        SYSTEMTIME time={(WORD)y,(WORD)m,65535,(WORD)d,(WORD)h,(WORD)n,(WORD)s,65535};
        double value=0;uint64_t bits=0;int ok=SystemTimeToVariantTime(&time,&value);memcpy(&bits,&value,8);
        printf("%d %016llx\n",ok,(unsigned long long)bits);
    }
    return 0;
}
'''


def unpack(value):
    return (2000+(value>>26),(value>>22)&15,0,(value>>17)&31,(value>>12)&31,(value>>6)&63,value&63,0)


def pack(calendar):
    y,m,_,d,h,n,s,_=calendar
    return ((((((y-16)*16+m)*32+d)*32+h)*64+n)*64+s)&0xffffffff


def calendar(date):
    return (date.year,date.month,date.weekday(),date.day,date.hour,date.minute,date.second,999)


def main():
    work=ROOT/'work'/'obelisk-calendar-probe';work.mkdir(parents=True,exist_ok=True)
    source=work/'probe.c';source.write_text(PROBE)
    with contextlib.redirect_stdout(sys.stderr), resource_lock(ROOT,'build',2):
        subprocess.run(['winegcc','-m64',str(source),'-o',str(work/'probe.exe'),'-loleaut32'],check=True,timeout=60,capture_output=True)
    rng=random.Random(0x4cb9d0)
    locals_=[(rng.randrange(65536),rng.randrange(65536),65535,rng.randrange(65536),rng.randrange(65536),rng.randrange(65536),rng.randrange(65536),65535) for _ in range(256)]
    locals_ += [(y,2,0,29,23,59,59,999) for y in (1999,2000,2004,2024,2063,2064,2099,2100)]
    pairs=[];flows=[]
    for i in range(1024):
        date=datetime.datetime(2000,1,1)+datetime.timedelta(days=rng.randrange(23376),seconds=rng.randrange(86400))
        offset=(0,1,2,59,60,86399,86400,86401,-1,-86399,2147483000)[i%11]
        then=date-datetime.timedelta(seconds=offset)
        pairs.append((pack(calendar(date)),pack(calendar(then))))
        if i%5==0: pairs.append((rng.getrandbits(32),rng.getrandbits(32)))
        if i<512: flows.append((calendar(date),0 if i%13==0 else pack(calendar(then)),(i*73)&255))
    keys=set()
    for now,then in pairs:
        for value in (now,then):
            y,m,_,d,h,n,s,_=unpack(value);keys.add((y,m,d,h,n,s))
    for local,then,_ in flows:
        for value in (pack(local),then):
            y,m,_,d,h,n,s,_=unpack(value);keys.add((y,m,d,h,n,s))
    keys=sorted(keys)
    cache=ROOT/'work'/'obelisk-calendar-cache';cache.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,XDG_CACHE_HOME=str(cache),WINEPREFIX=str(ROOT/'work'/'obelisk-calendar-wine'),WINEARCH='win64',WINEDEBUG='-all',
             WINEDLLOVERRIDES='winemenubuilder.exe=d;mscoree=d;mshtml=d')
    result=subprocess.run([str(work/'probe.exe')],input=''.join(' '.join(map(str,k))+'\n' for k in keys),text=True,
                          capture_output=True,env=env,check=True,timeout=120)
    responses=result.stdout.splitlines()
    if len(responses)!=len(keys): raise RuntimeError('Incomplete Wine OLE response table')
    variants={key:(int(line.split()[0]),int(line.split()[1],16)) for key,line in zip(keys,responses)}
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x100000)
    world,map_,obelisk=base,base+0x1000,base+0x2000
    stack,stop=base+0xe0000,base+0xf0000
    def put(at,*values): cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    def get(at): return struct.unpack('<I',cpu.mem_read(at,4))[0]
    state={}
    put(0x5210dc,stop+16);put(0x5212cc,stop+32)
    cpu.mem_write(stop+16,b'\xc2\x04\x00');cpu.mem_write(stop+32,b'\xc2\x08\x00')
    def hook(machine,address,size,data):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==stop+16:
            state['calls']+=1;machine.mem_write(get(sp+4),struct.pack('<8H',*state['local']))
        elif address==stop+32:
            y,m,_,d,h,n,s,_=struct.unpack('<8H',machine.mem_read(get(sp+4),16))
            ok,bits=variants[(y,m,d,h,n,s)]
            machine.mem_write(get(sp+8),struct.pack('<Q',bits));machine.reg_write(UC_X86_REG_EAX,ok)
    cpu.hook_add(UC_HOOK_CODE,hook)
    def run(address,*args):
        put(stack,stop,*args);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_FPCW,0x27f)
        cpu.emu_start(address,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop: raise RuntimeError('Original instruction limit exceeded')
        return cpu.reg_read(UC_X86_REG_EAX)
    print(f'# ps_game.exe SHA256 {digest}; real GetSystemTime 004cb8e0, SubTimeSecond 004cb9d0 and SetKeepObelisk 00413bd0. Only GetLocalTime/SystemTimeToVariantTime imports observed. OLE responses captured from installed Wine, not a Microsoft Windows DLL claim. CRT default 53-bit precision: _fpmath 00506081 / _setdefaultprecision 0051136e.')
    print('# V y month day hour minute second success variant_bits; P eight SYSTEMTIME words packed; S packed_now packed_change seconds; R eight local words change packed_country expected_country last remaining change local_calls.')
    for key,(ok,bits) in sorted(variants.items()): print('V',*key,ok,bits)
    for local in locals_:
        state.update(local=local,calls=0);print('P',*local,run(0x4cb8e0))
    for now,then in pairs:
        cpu.reg_write(UC_X86_REG_EAX,now);cpu.reg_write(UC_X86_REG_ECX,then)
        print('S',now,then,run(0x4cb9d0))
    for local,then,packed in flows:
        state.update(local=local,calls=0);put(world+8,1,map_);put(map_+0x1e8,1,obelisk);put(obelisk+0x14,123)
        cpu.reg_write(UC_X86_REG_EAX,world);run(0x413bd0,123,packed,then)
        print('R',*local,then,packed,*[get(obelisk+x) for x in (4,8,12,16)],state['calls'])

if __name__=='__main__': main()
