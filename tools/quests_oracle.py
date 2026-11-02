#!/usr/bin/env python3
"""Execute original NPC/item-link/quest loaders and quest restoration. Requires Unicorn."""
import hashlib
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_EBP, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game, ROOT


def main():
    cpu, digest = load_game()
    stack, object_ptr, file_ptr, user, packet, heap, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x33000000, 0x40000000, 0x10000000
    for address in (0, stack, object_ptr, file_ptr, user, packet, stop):
        cpu.mem_map(address, 0x10000)
    cpu.mem_map(heap, 0x4000000)
    for address in (0x503ae3, 0x505afd, 0x510785, 0x505316):
        cpu.mem_write(address, b'\xc3')
    context = dict(data=b'', position=0, allocation=heap, start=None)

    def hook(machine, address, size, _):
        sp = machine.reg_read(UC_X86_REG_ESP)
        if address == 0x503ae3:
            amount = struct.unpack('<I', machine.mem_read(sp + 4, 4))[0]
            pointer = context['allocation']
            context['allocation'] += (max(amount, 1) + 15) & ~15
            if context['allocation'] > heap + 0x4000000:
                raise RuntimeError('Oracle allocation limit')
            machine.reg_write(UC_X86_REG_EAX, pointer)
        elif address == 0x505afd:
            machine.reg_write(UC_X86_REG_EAX, 12)
        elif address == 0x510785:
            fd, ptr, amount = struct.unpack('<3I', machine.mem_read(sp + 4, 12))
            data = context['data'][context['position']:context['position'] + amount]
            if len(data) != amount:
                raise RuntimeError('Original loader reached truncated input')
            machine.mem_write(ptr, data)
            context['position'] += amount
            machine.reg_write(UC_X86_REG_EAX, amount)
        elif address == 0x4cb00d:
            context['start'] = context['position']

    cpu.hook_add(UC_HOOK_CODE, hook)
    print(f'# ps_game.exe SHA256 {digest}; DBInitializeConstNpcQuest 004caf80, CQuestData::Load 004ca5d0, PacketUserDBSetInfo 0046d780 case 0506')
    print('# A original file SHA256,quest block offset,quest count,consumed,FNV1a64 definition records. S input_hex,count,consumed,464-byte records. R maximum,minimum,delay,tick,end_type; start,end,success,mob_count,pc_count')
    original = (ROOT / '.local/server-data/NpcQuest.SData').read_bytes()
    context.update(data=original, position=0, allocation=heap)
    cpu.mem_write(stack + 0xf000, struct.pack('<I', stop))
    cpu.reg_write(UC_X86_REG_ESP, stack + 0xf000)
    cpu.emu_start(0x4caf80, 0x4cb012, count=15000000)
    if cpu.reg_read(UC_X86_REG_EIP) != 0x4cb012 or cpu.reg_read(UC_X86_REG_EAX) & 255 != 1:
        raise RuntimeError('Original distribution definition load failed')
    count, pointer = struct.unpack('<2I', cpu.mem_read(0x3ed35c8, 8))
    records = bytes(cpu.mem_read(pointer, count * 464))
    hash_value = 0xcbf29ce484222325
    for byte in records:
        hash_value = ((hash_value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    print('A', hashlib.sha256(original).hexdigest(), context['start'], count, context['position'], hash_value, sep='\t')
    regions = [(0x102, 14), (0x110, 7), (0x118, 9), (0x124, 37), (0x14a, 14), (0x15a, 1), (0x158, 2), (0x15b, 3)]
    for j in range(3):
        offset = 0x160 + 32 * j
        regions += [(offset, 5), (offset + 8, 9), (offset + 20, 12)]
    blocks = []
    for name in (b'', b'A short name', bytes(range(1, 256))):
        record = bytes((offset * 13 + len(name)) & 255 for offset in range(464))
        block = struct.pack('<IH', 1, 77) + struct.pack('<I', len(name)) + name + struct.pack('<I', 4) + b'text'
        block += b''.join(record[offset:offset + length] for offset, length in regions)
        block += b''.join(struct.pack('<I', i) + bytes([65 + i]) * i for i in range(7))
        blocks.append(block)
    for block in blocks:
        context.update(data=block, position=0)
        cpu.mem_write(file_ptr, struct.pack('<I', 12))
        cpu.mem_write(stack + 0xf000, struct.pack('<2I', stop, file_ptr))
        cpu.reg_write(UC_X86_REG_ESP, stack + 0xf000)
        cpu.reg_write(UC_X86_REG_ESI, object_ptr)
        cpu.emu_start(0x4ca5d0, stop, count=200000)
        count, pointer = struct.unpack('<2I', cpu.mem_read(object_ptr, 8))
        if cpu.reg_read(UC_X86_REG_EIP) != stop or count != 1 or cpu.reg_read(UC_X86_REG_EAX) & 255 != 1:
            raise RuntimeError('Original synthetic quest load failed')
        print('S', block.hex(), count, context['position'], bytes(cpu.mem_read(pointer, 464)).hex(), sep='\t')
    # Execute the arithmetic and list/counter updates of case 0506 with a supplied definition/allocated node.
    for address, cleanup in ((0x40b0b0, 0), (0x46ec50, 0)):
        cpu.mem_write(address, b'\xc2' + struct.pack('<H', cleanup))
    allocated = heap + 0x3000000

    def restore_hook(machine, address, size, _):
        if address == 0x40b0b0:
            machine.reg_write(UC_X86_REG_EAX, allocated)

    cpu.hook_add(UC_HOOK_CODE, restore_hook)
    for maximum in (0, 1, 60, 0xffffffff):
        for minimum in (0, 1, 17, 0xffffffff):
            for delay in (0, 1, 65535):
                for tick in (0, 1234567, 0xffffffff):
                    for kind in (0, 2, 3):
                        cpu.mem_write(user, bytes(0x6000))
                        cpu.mem_write(allocated, bytes(64))
                        cpu.mem_write(packet, struct.pack('<HIBHH3B', 0x506, 7001, 1, 1, delay, 3, 4, 5))
                        cpu.mem_write(0x3ed35c8, struct.pack('<2I', 1, object_ptr))
                        cpu.mem_write(object_ptr + 0x124, struct.pack('<2I', minimum, maximum))
                        cpu.mem_write(object_ptr + 0x147, bytes([kind]))
                        cpu.mem_write(stack + 0x8000, bytes(0x300))
                        cpu.mem_write(stack + 0x8018, struct.pack('<I', tick))
                        cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000)
                        cpu.reg_write(UC_X86_REG_EBX, packet)
                        cpu.reg_write(UC_X86_REG_EBP, user)
                        cpu.reg_write(UC_X86_REG_ESI, tick)
                        cpu.emu_start(0x46e3b9, 0x46e551, count=10000)
                        if cpu.reg_read(UC_X86_REG_EIP) != 0x46e551:
                            raise RuntimeError('Original quest restore did not finish')
                        # Actual CQuest offsets verified in Ghidra: start10,end14,success18; counters1c.
                        start, end, success = struct.unpack('<3I', cpu.mem_read(allocated + 0x10, 12))
                        pc_count, mob_count = struct.unpack('<2I', cpu.mem_read(user + 0x57c4, 8))
                        if bytes(cpu.mem_read(allocated + 0x1c, 3)) != bytes((3, 4, 5)):
                            raise RuntimeError('Original quest counters changed')
                        print('R', maximum, minimum, delay, tick, kind, start, end, success, mob_count, pc_count, sep='\t')


if __name__ == '__main__':
    main()
