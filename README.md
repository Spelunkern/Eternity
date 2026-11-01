# Eternity

Native Linux reconstruction of the Shaiya EP4.5 server in C++20, using SQL Server.
Complete means an original EP4 client can connect and play with original server behavior.
SQLite migration is deferred. Gameplay and client endpoints are not implemented yet.

`originals/` contains unmodified Windows executables and matching PDB symbols for
`ps_game`, `ps_dbAgent`, `ps_login`, `ps_session`, `ps_gameLog`, and `ps_userLog`.
All pairs have matching CodeView GUID and age. `ps_gamewar` is out of scope.

Source: the EP4.5 `PSM_Server/ShaiyaServerPatch/Server` distribution, including
its matching `ps_login.exe`. Database backups and deployment configuration are
not included. Implemented: SQL access, original definition loaders, the available
login SQL path, transport framing and login cryptography. The active MD5 login
path needs a missing procedure; see [research](research/protocol.md).

Verify original files with `sha256sum -c SHA256SUMS`.

```sh
python3 tools/database.py start --backups /path/to/ep4.5/PSM_Data
python3 tools/database.py restore
python3 tools/database.py provision
python3 tools/database.py compatibility
python3 tools/database.py prepare-logs
python3 tools/dev.py image
python3 tools/dev.py build
python3 tools/dev.py test
python3 tools/dev.py integration
python3 tools/dev.py run --check-original-loaders
```

Reverse engineering uses Ghidra: set `GHIDRA_HOME` and `JAVA_HOME`, then run
`python3 tools/reverse.py all`. Generated analysis stays in `work/`;
database exports and credentials stay in `.local/`. Both directories are ignored.
