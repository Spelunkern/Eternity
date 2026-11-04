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

## First milestone validation

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
  (principal and periodic replenishment) from heap allocation **0040ab80**
  (companions in complete respawn). Raw-heap companion preparation
  must atomically assign the world mob ID and seed drop RNG with Unix time XOR
  ID; initialize must call real `initialize_mob`; enter must call real zone owner.
- Required transports and KC totals/status-14 lookup remain external world
  owners. The `SendObeliskChange` **0042b200** serializer is now available in the
  publication helpers below; actual zone routing is still required.
  `UpdatePortalLink` **0042a000** still requires its world owner. No required
  service has a default no-op implementation.
- Production tick/reset dispatch and configuration remain unconnected: native
  `UpdateRespawnObelisk` and `ObeliskReset` bodies are available (see below), but
  loaders and the live ownership bridges still need integration. Existing
  `account_obelisk_mob_corpse` already implements the corpse deadline rule;
  `ZoneMobs` already supplies removal/corpse accounting (see correction below).
  Destroy/respawn do not themselves schedule the next principal respawn.

This is recovered native subsystem behavior, not a playable server.

## Scheduler milestone — UpdateRespawnObelisk

Implementation commit: `afccb884cb78f0ee54380bf24cdf52722ed2f2a6`.

Recovered `ps_game.exe` **CMap::UpdateRespawnObelisk 00429da0**, with a locked
focused Ghidra `--process` export to `work/exports/04-obelisks`. Native
`update_obelisk_respawns` traverses stable per-zone projections in source order:

- Requires a zone and a principal definition for the current country, even when
  checking companion replenishment.
- Signed principal current **< 1** chooses complete respawn only when unsigned
  deadline **<= now**; companions are not separately replenished in that branch.
  Complete respawn sets current=1 before allocation, so a failed principal
  allocation sends the next tick into the companion branch, as in the original.
- Otherwise each group with a definition, current **< count**, and deadline
  **<= now** gets exactly one random candidate. Invalid/blocked terrain and
  failed allocation leave its current/deadline unchanged, allowing a later retry.
- Periodic replenishment uses manager **CreateMob 0040a5e0**, not the raw heap
  path used by complete respawn's companions. `allocate(true)` means manager
  creation, not necessarily a principal; header contract now states this.
- On success: initialize, place, increment current, set deadline to **now + live
  mob definition respawn_time**, add to the first free member slot, then enter.
  Arithmetic wraps at 32 bits; deadline comparison is unsigned, not an elapsed
  time subtraction. No companion spawn log is emitted by the periodic path.
- Shared random-position/terrain helper preserves the previously validated
  complete-respawn behavior rather than introducing a second coordinate rule.

`obelisk_runtime_timers_oracle.py` executes the actual scheduler, actual complete
respawn body, `CBitArray::GetBit` **0041b3a0**, and `AddMob` **00429930**. Allocation,
Init, enter, transport, and random/time/atomic/seed remain observed external
ports. 768 scenarios each run three consecutive ticks over two obelisks, retaining
members/counters/deadlines. Fixtures cover equal/future/past deadlines, unsigned
boundaries and wrap, negative/zero/full counters, missing country definitions,
blocked/out-of-bounds candidates, failed allocation, and changed live definitions.
Counters/deadlines are also compared at the EnterMob boundary, before snapshots.
The trace contains 259 successful periodic replenishments, 230 principal spawns,
359 complete-respawn companion spawns, and 24 periodic live-definition changes.

Validation:

- `python3 tools/dev.py build`: exit 0, no compiler warnings
  (`work/obelisk-timers-build.log`).
- `python3 tools/dev.py test`: exit 0, **121/121 tests passed**, 33.02 seconds
  (`work/obelisk-timers-tests.log`), including both obelisk suites: the previous
  432 destroy/respawn scenarios and the new 768 scenarios / 2,304 retained ticks.
- Original fixture generation and independent regeneration/`cmp`: exit 0;
  ignored outputs `work/obelisk-runtime-timers-final.tsv` and
  `work/obelisk-runtime-timers-repeat.tsv` match the tracked fixture byte-for-byte.
