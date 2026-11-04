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

## Persisted ownership milestone — SetKeepObelisk

Implementation commit: `e6f7850aa24037620005e412f4a0c339ef392421`.

Recovered `ps_game.exe` **CWorld::SetKeepObelisk 00413bd0** plus real
**CMap::FindPortalLink 0042b500**, checked against assembly and a locked focused
Ghidra export. `restore_obelisk_ownership` applies decoded persisted triplets:

- Base ID bit **0x10000000** selects the portal branch exclusively. First matching
  portal link wins; a null portal object stops without changing flags. A present
  portal receives the complete signed country value and its two open flags:
  0→both, 1→first, 2→second, other values→neither. Timestamp/calendar are ignored.
- Ordinary IDs search maps and records in original order, stop at the first
  matching base ID, and do not require an active zone. Duplicate IDs and missing
  records preserve this behavior; no transport or persistence calls are emitted.
- Decode current from the low nibble and last from the next nibble minus one.
  Missing last falls back to current; any non-neutral current overrides last.
  Preserve the original low-nibble decoding rather than adding sanitization.
- For nonzero change timestamps, call required packed-calendar acquisition and
  unsigned seconds subtraction, then compute **max(86400 - seconds, 0) * 1000**.
  Timestamp zero stores remaining=0 and calls neither calendar service. Unsigned
  subtraction results above one day, including wrapped values, also store zero.
- Store remaining ownership milliseconds in original **dwResetTime +000c**,
  exposed as `ObeliskRuntime::reset_time`; retain the input change timestamp.
  This does **not** overwrite principal `respawn.next` or current. Calendar
  callbacks may mutate live record values; the already-selected target is kept.

The explicit portal projection binds nullable country and open-flag pointers;
ordinary inputs are stable spans of map records. Required
`ObeliskRuntimeOwnershipClock` exposes original packed `GetSystemTime`
**004cb8e0** and `SubTimeSecond` **004cb9d0** semantics without default no-ops.
Their native bodies are now implemented by the calendar milestone below, with
required OS APIs rather than Unix/host calendar approximations. These earlier
ownership-only fixtures observe only
these two ports; actual lookup, branch/packing and **_allmul 0050d3b0** execute.
512 cases cover ordinary/portal routing, duplicate/missing IDs, null portals,
zero timestamps, neutral/last-country decoding, raw signed portal countries,
86400-second limits and unsigned extremes, plus live mutation during clock calls.
The trace includes 221 ordinary writes and 177 paired calendar calls.

Validation:

- `python3 tools/dev.py build`: exit 0, no compiler warnings
  (`work/obelisk-ownership-build.log`).
- `python3 tools/dev.py test`: exit 0, **125/125 tests passed**, 32.44 seconds
  (`work/obelisk-ownership-tests.log`), including all six obelisk suites.
- Original fixture generation, independent regeneration and `cmp`: exit 0;
  `work/obelisk-runtime-ownership-oracle.tsv` and
  `work/obelisk-runtime-ownership-repeat.tsv` match the tracked fixture.
- Locked focused Ghidra export: exit 0 (`work/obelisk-ownership-ghidra.log`).
- Final `sha256sum -c SHA256SUMS`: exit 0 for all original EXE/PDB pairs
  (`work/obelisk-ownership-final-hashes.log`). `git diff --check`: clean.

No SQL queries/schema/fixtures changed; final isolated SQL integration remains
coordinator-owned. Decode persisted triplets and invoke this function before
normal startup; native calendar helpers are available below, while OS calendar
APIs and DB/world dispatch remain required.

Map ownership clarification from **CZone::MapInit 0041c8a0**, assembly
**0041d010–0041d01b**: only `m_nCreateType == 0` attaches the zone to the original
CMap and invokes InitObelisk. This path does not clone obelisk records as it does
linked-mob records. Earlier per-zone wording must not be read as permission to
instantiate independent ownership copies for every created map instance. Bind
normal map records to its primary zone, with stable respawn/member sources.
Further independent work remains configuration/source binding and OS calendar
adapter wiring; production world integration is not complete and the
server is not playable.

