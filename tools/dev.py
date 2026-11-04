#!/usr/bin/env python3
"""Build and exercise the native Linux runtime in an isolated development container."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
from parallel_lock import resource_lock

ROOT = Path(__file__).resolve().parents[1]
IMAGE = "eternity-dev:bookworm"


def run():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("image", "build", "test", "integration", "run"))
    parser.add_argument("arguments", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.action == "image":
        subprocess.run(["docker", "build", "-f", "deploy/Dockerfile.dev", "-t", IMAGE, "."], cwd=ROOT, check=True)
        return
    command = ["docker", "run", "--rm", "--mount", f"type=bind,source={ROOT},target=/src"]
    if args.action in ("run", "integration"):
        env = ROOT / ".local" / "runtime.env"
        if not env.is_file():
            parser.error("Run tools/database.py provision first")
        command += ["--network", "container:eternity-sqlserver", "--env-file", str(env)]
    if args.action == "run":
        for name in sorted(os.environ):
            if name.startswith(("ETERNITY_LOGIN_", "ETERNITY_WORLD_")) or name == "ETERNITY_MAP_INI":
                command += ["--env", name]
    command += [IMAGE]
    if args.action == "build":
        command += ["sh", "-c", "cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j4"]
    elif args.action == "test":
        command += ["ctest", "--test-dir", "build", "--output-on-failure"]
    elif args.action == "integration":
        subprocess.run(command + ["build/linked-mob-config-unit", "tests/data/linked-mob-config-oracle.tsv", ".local/server-data/Obelisk.ini"], cwd=ROOT, check=True)
        subprocess.run(command + ["build/item-pick-unit", "tests/data/item-pick-oracle.tsv", ".local/server-data/NpcQuest.SData"], cwd=ROOT, check=True)
        subprocess.run(command + ["build/pathfinding-unit", "tests/data/pathfinding-oracle.tsv", ".local/server-data/Map"], cwd=ROOT, check=True)
        subprocess.run(command + ["build/map-lines-unit", "tests/data/map-lines-oracle.tsv", ".local/server-data/Map"], cwd=ROOT, check=True)
        subprocess.run(command + ["build/zone-npcs-unit", "tests/data/zone-npcs-oracle.tsv", ".local/server-data/Map", ".local/server-data/NpcQuest.SData"], cwd=ROOT, check=True)
        subprocess.run(command + ["build/npcs-unit", "tests/data/npcs-oracle.tsv", ".local/server-data/NpcQuest.SData"], cwd=ROOT, check=True)
        subprocess.run(command + ["build/zone-environment-unit", "tests/data/zone-environment-oracle.tsv", ".local/server-data/Map"], cwd=ROOT, check=True)
        subprocess.run(command + ["build/map-config-unit", "tests/data/map-config-oracle.tsv", ".local/server-data"], cwd=ROOT, check=True)
        subprocess.run(command + ["sh", "-c", "build/maps-unit tests/data/maps-oracle.tsv tests/data/map-geometry-oracle.tsv .local/server-data/Map && build/quests-unit tests/data/quests-oracle.tsv .local/server-data/NpcQuest.SData && build/storage-integration && build/login-sql-integration && build/world-sql-integration"], cwd=ROOT, check=True)
        # Explicit fixture IDs require IDENTITY_INSERT. Keep those privileges out of the runtime login.
        admin_command = command[:-1] + ["--env-file", str(ROOT / ".local" / "sqlserver.env"), IMAGE,
            "sh", "-c", 'ETERNITY_DB_USER=sa ETERNITY_DB_PASSWORD="$MSSQL_SA_PASSWORD" exec build/characters-sql-integration']
        subprocess.run(admin_command, cwd=ROOT, check=True)
        from creation_integration import run
        run(command)
        from guild_integration import run as run_guild
        guild_command = command[:-1] + ["--env-file", str(ROOT / ".local" / "sqlserver.env"), IMAGE,
            "sh", "-c", 'ETERNITY_DB_USER=sa ETERNITY_DB_PASSWORD="$MSSQL_SA_PASSWORD" exec "$@"', "guild-integration"]
        run_guild(guild_command)
        return
    else:
        command += ["build/eternity-server", *args.arguments]
    subprocess.run(command, cwd=ROOT, check=True)


def main():
    action = sys.argv[1] if len(sys.argv) > 1 else ''
    if action == 'integration':
        with resource_lock(ROOT, 'sql'):
            run()
    elif action in ('image', 'build', 'test'):
        with resource_lock(ROOT, 'build', 2):
            run()
    else:
        run()


if __name__ == "__main__":
    main()