- Locked focused Ghidra export: exit 0, scheduler/respawn/GetBit exported.
- `sha256sum -c SHA256SUMS`: exit 0 for all original EXE/PDB pairs
  (`work/obelisk-timers-hashes.log`). `git diff --check`: clean.

No SQL changes; final isolated SQL integration remains owned by the coordinator.
No new canonical fields or virtual methods are required. The map tick owner must
call this function with stable per-zone obelisk storage and the original unsigned
millisecond tick, and implement the mandatory existing world services. Principal
corpse deadlines and production reset dispatch remain missing; this block does
not make the server playable. The reset body was subsequently reconstructed below.

## Reset milestone — ObeliskReset

Implementation commit: `0c4b246df036a3ecbcb30065d8d37624b88fb3f8`.

Recovered `ps_game.exe` **CMap::ObeliskReset 0042aa40**, checked against assembly
and a locked focused Ghidra `--process` export. `reset_obelisks` preserves:

- No zone: no clock or side effects. Otherwise sample GetTickCount once, even
  with no obelisks; compute unsigned **tick + 600000** once for all records.
- A present principal is removed by the required zone provider while
  `reset_send=true`, then the flag becomes false. Reset itself does not clear
  the principal, remove companions, or decrement current: these are removal
  provider responsibilities, including nested `ObeliskDestroy` when applicable.
- After removal returns, overwrite principal respawn deadline with the captured
  ten-minute deadline. Read the live base country, assign current country, and
  update last-destroy country only when the new country is non-neutral.
- Send the change notification, then persist **0207** unconditionally for each
  record, including unchanged countries and absent principals. Keep the live
  change timestamp unchanged. Finally update portal links once, even for an
  empty map. A nested destroy can already have persisted/relinked: preserve
  those additional calls and their order.
- An initially true reset-send flag is not normalized when no principal is
  removed. The flag suppresses nested destroy's change notification; it does
  not suppress reset's direct notification.

The original-execution fixture runs real Reset plus **404 real nested Destroy**
bodies across 648 cases, including zero/one/two/three records, neutral/base/last
country combinations, missing principals, existing reset flags, unsigned deadline
wrap, unchanged countries, and callback changes to base/last/change-time values.
Clock/removal/transports are observed ports. Removal variants explicitly project
an inert removal or a counter/deadline update followed by real Destroy; the full
**CZone::MobRemoveByID 0041e910** body is not claimed as executed/reconstructed.
The 271 changed nested destroys cause additional DB sends/portal updates, which
are compared alongside reset's unconditional persistence. Send-change callbacks
also observe current/deadline/country/flag, checking ordering before final state.
The native test composes the existing real `destroy_obelisk` implementation.

Validation:

- Native build: exit 0, no compiler warnings (`work/obelisk-reset-build.log`).
- First full suite: **121/122 passed**, 34.39 seconds; reset scenario 14 exposed
  an oracle trampoline cached with a previous case's immediate country argument
  (`work/obelisk-reset-tests.log`). This attempt did not pass. The oracle now
  reads the live country from memory, invalidates the reused code translation,
  and asserts the arguments at the real Destroy entry before accepting a case.
- Corrected original fixture generation, independent regeneration, and `cmp`:
  exit 0 (`work/obelisk-runtime-reset-checked.tsv`,
  `work/obelisk-runtime-reset-final-repeat.tsv`).
- Full native suite rerun: exit 0, **122/122 tests passed**, 35.57 seconds
  (`work/obelisk-reset-final-tests.log`). This includes all three obelisk suites:
  432 destroy/respawn scenarios, 2,304 retained scheduler ticks, and 648 reset
  scenarios with 404 real nested destroys. `git diff --check`: clean.
- Locked focused Ghidra export and original EXE/PDB hash verification: exit 0
  (`work/obelisk-reset-ghidra.log`, `work/obelisk-reset-final-hashes.log`).

No SQL contracts changed; final isolated SQL integration remains coordinator-owned.

