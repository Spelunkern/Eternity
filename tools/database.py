#!/usr/bin/env python3
"""Manage Eternity's isolated SQL Server and inspect the original backups."""

import argparse
import calendar
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import secrets
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
LOCAL = ROOT / ".local"
CONTAINER = "eternity-sqlserver"
IMAGE = "mcr.microsoft.com/mssql/server@sha256:4402d880dd4c34bfa7d8705e56a86cd6c88da80a1f6bbbe741f999e76264a090"
SQLCMD = "/opt/mssql-tools18/bin/sqlcmd"


def docker(*args, **kwargs):
    return subprocess.run(["docker", *args], check=True, text=True, **kwargs)


def sql(query, database="master", *, json_output=False):
    # The password is resolved inside the container, never passed on the CLI.
    command = ["exec", "-i", CONTAINER, "sh", "-c",
               'export SQLCMDPASSWORD="$MSSQL_SA_PASSWORD"; exec "$@"',
               "sqlcmd", SQLCMD, "-S", "localhost", "-U", "sa", "-C",
               "-b", "-r", "1", "-w", "65535", "-d", database]
    command += ["-y", "0"] if json_output else ["-h", "-1", "-W", "-s", "\t"]
    result = docker(*command, input="SET NOCOUNT ON;\n" + query + "\nGO\n",
                    capture_output=True)
    if json_output:
        if not result.stdout.strip():
            return []
        lines = result.stdout.splitlines()
        start = next((i for i, line in enumerate(lines) if line.lstrip().startswith(("[", "{"))), None)
        if start is None:
            raise ValueError("SQL Server returned no JSON result")
        return json.loads("".join(lines[start:]).strip())
    return result.stdout.strip()


def literal(value):
    return "N'" + str(value).replace("'", "''") + "'"


def identifier(value):
    return "[" + value.replace("]", "]]") + "]"


def databases():
    return sql("SELECT name FROM sys.databases WHERE database_id > 4 ORDER BY name FOR JSON PATH;",
               json_output=True)


def start(backups):
    LOCAL.mkdir(mode=0o700, exist_ok=True)
    staged = LOCAL / "backups"
    staged.mkdir(exist_ok=True)
    if backups:
        for source in sorted(backups.iterdir()):
            if source.is_file() and re.fullmatch(r"(?:PS|OMG)_[A-Za-z0-9_]+", source.name):
                destination = staged / source.name
                if destination.exists() and destination.read_bytes() != source.read_bytes():
                    raise ValueError(f"Refusing to replace staged backup {source.name}")
                if not destination.exists():
                    shutil.copyfile(source, destination)
    env = LOCAL / "sqlserver.env"
    if not env.exists():
        content = ("ACCEPT_EULA=Y\nMSSQL_PID=Developer\nMSSQL_SA_PASSWORD=Et!" +
                   secrets.token_urlsafe(28) + "9a\nMSSQL_MEMORY_LIMIT_MB=3072\n")
        with os.fdopen(os.open(env, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600), "w") as stream:
            stream.write(content)
    existing = subprocess.run(["docker", "inspect", CONTAINER], capture_output=True, text=True)
    if existing.returncode == 0:
        info = json.loads(existing.stdout)[0]
        if info["Config"].get("Labels", {}).get("com.eternity.project") != "Eternity":
            raise ValueError("Container name is already used by another project")
        if not info["State"]["Running"]:
            docker("start", CONTAINER, capture_output=True)
    else:
        docker("run", "-d", "--name", CONTAINER,
               "--label", "com.eternity.project=Eternity", "--env-file", str(env),
               "-p", "127.0.0.1:14345:1433",
               "--mount", "type=volume,source=eternity-sqlserver-data,target=/var/opt/mssql",
               "--mount", f"type=bind,source={staged},target=/backups,readonly", IMAGE,
               capture_output=True)
    deadline = time.monotonic() + 120
    while True:
        try:
            if sql("SELECT 1;") == "1":
                break
        except subprocess.CalledProcessError:
            pass
        if time.monotonic() >= deadline:
            raise TimeoutError("SQL Server did not become ready within 120 seconds")
        time.sleep(1)
    print("SQL Server ready on 127.0.0.1:14345")


