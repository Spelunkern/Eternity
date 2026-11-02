#!/usr/bin/env python3
"""Original DB NPC SQL owner, DB packet, game decoding and client packet.

Ghidra work/guild-bank-db, work/guild-house-db-output,
work/guild-global-init, work/guild-house-client. ODBC/pool/OS/transport ports;
original slot writes, omitted rows, byte widths and all packet code execute.
"""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_original


def main():
    db,digest=load_original('ps_dbAgent');game,game_digest=load_original('ps_game');stack,info,client,data,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
    for machine in (db,game):
        for at in (stack,info,client,data,stop):machine.mem_map(at,0x10000)
    def word(machine,at):return struct.unpack('<I',machine.mem_read(at,4))[0]
    def put(machine,at,*v):machine.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    state={};db_packets=[];client_packets=[]
    def db_hook(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x43ccc0:machine.reg_write(UC_X86_REG_EAX,data)
        elif address==0x43f350:state['row']=-1;machine.reg_write(UC_X86_REG_EAX,0xffffffff if state['fault']==-2 else 0)
        elif address==0x43f390:state['row']+=1;machine.reg_write(UC_X86_REG_EAX,100 if state['row']>=len(state['rows']) else 0)
        elif address==0x43e8b0:
            column,n=word(machine,sp+4)-1,word(machine,sp+8);at=word(machine,sp+16)
            if state['row']==state['fault']:machine.reg_write(UC_X86_REG_EAX,0xffffffff)
            else:
                value=state['rows'][state['row']][column]
                if value is not None:machine.mem_write(at,int(value&((1<<(n*8))-1)).to_bytes(n,'little'))
                machine.reg_write(UC_X86_REG_EAX,0)
        elif address==0x4327a0:db_packets.append(bytes(machine.mem_read(word(machine,sp+4),word(machine,sp+8))))
    def game_hook(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x4328c0:machine.reg_write(UC_X86_REG_EAX,info)
        elif address==0x4d4d20:client_packets.append(bytes(machine.mem_read(word(machine,sp+4),word(machine,sp+8))))
    for at,cleanup in ((0x43ccc0,0),(0x43cd80,4),(0x43f350,4),(0x43f390,0),(0x43e8b0,20),(0x4327a0,8),(0x432d80,0),(0x44447f,0),(0x444dc3,0),(stop+0x100,4)):
        db.mem_write(at,b'\xc2'+struct.pack('<H',cleanup));db.hook_add(UC_HOOK_CODE,db_hook,begin=at,end=at)
    for iat in (0x4600d4,0x4600d8):put(db,iat,stop+0x100)
    for at,cleanup in ((0x4328c0,4),(0x4d4d20,8),(0x505316,0),(stop+0x100,4)):
        game.mem_write(at,b'\xc2'+struct.pack('<H',cleanup));game.hook_add(UC_HOOK_CODE,game_hook,begin=at,end=at)
    for iat in (0x5210cc,0x5210d0):put(game,iat,stop+0x100)
    def call(machine,address,args=(),regs=()):
        machine.mem_write(stack,bytes(0x10000));put(machine,stack+0xf000,stop,*args);machine.reg_write(UC_X86_REG_ESP,stack+0xf000)
        for reg,value in regs:machine.reg_write(reg,value)
        machine.emu_start(address,stop,count=100000)
        if machine.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original guild NPC function did not return')
        return machine.reg_read(UC_X86_REG_EAX)
    rng=random.Random(0x427f40)
    print(f'# ps_dbAgent.exe SHA256 {digest}; LoadNpcLvList 00427f40, SendGuildNpcLv 0040f660; ps_game.exe SHA256 {game_digest}; PacketGuild 00408e00, SendNpcLvList 0042f0e0; ODBC/OS/pool/transport ports')
    for case in range(512):
        gid=rng.getrandbits(32);initial=[rng.choice((0,1,65535,rng.getrandbits(16))) for _ in range(24)];previous=[rng.choice((0,1,65535,rng.getrandbits(16))) for _ in range(16)]
        rows=[]
        for i in range(case%17):rows.append((rng.choice((0,1,-1,32767,-32768,None)),rng.choice((0,1,255,None)),rng.choice((0,1,7,8,255,None))))
        fault=-2 if case%13==0 else rng.randrange(len(rows)) if rows and case%5==0 else -1
        db.mem_write(info,bytes(0x10000));game.mem_write(info,bytes(0x10000));put(db,info+12,gid);db.mem_write(info+0x470,struct.pack('<24H',*initial));game.mem_write(info+0x47c,struct.pack('<16H',*previous));state.update(rows=rows,fault=fault,row=-1);db_packets.clear();client_packets.clear()
        print('CASE',gid,fault,len(rows),*initial,*previous)
        for row in rows:print('ROW',*['N' if value is None else value for value in row])
        returned=call(db,0x427f40,(gid,),((UC_X86_REG_EDI,info),));call(db,0x40f660,(client,),((UC_X86_REG_ESI,info),))
        print('RETURN',returned);print('DBSTATE',*struct.unpack('<24H',db.mem_read(info+0x470,48)))
        for p in db_packets:
            print('DBPACKET',p.hex());game.mem_write(data,p);call(game,0x408e00,regs=((UC_X86_REG_EBX,data),))
        call(game,0x42f0e0,(client,),((UC_X86_REG_ESI,info),))
        print('GAMESTATE',*struct.unpack('<16H',game.mem_read(info+0x47c,32)))
        for p in client_packets:print('CLIENT',p.hex())
        print('END')


if __name__=='__main__':main()
