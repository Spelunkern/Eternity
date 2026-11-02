#!/usr/bin/env python3
"""Build and exercise the native Linux runtime in an isolated development container."""
import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
IMAGE = "eternity-dev:bookworm"


def main():
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
    command += [IMAGE]
    if args.action == "build":
        command += ["sh", "-c", "cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j4"]
    elif args.action == "test":
        command += ["ctest", "--test-dir", "build", "--output-on-failure"]
    elif args.action == "integration":
        subprocess.run(command + ["sh", "-c", "build/storage-integration && build/login-sql-integration && build/world-sql-integration"], cwd=ROOT, check=True)
        # Explicit fixture IDs require IDENTITY_INSERT. Keep those privileges out of the runtime login.
        admin_command = command[:-1] + ["--env-file", str(ROOT / ".local" / "sqlserver.env"), IMAGE,
            "sh", "-c", 'ETERNITY_DB_USER=sa ETERNITY_DB_PASSWORD="$MSSQL_SA_PASSWORD" exec build/characters-sql-integration']
        subprocess.run(admin_command, cwd=ROOT, check=True)
        from creation_integration import run
        run(command)
        return
    else:
        command += ["build/eternity-server", *args.arguments]
    subprocess.run(command, cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
