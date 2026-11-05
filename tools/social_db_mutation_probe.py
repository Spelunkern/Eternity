#!/usr/bin/env python3
"""Check ps_dbAgent PacketUserFriend slot transitions against original instructions."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP
from original_oracle import load_original

cpu, digest = load_original('ps_dbAgent')
stack, user, packet, stop = 0x20000000, 0x30000000, 0x40000000, 0x10000000
for address in (stack, user, packet, stop):
    cpu.mem_map(address, 0x10000)
def put(address, value):
    cpu.mem_write(address, struct.pack('<I', value))
def get(address):
    return struct.unpack('<I', cpu.mem_read(address, 4))[0]
calls = []
for address in (0x4053f0, 0x405450):
    cpu.mem_write(address, b'\xc3')
    def capture(machine, at, size, context):
        slot = machine.reg_read(UC_X86_REG_EAX)
        calls.append((at, get(slot), get(slot + 4)))
    cpu.hook_add(UC_HOOK_CODE, capture, begin=address, end=address)
def run(op, ident, tail=b''):
    cpu.mem_write(packet, struct.pack('<HII', op, 42, ident) + tail)
    sp = stack + 0x8000
    put(sp, stop)
    cpu.reg_write(UC_X86_REG_ESP, sp)
    cpu.reg_write(UC_X86_REG_EAX, packet)
    cpu.reg_write(UC_X86_REG_ECX, user)
    cpu.emu_start(0x406930, stop, count=100000)
def cstr(address, size):
    return bytes(cpu.mem_read(address, size)).split(b'\0', 1)[0]

cases = 0
for friend in (True, False):
    base, stride = (user + 0x4060, 84) if friend else (user + 0x6130, 80)
    add, delete, memo = (0xc02, 0xc03, 0xc04) if friend else (0xc06, 0xc07, 0xc08)
    for marker in (0, 1, 2):
        cpu.mem_write(user, bytes(0x10000)); calls.clear()
        put(base, marker); put(base + 4, 10)
        run(memo, 10, b"x'y\0")
        assert get(base) == (2 if marker == 0 else marker)
        assert cstr(base + (30 if friend else 29), 51) == b'x y'
        run(delete, 10)
        assert get(base + 4) == 0 and len(calls) == 1
        assert calls[0] == ((0x4053f0 if friend else 0x405450), 2 if marker == 0 else marker, 10)
        cases += 1
    cpu.mem_write(user, bytes(0x10000)); calls.clear()
    put(base + 4, 99)
    run(add, 10, (bytes([7, 8, 3]) if friend else b'') + b'Name\0')
    second = base + stride
    assert get(second) == 1 and get(second + 4) == 10
    assert cstr(second + (9 if friend else 8), 21) == b'Name'
    if friend:
        assert bytes(cpu.mem_read(second + 0x51, 2)) == bytes([7, 8]) and bytes(cpu.mem_read(second + 8, 1)) == b'\x03'
    for i in range(100): put(base + i * stride + 4, i + 1)
    run(add, 200, (bytes([7, 8, 3]) if friend else b'') + b'Name\0')
    assert all(get(base + i * stride + 4) == i + 1 for i in range(100))
    cases += 2
print(cases, 'original social mutation cases passed; deletion helper intercepted; ps_dbAgent.exe SHA256', digest)
