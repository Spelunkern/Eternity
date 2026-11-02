#!/usr/bin/env python3
"""Stage the supplied server Data directory locally without changing existing files."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="Original server Bin/Data directory")
    args = parser.parse_args()
    source = args.source.resolve()
    entries = sorted(source.rglob("*"))
    if any(path.is_symlink() for path in entries):
        parser.error("Server data must not contain symbolic links")
    files = [path for path in entries if path.is_file()]
    if not any(path.name.lower() == "map.ini" and path.parent == source for path in files):
        parser.error("Source must contain the original Map.ini")
    target = ROOT / ".local" / "server-data"
    manifest = []
    destinations = set()
    for path in files:
        relative = path.relative_to(source)
        if relative.name.lower() == "map.ini" and path.parent == source:
            relative = Path("Map.ini")
        if relative in destinations:
            parser.error(f"Duplicate server data destination: {relative}")
        destinations.add(relative)
        checksum = digest(path)
        destination = target / relative
        if destination.exists() and digest(destination) != checksum:
            parser.error(f"Existing local server data differs: {relative}")
        manifest.append({"path": relative.as_posix(), "sha256": checksum, "source": path})
    for entry in manifest:
        destination = target / entry["path"]
        destination.parent.mkdir(parents=True, exist_ok=True)
        if not destination.exists():
            shutil.copyfile(entry["source"], destination)
        if digest(destination) != entry["sha256"]:
            raise ValueError(f"Server data changed during staging: {entry['path']}")
        del entry["source"]
    (ROOT / ".local" / "server-data-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Staged {len(manifest)} original server data files in .local/server-data")


if __name__ == "__main__":
    main()
