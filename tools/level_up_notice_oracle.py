#!/usr/bin/env python3
"""Execute complete LevelUp notification coordinator with observed owner ports."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game

def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x10000);cpu.mem_map(0,0x10000);stop=base+0xf000;context={}
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    ports={0x454280:0,0x4d4f10:8,0x4d4d20:8,0x421220:32,0x444d80:0,0x4455d0:0,0x4822f0:0,0x481f20:8,0x45b0d0:0,0x4d4e50:8,0x42ebd0:12,0x505316:0,stop+16:4}
    for at,n in ports.items():cpu.mem_write(at,b'\xc2'+struct.pack('<H',n))
    put(0x5210cc,stop+16);put(0x5210d0,stop+16)
    def event(tag,*values):context['events']+=tag.encode()+struct.pack('<'+'I'*len(values),*values)
    def packet(tag,at,n):event(tag,n);context['events']+=bytes(cpu.mem_read(at,n))
    def hook(machine,address,size,data):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x454280:event('A')
        elif address in (0x4d4f10,0x4d4d20,0x4d4e50):packet({0x4d4f10:'B',0x4d4d20:'C',0x4d4e50:'G'}[address],get(sp+4),get(sp+8))
        elif address==0x421220:packet('V',get(sp+8),get(sp+12))
        elif address==0x444d80:event('P')
        elif address==0x4455d0:packet('T',machine.reg_read(UC_X86_REG_ECX),machine.reg_read(UC_X86_REG_EAX))
        elif address==0x4822f0:event('E')
        elif address==0x481f20:event('R',machine.reg_read(UC_X86_REG_EAX),get(sp+4),get(sp+8))
        elif address==0x45b0d0:machine.mem_write(machine.reg_read(UC_X86_REG_ESI),bytes(74));event('M')
        elif address==0x42ebd0:event('U',get(sp+8),get(sp+12)&65535)
    cpu.hook_add(UC_HOOK_CODE,hook)
    for grow in range(4):
        for level in range(61):put(0x54cedc+(grow*61+level)*4,level*1000)
    print(f'# ps_game.exe SHA256 {digest}; complete CUser LevelUp 0048ad70; ports attack, transport, scene, party/guild, energy/recovery, log-main zero header, locks/cookie. Actual packet formatting and ordering execute.')
    print('# scenario grow level gain source scene party(-1/0/1) guild events')
    for scenario in range(64):
        cpu.mem_write(base,bytes(0xd000));grow=scenario%5;level=(1,2,30,60,61)[(scenario//3)%5];gain=(0,1,0xffffffff)[scenario%3];source=42 if scenario%2 else 777;scene=scenario%2;party=(-1,0,1)[(scenario//2)%3];guild=(scenario//5)%2
        put(base+0xdc,42,base+0x8000 if scene else 0);cpu.mem_write(base+0x12f,bytes([grow]));cpu.mem_write(base+0x136,struct.pack('<3H',level,11,13));put(base+0x13c,12345);cpu.mem_write(base+0x170,struct.pack('<6H',14,12,8,7,9,15));put(base+0x17c,1001,1002,1003);put(base+0x1238,65537,65538,65539);put(base+0x578c,123)
        if party>=0:put(base+0x173c,base+0x9000);cpu.mem_write(base+0x9114,bytes([party]))
        if guild:put(base+0x1758,base+0xa000)
        put(base+0xe000,stop,base);cpu.reg_write(UC_X86_REG_ESP,base+0xe000);cpu.reg_write(UC_X86_REG_ECX,gain);cpu.reg_write(UC_X86_REG_EDX,source);context['events']=b'';cpu.emu_start(0x48ad70,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Level notice did not return')
        print(scenario,grow,level,gain,source,scene,party,guild,context['events'].hex(),sep='\t')
if __name__=='__main__':main()
