#!/usr/bin/env python3
"""Generate the reconstruction map: every original function versus repository evidence.

The function inventory comes straight from the original PDBs (address, size and
source object). Evidence comes from git refs, so results do not depend on any
worktree state. A citation is a source claim, not proof of behavioral equivalence.
"""
import argparse
import bisect
import collections
import datetime
import fnmatch
from pathlib import Path
import re
import shutil
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SERVICES = ("ps_game", "ps_dbAgent", "ps_login", "ps_session", "ps_gameLog", "ps_userLog")
IMAGE_BASE = 0x400000
PROC_KINDS = {0x110F, 0x1110, 0x1147, 0x1148}
SOURCE_SUFFIXES = (".cpp", ".hpp", ".py")
EXE = re.compile(r"(?<![\w.-])(ps_game|ps_dbAgent|ps_login|ps_session|ps_gameLog|ps_userLog|game)(?:\.exe|\.pdb)\b")
SERVICE_WORD = re.compile(r"\b(ps_game|ps_dbAgent|ps_login|ps_session|ps_gameLog|ps_userLog)\b")
ADDRESS = re.compile(r"(?<![0-9A-Za-z_])(?:0x)?(00[4-9a-fA-F][0-9a-fA-F]{5})(?![0-9A-Za-z_])")
QUALIFIED = re.compile(r"\b([A-Za-z_]\w*)::(~?[A-Za-z_]\w*)")
IDENTIFIER = re.compile(r"\b[A-Za-z_]\w{5,}\b")
NOT_DONE = re.compile(r"\b(missing|unimplemented|not implemented|not reconstructed|unrecovered|remains?|pending|still)\b", re.I)
CONTEXT_LINES = 2
STATUS_ORDER = ("enlazado", "con_oraculo", "sin_oraculo", "parcial", "referencia", "nada")
STATUS_LABEL = {
    "enlazado": "En código enlazado al ejecutable",
    "con_oraculo": "Código con prueba u oráculo",
    "sin_oraculo": "Código sin prueba",
    "parcial": "Solo un tramo interno",
    "referencia": "Solo mencionada",
    "nada": "Sin rastro",
}


def read_msf(path):
    data = path.read_bytes()
    if not data.startswith(b"Microsoft C/C++ MSF 7.00"):
        raise ValueError(f"{path}: not an MSF 7.00 PDB")
    block, _, _, directory_size, _, map_block = struct.unpack("<6I", data[32:56])
    count = (directory_size + block - 1) // block
    blocks = struct.unpack(f"<{count}I", data[map_block * block:map_block * block + 4 * count])
    directory = b"".join(data[b * block:b * block + block] for b in blocks)[:directory_size]
    streams_count = struct.unpack("<I", directory[:4])[0]
    sizes = struct.unpack(f"<{streams_count}I", directory[4:4 + 4 * streams_count])
    offset = 4 + 4 * streams_count
    streams = []
    for size in sizes:
        if size == 0xFFFFFFFF:
            streams.append(b"")
            continue
        used = (size + block - 1) // block
        parts = struct.unpack(f"<{used}I", directory[offset:offset + 4 * used])
        offset += 4 * used
        streams.append(b"".join(data[b * block:b * block + block] for b in parts)[:size])
    return streams


def object_scope(service, module):
    lowered = module.lower().replace("\\", "/")
    if "microsoft visual studio" in lowered or "/vc7/" in lowered or "platformsdk" in lowered:
        return "crt"
    if f"/release/{service.lower()}/" in lowered:
        return "game"
    return "common"