Integration: map reset command dispatch must call the new function with stable
per-zone records, a mandatory millisecond clock callback, and the existing
mandatory world services. `remove_by_id` must implement real zone unlinking,
respawn accounting and nested obelisk destruction, not a no-op or unconditional
pointer clear. No new canonical fields or virtual methods are needed. Further
independent work: bind obelisk configuration/state loading and connect existing
native corpse/removal owners; production world provider wiring remains missing.
This subsystem milestone is not a playable server.

## Startup milestone — InitObelisk

Implementation commit: `71b37a97b95b48e2f4d24e93d85e284758d26476`.

Recovered `ps_game.exe` **CMap::InitObelisk 00429a60**, assembly plus locked Ghidra
`--process` export. Native `initialize_obelisks` initializes stable, already-bound
per-zone records in map order, selecting the principal and companions by current
country. Missing definitions leave their counters/deadlines untouched. Principal
current=1/deadline=0 are written before allocation, and failed allocation skips
that principal's companion initialization. Defined companion groups set current
exactly to count/deadline=0 before exactly count random attempts. Terrain and
allocation failures do not roll counts back or retry; first-free membership
insertion retains existing slots.

Startup uses **raw heap 0040ab80 for both principal and companions**, followed by
world ID assignment and time-XOR-ID seeding before template assignment/Init.
It emits no game-log spawn packet. Later complete respawn still uses manager
**CreateMob 0040a5e0 for the principal** and emits its spawn log. A private shared
population helper preserves these two differences explicitly; there are no new
virtual methods. Historical `prepare_companion` now documents that the raw-heap
preparation boundary also applies to startup principals.

`obelisk_runtime_initialize_oracle.py` reuses the observed-port scheduler harness
with real original InitObelisk followed by three real scheduler ticks. 768
scenarios retain two obelisks and their companion groups; controlled EnterMob
callbacks decrement companion current in one fifth of scenarios to drive later
replenishment. This is explicit live input, not a reconstructed kill/zone service.
The trace checks ID/seed preparation ordering, initialization/entry, counters,
deadlines, coordinates, membership identities, absence of startup logs, failed
allocation, missing definitions and unsigned time boundaries. It contains 622
startup principals, 886 startup companions, 1,508 prepared IDs and 258 later
companion replenishments across 2,304 retained ticks.

Validation:

- `python3 tools/dev.py build`: exit 0, no compiler warnings
  (`work/obelisk-initialize-build.log`).
- `python3 tools/dev.py test`: exit 0, **123/123 tests passed**, 33.81 seconds
  (`work/obelisk-initialize-tests.log`). Includes the new 768 startup scenarios /
  2,304 retained ticks and all prior destroy/respawn/scheduler/reset comparisons.
- Original startup fixture generation/regeneration/`cmp`: exit 0, outputs
  `work/obelisk-runtime-initialize-oracle.tsv` and
  `work/obelisk-runtime-initialize-repeat.tsv` match byte-for-byte.
- Shared oracle regression: exit 0; `work/obelisk-runtime-timers-regression.tsv`
  remains byte-identical to the tracked original scheduler fixture.
- Locked focused Ghidra export: exit 0 (`work/obelisk-initialize-ghidra.log`).
  Final `sha256sum -c SHA256SUMS`: exit 0 for all original EXE/PDB pairs
  (`work/obelisk-initialize-final-hashes.log`). `git diff --check`: clean.

No SQL changes; final isolated SQL integration remains coordinator-owned.

Integration correction: the existing canonical `account_obelisk_mob_corpse`
(`src/game/mobs.cpp`, original UpdateMobDeath **00421bdd**) already implements
`next < now || next == UINT32_MAX` followed by `now + respawn_time` without count
changes. `ZoneMobs::update_deaths` already calls it, and
`ZoneMobs::remove_by_id` already implements forced removal/accounting with required
`MobRemovalOwners` callbacks. Earlier report statements that these bodies needed
reconstruction were too broad: the missing work is their binding to this obelisk
projection and real map/pool/transport owners. Do not duplicate those bodies.

