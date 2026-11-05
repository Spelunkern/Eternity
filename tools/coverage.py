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
STATUS_ORDER = ("linked", "oracle", "untested", "partial", "mentioned", "none")
STATUS_LABEL = {
    "linked": "In code linked into the executable",
    "oracle": "Code with a test or oracle",
    "untested": "Code without a test",
    "partial": "Internal range only",
    "mentioned": "Mentioned only",
    "none": "No trace",
}
README_START, README_END = "<!-- progress:start -->", "<!-- progress:end -->"


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
            status, witness = "linked", direct["wired"]
        elif code and tested:
            status, witness = "oracle", code
        elif code:
            status, witness = "untested", code
        elif inner.get("code"):
            status, witness = "partial", inner["code"]
        elif tested or direct.get("mention") or inner.get("mention"):
            status, witness = "mentioned", tested or direct.get("mention") or inner.get("mention")
        else:
            status, witness = "none", ""
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
    return [item for item in items if item["status"] in ("linked", "oracle", "untested", "partial")]


def bar(fraction, width=12):
    filled = round(fraction * width)
    return "█" * filled + "░" * (width - filled)


def percent(part, whole):
    return f"{100 * part / whole:.1f}%" if whole else "—"


def size_text(value):
    return f"{value / 1024:,.0f} KB"


def binary_table(maps):
    lines = ["| Binary | Functions | With code | % functions | Bytes | With code | % bytes | Progress by bytes |",
             "|---|---:|---:|---:|---:|---:|---:|---|"]
    for service in SERVICES:
        items = [item for item in maps[service] if item["scope"] == "game"]
        done = covered(items)
        total, part = sum(i["size"] for i in items), sum(i["size"] for i in done)
        fraction = part / total if total else 0
        lines.append(f"| `{service}` | {len(items)} | {len(done)} | {percent(len(done), len(items))} | "
                     f"{size_text(total)} | {size_text(part)} | {percent(part, total)} | `{bar(fraction)}` |")
    return lines


NOT_PLAYABLE = ("**The server is not playable yet.** These figures show which original functions are cited by code, "
                "not that behavior is identical, and a function's bytes count in full even when incomplete, so the "
                "percentages are ceilings. The goal is met only when an original EP4 client connects and plays.")
SCOPE = "Project code only: excludes the CRT and shared libraries (`pscommon`, `snet`)."


def report(maps, refs, canonical, today):
    game = [item for item in maps["ps_game"] if item["scope"] == "game"]
    lines = [
        "# Eternity progress", "",
        f"Generated on {today} by `python3 tools/coverage.py` from `{canonical}`. Do not edit by hand.", "",
        NOT_PLAYABLE, "",
        "## Reading the statuses", "",
        "| Status | Meaning |", "|---|---|",
        "| In code linked into the executable | Cited by files `src/main.cpp` includes. This does not mean it runs during play: selecting a character does not enter the world yet |",
        "| Code with a test or oracle | Cited in `src`/`include` and in tests or tools |",
        "| Code without a test | Cited only in `src`/`include` |",
        "| Internal range only | Code cites addresses inside the function, not the function itself |",
        "| Mentioned only | Appears in tests, tools or as pending, with no code |",
        "| No trace | No citation in code, tests or tools |", "",
        "## Original binaries", "", SCOPE, "", *binary_table(maps),
    ]
    counts, weights = tally(game)
    total = sum(weights.values())
    lines += ["", "## `ps_game` by status", "", "| Status | Functions | Bytes | % bytes |", "|---|---:|---:|---:|"]
    for status in STATUS_ORDER:
        lines.append(f"| {STATUS_LABEL[status]} | {counts[status]} | {size_text(weights[status])} | "
                     f"{percent(weights[status], total)} |")
    lines += ["", "## `ps_game` by branch", "",
              "A function counts when the branch has code citing it, whole or in part.", "",
              "| Branch | Functions with code | % bytes |", "|---|---:|---:|"]
    for ref, items in refs:
        done = covered([i for i in items if i["scope"] == "game"])
        lines.append(f"| `{ref}` | {len(done)} | {percent(sum(i['size'] for i in done), total)} |")
    lines += ["", "## `ps_game` by original source file", "",
              "| File | Functions | With code | Bytes | % bytes | Progress |", "|---|---:|---:|---:|---:|---|"]
    groups = collections.defaultdict(list)
    for item in game:
        groups[item["object"]].append(item)
    for name, items in sorted(groups.items(), key=lambda pair: -sum(i["size"] for i in pair[1])):
        done = covered(items)
        whole, part = sum(i["size"] for i in items), sum(i["size"] for i in done)
        lines.append(f"| `{name}` | {len(items)} | {len(done)} | {size_text(whole)} | {percent(part, whole)} | "
                     f"`{bar(part / whole if whole else 0)}` |")
    handlers = [item for item in game if re.fullmatch(r"CUser::Packet\w*", plain_name(item["name"]))]
    lines += ["", "## `CUser` packet handlers", "", "| Handler | Address | Size | Status |", "|---|---|---:|---|"]
    for item in sorted(handlers, key=lambda i: (STATUS_ORDER.index(i["status"]), -i["size"])):
        lines.append(f"| `{plain_name(item['name'])}` | `{item['address']:08x}` | {item['size']} | {STATUS_LABEL[item['status']]} |")
    pending = sorted((i for i in game if i["status"] in ("none", "mentioned")), key=lambda i: -i["size"])[:40]
    lines += ["", "## Largest pending `ps_game` functions", "", "| Function | Address | Size | File |", "|---|---|---:|---|"]
    for item in pending:
        lines.append(f"| `{plain_name(item['name'])}` | `{item['address']:08x}` | {item['size']} | `{item['object']}` |")
    lines += ["", "Function-by-function detail is in `research/coverage/<binary>.tsv`.", ""]
    return "\n".join(lines)


def update_readme(maps, canonical, today):
    """Keep a generated summary at the very top of the README."""
    path = ROOT / "README.md"
    text = path.read_text(encoding="utf8")
    if README_START not in text or README_END not in text:
        raise ValueError("README.md lacks its progress markers")
    summary = [README_START, "## Progress", "", NOT_PLAYABLE, "", SCOPE, "", *binary_table(maps), "",
               f"Full map: [PROGRESS.md](PROGRESS.md). Generated on {today} from `{canonical}`.", README_END]
    head, tail = text.split(README_START, 1)[0], text.split(README_END, 1)[1]
    path.write_text(head + "\n".join(summary) + tail, encoding="utf8")


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
        rows.append(("all compared branches", classify(inventory["ps_game"], union, "ps_game")))
    output = ROOT / "research" / "coverage"
    output.mkdir(parents=True, exist_ok=True)
    for service, items in maps.items():
        with (output / f"{service}.tsv").open("w", encoding="utf8", newline="\n") as stream:
            stream.write("address\tsize\tname\tobject\tscope\tstatus\twitness\n")
            for item in items:
                stream.write(f"{item['address']:08x}\t{item['size']}\t{item['name']}\t{item['object']}\t"
                             f"{item['scope']}\t{item['status']}\t{item['witness']}\n")
    label = git("rev-parse", "--abbrev-ref", args.ref).decode().strip()
    name = label if label != "HEAD" else args.ref
    today = datetime.date.today().isoformat()
    (ROOT / "PROGRESS.md").write_text(report(maps, rows, name, today), encoding="utf8")
    update_readme(maps, name, today)
    if args.copy:
        shutil.copyfile(ROOT / "PROGRESS.md", args.copy)


if __name__ == "__main__":
    run()
