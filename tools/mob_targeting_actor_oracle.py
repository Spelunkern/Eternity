#!/usr/bin/env python3
"""Original SetTarget -> real stack DispatchEvent -> original event callbacks.
Reuse the canonical worker09 instruction/container harness without copying its
owner. Supply its path when validating a branch predating that delivery.
"""
import argparse
from pathlib import Path
import struct
import random


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--fsm-oracle',type=Path,default=Path(__file__).with_name('mob_ai_states_oracle.py'))
    args=parser.parse_args()
    source=args.fsm_oracle.read_text()
    marker="print(f'# ps_game.exe SHA256"
    if marker not in source:
        raise RuntimeError('Unsupported canonical FSM oracle harness')
    h={'__name__':'mob_targeting_actor_original_harness'}
    exec(compile(source.split(marker,1)[0],str(args.fsm_oracle),'exec'),h)
    cpu,put,get=h['cpu'],h['put'],h['get'];mob=h['mob'];context=h['context']
    user=h['base']+0x18000;info=h['base']+0x10000
    print(f"# ps_game.exe SHA256 {h['digest']}; real SetTarget 0049fad0, SetTargetEx 004b65e0, DispatchEvent 004925f0/004929b0, OnDispatch 004938c0 and OnAttacked 004b6120. Ports: existing container/Lua/clock diagnostics harness.")
    print('# initial move life fixed lua id | rawstatus targetkind targetid timeout nextmove times0..2 discriminator stack-count states | OnAttacked count tick argument observed-stack-count states')
    raw_states=(-2147483648,-1,0,1,2,3,4,5,99,2147483647)
    for case in range(400):
        initial=case%5;move=(case//5)%2;life=raw_states[(case//10)%10];fixed=(case//100)%2;lua=(case//200)%2;identity=0xf1230000+case
        h['set_stack']([initial]);put(mob+0x44,1);put(mob+0x124,42,7,12345);put(mob+0x280,8765);put(mob+0x2a0,move);put(mob+0x2b8,fixed)
        put(mob+0xcbc,777);put(mob+0x1b8,111,222,333);put(mob+0x290,0);put(mob+0xa8,info);cpu.mem_write(info,bytes(268));cpu.mem_write(info+0x26,b'\1');put(mob+0x284,lua);put(user+0xdc,identity);put(user+0x142c,life)
        context.update(phase='execute',calls=[],errors=[],updates=[],tick=0xfffffff0,mutate=None)
        h['execute'](0x49fad0,[(h['UC_X86_REG_ECX'],mob)],(user,0))
        if context['errors']:raise RuntimeError('Unexpected FSM error')
        values=h['get_stack']();calls=context['calls']
        if any(call[0]!='OnAttacked' for call in calls):raise RuntimeError('Unexpected event callback')
        tick=calls[0][1] if calls else 0;argument=calls[0][2] if calls else 0
        observed=calls[0][3] if calls else ()
        print(initial,move,life,fixed,lua,identity,'|',get(mob+0x124),get(mob+0x128),get(mob+0x12c),get(mob+0x280),get(mob+0xcbc),get(mob+0x1b8),get(mob+0x1bc),get(mob+0x1c0),get(mob+0x290),len(values),*values,'|',len(calls),tick,argument,len(observed),*observed)

    # Complete original StatusAttackStop body followed by real SetTarget gates
    # on the SAME raw +142c word. No replacement life owner or HP inference.
    print('# S raw-state stopped-state accepted-before accepted-after')
    samples=list(raw_states)
    rng=random.Random(0x142c)
    samples += [struct.unpack('<i',struct.pack('<I',rng.getrandbits(32)))[0] for _ in range(128)]
    for raw in samples:
        put(user+0xdc,42);put(user+0x142c,raw);put(user+0x1274,0);put(user+0x1528,7);put(user+0x143c,9,11)
        def acquire():
            h['set_stack']([0]);put(mob+0x44,1);put(mob+0x124,42,7,0);put(mob+0x284,0);put(mob+0x2b8,0);put(mob+0x2a0,0)
            context.update(calls=[],errors=[],updates=[],mutate=None)
            h['execute'](0x49fad0,[(h['UC_X86_REG_ECX'],mob)],(user,0))
            if context['errors']:raise RuntimeError('Unexpected target FSM error')
            return int(get(mob+0x12c)==42)
        before=acquire()
        h['execute'](0x489ed0,[(h['UC_X86_REG_EAX'],user)])
        result=struct.unpack('<i',struct.pack('<I',get(user+0x142c)))[0]
        after=acquire()
        print('S',raw,result,before,after)


if __name__=='__main__':main()
