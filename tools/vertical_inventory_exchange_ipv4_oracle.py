#!/usr/bin/env python3
"""Execute the original SInternetAddress::GetIP on sockaddr_in byte layouts."""
import random
import struct
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,address,stop=0x20000000,0x30000000,0x10000000
    for base in (stack,address,stop):cpu.mem_map(base,0x10000)
    rng=random.Random(0x404e10)
    print(f'# ps_game.exe SHA256 {digest}; SInternetAddress::GetIP 00404e10, SConnection::m_Address +006c, IPv4 bytes +0070')
    print('# ipv4_octets get_ip_dword')
    for case in range(256):
        octets=bytes(rng.randrange(256) for _ in range(4));cpu.mem_write(address,b'\x02\x00\x13\x88'+octets+bytes(8));cpu.mem_write(stack+0x8000,struct.pack('<I',stop));cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ECX,address);cpu.emu_start(0x404e10,stop,count=20)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original GetIP did not return')
        print(octets.hex(),cpu.reg_read(UC_X86_REG_EAX),sep='\t')


if __name__=='__main__':main()