def restore():
    # Discover the staged mount, including pre-existing Eternity containers.
    info = json.loads(docker("inspect", CONTAINER, capture_output=True).stdout)[0]
    if info["Config"].get("Labels", {}).get("com.eternity.project") != "Eternity":
        raise ValueError("Container does not belong to Eternity")
    source = Path(next(m["Source"] for m in info["Mounts"] if m["Destination"] == "/backups"))
    existing = {row["name"] for row in databases()}
    manifest_path = LOCAL / "restore-manifest.json"
    manifest = json.loads(manifest_path.read_text()) if manifest_path.exists() else []
    for backup in sorted(source.iterdir()):
        name = backup.name
        if not backup.is_file() or not re.fullmatch(r"(?:PS|OMG)_[A-Za-z0-9_]+", name):
            continue
        if name in existing:
            print(f"{name}: already exists (not overwritten)", flush=True)
            continue
        path = "/backups/" + name
        listing = sql(f"RESTORE FILELISTONLY FROM DISK = {literal(path)};")
        moves = []
        for line in listing.splitlines():
            columns = [part.strip() for part in line.split("\t")]
            if len(columns) < 3 or columns[2] not in ("D", "L"):
                continue
            suffix = "mdf" if columns[2] == "D" else "ldf"
            target = f"/var/opt/mssql/data/{name}_{len(moves)}.{suffix}"
            moves.append(f"MOVE {literal(columns[0])} TO {literal(target)}")
        if not moves:
            raise ValueError(f"No data/log files found in {name}")
        sql(f"RESTORE DATABASE {identifier(name)} FROM DISK = {literal(path)} WITH " +
            ", ".join(moves) + ";")
        sql(f"DBCC CHECKDB ({literal(name)}) WITH NO_INFOMSGS;")
        manifest.append({"database": name, "backup_sha256": hashlib.sha256(backup.read_bytes()).hexdigest()})
        print(f"{name}: restored and CHECKDB passed", flush=True)
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")


def export():
    output = LOCAL / "database"
    output.mkdir(parents=True, exist_ok=True)
    summary = []
    for database in databases():
        name = database["name"]
        if not re.fullmatch(r"(?:PS|OMG)_[A-Za-z0-9_]+", name) or re.fullmatch(r"PS_UserLog_\d{6}", name):
            continue
        folder = output / name
        folder.mkdir(exist_ok=True)
        tables = sql("""SELECT s.name AS [schema],t.name,
            (SELECT SUM(p.rows) FROM sys.partitions p WHERE p.object_id=t.object_id AND p.index_id IN (0,1)) AS rows
            FROM sys.tables t JOIN sys.schemas s ON s.schema_id=t.schema_id
            WHERE t.is_ms_shipped=0 ORDER BY s.name,t.name FOR JSON PATH;""", name, json_output=True)
        columns = sql("""SELECT s.name AS [schema],t.name AS [table],c.column_id,c.name,
            ty.name AS [type],c.max_length,c.precision,c.scale,c.is_nullable,c.is_identity,c.collation_name,
            dc.definition AS [default]
            FROM sys.tables t JOIN sys.schemas s ON s.schema_id=t.schema_id
            JOIN sys.columns c ON c.object_id=t.object_id JOIN sys.types ty ON ty.user_type_id=c.user_type_id
            LEFT JOIN sys.default_constraints dc ON dc.object_id=c.default_object_id
            WHERE t.is_ms_shipped=0 ORDER BY s.name,t.name,c.column_id FOR JSON PATH;""", name, json_output=True)
        modules = sql("""SELECT s.name AS [schema],o.name,o.type_desc,m.definition
            FROM sys.objects o JOIN sys.schemas s ON s.schema_id=o.schema_id
            LEFT JOIN sys.sql_modules m ON m.object_id=o.object_id
            WHERE o.is_ms_shipped=0 AND o.type IN ('P','V','FN','IF','TF','TR')
            ORDER BY s.name,o.name FOR JSON PATH;""", name, json_output=True)
        for filename, content in (("tables.json", tables), ("columns.json", columns), ("modules.json", modules)):
            (folder / filename).write_text(json.dumps(content, ensure_ascii=False, indent=2) + "\n")
        entry = {"database": name, "tables": len(tables), "modules": len(modules),
                 "unavailable_definitions": sum(m.get("definition") is None for m in modules)}
        summary.append(entry)
        print(json.dumps(entry), flush=True)
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")