## Calendar helper milestone — local packing and SubTimeSecond

Implementation commit: `16f1e66075d50228f5dac44166e498890b8ceef1`.

Recovered `ps_game.exe` **GetSystemTime 004cb8e0** and
**SubTimeSecond(uint32,uint32) 004cb9d0**, locked focused Ghidra export plus
assembly. `ObeliskRuntimeCalendarClock` now implements the existing ownership
clock interface with these native helper bodies:

- The misleadingly named GetSystemTime wrapper calls **GetLocalTime**, not UTC.
  Pack the 16-bit calendar fields by wrapping additive shifts. There are no
  original year/range checks; valid 2000..2063 timestamps match the usual format,
  but years and oversized fields wrap/carry rather than throwing. Weekday and
  milliseconds are ignored.
- Decode the second operand first, then the first, with six-bit year +2000 and
  month/day/hour/minute/second masks. Call SystemTimeToVariantTime in that order,
  starting each output at +0.0 and ignoring its boolean return, as the original.
- Spill the OLE-day subtraction to binary64, multiply by 86400 at default 53-bit
  precision, truncate to signed 64 bits and return only the low 32 bits. Preserve
  unsigned wrap for negative differences. Do not replace this with rounded or
  exact civil-time seconds: 427 of the 1,065 valid-calendar fixture pairs differ
  from exact integer civil seconds because of the original floating truncation.
- Original CRT evidence: **_fpmath 00506081**, call **00506097**, and
  **_setdefaultprecision 0051136e** set 53-bit precision. The oracle explicitly
  uses its 0x027f control word; the native arithmetic uses binary64 spills.

Only the OS imports remain mandatory platform services:
`ObeliskRuntimeCalendarPlatform::local_calendar` and `variant_day`. They have no
no-op/default approximation. Calendar acquisition/selection and OLE conversion
semantics must be supplied by the platform owner. This block does not claim a
native SystemTimeToVariantTime implementation or substitute generic Gregorian
normalization for unverified OLE behavior. Existing ownership restoration can
use the concrete calendar clock without further shared-header changes.

The fixture executes real original GetSystemTime/SubTimeSecond and 512 complete
SetKeepObelisk restorations with those helpers unpatched. Only GetLocalTime and
SystemTimeToVariantTime imports are observed. 2,365 OLE responses are captured
from **Wine 11.20** through an isolated generated Winelib probe; this is platform
compatibility evidence, not execution of a Microsoft Windows OLE DLL. Wine's
prefix/probe/output stay ignored under `work/`; probe compilation takes the
shared build lock. Production native targets do not link or invoke Wine.

Cases: 264 raw/wrapping calendar packs, 1,229 unsigned second differences,
leap/day/year boundaries, future/reversed timestamps, failed OLE conversion,
zero timestamps and 512 restorations retaining the original 24-hour calculation.
Required calendar provider boundaries and real ownership composition are checked;
no live SQL or world provider completion is claimed.

Validation:

- `python3 tools/dev.py build`: exit 0, no compiler warnings
  (`work/obelisk-calendar-build.log`).
- `python3 tools/dev.py test`: exit 0, **126/126 tests passed**, 44.97 seconds
  (`work/obelisk-calendar-tests.log`), including all seven obelisk suites.
- Original generator/regenerator: exit 0; final
  `work/obelisk-runtime-calendar-final.tsv` matches the tracked fixture byte-for-byte.
  First comparison after adding probe build locking failed because its waiting
  diagnostic polluted stdout. Routed lock diagnostics to stderr and verified
  regeneration again (`work/obelisk-calendar-generation.log`); that first
  comparison is not counted as passed. Native fixtures/tests were unchanged.
- Independent arithmetic check: all 1,229 original differences match explicit
  binary64 subtraction/multiplication/truncation with the captured OLE values.
- Locked focused Ghidra export: exit 0 (`work/obelisk-calendar-ghidra.log`).
- Final `sha256sum -c SHA256SUMS`: exit 0 for all original EXE/PDB pairs
  (`work/obelisk-calendar-final-hashes.log`). `git diff --check`: clean.