def read_functions(service, path):
    """Return one record per linked code address: address, size, name, object, scope."""
    streams = read_msf(path)
    dbi = streams[3]
    sizes = struct.unpack("<5i", dbi[24:44])
    optional_size, ec_size = struct.unpack("<ii", dbi[48:56])
    optional = 64 + sum(sizes) + ec_size
    header_stream = struct.unpack("<H", dbi[optional + 10:optional + 12])[0]
    if optional_size < 12:
        raise ValueError(f"{path}: missing section headers")
    headers = streams[header_stream]
    sections = [struct.unpack("<I", headers[i + 12:i + 16])[0] for i in range(0, len(headers), 40)]
    functions = {}
    position, end = 64, 64 + sizes[0]
    while position < end:
        stream = struct.unpack("<H", dbi[position + 34:position + 36])[0]
        symbols = struct.unpack("<I", dbi[position + 36:position + 40])[0]
        start = position + 64
        module_end = dbi.index(b"\0", start)
        object_end = dbi.index(b"\0", module_end + 1)
        module = dbi[start:module_end].decode("latin1")
        library = dbi[module_end + 1:object_end].decode("latin1")
        position = (object_end + 4) & ~3
        if stream == 0xFFFF or stream >= len(streams):
            continue
        record_data = streams[stream]
        cursor = 4
        limit = min(symbols, len(record_data))
        while cursor + 4 <= limit:
            length, kind = struct.unpack("<HH", record_data[cursor:cursor + 4])
            if length < 2:
                break
            if kind in PROC_KINDS:
                size = struct.unpack("<I", record_data[cursor + 16:cursor + 20])[0]
                offset, segment = struct.unpack("<IH", record_data[cursor + 32:cursor + 38])
                name_end = record_data.index(b"\0", cursor + 39)
                name = record_data[cursor + 39:name_end].decode("latin1")
                if 0 < segment <= len(sections):
                    address = IMAGE_BASE + sections[segment - 1] + offset
                    source = library if library.lower().endswith(".lib") else module
                    known = functions.get(address)
                    if not known or object_scope(service, source) == "game":
                        functions[address] = {"address": address, "size": size, "name": name,
                                              "object": source.replace("\\", "/").rsplit("/", 1)[-1],
                                              "scope": "crt" if name.startswith(("std::", "ATL::", "stdext::")) else object_scope(service, source)}
            cursor += length + 2
    return sorted(functions.values(), key=lambda item: item["address"])


def plain_name(name):
    previous = None
    while previous != name:
        previous = name
        name = re.sub(r"<[^<>]*>", "", name)
    return name.strip()


def git(*arguments, data=None):
    return subprocess.run(["git", "-C", str(ROOT), *arguments], input=data, check=True, capture_output=True).stdout