def provision():
    """Create a project-scoped runtime login without server administration rights."""
    path = LOCAL / "runtime.env"
    if path.exists():
        print("Runtime environment already exists (not replaced)")
        return
    password = "Et!" + secrets.token_urlsafe(28) + "9a"
    if sql("SELECT COUNT(*) FROM sys.server_principals WHERE name=N'eternity';") != "0":
        raise ValueError("Runtime login exists without its environment file; not replacing credentials")
    sql("CREATE LOGIN [eternity] WITH PASSWORD=" + literal(password) + ", CHECK_POLICY=ON;")
    try:
        for database in databases():
            name = database["name"]
            if re.fullmatch(r"(?:PS|OMG)_[A-Za-z0-9_]+", name):
                sql("CREATE USER [eternity] FOR LOGIN [eternity]; "
                    "GRANT SELECT, INSERT, UPDATE, DELETE, EXECUTE ON SCHEMA::dbo TO [eternity];", name)
    except Exception:
        # Do not reset or silently replace an incompletely provisioned login.
        raise RuntimeError("Runtime provisioning failed; inspect the isolated SQL Server login") from None
    content = ("ETERNITY_DB_SERVER=127.0.0.1\nETERNITY_DB_PORT=1433\nETERNITY_DB_USER=eternity\n"
               "ETERNITY_DB_PASSWORD=" + password + "\nETERNITY_DB_DATABASE=PS_GameDefs\n")
    with os.fdopen(os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600), "w") as stream:
        stream.write(content)
    print("Provisioned runtime login and .local/runtime.env")


def compatibility():
    sql((ROOT / "sql" / "compatibility" / "master_md5.sql").read_text())
    digest = sql("SELECT dbo.fn_md5('abc');")
    if digest != "900150983cd24fb0d6963f7d28e17f72":
        raise ValueError("Existing master.dbo.fn_md5 does not match the required MD5 contract")
    sql("IF USER_ID(N'eternity') IS NULL CREATE USER [eternity] FOR LOGIN [eternity]; "
        "GRANT EXECUTE ON OBJECT::dbo.fn_md5 TO [eternity];")
    print("Login MD5 compatibility function verified")


def prepare_logs():
    # The original usp_Insert_LoginLog_E selects a monthly database and daily table.
    # Use SQL Server's clock, not the operator's local timezone.
    today = datetime.datetime.strptime(sql("SELECT CONVERT(char(8), GETDATE(), 112);"), "%Y%m%d").date()
    for offset in (0, 1):
        year = today.year + (today.month - 1 + offset) // 12
        month = (today.month - 1 + offset) % 12 + 1
        database = f"PS_UserLog_{year:04d}{month:02d}"
        if sql(f"SELECT COUNT(*) FROM sys.databases WHERE name={literal(database)};") == "0":
            sql(f"CREATE DATABASE {identifier(database)};")
        statements = []
        for day in range(1, calendar.monthrange(year, month)[1] + 1):
            table = f"UserLog{day:02d}"
            statements.append(f"""IF OBJECT_ID(N'dbo.{table}', N'U') IS NULL
                CREATE TABLE dbo.{identifier(table)} (
                    SessionID bigint NOT NULL, UserUID int NOT NULL, UserIP varchar(15) NOT NULL,
                    LogType bit NOT NULL, LogTime datetime NOT NULL, LoginType smallint NOT NULL);""")
        statements.append("IF USER_ID(N'eternity') IS NULL CREATE USER [eternity] FOR LOGIN [eternity]; "
                          "GRANT SELECT, INSERT ON SCHEMA::dbo TO [eternity];")
        sql("\n".join(statements), database)
        print(f"{database}: daily login log tables ready")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    subs = parser.add_subparsers(dest="action", required=True)
    command = subs.add_parser("start")
    command.add_argument("--backups", type=Path)
    subs.add_parser("restore")
    subs.add_parser("export")
    subs.add_parser("provision")
    subs.add_parser("compatibility")
    subs.add_parser("prepare-logs")
    command = subs.add_parser("query")
    command.add_argument("--database", default="master")
    command.add_argument("--file", type=Path, required=True)
    args = parser.parse_args()
    if args.action == "start":
        start(args.backups)
    elif args.action == "restore":
        restore()
    elif args.action == "export":
        export()
    elif args.action == "provision":
        provision()
    elif args.action == "compatibility":
        compatibility()
    elif args.action == "prepare-logs":
        prepare_logs()
    else:
        print(sql(args.file.read_text(), args.database))


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        print(error.stderr or str(error), file=sys.stderr)
        sys.exit(1)
