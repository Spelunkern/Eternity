#!/usr/bin/env python3
"""Compare SQL calls recovered by Ghidra with modules restored from the original backups."""
import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / ".local" / "contracts.json")
    args = parser.parse_args()
    modules = {}
    unavailable = []
    for path in sorted((ROOT / ".local" / "database").glob("*/modules.json")):
        for module in json.loads(path.read_text()):
            name = module["name"]
            modules.setdefault(name.casefold(), []).append(path.parent.name + "." + module["schema"] + "." + name)
            if module.get("definition") is None:
                unavailable.append(path.parent.name + "." + module["schema"] + "." + name)
    if not modules:
        parser.error("Run tools/database.py export first")
    executables = []
    for folder in sorted((ROOT / "originals").iterdir()):
        if not folder.is_dir():
            continue
        source = ROOT / "work" / "reverse" / (folder.name + ".exe") / "strings.tsv"
        if not source.is_file():
            continue
        calls = {}
        for line in source.read_text().splitlines()[1:]:
            address, _, value = line.partition("\t")
            for match in re.finditer(r"\busp_[A-Za-z0-9_]+", value, re.IGNORECASE):
                name = match.group()
                calls.setdefault(name.casefold(), {"name": name, "addresses": [], "matches": modules.get(name.casefold(), [])})["addresses"].append(address)
        executable = folder / (folder.name + ".exe")
        executables.append({"executable": executable.relative_to(ROOT).as_posix(),
                            "sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
                            "calls": list(calls.values())})
    report = {"source": "Ghidra defined strings and restored SQL Server sys.sql_modules",
              "note": "Presence is a static candidate; database routing and conditional use require call-site validation.",
              "unavailable_module_definitions": unavailable, "executables": executables}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    for executable in executables:
        missing = [call["name"] for call in executable["calls"] if not call["matches"]]
        print(f"{executable['executable']}: {len(executable['calls'])} SQL call candidates; unmatched: {', '.join(missing) or 'none'}")


if __name__ == "__main__":
    main()
