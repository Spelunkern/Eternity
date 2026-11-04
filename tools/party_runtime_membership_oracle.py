#!/usr/bin/env python3
"""Execute full original normal-party entry/leave; observe required external owners."""
import struct
import sys
from original_oracle import load_game
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP


def main():
    real_send = '--real-party-send' in sys.argv[1:]
    real_type = real_send or '--real-party-type' in sys.argv[1:]
    cpu, digest = load_game()
    base = 0x30000000
    cpu.mem_map(base, 0x100000)
    party, stack, stop = base, base + 0xf0000, base + 0xff000
    users = [base + 0x10000 + i * 0x6000 for i in range(8)]
    heads = [base + 0x90000 + i * 0x1000 for i in range(8)]
    lock, unlock, tick = stop + 16, stop + 32, stop + 48
    buffer, frame, agent, worker = [base + n for n in (0xa0000, 0xa1000, 0xa2000, 0xa3000)]
    increment, decrement = stop + 64, stop + 80
    pools = {0x4df710:32, 0x4df7b0:128, 0x4df850:2048}
    def put(at, value): cpu.mem_write(at, struct.pack('<I', value & 0xffffffff))
    def get(at): return struct.unpack('<I', cpu.mem_read(at, 4))[0]
    def identity(at): return users.index(at) + 100 if at in users else 0
    def cursor(i):
        at = get(users[i] + 0xa98)
        return context['effect_counts'][i] if at == heads[i] else (at - heads[i]) // 64 - 1
    def state():
        values = [get(party + n) for n in (0x10, 0xc, 0x108, 0x10c)]
        values += [sum((get(u + 0x173c) == party) << i for i,u in enumerate(users))]
        values += [identity(get(party + 0x18 + i * 8)) for i in range(7)]
        values += [cpu.mem_read(u + 0x1744, 1)[0] for u in users]
        values += [cursor(i) for i in range(8)]
        return struct.pack('<' + 'I' * len(values), *values)
    def event(tag, payload=b''):
        context['events'] += tag.encode() + state() + struct.pack('<I', len(payload)) + payload
    for at, cleanup in {lock:4, unlock:4, tick:0, 0x4455d0:0, 0x4d4d20:8,
                        0x48a330:4, 0x4208c0:12, 0x45b0d0:0, 0x4d4e50:8, 0x505316:0}.items():
        if at == 0x48a330 and real_type: continue
        if at == 0x4455d0 and real_send: continue
        cpu.mem_write(at, b'\xc2' + struct.pack('<H', cleanup))
    put(0x5210cc, lock); put(0x5210d0, unlock); put(0x5210e4, tick)
    if real_send:
        for at, cleanup in {increment:4, decrement:4, 0x4dc1b0:8, 0x4df070:0, **{p:0 for p in pools}}.items():
            cpu.mem_write(at, b'\xc2' + struct.pack('<H', cleanup))
        put(0x5210f8, increment); put(0x5211f0, decrement)
    context = {}
    def hook(machine, at, size, data):
        sp = machine.reg_read(UC_X86_REG_ESP)
        if real_send and at in pools:
            event('A', struct.pack('<I', pools[at])); machine.reg_write(UC_X86_REG_EAX, buffer)
        elif real_send and at in (increment, decrement):
            address = get(sp + 4); value = (get(address) + (1 if at == increment else -1)) & 0xffffffff
            put(address, value); machine.reg_write(UC_X86_REG_EAX, value)
        elif real_send and at == 0x4df070:
            assert get(sp + 4) == buffer and get(buffer + 8) == 0
            event('B', struct.pack('<I', get(buffer + 8)))
        elif real_send and at == 0x4dc1b0:
            assert get(sp + 8) == buffer
            length = struct.unpack('<H', cpu.mem_read(frame, 2))[0]
            event('E', struct.pack('<2I', identity(get(sp + 4)), get(buffer + 8)) + bytes(cpu.mem_read(frame, length)))
            put(buffer + 8, get(buffer + 8) - 1) # explicit synchronous completion provider
        elif at == tick:
            event('K'); machine.reg_write(UC_X86_REG_EAX, context['tick'])
        elif at in (lock, unlock):
            arg = get(sp + 4)
            if arg == party + 0x118: event('[' if at == lock else ']')
            else:
                index = next(i for i,u in enumerate(users) if u + 0xa9c == arg)
                event('{' if at == lock else '}', struct.pack('<I', index + 100))
        elif at == 0x4455d0:
            event('P', bytes(machine.mem_read(machine.reg_read(UC_X86_REG_ECX), machine.reg_read(UC_X86_REG_EAX))))
        elif at == 0x4d4d20:
            event('U', struct.pack('<I', identity(machine.reg_read(UC_X86_REG_ECX))) + bytes(machine.mem_read(get(sp + 4), get(sp + 8))))
        elif at == 0x48a330 and not real_type:
            user = get(sp + 4)
            membership = get(user + 0x173c)
            result = 0 if not membership else 2 if get(party + 0x10) and get(party + 0x18 + get(party + 0xc)*8) == user else 1
            event('Y', struct.pack('<2I', identity(user), result)); machine.reg_write(UC_X86_REG_EAX, result)
        elif real_type and at in (0x48a3a6,0x48a3c8,0x48a3e1):
            event('Y', struct.pack('<2I', identity(get(sp + 4)), machine.reg_read(UC_X86_REG_EAX)))
        elif at == 0x4208c0:
            event('S', bytes(machine.mem_read(get(sp + 8), get(sp + 12))))
        elif at == 0x45b0d0:
            user_id = identity(machine.reg_read(UC_X86_REG_EDI))
            event('F', struct.pack('<I', user_id))
            cpu.mem_write(machine.reg_read(UC_X86_REG_ESI), bytes((user_id + i) & 255 for i in range(74)))
        elif at == 0x4d4e50:
            event('L', bytes(machine.mem_read(get(sp + 4), get(sp + 8))))
        elif at == 0x446a37:
            event('D'); machine.reg_write(UC_X86_REG_EIP, 0x446a7d)
        elif at == 0x446ac1:
            user = machine.reg_read(UC_X86_REG_EDI)
            if get(user + 0x175c):
                event('G', struct.pack('<I', identity(user))); put(user + 0x175c, 0)
                machine.reg_write(UC_X86_REG_EBX, unlock); machine.reg_write(UC_X86_REG_EIP, 0x446bae)
    cpu.hook_add(UC_HOOK_CODE, hook)
    print(f'# ps_game.exe SHA256 {digest}; full EnterNormalParty 00445fe0 / LeaveNormalParty 00446960 and real ChkPartyExpByLevel 00444d80. Ports: clock, locks, transport, GetPartyType, log context, party manager unregister block 00446a37..00446a7d, guild creation cleanup block 00446ac1..00446bae. Fixture stack initialized to zero, including unspecified name padding.')
    print('# GetPartyType mode:', 'real original body' if real_type else 'observed external port')
    print('# CParty::Send mode:', 'real original allocation/fan-out/connection bodies; pool/worker ports' if real_send else 'observed external port')
    print('# C operation count leader cursor old_max party_id tick target; 8 U user projections/effects; R success state_hex events_hex')
    for case in range(512):
        cpu.mem_write(base, bytes(0xfd000))
        op = case % 2
        count = (case // 2) % 8
        if op == 0 and case % 64 == 0: count = 8
        if op == 0 and case % 64 == 32: count = 0x7fffffff
        leader = (case // 16) % max(1,count)
        item_cursor = (0, 1, 3, 7, 99)[(case // 11) % 5]
        maximum = (0, 60, 65535, 0x80000000)[(case // 19) % 4]
        party_id = 5000 + case
        now = (0, 1000, 0x7fffffff, 0xffffffff)[(case // 23) % 4]
        target = 7 if op == 0 or case % 17 == 0 else (case // 7) % max(1,count)
        if real_send:
            put(buffer + 4, frame); put(agent + 0x16c, worker)
        context.update(events=b'', tick=now, effect_counts=[])
        print('C', op, count, leader, item_cursor, maximum, party_id, now, target)
        for n,v in ((8,party_id), (0xc,leader), (0x10,count), (0x108,item_cursor), (0x10c,maximum)):
            put(party+n,v)
        for i in range(7):
            put(party+0x14+i*8, 900+i); put(party+0x18+i*8, users[i])
        for i,user in enumerate(users):
            level = (i*13 + case) % 61
            job = (i+case) % 6
            stats = [(case*17 + i*19 + j*23) & 65535 for j in range(7)]
            xyz = (i+0.25, -i-0.5, case+0.75)
            name = f'user-{case:04}-{i:010}' if case % 3 else f'u{case}-{i}'
            assert len(name) <= 20
            zone = (case+i) % 3 != 0
            guild = op == 1 and (case+i) % 5 == 0
            effects_count = (0, 1, 4, 41, 48)[(case+i) % 5] if op == 0 else 0
            context['effect_counts'].append(effects_count)
            # cursor state is required by snapshots before any external call.
            put(user+0xa94, heads[i]); put(user+0xa98, heads[i])
            put(heads[i]+4, heads[i]+64 if effects_count else heads[i])
            effects=[]
            for j in range(effects_count):
                skill=(case*3+j)&65535; skill_level=(j%9)+1; visibility=1 if j%7==0 else (2 if j%3==0 else 0)
                deadline=(now+(0,999,1000,2500,-1000,0x80000000)[j%6])&0xffffffff
                effects += [skill,skill_level,visibility,deadline]
                node=heads[i]+64*(j+1)
                put(node+4, node+64 if j+1<effects_count else heads[i])
                cpu.mem_write(node+0xc,struct.pack('<H',skill));cpu.mem_write(node+0x10,bytes([skill_level]));put(node+0x20,deadline)
                definition=base+0x80000+visibility*0x100;put(node+0x30,definition);cpu.mem_write(definition+0x97,bytes([visibility]))
            put(user+0xdc,100+i);cpu.mem_write(user+0x136,struct.pack('<H',level));cpu.mem_write(user+0x134,bytes([job]))
            for offset,value in zip((0x17c,0x1238,0x184,0x1240,0x180,0x123c,0x164),stats): cpu.mem_write(user+offset,struct.pack('<H',value))
            cpu.mem_write(user+0xd0,struct.pack('<3f',*xyz));cpu.mem_write(user+0x188,name.encode()+b'\0')
            put(user+0xe0,123 if zone else 0);put(user+0x173c,party if i<count else 0);cpu.mem_write(user+0x1744,b'\x09');put(user+0x175c,base+0x81000 if guild else 0)
            socket = (-1, 0, 5)[(case+i) % 3]
            if real_send: put(user + 0x68, socket); put(user + 0xc8, agent)
            print('U', 100+i, level, job, *stats, *xyz, name, int(zone), int(guild), *([socket] if real_send else []), effects_count, *effects)
        put(stack,stop);put(stack+4,users[target]);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ECX,party)
        cpu.emu_start(0x445fe0 if op==0 else 0x446960,stop,count=1000000)
        print('R', cpu.reg_read(UC_X86_REG_EAX)&255, state().hex(), context['events'].hex() or '-')

if __name__ == '__main__':
    main()