SQL remains unchanged and final isolated integration remains coordinator-owned. Configuration/source binding, actual OS
calendar adapter, transport/KC/portal services and production dispatch remain
open. The server is not playable.

## Live forced-removal adapter — real zone, obelisk and pool owners

Implementation commit: `df1a39c46e446b31193dc6072fa8944ba7c9108a`.

`ObeliskRuntimeZoneRemoval` implements the existing canonical `MobRemovalOwners`
interface and calls actual `ZoneMobs::remove_by_id(id,pool,*this)`. It connects
forced removal to recovered **CZone::MobRemoveByID 0041e910** / nested
**CMap::ObeliskDestroy 0042a410** without reimplementing zone indexing, cells,
respawn accounting, or recycling:

- Principal callbacks read the live base country, pass reset=true and the
  original index to native Destroy. This is the forced-removal contract at
  **0041e9c3–0041e9f4**, not combat kill-credit ownership. Negative/above-count
  indices preserve the source guard; the unsafe one-past index is explicitly
  rejected instead of dereferencing missing backing storage.
- Companion callbacks resolve a stable map-owned spawn source and call the real
  first-match removal method (**stMapObeliskMob::RemoveMob 0041d700**). Unknown
  sources fail explicitly; valid bindings must not move during callbacks.
- Linked-group callbacks delegate to a required separate `MobRemovalOwners`.
  No default no-op owner or swallowed regular-gameplay branch is provided.
- Global/cell unlink, disappearance publication, full-count deadline update and
  decrement remain in ZoneMobs. Object recycling remains in MobPool, exactly
  once, after the obelisk/companion callback returns.

The original oracle now runs actual FindMob/global-tree removal/cell unlink,
MobRemoveByID, nested Destroy/Reset/RemoveMob, **FreeMob 0040a630**, and pooled
**CreateMob 0040a5e0** plus actual RNG seeding. Initial three actors are explicit
configured inputs; original constructors/Init are not claimed for that setup.
Only heap, locks, clocks/atomic, compiled-AI cleanup, transports and portal
publication are observed ports. Native comparisons use actual ZoneMobs, MobPool,
MobDamagePool and the new adapter, not the earlier removal-port projection.

256 retained sequences compare 1,536 operations, 768 actual recycling callbacks,
and 512 pooled reuses. Cases include direct forced removal and map reset, holes,
duplicate/stale companion slots, reversed group order, already-absent IDs,
zero/wrapping IDs and tick deadlines, full/non-full/negative counters, country
changes, suppression flags, and LIFO object identity/ID/RNG reseeding. Duplicate
slots do not cause double recycling; unlisted actors remain live until explicitly
removed. The trace also compares all disappearance/log/change/DB packets, registry
size and reset flags at cleanup, and respawn/member/pool state after every step.

Validation:

- `python3 tools/dev.py build`: exit 0, no compiler warnings
  (`work/obelisk-zone-build.log`).
- `python3 tools/dev.py test`: exit 0, **127/127 tests passed**, 30.64 seconds
  (`work/obelisk-zone-tests.log`), including all eight obelisk suites.
- Original generation/regeneration/`cmp`: exit 0;
  `work/obelisk-runtime-zone-oracle.tsv` and `work/obelisk-runtime-zone-repeat.tsv`
  match the tracked fixture byte-for-byte.
- Locked focused Ghidra export: exit 0 (`work/obelisk-zone-ghidra.log`).
- Final `sha256sum -c SHA256SUMS`: exit 0 for every original EXE/PDB
  (`work/obelisk-zone-final-hashes.log`). `git diff --check`: clean.

Integration: construct this bridge over the primary normal map's stable records,
its actual ZoneMobs/MobPool, existing ObeliskRuntimeWorld and linked owners. The
world's mandatory `remove_by_id` should delegate to the bridge. Do not decrement
counts, clear all sources, or recycle again in that world callback. Reset can
then use the native function directly and retain its temporary suppression flag
through real nested zone removals. Combat Death must retain its separate country
and flag semantics instead of being routed through forced removal.

