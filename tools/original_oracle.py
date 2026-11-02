"""Load immutable original PE sections for isolated instruction-level regression oracles."""
import hashlib
from pathlib import Path
import struct
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32

ROOT = Path(__file__).resolve().parents[1]


def load_game():
    return load_original("ps_game")


def load_original(service):
    relative = f"originals/{service}/{service}.exe"
    data = (ROOT / relative).read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    expected = next(line.split()[0] for line in (ROOT / "SHA256SUMS").read_text().splitlines()
                    if line.split()[-1].removeprefix("*") == relative)
    if digest != expected:
        raise RuntimeError("Original executable hash mismatch")
    pe = struct.unpack_from("<I", data, 0x3c)[0]
    count, optional_size = struct.unpack_from("<H12xH", data, pe + 6)
    optional = pe + 24
    base = struct.unpack_from("<I", data, optional + 28)[0]
    size = struct.unpack_from("<I", data, optional + 56)[0]
    machine = Uc(UC_ARCH_X86, UC_MODE_32)
    machine.mem_map(base, (size + 4095) & ~4095)
    for section in range(count):
        offset = optional + optional_size + 40 * section
        virtual, raw_size, raw_offset = struct.unpack_from("<III", data, offset + 12)
        machine.mem_write(base + virtual, data[raw_offset:raw_offset + raw_size])
    return machine, digest
