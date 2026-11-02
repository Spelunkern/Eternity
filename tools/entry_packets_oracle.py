#!/usr/bin/env python3
"""Execute original initial inventory/bank/skill/effect/quest/quick/speed serialization. Requires Unicorn."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    stack, user, objects, nodes, definitions, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x33000000, 0x10000000
    for address in (0, stack, user, objects, nodes, stop):
        cpu.mem_map(address, 0x10000)
    cpu.mem_map(definitions, 0x40000)
    cpu.mem_write(0x4d4d20, b'\xc2\x08\x00')
    cpu.mem_write(0x505316, b'\xc3')
    cpu.mem_write(stop + 0x100, b'\xc2\x04\x00')
    for iat in (0x5210cc, 0x5210d0):
        cpu.mem_write(iat, struct.pack('<I', stop + 0x100))
    cpu.mem_write(stop + 0x200, b'\xc3')
    cpu.mem_write(0x5210e4, struct.pack('<I', stop + 0x200))
    context = dict(tick=0, packets=[])

    def hook(machine, address, size, _):
        if address == 0x4d4d20:
            ptr, length = struct.unpack('<2I', machine.mem_read(machine.reg_read(UC_X86_REG_ESP) + 4, 8))
            packet = bytearray(machine.mem_read(ptr, length))
            opcode = struct.unpack_from('<H', packet)[0]
            # strcpy's unused tail contains prior stack contents; it has no string semantics.
            # Normalize only bytes after the first NUL in each fixed-width craft field.
            if opcode in (0x106, 0x711):
                header, width, craft = (3, 34, 13) if opcode == 0x106 else (7, 33, 12)
                for begin in range(header, length, width):
                    end = packet.index(0, begin + craft, begin + craft + 21)
                    packet[end + 1:begin + width] = bytes(begin + width - end - 1)
            context['packets'].append(packet.hex())
        elif address == stop + 0x200:
            machine.reg_write(UC_X86_REG_EAX, context['tick'])

    cpu.hook_add(UC_HOOK_CODE, hook)
    cases = []
    for maximum in (-1, 0, 2, 5):
        cases.extend((0, count, 10000, maximum) for count in (0, 1, 49, 50, 51, 100, 144))
    cases.extend((1, count, 10000, 0) for count in (0, 1, 49, 50, 51, 100, 101, 239, 240))
    for op in range(2, 7):
        counts = (0, 1, 127, 128, 129, 255) if op in (2, 4) else (0, 1, 127, 128, 129, 255, 256, 257, 300)
        for count in counts:
            for tick in (0, 10000, 0x80000000, 0xffffffff):
                cases.append((op, count, tick, 0))
    cases.extend((7, 0, 0, flags) for flags in range(128))
    print(f'# ps_game.exe SHA256 {digest}; SendCharacterItem 004832b0, Bank 00483410, Skill 00483540, SetSkill 00483890, Quick 00483800, Quest 004835f0, QuestFinish 00483730, SpeedMe 00482730')
    print('# operation count tick parameter; packet_count; hex payloads. Item bytes after craft NUL normalized to zero; generated inputs specified in tools/entry_packets_oracle.py.')
    entries = (0x4832b0, 0x483410, 0x483540, 0x483890, 0x483800, 0x4835f0, 0x483730, 0x482730)
    for op, count, tick, parameter in cases:
        context.update(tick=tick, packets=[])
        for address in (user, objects, nodes):
            cpu.mem_write(address, bytes(0x10000))
        cpu.mem_write(definitions, bytes(0x40000))
        cpu.mem_write(stack, bytes([0xa5]) * 0x10000)
        cpu.mem_write(user + 0xdc, struct.pack('<I', 9001))
        cpu.mem_write(user + 0x1304, struct.pack('<i', parameter))
        cpu.mem_write(user + 0x148, struct.pack('<I', 0xdcba9876))
        root = user + 0x8000
        if op in (0, 1):
            for i in range(count):
                item = objects + i * 0xc0
                position = i * 37 % (144 if op == 0 else 240)
                cpu.mem_write(user + (0x1c4 if op == 0 else 0x404) + 4 * position, struct.pack('<I', item))
                cpu.mem_write(item + 0x40, bytes(((i * 7 + 1) & 255, (i * 3 + 2) & 255, (i * 5 + 1) & 255)))
                cpu.mem_write(item + 0x44, struct.pack('<H', (i * 997) & 65535))
                cpu.mem_write(item + 0x46, bytes((i + j * 13) & 255 for j in range(6)))
                craft = (b'', b'1234567', b'01234567890123456789')[i % 3]
                cpu.mem_write(item + 0x4c, craft + b'\0')
        elif op == 2:
            cpu.mem_write(user + 0xac0, struct.pack('<I', count))
            for i in range(count):
                item = objects + i * 0xc0
                cpu.mem_write(user + 0xac4 + i * 4, struct.pack('<I', item))
                cpu.mem_write(item + 0xc, struct.pack('<I', 1000 + i))
                cpu.mem_write(item + 0x10, struct.pack('<I', i % 3 + 1))
                cpu.mem_write(item + 0x14, struct.pack('<I', i))
                cpu.mem_write(item + 0x1c, struct.pack('<I', (tick + (i % 9 - 4) * 1001) & 0xffffffff))
        elif op in (3, 5, 6):
            head_offset = {3: 0xa94, 5: 0x11f4, 6: 0x11c8}[op]
            cpu.mem_write(user + head_offset, struct.pack('<I', root))
            cpu.mem_write(root, struct.pack('<2I', root, nodes if count else root))
            for i in range(count):
                node, info = nodes + i * 0x40, definitions + i * 0x140
                following = nodes + (i + 1) * 0x40 if i + 1 < count else root
                cpu.mem_write(node, struct.pack('<2I', root if i == 0 else node - 0x40, following))
                if op == 3:
                    cpu.mem_write(node + 8, struct.pack('<3I', 0x12340000 + i, 1000 + i, i % 3 + 1))
                    cpu.mem_write(node + 0x20, struct.pack('<I', ((tick // 1000 if i % 2 else tick) + (i % 9 - 4) * 1001) & 0xffffffff))
                    cpu.mem_write(node + 0x30, struct.pack('<I', info))
                    cpu.mem_write(info + 0x97, bytes([i % 2]))
                elif op == 5:
                    cpu.mem_write(node + 8, struct.pack('<H', 1000 + i))
                    cpu.mem_write(node + 0x14, struct.pack('<I', (tick + (i % 9 - 4) * 60001) & 0xffffffff))
                    cpu.mem_write(node + 0x1c, bytes((i + j * 13) & 255 for j in range(3)))
                    cpu.mem_write(node + 0x20, struct.pack('<I', info))
                    cpu.mem_write(info + 0x128, struct.pack('<I', int(i % 3 != 0)))
                else:
                    cpu.mem_write(node + 8, struct.pack('<HB', 1000 + i, 0))
                    cpu.mem_write(node + 0xc, bytes([(i * 7) & 255]))
        elif op == 4:
            cpu.mem_write(user + 0xec4, struct.pack('<I', count))
            for i in range(count):
                cpu.mem_write(user + 0xec8 + i * 6, struct.pack('<4BH', i % 8, i % 24, i % 4, 0xa5, (i * 777 + 123) & 65535))
        elif op == 7:
            for i, offset in enumerate((0x133c, 0x1340, 0x1344, 0x1428, 0x1348, 0x5778)):
                cpu.mem_write(user + offset, struct.pack('<I', (parameter >> i) & 1))
            cpu.mem_write(user + 0x12ec, struct.pack('<i', parameter % 13 - 2))
            cpu.mem_write(user + 0x12f0, struct.pack('<i', parameter % 8 - 2))
        cpu.mem_write(stack + 0xf000, struct.pack('<II', stop, user))
        cpu.reg_write(UC_X86_REG_ESP, stack + 0xf000)
        for reg in (UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI):
            cpu.reg_write(reg, user)
        cpu.emu_start(entries[op], stop, count=200000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('Original entry packet routine did not return')
        print(op, count, tick, parameter, len(context['packets']), *context['packets'], sep='\t')


if __name__ == '__main__':
    main()
