#!/usr/bin/env python3
"""Original DB guild bank SQL owner and game bank receive/client owners.

Ghidra work/guild-bank-db, work/guild-house-db-output, work/guild-global-init,
work/guild-house-client and work/guild-bank-kind. Original temporary intrusive
list, relocation/error cleanup, pooled reuse, craft trim, packet code, count
clamp and CopyCraftExpassion run. ODBC, allocation, lookup and transport ports.
"""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_original


def main():
    db,digest=load_original('ps_dbAgent');game,game_digest=load_original('ps_game');stack,info,items,data,stop=0x20000000,0x30000000,0x31000000,0x35000000,0x10000000
    for machine in (db,game):
        for at in (0,stack,info,data,stop):machine.mem_map(at,0x10000)
        machine.mem_map(items,0x200000)
    def word(m,at):return struct.unpack('<I',m.mem_read(at,4))[0]
    def put(m,at,*v):m.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    state={};db_packets=[];game_packets=[];corrections=[]
    def dbhook(m,address,size,context):
        sp=m.reg_read(UC_X86_REG_ESP)
        if address==0x443986:
            n=word(m,sp+4);at=state['heap'];state['heap']+=(n+15)&~15
            fail=False
            if n==72:state['fresh']+=1;fail=state['fail'] and state['fresh']%5==0
            if not fail:m.mem_write(at,bytes(n))
            m.reg_write(UC_X86_REG_EAX,0 if fail else at)
        elif address==0x43ccc0:m.reg_write(UC_X86_REG_EAX,data)
        elif address==0x43f350:state['row']=-1;m.reg_write(UC_X86_REG_EAX,0xffffffff if state['fault']==-2 else 0)
        elif address==0x43f390:state['row']+=1;m.reg_write(UC_X86_REG_EAX,100 if state['row']>=len(state['rows']) else 0)
        elif address==0x43e8b0:
            column,n,at=word(m,sp+4)-1,word(m,sp+8),word(m,sp+16)
            if state['row']==state['fault']:m.reg_write(UC_X86_REG_EAX,0xffffffff)
            else:
                value=state['rows'][state['row']][column]
                if isinstance(value,bytes):m.mem_write(at,value+b'\0')
                else:m.mem_write(at,(value&((1<<(8*n))-1)).to_bytes(n,'little'))
                m.reg_write(UC_X86_REG_EAX,0)
        elif address==0x4327a0:
            p=bytearray(m.mem_read(word(m,sp+4),word(m,sp+8)))
            for at in range(7,len(p),46):
                end=p.index(0,at+25,at+46);p[end+1:at+46]=bytes(at+46-end-1)
            db_packets.append(bytes(p))
    def ghook(m,address,size,context):
        sp=m.reg_read(UC_X86_REG_ESP)
        if address==0x4328c0:m.reg_write(UC_X86_REG_EAX,info)
        elif address==0x402710:m.reg_write(UC_X86_REG_EAX,data+0x1000)
        elif address==0x40a510:
            state['game_attempts']+=1;failed=state['game_fail'] and state['game_attempts']%7==0
            at=items+state['game_attempts']*256;m.mem_write(at,bytes(160));m.reg_write(UC_X86_REG_EAX,0 if failed else at)
        elif address==0x4d4f10:corrections.append(bytes(m.mem_read(word(m,sp+4),word(m,sp+8))))
        elif address==0x4d4d20:
            p=bytearray(m.mem_read(word(m,sp+4),word(m,sp+8)))
            for at in range(3,len(p),33):
                end=p.index(0,at+12,at+33);p[end+1:at+33]=bytes(at+33-end-1)
            game_packets.append(bytes(p))
    for at,cleanup in ((0x443986,0),(0x443c3e,0),(0x43ccc0,0),(0x43cd80,4),(0x43f350,4),(0x43f390,0),(0x43e8b0,20),(0x4327a0,8),(0x432d80,0),(0x44447f,0),(0x444dc3,0),(stop+0x100,4)):
        db.mem_write(at,b'\xc2'+struct.pack('<H',cleanup));db.hook_add(UC_HOOK_CODE,dbhook,begin=at,end=at)
    for iat in (0x4600d4,0x4600d8):put(db,iat,stop+0x100)
    for at,cleanup in ((0x4328c0,4),(0x402710,4),(0x40a510,0),(0x4d4f10,8),(0x4d4d20,8),(0x4d9510,0),(0x505316,0),(stop+0x100,4)):
        game.mem_write(at,b'\xc2'+struct.pack('<H',cleanup));game.hook_add(UC_HOOK_CODE,ghook,begin=at,end=at)
    for iat in (0x5210cc,0x5210d0):put(game,iat,stop+0x100)
    put(game,0x546f44,data+0x4000)
    def call(m,address,args=(),regs=()):
        m.mem_write(stack,bytes(0x10000));put(m,stack+0xf000,stop,*args);m.reg_write(UC_X86_REG_ESP,stack+0xf000)
        for reg,value in regs:m.reg_write(reg,value)
        try:m.emu_start(address,stop,count=2000000)
        except Exception as e:raise RuntimeError(f'Original bank fault {m.reg_read(UC_X86_REG_EIP):08x}') from e
        if m.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError(f'Original bank did not return {address:08x}')
        return m.reg_read(UC_X86_REG_EAX)
    stamp=((2024-2000)<<26)|(5<<22)|(10<<17)|(12<<12)|(13<<6)|14
    def row(i,slot):
        craft=(b'01020304050607080903',b' 01 02 ',b'')[i%3];iid=10000+i;uid=0x10000000000+i;quality=(100+i)&65535;count=(i*7)&255;gems=bytes((i+j)&255 for j in range(6))
        image=struct.pack('<QBBB H',uid,slot&255,5,1,quality)+gems+bytes([count])+struct.pack('<IB',stamp,ord('M'))+craft+b'\0'+bytes(20-len(craft))
        sql=(iid,5,1,uid,slot,quality,*gems,craft,count,b'M',2024,5,10,12,13,14)
        return iid,image,sql
    def write_item(at,i,slot):
        iid,image,sql=row(i,slot);put(db,at+8,0);db.mem_write(at+16,image[:8]);put(db,at+24,iid);db.mem_write(at+28,image[9:11]+image[19:20]);db.mem_write(at+32,image[11:19]);raw=image[25:46].split(b'\0')[0].replace(b' ',b'');db.mem_write(at+40,raw+b'\0'+bytes(20-len(raw)));put(db,at+64,stamp);db.mem_write(at+68,b'M');return iid,image,sql
    print(f'# ps_dbAgent.exe SHA256 {digest}; LoadStoredItemList 00427800, SendGuildBank 0040f7d0; ps_game.exe SHA256 {game_digest}; PacketGuild 00408e00, SendBankItemList 0042f250; actual lists/trim/craft; ODBC/allocator/lookup/OS/transport ports')
    for case in range(96):
        seed=(0,1,230,240)[case%4];count=(0,1,39,40,41,49,50,51,240,260)[case%10];fault=-2 if case%23==0 else count//2 if case%11==0 and count else -1;pool_count=case%4;real_type=(0,1,25,26,255)[case%5];limit=(0,1,3,255)[case%4];gid=0x80000000+case
        for m in (db,game):m.mem_write(info,bytes(0x10000));m.mem_write(items,bytes(0x200000))
        put(db,info+12,gid);put(db,0x470ab8,0,0,0);put(game,data+0x10a0,real_type);game.mem_write(data+0x104b,bytes([limit]));put(game,info+8,gid)
        for i in range(seed):at=items+i*80;write_item(at,9000+i,i);put(db,info+0x6c+4*i,at)
        for i in range(pool_count):at=items+0x10000+i*80;put(db,at+4,word(db,0x470ab8));put(db,at+8,(0,1,17)[i%3]);put(db,0x470ab8,at);put(db,0x470ac0,i+1)
        state.update(heap=items+0x20000,row=-1,rows=[],fresh=0,fail=case%7==0,fault=fault,game_attempts=0,game_fail=case%6==0);db_packets.clear();game_packets.clear();corrections.clear()
        print('CASE',case,seed,count,fault,pool_count,real_type,limit,gid)
        for i in range(count):
            slot=(0 if i%3==0 else i%240) if case%3 else (239 if i%2 else 240+i)
            iid,image,sql=row(i,slot);state['rows'].append(sql);print('ROW',slot,iid,image.hex())
        result=call(db,0x427800,(gid,),((UC_X86_REG_ECX,info),));print('RETURN',result);print('POOL',word(db,0x470ac0))
        for slot in range(240):
            at=word(db,info+0x6c+slot*4)
            if at:print('ITEM',slot,word(db,at+8),word(db,at+24),bytes(db.mem_read(at+16,8)).hex(),bytes(db.mem_read(at+28,3)).hex(),bytes(db.mem_read(at+32,8)).hex(),bytes(db.mem_read(at+40,21)).split(b'\0')[0].hex() or '-',word(db,at+64),db.mem_read(at+68,1)[0])
        call(db,0x40f7d0,(data,),((UC_X86_REG_ECX,info),))
        for p in db_packets:print('DB',p.hex())
        for repeat in range(1+case%2):
            for p in db_packets:
                game.mem_write(data,p);call(game,0x408e00,regs=((UC_X86_REG_EBX,data),))
        call(game,0x42f250,(data,),((UC_X86_REG_ECX,info),(UC_X86_REG_EDI,data)))
        for p in corrections:print('CORRECTION',p.hex())
        for p in game_packets:print('CLIENT',p.hex())
        print('END')


if __name__=='__main__':main()