def read_tree(ref):
    """Read relevant text blobs of a ref. Oracle tables contribute their headers only."""
    listing = git("ls-tree", "-r", "-z", ref).decode("utf8", "replace").split("\0")
    wanted = {}
    for line in listing:
        if not line:
            continue
        info, path = line.split("\t", 1)
        kept = (path.startswith(("src/", "include/", "tools/", "tests/")) and path.endswith(SOURCE_SUFFIXES)) or \
               (path.startswith("tests/data/") and path.endswith((".tsv", ".txt"))) or \
               (path.startswith(("research/", "reports/")) and path.endswith(".md"))
        if kept and path != "tools/coverage.py" and not path.startswith("research/coverage/"):
            wanted[path] = info.split()[2]
    process = subprocess.Popen(["git", "-C", str(ROOT), "cat-file", "--batch"],
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    files = {}
    for path, blob in wanted.items():
        process.stdin.write(blob.encode() + b"\n")
        process.stdin.flush()
        size = int(process.stdout.readline().split()[2])
        content = process.stdout.read(size)
        process.stdout.read(1)
        text = content.decode("utf8", "replace")
        if path.startswith("tests/data/"):
            text = "\n".join(text.split("\n", 6)[:6])
        files[path] = text
    process.stdin.close()
    process.wait()
    return files


def wired_files(files):
    """Upper bound of what eternity-server can reach, through its include closure."""
    pending, seen = ["src/main.cpp"], set()
    include = re.compile(r'#include\s+"eternity/([^"]+)"')
    while pending:
        path = pending.pop()
        if path in seen or path not in files:
            continue
        seen.add(path)
        for relative in include.findall(files[path]):
            pending.append(f"include/eternity/{relative}")
            pending.append("src/" + relative.rsplit(".", 1)[0] + ".cpp")
    return seen


def file_kind(path):
    if path.startswith(("src/", "include/")):
        return "code"
    if path.startswith(("tests/", "tools/")):
        return "test"
    return "docs"


class Evidence:
    """Per-service citations by kind: entry addresses, addresses and names."""

    def __init__(self):
        self.addresses = collections.defaultdict(lambda: collections.defaultdict(dict))
        self.names = collections.defaultdict(lambda: collections.defaultdict(dict))

    def add(self, files, wired):
        for path, text in files.items():
            kind = file_kind(path)
            context = [None] * CONTEXT_LINES
            default = "ps_game"
            for line in text.split("\n"):
                mentions = EXE.findall(line) or SERVICE_WORD.findall(line)
                active = mentions[-1] if mentions else next((c for c in reversed(context) if c), default)
                context = (context + [mentions[-1] if mentions else None])[-CONTEXT_LINES:]
                if active == "game":
                    continue
                effective = "mention" if NOT_DONE.search(line) else kind
                if effective == "docs":
                    continue
                for value in ADDRESS.findall(line):
                    self._store(self.addresses, active, int(value, 16), effective, path, path in wired)
                for owner, member in QUALIFIED.findall(line):
                    self._store(self.names, active, f"{owner}::{member}", effective, path, path in wired)
                if "//" in line or mentions or ADDRESS.search(line) or kind == "test":
                    for token in IDENTIFIER.findall(line):
                        self._store(self.names, active, token, effective, path, path in wired)

    @staticmethod
    def _store(table, service, key, kind, path, wired):
        entry = table[service][key]
        entry.setdefault(kind, path)
        if wired and kind == "code":
            entry["wired"] = path


def classify(functions, evidence, service):
    """Attach each function's strongest status; interior addresses count as partial."""
    starts = sorted(item["address"] for item in functions)
    by_start = {item["address"]: item for item in functions}
    interior = collections.defaultdict(dict)
    for value, entry in evidence.addresses[service].items():
        index = bisect.bisect_right(starts, value) - 1
        if index < 0:
            continue
        item = by_start[starts[index]]
        if value == item["address"] or value >= item["address"] + max(item["size"], 1):
            continue
        for kind, path in entry.items():
            interior[item["address"]].setdefault(kind, path)
    results = []
    for item in functions:
        direct = {}
        for source in (evidence.addresses[service].get(item["address"], {}),
                       evidence.names[service].get(plain_name(item["name"]), {})):
            for kind, path in source.items():
                direct.setdefault(kind, path)
        inner = interior.get(item["address"], {})
        code, tested = direct.get("code"), direct.get("test") or inner.get("test")
        if direct.get("wired"):
            status, witness = "enlazado", direct["wired"]
        elif code and tested:
            status, witness = "con_oraculo", code
        elif code:
            status, witness = "sin_oraculo", code
        elif inner.get("code"):
            status, witness = "parcial", inner["code"]
        elif tested or direct.get("mention") or inner.get("mention"):
            status, witness = "referencia", tested or direct.get("mention") or inner.get("mention")
        else:
            status, witness = "nada", ""
        results.append({**item, "status": status, "witness": witness})
    return results


def tally(items):
    counts = collections.Counter()
    weights = collections.Counter()
    for item in items:
        counts[item["status"]] += 1
        weights[item["status"]] += item["size"]
    return counts, weights


def covered(items):
    """Functions with code evidence, whole or partial."""
    return [item for item in items if item["status"] in ("enlazado", "con_oraculo", "sin_oraculo", "parcial")]


def bar(fraction, width=12):
    filled = round(fraction * width)
    return "█" * filled + "░" * (width - filled)


def percent(part, whole):
    return f"{100 * part / whole:.1f} %" if whole else "—"


def size_text(value):
    return f"{value / 1024:,.0f} KB".replace(",", ".")


def report(maps, refs, canonical, today):
    game = [item for item in maps["ps_game"] if item["scope"] == "game"]
    lines = [
        "# Progreso de Eternity", "",
        f"Generado el {today} con `python3 tools/coverage.py` sobre `{canonical}`. No editar a mano.", "",
        "**El servidor no es jugable todavía.** Estas cifras dicen de qué funciones originales hay código "
        "que las cita, no que el comportamiento sea idéntico. El objetivo se cumple recién cuando un cliente "
        "EP4 original conecta y juega.", "",
        "## Cómo leer los estados", "",
        "| Estado | Significa |", "|---|---|",
        "| En código enlazado al ejecutable | Citada en archivos que `src/main.cpp` incluye. No implica que se ejecute durante el juego: hoy seleccionar un personaje todavía no entra al mundo |",
        "| Código con prueba u oráculo | Citada en `src`/`include` y en pruebas o herramientas |",
        "| Código sin prueba | Citada solo en `src`/`include` |",
        "| Solo un tramo interno | El código cita direcciones dentro de la función, no la función entera |",
        "| Solo mencionada | Aparece en pruebas, herramientas o como pendiente, sin código |",
        "| Sin rastro | No hay ninguna cita en código, pruebas ni herramientas |", "",
        "## Binarios originales", "",
        "Solo código propio del proyecto: sin CRT ni bibliotecas comunes (`pscommon`, `snet`). Los bytes de una función "
        "citada cuentan enteros aunque esté incompleta, así que el porcentaje es un techo.", "",
        "| Binario | Funciones | Con código | % funciones | Bytes | Con código | % bytes | Avance por bytes |",
        "|---|---:|---:|---:|---:|---:|---:|---|",
    ]
    for service in SERVICES:
        items = [item for item in maps[service] if item["scope"] == "game"]
        done = covered(items)
        total, part = sum(i["size"] for i in items), sum(i["size"] for i in done)
        fraction = part / total if total else 0
        lines.append(f"| `{service}` | {len(items)} | {len(done)} | {percent(len(done), len(items))} | "
                     f"{size_text(total)} | {size_text(part)} | {percent(part, total)} | `{bar(fraction)}` |")
    counts, weights = tally(game)
    total = sum(weights.values())
    lines += ["", "## `ps_game` por estado", "", "| Estado | Funciones | Bytes | % bytes |", "|---|---:|---:|---:|"]
    for status in STATUS_ORDER:
        lines.append(f"| {STATUS_LABEL[status]} | {counts[status]} | {size_text(weights[status])} | "
                     f"{percent(weights[status], total)} |")
    lines += ["", "## `ps_game` por rama", "",
              "Una función cuenta si la rama tiene código que la cite, entera o en parte.", "",
              "| Rama | Funciones con código | % bytes |", "|---|---:|---:|"]
    for ref, items in refs:
        done = covered([i for i in items if i["scope"] == "game"])
        lines.append(f"| `{ref}` | {len(done)} | {percent(sum(i['size'] for i in done), total)} |")
    lines += ["", "## `ps_game` por archivo original", "",
              "| Archivo | Funciones | Con código | Bytes | % bytes | Avance |", "|---|---:|---:|---:|---:|---|"]
    groups = collections.defaultdict(list)
    for item in game:
        groups[item["object"]].append(item)
    for name, items in sorted(groups.items(), key=lambda pair: -sum(i["size"] for i in pair[1])):
        done = covered(items)
        whole, part = sum(i["size"] for i in items), sum(i["size"] for i in done)
        lines.append(f"| `{name}` | {len(items)} | {len(done)} | {size_text(whole)} | {percent(part, whole)} | "
                     f"`{bar(part / whole if whole else 0)}` |")
    handlers = [item for item in game if re.fullmatch(r"CUser::Packet\w*", plain_name(item["name"]))]
    lines += ["", "## Handlers de paquetes de `CUser`", "", "| Handler | Dirección | Tamaño | Estado |", "|---|---|---:|---|"]
    for item in sorted(handlers, key=lambda i: (STATUS_ORDER.index(i["status"]), -i["size"])):
        lines.append(f"| `{plain_name(item['name'])}` | `{item['address']:08x}` | {item['size']} | {STATUS_LABEL[item['status']]} |")
    pending = sorted((i for i in game if i["status"] in ("nada", "referencia")), key=lambda i: -i["size"])[:40]
    lines += ["", "## Funciones pendientes más grandes de `ps_game`", "", "| Función | Dirección | Tamaño | Archivo |", "|---|---|---:|---|"]
    for item in pending:
        lines.append(f"| `{plain_name(item['name'])}` | `{item['address']:08x}` | {item['size']} | `{item['object']}` |")
    lines += ["", "El detalle función por función está en `research/coverage/<binario>.tsv`.", ""]
    return "\n".join(lines)


def run():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ref", default="HEAD", help="ref to report (default: HEAD)")
    parser.add_argument("--compare", nargs="*", default=["main", "coordinator/foundation", "overnight/*"],
                        help="refs or patterns compared in the per-branch table")
    parser.add_argument("--copy", type=Path, help="also copy the report to this path")
    args = parser.parse_args()
    inventory = {service: read_functions(service, ROOT / "originals" / service / f"{service}.pdb") for service in SERVICES}
    branches = git("for-each-ref", "--format=%(refname:short)", "refs/heads").decode().split()
    compared = []
    for pattern in args.compare:
        compared += [b for b in sorted(branches) if fnmatch.fnmatchcase(b, pattern) and b not in compared]
    canonical_files = read_tree(args.ref)
    evidence = Evidence()
    evidence.add(canonical_files, wired_files(canonical_files))
    maps = {service: classify(functions, evidence, service) for service, functions in inventory.items()}
    rows = []
    union = Evidence()
    for ref in compared:
        files = read_tree(ref)
        single = Evidence()
        single.add(files, wired_files(files))
        union.add(files, set())
        rows.append((ref, classify(inventory["ps_game"], single, "ps_game")))
    if compared:
        rows.append(("todas las ramas comparadas", classify(inventory["ps_game"], union, "ps_game")))
    output = ROOT / "research" / "coverage"
    output.mkdir(parents=True, exist_ok=True)
    for service, items in maps.items():
        with (output / f"{service}.tsv").open("w", encoding="utf8", newline="\n") as stream:
            stream.write("address\tsize\tname\tobject\tscope\tstatus\twitness\n")
            for item in items:
                stream.write(f"{item['address']:08x}\t{item['size']}\t{item['name']}\t{item['object']}\t"
                             f"{item['scope']}\t{item['status']}\t{item['witness']}\n")
    label = git("rev-parse", "--abbrev-ref", args.ref).decode().strip()
    text = report(maps, rows, label if label != "HEAD" else args.ref, datetime.date.today().isoformat())
    (ROOT / "PROGRESO.md").write_text(text, encoding="utf8")
    if args.copy:
        shutil.copyfile(ROOT / "PROGRESO.md", args.copy)


if __name__ == "__main__":
    run()
