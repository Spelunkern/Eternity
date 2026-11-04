# Worker 04 — obelisk ownership and respawn

Implementation commit: `56960389a5e3486d841ee53c87cf0b7180151123`.

## Evidence and recovered behavior

Original `originals/ps_game/ps_game.exe`, SHA256
`0fb7c9f13b5bcf186118f8bfe73ab214226b93e8aa502b8e6fded4a8312bc547`.
The authoritative digest is verified by `SHA256SUMS` and recorded in the fixture.
Ghidra evidence: full read-only exports in the coordinator's
`work/reverse/ps_game.exe`; focused locked `--process` export in this worker's
ignored `work/exports/04-obelisks/ps_game.exe`.

- `CMap::ObeliskDestroy` **0042a410**: zone guard, clear principal, remove and
  clear companion slots in order, neutral-to-base fallback, optional change
  notification, kill-count add/status-14/subtract ordering, reset suppression of
  world broadcast/KC updates, destruction log, changed-country persistence and
  portal relinking. Same-country destruction still logs and applies KC logic.
- `CMap::ObeliskRespawn` **0042a6f0**: select definition by current country;
  update principal current/deadline before allocation; initialize/place/enter/log
  principal; then initialize companion counters and perform exactly count random
  attempts. Truncate spilled float coordinates toward zero for terrain tests;
  reject blocked/out-of-bounds attempts without retry. Allocation failure does
  not roll back counters. Preserve stale member slots and fill first empty slot.
- `stMapObeliskMob::AddMob` **00429930** and `RemoveMob` **0041d700**:
  first empty / first match; duplicate pointers allowed. Null lists are no-ops.
- Packet layouts: client `2103` (6 bytes, destroyed mob type and translated old/new
  countries), log `0a03` (50 bytes), log `0a02` (49 bytes), DB `0207` (11 bytes,
  base ID, packed last/current country, unchanged change timestamp).

Native short-name log padding is zeroed; original unused stack padding is
unspecified. Original fixtures use a 31-byte name to compare every packet byte
without treating stack garbage as protocol behavior.

## Validation

- `python3 tools/dev.py build`: exit 0, no compiler warnings
  (`work/obelisk-final-build.log`).
- `python3 tools/dev.py test`: exit 0, **120/120 tests passed**, 30.24 seconds
  (`work/obelisk-final-tests.log`). Includes **432 complete original-execution
  comparisons** and membership ownership checks in `obelisk-runtime-unit`.
- `.local/oracle-env/bin/python tools/obelisk_runtime_oracle.py` regenerated
  `work/obelisk-runtime-final.tsv`; byte-identical comparison with the tracked
  fixture exited 0. Original allocation/transport/initialization services are
  observed ports, not claims of completed world providers.
- Focused Ghidra `--process` export: exit 0; all four symbols exported.
- `sha256sum -c SHA256SUMS`: exit 0 for every original EXE/PDB
  (`work/obelisk-hashes.log`). `git diff --check`: clean.

No SQL schema, query or fixture changes.
SQL lifecycle and final isolated SQL integration remain coordinator-owned as
required by the assignment; this worker does not provision or reset databases.

## Integration dependencies and canonical interfaces

The new API is an explicit projection: `ObeliskRuntime`, `ObeliskRuntimeSpawn`,
`ObeliskRuntimeMap`, and mandatory `ObeliskRuntimeWorld` services. No shared
canonical header was modified. CMake additions register only this source/test.

- Map owner must retain stable per-zone records and bind principal
  `MobRespawn.type` to the obelisk index, `obelisk=true`, and companion
  `obelisk_source` to its stable spawn record. Existing `MobRespawn` fields
  suffice; caller must connect death's destroy/remove callbacks to this owner.
- `MobRuntime::type` already represents original `CMob::m_wMobID` at **+0220**;
  use this for `2103`, not the definition ID (they may differ).
- `MobRuntimeTemplate` lacks original `stMOBInfo::szName` (**+0002**, 32 bytes).
  Required `mob_name` supplies it meanwhile. Canonical loader should retain this
  bounded name or expose an equivalent lookup by template identity.
- Allocation provider must distinguish `CObjectMgr::CreateMob` **0040a5e0**
  (principal) from heap allocation **0040ab80** (companion). Companion preparation
  must atomically assign the world mob ID and seed drop RNG with Unix time XOR
  ID; initialize must call real `initialize_mob`; enter must call real zone owner.
- Required transports, KC totals/status-14 lookup, `SendObeliskChange`
  **0042b200** and `UpdatePortalLink` **0042a000** remain external world owners.
  These services have no default no-op implementation.
- Production scheduler/configuration/reset orchestration remains unconnected:
  `UpdateRespawnObelisk` **00429da0**, `ObeliskReset` **0042aa40**, loaders and
  corpse deadline accounting need their world/map integration. The two assigned
  destroy/respawn bodies do not themselves schedule the next principal respawn.

This is recovered native subsystem behavior, not a playable server.
