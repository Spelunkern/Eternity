#!/usr/bin/env python3
"""Execute original priority-worker gate with explicit queue/IO test ports."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EIP, UC_X86_REG_ESP
from original_oracle import load_game


def run(queue_nonempty, pending, limit, socket, available, close_before_sendpost=False):
    cpu, digest = load_game()
    stack, stop, connection, config, queue, batch, worker = (0x20000000, 0x21000000, 0x22000000,
        0x23000000, 0x24000000, 0x25000000, 0x26000000)
    for base in (stack, stop, connection, config, queue, batch, worker):
        cpu.mem_map(base, 0x10000)

    def put(address, *values):
        cpu.mem_write(address, struct.pack('<' + 'I' * len(values), *(x & 0xffffffff for x in values)))

    def get(address):
        return struct.unpack('<I', cpu.mem_read(address, 4))[0]

    put(connection + 0x44, pending)
    put(connection + 0x68, socket)
    put(connection + 0xc8, config)
    put(config + 0x14, limit)
    put(queue, int(queue_nonempty))
    put(batch + 0x24, 0)  # Skip the optional original encryption loop.
    for pointer in (0x5210cc, 0x5210d0):
        put(pointer, stop + 0x100)
    put(0x5210f8, stop + 0x200)
    cpu.mem_write(stop + 0x100, b'\xc2\x04\x00')
    cpu.mem_write(stop + 0x200, b'\xc2\x04\x00')
    cpu.mem_write(0x4e04c0, b'\xc3')  # Queue extraction port, retains test batch.
    cpu.mem_write(0x4dc080, b'\xc2\x08\x00')  # Async send port; no real IO claimed.
    state = {'fills': 0, 'posts': 0, 'available': available}

    def hook(machine, address, size, data):
        if address == 0x4e04c0:
            state['fills'] += 1
            if state['available']:
                state['available'] -= 1
                machine.reg_write(UC_X86_REG_EAX, batch)
            else:
                machine.reg_write(UC_X86_REG_EAX, 0)
        elif address == stop + 0x200:
            target = get(machine.reg_read(UC_X86_REG_ESP) + 4)
            value = (get(target) + 1) & 0xffffffff
            put(target, value)
            machine.reg_write(UC_X86_REG_EAX, value)
        elif address == 0x4dc080:
            state['posts'] += 1
            if close_before_sendpost:
                put(connection + 0x68, 0xffffffff)
            # The original SendPost closed-socket path decrements pending.
            if get(connection + 0x68) == 0xffffffff:
                put(connection + 0x44, get(connection + 0x44) - 1)

    cpu.hook_add(UC_HOOK_CODE, hook)
    put(stack + 0x8000, stop, connection, queue)
    cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000)
    cpu.reg_write(UC_X86_REG_ECX, worker)
    cpu.emu_start(0x4dc2c0, stop, count=100000)
    assert cpu.reg_read(UC_X86_REG_EIP) == stop
    return digest, (state['fills'], state['posts'], get(connection + 0x44))


cases = [
    ((False, 0, 2, 5, 4, False), (0, 0, 0)),
    ((True, 0, 2, 5, 4, False), (2, 2, 2)),
    ((True, 2, 2, 5, 4, False), (0, 0, 2)),
    ((True, 0, -1, 5, 4, False), (0, 0, 0)),
    ((True, 0, 2, 5, 0, False), (1, 0, 0)),
    ((True, 0, 2, -1, 4, False), (1, 1, 0)),
    ((True, 0, 2, 5, 4, True), (1, 1, 0)),
]
for args, expected in cases:
    digest, result = run(*args)
    if result != expected:
        raise AssertionError((args, expected, result))


def run_connection_entry(socket):
    cpu, digest = load_game()
    stack, stop, connection, manager, queue = (0x20000000, 0x21000000, 0x22000000, 0x23000000, 0x24000000)
    for base in (stack, stop, connection, manager, queue):
        cpu.mem_map(base, 0x10000)
    cpu.mem_write(connection + 0x68, struct.pack('<I', socket & 0xffffffff))
    cpu.mem_write(connection + 0xc8, struct.pack('<I', manager))
    cpu.mem_write(0x4d8960, b'\xc2\x08\x00')  # Worker-dispatch test port.
    calls = []
    cpu.hook_add(UC_HOOK_CODE, lambda machine, address, size, data: calls.append(address) if address == 0x4d8960 else None)
    cpu.mem_write(stack + 0x8000, struct.pack('<II', stop, queue))
    cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000)
    cpu.reg_write(UC_X86_REG_ECX, connection)
    cpu.emu_start(0x4d4470, stop, count=10000)
    assert cpu.reg_read(UC_X86_REG_EIP) == stop
    return digest, len(calls)


for socket, expected in ((5, 1), (-1, 0)):
    digest, count = run_connection_entry(socket)
    if count != expected:
        raise AssertionError((socket, expected, count))
print('ps_game.exe', digest, 'SWorker::SendPostQueuePriority 004dc2c0:', len(cases),
      'worker gate cases and SConnection 004d4470: 2 dispatch cases passed')
