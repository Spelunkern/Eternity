#!/usr/bin/env python3
"""Execute original map configuration routines with an isolated profile API. Requires Unicorn."""
import hashlib
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game, ROOT


def profile(data):
    result, section = {}, None
    for line in data.decode('latin1').splitlines():
        line = line.strip()
        if not line or line.startswith(';'):
            continue
        if line.startswith('[') and ']' in line:
            section = line[1:line.index(']')].strip().lower()
        elif section is not None and '=' in line:
            key, value = line.split('=', 1);value = value.strip()
            if len(value) >= 2 and value[0] == value[-1] and value[0] in ('"', "'"):
                value = value[1:-1]
            result.setdefault((section, key.strip().lower()), value.encode('latin1'))
    return result


def main():
    cpu, digest = load_game()
    stack, obj, stop = 0x20000000, 0x30000000, 0x10000000
    for address in (0, stack, obj, stop):
        cpu.mem_map(address, 0x10000)
    cpu.reg_write(UC_X86_REG_FPCW, 0x27f)
    cpu.mem_write(0x505316, b'\xc3')
    for address in (0x50b152, 0x50b21f):
        cpu.mem_write(address, b'\xc3') # Single-threaded CRT synchronization port.
    cpu.mem_write(0x50d0ca, b'\xb8' + struct.pack('<I', stop + 0x1000) + b'\xc3')
    cpu.mem_write(stop + 0x100, b'\xc2\x18\x00')
    cpu.mem_write(0x5210ec, struct.pack('<I', stop + 0x100))
    context = {}

    def string(address):
        result = bytearray()
        while address and len(result) < 8192:
            byte = cpu.mem_read(address, 1)[0]
            if not byte:
                break
            result.append(byte);address += 1
        return bytes(result)

    def hook(machine, address, size, _):
        if address == stop + 0x100:
            sp = machine.reg_read(UC_X86_REG_ESP)
            section, key, default, target, capacity, path = struct.unpack('<6I', machine.mem_read(sp + 4, 24))
            value = context['profile'].get((string(section).decode('latin1').lower(), string(key).decode('latin1').lower()), string(default))[:capacity - 1]
            machine.mem_write(target, value + b'\0');machine.reg_write(UC_X86_REG_EAX, len(value))

    cpu.hook_add(UC_HOOK_CODE, hook)

    def call(address, arguments=(), this=0):
        cpu.mem_write(stack + 0x8000, struct.pack('<' + 'I' * (len(arguments) + 1), stop, *arguments))
        cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000);cpu.reg_write(UC_X86_REG_ECX, this)
        cpu.emu_start(address, stop, count=1000000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('Original map configuration did not return')

    def number(address):
        return struct.unpack('<I', cpu.mem_read(address, 4))[0]

    def load(data):
        context['profile'] = profile(data)
        cpu.mem_write(0x544e90, bytes(4096));cpu.mem_write(0x5471a8, bytes(20))
        call(0x4270a0)
        count = number(0x5471a8)
        if count > 1000:
            raise RuntimeError('Map configuration oracle count outside fixture range')
        records = struct.pack('<I', count)
        for family in range(4):
            size = number(0x5471ac + family * 4)
            records += struct.pack('<I', size) + bytes(cpu.mem_read(0x544e90 + family * 1024, size * 16))
        for map_id in range(count):
            cpu.mem_write(obj, bytes(0x1000));call(0x425390, this=obj);call(0x426940, (obj, map_id))
            name = string(obj + 0x84)
            records += struct.pack('<I', map_id) + bytes(cpu.mem_read(obj + 0x80, 4)) + bytes(cpu.mem_read(obj + 0x7c, 4)) + bytes(cpu.mem_read(obj + 0x1a4, 4))
            records += struct.pack('<I', len(name)) + name + bytes(cpu.mem_read(obj + 0x184, 32))
            records += bytes(cpu.mem_read(obj + 0x24, 20)) + bytes(cpu.mem_read(obj + 0x1a8, 42)) + bytes(cpu.mem_read(obj + 0x1d4, 16))
        return records

    def fingerprint(data):
        value = 0xcbf29ce484222325
        for byte in data:
            value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
        return value

    print(f'# ps_game.exe SHA256 {digest}; LoadGameMapConfig 004270a0, CMap::LoadConfig 00426940, CMap constructor 00425390 and original ParseMap* helpers. Windows profile/TLS ports isolated.')
    print('# A Map.ini SHA256 canonical_bytes FNV1a64; S INI_hex canonical_hex. Canonical fields documented in tests/map_config_unit.cpp; uninitialized unused schedule hours are zero in oracle allocation.')
    data = (ROOT / '.local/server-data/Map.ini').read_bytes();records = load(data)
    print('A', hashlib.sha256(data).hexdigest(), len(records), fingerprint(records), sep='\t')
    cases = ['[BASE]\nMapCount=1\n', '[BASE]\nMapCount=0\n',
             '[BASE]\nMapCount=1\n[DEFAULT_SET_ZONE]\nMapType=D\nMapName=ignored\nWarType=P\nCreateType=G\nWeatherState=99\nWeatherRate=-7\nWeatherDelay=-1\nWeatherNoneDelay=99999\nEnterMaxUser=99\nRebirthMapPos=-1, 1.25, 2.5, 3.75\n']
    for mode in ('N', 'S', 'F', 'P', 'G', 'R', 'H', 'x', 'p', 'P // comment'):
        cases.append('[BASE]\nMapCount=1\n[SET_ZONE_0]\nMapType=d\nWarType=p\nCreateType=' + mode + '\nMapName=custom.svmap\n')
    cases += ['[BASE]\nMapCount=43\n[SET_ZONE_42]\nCreateType=G\n',
              '[BASE]\nMapCount=1\nHUGameStartMapPos={1=1.25,2.5,3.75} [2,4,5,6]\nELGameStartMapPos=1,7\n[SET_ZONE_0]\nRebirthMapPos=9 1.25\n',
              '[BASE]\nMapCount=1\n[SET_ZONE_0]\nRebirthMapPos=\nRebirthMapPos1=1, 2, 3, 4\nRebirthMapPos2=2, 5, 6, 7\nCreateTime=(1,21,1)(2,20,2)(3,19,3)(4,18,4)(5,17,5)(6,16,6)(7,15,7)(8,14,8)\n',
              '[BASE]\nMapCount=1\n[SET_ZONE_0]\nRebirthMapPos1=1\nRebirthMapPos2=2,5\nCreateTime=(-1,65537,-2)(2)\nExpireTime=99999\nWeatherDelay=-123\nWeatherNoneDelay=2147483647\nCreateMinUser=-5\nEnterMaxUser=0\n',
              '[base]\nmapcount="1"\n[set_zone_0]\nmaptype= "D"\nWarType= "P"\nMapName=\'case.svmap\'\nRebirthMapPos=xyzabc\nWeatherState=-1\nWeatherRate=123text\nCreateTime=bad,words,here\n']
    for case in cases:
        data = case.encode('latin1');records = load(data)
        print('S', data.hex(), records.hex(), sep='\t')
    data = ('[BASE]\nMapCount=1\nHUGameStartMapPos=' + '(1,0,0,0)' * 64 + '\n[SET_ZONE_0]\nMapName=' + 'a' * 300 + '\nWeatherState=4294967297\nWeatherRate=-4294967297\n').encode('ascii')
    print('S', data.hex(), load(data).hex(), sep='\t')


if __name__ == '__main__':
    main()
