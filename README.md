# Eternity

Native Linux reconstruction of the Shaiya EP4.5 server in C++20, using SQL Server.
Complete means an original EP4 client can connect and play with original server behavior.
SQLite migration is deferred. Gameplay is not implemented yet.

`originals/` contains unmodified Windows executables and matching PDB symbols for
`ps_game`, `ps_dbAgent`, `ps_login`, `ps_session`, `ps_gameLog`, and `ps_userLog`.
All pairs have matching CodeView GUID and age. `ps_gamewar` is out of scope.

Source: the EP4.5 `PSM_Server/ShaiyaServerPatch/Server` distribution, including
its matching `ps_login.exe`. Database backups, configuration, credentials and
runtime assets stay local.

Implemented and independently tested: SQL contracts, encrypted login/game TCP
handshakes, account sessions, character lists/creation, actor preparation and
persistence, map/zone/entity ownership, mob corpse cleanup and respawn, linked mob lifecycle
and configuration, guild persistence and several inventory/combat primitives. See the
[reconstruction notes](research/protocol.md) for evidence and remaining work.

`--serve` starts login/game listeners with shared sessions and the persisted
calendar. Faction selection and character creation work over TCP. Character
selection still does not enter the world. Live world integration, complete AI,
combat and gameplay remain unfinished; the server is not playable. Client work
is deferred. The active MD5 login path requires a missing original procedure;
the available legacy SQL path is tested.

Verify original files with `sha256sum -c SHA256SUMS`.

```sh
python3 tools/database.py start --backups /path/to/ep4.5/PSM_Data
python3 tools/database.py restore
python3 tools/database.py provision
python3 tools/database.py compatibility
python3 tools/database.py prepare-logs
python3 tools/data.py /path/to/ep4.5/PSM_Client/Bin/Data
python3 tools/dev.py image
python3 tools/dev.py build
python3 tools/dev.py test
python3 tools/dev.py integration
python3 tools/dev.py run --check-original-loaders
python3 tools/dev.py run --serve-login
python3 tools/dev.py run --serve
```

Reverse engineering uses Ghidra: set `GHIDRA_HOME` and `JAVA_HOME`, then run
`python3 tools/reverse.py all`. Generated analysis stays in `work/`;
database exports and credentials stay in `.local/`. Both directories are ignored.
`--serve-login` advertises its world offline; `--serve` makes character creation
available through the legacy SQL login path. Ports default to 30800/30810;
`ETERNITY_LOGIN_PORT` and `ETERNITY_WORLD_PORT` accept 0 for an ephemeral port.
`ETERNITY_WORLD_DATABASE_ID` defaults to 1 and selects the original SQL ServerID.
Bind/advertised addresses default to loopback (`ETERNITY_LOGIN_BIND`,
`ETERNITY_WORLD_BIND`, `ETERNITY_WORLD_ADVERTISE`). The runner shares the isolated
SQL Server container's network namespace.
Server assets remain in ignored `.local/server-data`; staged `Map.ini` supplies
racial start positions and map rules. `--check-original-loaders` checks SQL definitions
and loads every configured map file and linked groups from `Obelisk.ini`. `ETERNITY_MAP_INI` overrides the INI path;
map files are read from its adjacent `Map` directory inside the container.