No canonical header/interface change is required for this removal bridge. The
raw allocation/preparation split remains a separate startup/respawn dependency;
this bridge does not need it. Configuration/source binding and production world
assembly/transports/calendar/KC/portal providers remain open. SQL is unchanged;
final isolated SQL integration remains coordinator-owned. This is an actual owner
adapter and verified composition, not a playable-server claim.

## Combat death owner adapter — real death, nested removals and corpse cleanup

Implementation commit: `9fd122fceadb3c9f8b826832a916913c1c1088a0`.

`ObeliskRuntimeDeathOwnership` supplies the ownership portion of the existing
`MobDeathWorld`: delegate `obelisk_country` to `country`, `destroy_obelisk` to
`destroyed`, and `remove_obelisk_member` to `remove_member`. Keep stable primary
map records, zone, world, kill-count configuration and removal owner references.
No canonical interface change is required. Wrong-zone/unknown-source bindings
fail explicitly; the original unsafe inclusive one-past record is rejected.

Evidence is **ps_game.exe, CMob::Death 004910f0**. Its fallback reads the raw
record country at **00491604**, chooses 0->1, 1->0, otherwise 2, and calls
**CMap::ObeliskDestroy 0042a410** at **0049161e** with reset=false. It must not use
**stMapObelisk::GetCountry 0042afe0**, which translates country for publication.
The adapter therefore returns the raw live country and invokes actual native
Destroy with reset=false. Existing canonical Death retains the per-call
UINT32_MAX deadline/count decrement (**00491623–00491635**); the adapter neither
accounts again nor removes the dying principal. Companion ownership delegates
to the existing first-match removal bridge (**RemoveMob 0041d700**, called from
**0049163a–00491643**). Forced removal retains its separate reset=true contract.

The original fixture runs actual Death, damage selection/reset, nested
MobRemoveByID/Destroy, pooled recycling/reuse, and **UpdateMobDeath 00421a80**
(deadline subsection **00421bdd**). Native comparison uses actual mob_death,
ZoneMobs, MobPool and both ownership adapters. 256 sequences compare 2,048
operations: absent/user/non-user/unresolved credit, mutable user country and
record country across reward callbacks, duplicate/hole/reversed member slots,
wrapping IDs/counters/deadlines, and strict corpse-time equality. Death leaves
the principal registered; the explicit caller then marks its corpse state.
Natural cleanup updates the sentinel deadline without a second decrement.
Reward, quest, skill, KC and transport calls remain observed service boundaries;
this does not implement their production providers or the caller's FSM.

An initial run passed 127/128 tests (39.63 seconds): original corpse traversal
skipped the principal. The raw fixture omitted the constructor's global-node
payload. **CMob::CMob 0048fd50**, instructions **0048fd6b/0048fd7c**, require
mob+0x74 = mob+0x50. Correcting that explicit input and asserting it before
traversal fixed the fixture; no native behavior was changed to mask the failure.
Initial actors remain projected inputs, not original constructor execution.

Validation:

- `python3 tools/dev.py build`: exit 0, no warnings
  (`work/obelisk-death-build.log`).
- Corrected `python3 tools/dev.py test`: exit 0, **128/128 passed**, 44.72 seconds
  (`work/obelisk-death-final-tests.log`). Initial failure is retained in
  `work/obelisk-death-tests.log`.
- Independent original regeneration matches the retained death fixture exactly
  (`work/obelisk-runtime-death-final-repeat.tsv`). Default forced-removal oracle
  still matches its existing fixture byte-for-byte
  (`work/obelisk-runtime-zone-final-regression.tsv`).
- Locked focused Ghidra export completed (`work/obelisk-death-ghidra.log`).
  All original EXE/PDB SHA256SUMS passed
  (`work/obelisk-death-final-hashes.log`); `git diff --check` passed.

Remaining integration: assemble these delegates in the production death world
and retain actual zone/pool ownership; provide reward/quest/skill/KC/portal and
transport services. Configuration/source binding, raw allocation/preparation
split for startup/respawn, and platform calendar providers remain separate open
dependencies from prior milestones. SQL is unchanged; final isolated SQL checks
belong to the coordinator. The server is not playable.

