# Eternity

Native Linux reconstruction of the Shaiya EP4.5 server in C++20, using SQL Server.
Complete means an original EP4 client can connect and play with original server behavior.
SQLite migration is deferred. Gameplay is not implemented yet.

`originals/` contains unmodified Windows executables and matching PDB symbols for
`ps_game`, `ps_dbAgent`, `ps_login`, `ps_session`, `ps_gameLog`, and `ps_userLog`.
All pairs have matching CodeView GUID and age. `ps_gamewar` is out of scope.

Source: the EP4.5 `PSM_Server/ShaiyaServerPatch/Server` distribution, including
its matching `ps_login.exe`. Database backups and deployment configuration are
not included. Implemented: SQL access, original definition loaders, asynchronous
encrypted TCP login on the available SQL path, account session replacement,
realm handoff reservations, game TCP handshake/ciphers, and SQL-backed character
listings. `--serve` starts login and game listeners with shared sessions and the
original persisted world calendar. Faction selection and character creation work
over TCP with original SQL persistence and starter items. World entry is pending.
Character detail loading and initial data/item/bank/skill/effect/quest/quick-slot
packets are implemented and tested separately; selection does not enter the world yet.
Quest definitions, saved quest timers, learned-skill cooldowns and quick slots
load with original reconstruction rules; quest gameplay remains pending.
Base statistics, equipment modifiers, faction blessing initialization, passive
skills and active-effect state are reconstructed. Effect restoration, expiry and SQL snapshots are tested
independently. Resource recovery and its ordered view/self/group notifications
are reconstructed; live world operations and gameplay remain pending.
Scalar HP damage, its protection/group notifications and the attacker damage ledger
are reconstructed. Equipment removal reverses cached durability, gems and craft modifiers, cancels weapon
elements and sends original speed/resource updates. Inventory move/drop/removal
dispatch remains pending. Defense durability wear updates native actor equipment and defenses;
scalar damage, attribution and low-HP passive activation now run through the actor.
Death effect cleanup, corpse snapshots/cell visibility and the global corpse registry
are reconstructed; death orchestration, combat dispatch and the live world remain pending.
Attack-triggered effect, vehicle and pending-move cancellation run through native actors;
live actor lookup and ordinary ranged broadcasts remain required world services.
Trade/duel offer reference cleanup and readiness cancellation run through native
actors; transfer, acceptance, persistence and TCP handlers remain pending.
Duel cleanup, attack-triggered exit, wager-drop sequencing and result/log packets
run through actors; live transport, inventory transfers and the remaining duel
handlers still require integration.
Ground-item zone/cell indexes, visibility and expiry are reconstructed, including
native wager placement, expiry and pool recycling. The item pool preserves reused
IDs and partial initialization. Pickup permissions and item/quest links are
reconstructed. Detached-item admission now stacks, stores or returns remainders
with original DB/client/group/log messages. ID-based pickup connects zone
permissions, money bonuses, capacity errors and inventory admission; TCP world
dispatch and shared loot distribution remain pending.
Actor money credits preserve unsigned limits and ordered DB-status/client/log
messages, with money-item release to the pool. Periodic money saving and shared
loot dispatch still require integration.
Actor preparation connects these stages, including effect-driven resource clamps
and cleanup. Equipment/transformation attack rebuilding and elemental-effect
replacement run natively in the actor. User appearance packets include visible
equipment, enchantments, cloak and social fields, with a copied-user snapshot path.
Native user-cell queries now reveal hidden enemies using original duel, guild
and map rules. Server map files/configuration, collision/line checks, A* paths, entry bounds and movement cell resolution are
reconstructed; world admission, broadcasts, combat and TCP world entry remain pending.
Zone selection/reuse and access rules are reconstructed; world object initialization
and zone queue services remain required dependencies.
Door/region templates, door operation and weather transitions are reconstructed.
NPC definitions, populations, patrols, guard navigation/target queries and visibility
packets are reconstructed; mob runtime, combat and the running world remain pending.
The active MD5 login
path needs a missing procedure; see [research](research/protocol.md).

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
and loads every configured map file. `ETERNITY_MAP_INI` overrides the INI path;
map files are read from its adjacent `Map` directory inside the container.