A faithful raw-allocation bridge needs a canonical pool interface decision:
`MobPool::allocate()` currently combines heap allocation, ID assignment and RNG
seeding. This runtime deliberately exposes raw allocation then preparation, as
seen at **00429acf–00429b05** (startup principal) and
**0042a96b–0042a99d** (respawn companion). Suggested canonical split:
`MobPool::allocate_raw()` plus `prepare_allocated(MobRuntime&)`, with existing
`allocate()` retaining manager CreateMob semantics by composing both. A provider
must not increment/reseed twice or substitute a default no-op preparation.
No shared pool/header changes were made here.

Next independent work: obelisk configuration/state loading and stable per-zone
respawn/source binding. Production startup/tick/reset dispatch and world ports
remain unconnected; this milestone does not make the server playable.

## Publication milestone — change and database list

Implementation commit: `9900269130e02b151b237881500a4284df9e8602`.

Recovered `ps_game.exe` **CMap::SendObeliskChange 0042b200**,
**SendDBObeliskChange 0042b260**, and **SendDBObeliskList 0042b370**, with original
assembly and a locked focused Ghidra export. New publication helpers implement:

- Zone **2102**, seven bytes: opcode, full 32-bit local obelisk ID, translated
  current country (0→1, 1→2, all others→0). Only the zone guard suppresses this
  function; reset-send suppression belongs to its caller, as in Destroy.
- DB **0207**, eleven bytes: base ID, original byte-packed last/current country,
  change timestamp. No zone guard. Destroy/Reset now share this exact serializer.
- DB **0206**: opcode, byte count, nine-byte records in source order; send each
  full chunk of **100** immediately and then a nonempty remainder. Empty input
  sends nothing. Read future records after previous transport callbacks, rather
  than building a snapshot of all packets in advance.

`ObeliskRuntime::id` projects original `stMapObelisk::dwID` at **+0000**, distinct
from `base_id` at **+0014**. The map/configuration owner must populate both;
substituting base ID or vector index for the local ID produces the wrong packet.
`ObeliskRuntimePublication` requires concrete zone/DB transports without defaults.
Existing `ObeliskRuntimeWorld::send_change` can now call
`publish_obelisk_change(map, records[index], transport)`; its database callback
continues receiving the already-serialized 0207 packet. World DB-list dispatch
can call `publish_obelisk_database_list` with stable per-map projections.
No canonical shared header changes were made.

The original oracle executes all three serializer/loop bodies; only zone and DB
transport calls are observed. 216 cases cover zero/one/two and 99/100/101,
199/200/201, 255/256/301 records, nonzero selected indices, absent zones, reset
flags, IDs above 16 bits, and signed/extreme country values to verify byte
wrapping without C++ signed overflow. Transport callbacks change later country,
base-ID and timestamp values after a full chunk. The resulting trace contains
62 zone packets and 210 database packets, including 138 list packets containing
10,290 records. This validates protocol assembly, not live SQL persistence.

Validation:

- `python3 tools/dev.py build`: exit 0, no compiler warnings
  (`work/obelisk-publication-build.log`).
- `python3 tools/dev.py test`: exit 0, **124/124 tests passed**, 29.34 seconds
  (`work/obelisk-publication-tests.log`), including all five obelisk suites.
- Original fixture generation, independent regeneration and `cmp`: exit 0;
  `work/obelisk-runtime-publication-oracle.tsv` and
  `work/obelisk-runtime-publication-repeat.tsv` match the tracked fixture.
- Locked focused Ghidra export: exit 0 (`work/obelisk-publication-ghidra.log`).
- Final `sha256sum -c SHA256SUMS`: exit 0 for every original EXE/PDB
  (`work/obelisk-publication-final-hashes.log`). `git diff --check`: clean.

No SQL queries/schema/fixtures changed; final isolated SQL integration remains
coordinator-owned. Production transport routing, portal/KC services, stable
configuration/state loading and world dispatch remain required. The canonical
pool interface proposal from the startup block is still pending. This milestone
does not make the server playable.