## Portal ownership provider — both update passes and actual publication

Implementation commit: `a8ee553eb8f48d84554cf9491248750a0cfded68`.

Recovered **ps_game.exe, CMap::UpdatePortalLink 0042a000** and
**SendPortalLinkChange 0042b2c0** as `update_obelisk_portal_links` and
`publish_obelisk_portal_change`. This supplies the previously missing required
portal coordination provider called after Destroy/Reset changes ownership.
The owning world's `update_portal_links` can now delegate to this function over
its retained global portal-link array. No canonical interface change is needed.

The projection retains each link's nullable target country, two open flags,
two counts, all eight obelisk references, publication IDs and target-zone
presence. Update traverses the global link table without a calling-map zone guard.
The country matching reads `last_destroy_country`, not current
country. Side zero takes priority whenever its count is positive; side one is
considered only otherwise. All matching opens a closed side, zero matching
closes an open side, and partial matching preserves it. Country conversion
preserves the original four portal states, including out-of-range input rules.
Opening side zero marks the next link; opening side one marks the previous
link. A separate second pass closes marked neighbors. Missing targets skip
both passes. The original twelve-element stack flag array is explicit: larger
arrays fail before publication instead of reproducing stack corruption.

Actual native packet assembly now preserves DB 0207 (base ID, raw country byte,
zero timestamp), target-map 2102 (full DWORD local ID, translated country), and
world 2104 (WORD portal ID, old/new translated countries), in original order.
Translations are 0->103, 1->101, 2->102, otherwise100. Map publication is skipped
when its zone is absent; DB and world publication remain required services.
The DB callback precedes live map reads; both precede the live global transition
packet reads. Storage and bindings must remain stable throughout callbacks.
Existing persisted ownership views can bind to this link's country/open fields.

1,024 original-execution scenarios run both actual original functions, observing
only three transport boundaries. They include 0/1/2/3/4/6/11/12 links, nullable
portals/references, signed counts, both country lists, partial matches, existing
open flags, invalid initial country values, absent map zones, and live country/
last-destroy mutation at the first transport. Dedicated all-matching scenarios
exercise both neighbor directions: 1,747 records publish twice in a call.
The final fixture includes 4,400 DB, 2,936 map and 4,400 world packets and 4,992
final portal-state snapshots. Initial structures are explicit input projections;
map loading and transport delivery are not claimed.

The first full run failed the new test (128/129 passed, 34.17 seconds): native
scenario38 diverged. I initially misread the address arithmetic for country-one
references. Instructions **0042a06f–0042a07b** multiply side by four before the
12-byte entry stride; the corrected implementation uses `[side*4+n]`. The
original oracle was unchanged by that native correction. Its final fixture adds
stronger two-pass transitions; the initial failure log is preserved.

Validation:

- `python3 tools/dev.py build`: exit 0, no warnings
  (`work/obelisk-portal-final-build.log`).
- Corrected `python3 tools/dev.py test`: exit 0, **129/129 passed**, 35.48 seconds
  (`work/obelisk-portal-final-tests.log`). Initial failed run remains in
  `work/obelisk-portal-tests.log`.
- Independent original regeneration and `cmp`: exit 0, byte-identical to retained
  fixture (`work/obelisk-runtime-portal-final-repeat.tsv`).
- Locked focused Ghidra export: exit 0 (`work/obelisk-portal-ghidra.log`).
- `sha256sum -c SHA256SUMS`: all original EXE/PDB files passed
  (`work/obelisk-portal-hashes.log`). `git diff --check`: clean.

Remaining: wire this provider into the production world together with stable
configured map/portal records and required DB/map/world transports. A combined
original/native Death->Destroy->portal update fixture can next replace the
previous composition fixture's observed portal callback. Raw pool preparation,
configuration loading, calendar/KC and other production world dependencies from
prior blocks remain open. SQL is unchanged and final isolated SQL validation
remains coordinator-owned. The server is not playable.
