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

## Combat death through real portal ownership — composed evidence

Evidence/test commit: `31d85d42a03416a780d4beb7f051a34541625a76`.

The new `obelisk-runtime-death-portal-unit` replaces the earlier composition's
observed portal-update callback with actual native `update_obelisk_portal_links`
and publication. Its original counterpart now executes **ps_game.exe,
CMob::Death 004910f0 -> CMap::ObeliskDestroy 0042a410 -> UpdatePortalLink
0042a000 -> SendPortalLinkChange 0042b2c0**, alongside real nested zone removal,
**FreeMob 0040a630**, **UpdateMobDeath 00421a80** and pooled **CreateMob
0040a5e0**. No production implementation changed in this evidence block.
Previously focused Ghidra exports provide the function evidence; no new symbols
or speculative ownership rules were introduced.

Four stable portal links reference the actual destroyed obelisk plus three
explicit other-map ownership inputs. Cases vary nullable targets/references,
country-zero priority versus country-one groups, existing open states, missing
map zones, suppressed obelisk publication and both credited/fallback death
ownership. The first portal DB callback in scenarios divisible by17 changes
live portal countries and the actual obelisk's last-destroy country. Subsequent
portal matching/publication and corpse snapshots must reflect those changes.
The test preserves original DB/map/world packet order across this callback.

256 retained sequences compare 2,048 operations, 202 actual portal-update
entries, 106 portal DB changes, 69 target-map publications, 106 global portal
transitions and 9,216 portal-state snapshots, together with 768 real actor
recycles and 512 pool reuses. The principal remains zone-owned until strict
natural corpse cleanup; portal work neither decrements its respawn counter
again nor recycles it. Required production reward/quest/skill/KC/transport
services remain observed boundaries. Initial actors/map/link structures are
explicit projections; configured loading and network delivery are not claimed.

Validation:

- `python3 tools/dev.py build`: exit0, no compiler warnings
  (`work/obelisk-death-portal-build.log`).
- `python3 tools/dev.py test`: exit0, **130/130 passed**, 39.20 seconds
  (`work/obelisk-death-portal-tests.log`).
- Independent original generation/regeneration and `cmp`: exit0
  (`work/obelisk-runtime-death-portal-final.tsv`,
  `work/obelisk-runtime-death-portal-final-repeat.tsv`). The final metadata
  correction clarifies that portal coordination executes; all256 data rows are
  identical to those tested. The retained fixture was replaced after tests ended.
- Earlier death and forced-removal original fixtures remain byte-identical
  (`work/obelisk-runtime-death-portal-{death,zone}-regression.tsv`).
- All original EXE/PDB SHA256SUMS passed
  (`work/obelisk-death-portal-hashes.log`); `git diff --check` passed.

Integration: the real death world's mandatory `update_portal_links` delegates
to the retained global link table and real portal provider. Its DB and global
send methods can serve both obelisk and portal services; `map_broadcast(index,p)`
must route to that link's actual target map, rather than always the dying
actor's map (the fixture observes that routing boundary). Retain the configured
portal bindings and pointed-to country/obelisk storage through nested callbacks.
No canonical interface change is needed for this composition. Production
configuration, map routing, raw pool preparation, calendar/KC and other world
providers remain open. SQL is unchanged; final isolated SQL checks remain
coordinator-owned. This evidence does not establish client playability.

## Forced removal/reset through real portals — suppression-order evidence

Evidence/test commit: `b6c81d2d5cc688e82fe3c41df254584ea66e19fd`.

`obelisk-runtime-reset-portal-unit` extends the real zone/pool composition with
actual portal ownership during both direct forced removal and map reset. The
original executes **ps_game.exe, CMap::ObeliskReset 0042aa40**, **CZone::
MobRemoveByID 0041e910**, nested **ObeliskDestroy 0042a410**, and actual
**UpdatePortalLink 0042a000 / SendPortalLinkChange 0042b2c0**. Native comparison
uses the existing concrete forced-removal owner and actual reset/portal/zone/
pool implementations. No production implementation changed in this block.

The important integration contract is now composed rather than inferred from
separate tests: **0042aa81** sets reset_send before nested removal,
**0042aa95** clears it after removal, and **0042ab27** performs the final portal
update after base-country restoration and reset publication. Obelisk change
suppression must not suppress the nested portal DB/map/world publications.
Nested portal transport callbacks may change the actual last-destroy country;
subsequent reset writes and the final portal update must observe live storage.
Existing tests' initial reset_send=true direct-removal cases also remain live.

256 retained sequences comprise 126 map resets and130 direct forced removals,
1,536 operations, 297 actual portal update entries (124 with reset_send=true),
116 portal DB changes, 84 target-map and116 global portal publications, and
7,168 portal snapshots. 47 target-map portal packets occur while reset_send is
true, across41 sequences. The same traces retain 768 actual actor recycles and
512 pooled reuses, duplicate/stale companion slots, wrapping IDs/deadlines,
first-match ownership and final pool identity. Four configured portal links
remain explicit inputs; heap/locks/clocks/cleanup/transports are observed ports.
This does not establish loader/network/provider completion.

Validation:

- `python3 tools/dev.py build`: exit0, no warnings
  (`work/obelisk-reset-portal-build.log`). Shared build-slot wait was respected.
- `python3 tools/dev.py test`: exit0, **131/131 passed**, 34.86 seconds
  (`work/obelisk-reset-portal-tests.log`), including earlier forced-removal,
  combat-death, standalone portal and composed death/portal regressions.
- Independent original regeneration and `cmp`: exit0, byte-identical fixture
  (`work/obelisk-runtime-reset-portal-repeat.tsv`).
- All original EXE/PDB SHA256SUMS passed
  (`work/obelisk-reset-portal-hashes.log`); `git diff --check`: clean.
- Existing focused Ghidra evidence was reused; no new symbol recovery or
  separate SQL integration was needed for this composition-only change.

Integration: bind the real forced-removal owner as world.remove_by_id, and bind
world.update_portal_links to the retained global portal provider. Preserve the
synchronous nested call while reset_send is set. Apply suppression at the
recovered obelisk publication gate, not to all DB/map/world traffic. Route
portal map packets to each link's configured target map. No canonical interface
change is required for this evidence block. Production config/source retention,
raw pool allocation/preparation, platform calendar/KC and transport services
remain dependencies from prior blocks. SQL is unchanged; final isolated SQL
integration remains coordinator-owned. The server is not playable.

## Stable configuration materialization from parsed obelisk records

Implementation commit: `46416f8cbacaba88355eb2fa16f512c5f6f36765`.

`ObeliskRuntimeConfiguration::bind` recovers the ordinary obelisk materialization
phase of **ps_game.exe, CMap::LoadObelisk 00427300**. It accepts explicit parsed
stObeliskZone/stObelisk/stObeliskMob projections, map sizes and actual canonical
MobRuntimeTemplates. It owns stable map/record/group/member backing and binds
companion respawn sources to actual group addresses. Copying is disabled;
moving preserves the nested vector storage. Configuration must remain retained
and unmodified while mobs, portal links and death/removal adapters reference it.
Text parsing, portal/linked-group configuration and allocation-failure emulation
are not claimed by this API.

File definitions are reordered neutral/light/fury -> light/fury/neutral, with
actual template find validity (ID/HP/level). Countries1/2 become0/1; all other
values become neutral2. IDs consume the retained global next-ID counter even
when positions later invalidate spawning (**0042814b–00428160**). Principal
definitions are marked in shared source storage before the x/z position guard
(**004281b1/004281f0/0042822e**, byte offset0x7c); invalid positions clear only
runtime definition references. Valid positions increment the retained total
(**0042829c**), even with zero groups. Zero groups leave the principal respawn
zeroed; nonempty groups set its index/count/obelisk flag. Group members start
null and group respawn count/source are bound. Principal flags do not propagate
to companion definitions. Radius conversion truncates to int first, converts
back to float, doubles, then truncates again (**00428493–004284d6**), preserving
negative/fractional and overflow behavior. Positive retained total updates KC
amounts using signed maximum/total+1 (**00428534–00428575**); nonpositive total
preserves prior amounts.

The original oracle runs the complete original parser/materializer/constructors
on generated valid INI input; native comparison starts from the equivalent
explicit parsed input. 64 cases compare all projected record/respawn fields,
null member slots, resolved definition identities, shared definition marking,
ID/total/rate state, zero groups, out-of-map positions, negative/invalid IDs,
invalid template level, wrapped IDs and fractional/overflowing radii. Native
configuration is moved before comparison to verify retained source pointers.
File/heap/CRT/TLS/locks/cookie and exact temporary-array resets are observed
execution boundaries. No native parser equivalence is implied.

Validation:

- `python3 tools/dev.py build`: exit0, no compiler warnings
  (`work/obelisk-config-build.log`).
- `python3 tools/dev.py test`: exit0, **132/132 passed**, 37.82 seconds
  (`work/obelisk-config-tests.log`).
- Independent full original loader regeneration and `cmp`: exit0, byte-identical
  retained fixture (`work/obelisk-runtime-config-repeat.tsv`).
- Locked focused Ghidra export: exit0 (`work/obelisk-config-ghidra.log`).
- All original EXE/PDB SHA256SUMS passed (`work/obelisk-config-hashes.log`);
  `git diff --check`: clean. No SQL behavior changed or lifecycle action ran.

Integration: provide the real parser's completed raw records and retain this
configuration beside the actual maps and shared template owner. Required
`ObeliskRuntimeDefinitionOwner::mark_obelisk` must mutate the shared definition's
original bObelisk flag; it cannot default to a no-op. The current canonical
MobRuntimeTemplate projection has no such field, so the coordinator must bind
this mutation to the retained full definition owner (or add a canonical flag/
setter with evidence at stMOBInfo+0x7c). No canonical file was edited here.
The loader's top-KC thresholds and retained counters are explicit state inputs,
not fabricated defaults. Full text loading, portal binding, raw pool preparation
and world assembly remain open. SQL is unchanged; final isolated SQL validation
remains coordinator-owned. The server is not playable.

## Ordinary obelisk text parser and materialization composition

Implementation commit: `3c576f5a4575d0a8219752df77f38d875477d1c0`.

`parse_obelisk_runtime_configuration` supplies the missing ordinary-obelisk
parser from **ps_game.exe, CMap::LoadObelisk 00427300**. Its output feeds the
existing stable configuration binder. It recovers case-insensitive zone/
obelisk/mob labels, mode changes at Portal/BothMob headers, delimiter/numeric
rules, wrapped base IDs, partial writes into uncommitted record/group slots,
and commit only after the final required token. Portal and linked-group
contents remain separate providers; this parser only observes their mode gates.
Fixed original capacities (64 zones/records/groups) have explicit unsupported
bounds errors instead of reproducing memory corruption.

Windows text input semantics include CRLF translation, Ctrl-Z termination,
8191-byte chunks and embedded NUL handling. The original loop ignores failed
fgets and reexamines the tokenized last buffer (**00427ed4–00427eef**). Most
records cannot recommit because only their first token remains, but a final
newline-terminated zone header needs only that token: it creates a second empty
zone. A header without the final newline creates one. Native behavior retains
this distinction. Empty-file fixtures explicitly zero the original stack buffer;
arbitrary uninitialized original stack contents are not an equivalence claim.
Numeric fixtures cover signed/wrapped integers, decimal D exponents and original
non-finite/hex-prefix handling rather than accepting host atof extensions.

45 original parser fixtures stop at **00427eef**, before materialization mutates
temporary country fields. Original strtok/atoi/atof execute; file/CRT/TLS/locks/
cookie and exact temporary-array resets remain observed boundaries. Cases cover
comments, mixed case/whitespace, repeated zones, section switches, truncated
parent/group retries, pending group retention, unknown lines, EOF variants and
64 groups. A separate native overflow check rejects a65th group. A second test
composes text->native parser->actual native binder against the prior64 complete
original loader/materialization scenarios, retaining shared-definition marking
and stable source-pointer checks. No source-template or world owner is duplicated.

Validation:

- Final `python3 tools/dev.py build`: exit0, no warnings
  (`work/obelisk-config-parser-final-build.log`). Shared-slot waits respected.
- Final `python3 tools/dev.py test`: exit0, **134/134 passed**, 35.50 seconds
  (`work/obelisk-config-parser-final-tests.log`). The earlier43-case iteration
  also passed134/134 in40.89 seconds; final fixture expansion was validated anew.
- Independent original parser regeneration/`cmp`: exit0, byte-identical45 cases
  (`work/obelisk-runtime-config-parser-final-repeat.tsv`). Previous complete
  configuration fixture regeneration still matches byte-for-byte
  (`work/obelisk-runtime-config-parser-regression.tsv`).
- All original EXE/PDB SHA256SUMS passed
  (`work/obelisk-config-parser-final-hashes.log`); `git diff --check`: clean.
- Existing focused Ghidra LoadObelisk export was reused; no SQL behavior changed
  or SQL lifecycle action ran.

Integration: feed complete Obelisk.ini bytes to this parser, then pass its result
to `ObeliskRuntimeConfiguration::bind` with actual map sizes, retained templates,
shared bObelisk mutation provider and original KC/counter state. Keep the owner
alive through startup, respawn, death, removal and portal callbacks. Remaining
configuration work is portal-link parsing/binding and recovered asset-wide
comparison, plus production root-world assembly. The required shared definition
flag setter and raw pool preparation changes from prior blocks remain canonical
owner dependencies; worker03 now exclusively owns mobs/mob_pool and was not
edited. SQL is unchanged; final isolated SQL validation remains coordinator-owned.
The server is not playable.

## Stable portal configuration binding and target-map routing

Implementation commit: `ea242b556bbe59b4e8736eba667e6ac41378ed58`.

`ObeliskRuntimePortalConfiguration::bind` recovers the portal materialization
phase of **ps_game.exe, CMap::LoadObelisk 00427300**, beginning at **00428a94**.
Explicit completed portal inputs carry the already decorated base ID, target
map/portal and two lists of four (map, one-based ordinal) references. The binder
uses retained map portal targets and actual stable obelisk spans. It owns the
compacted live runtime link table and a matching target-map routing table for
`ObeliskRuntimePortalWorld::map_broadcast(index,packet)`. Target countries and
obelisk records are referenced directly, not copied.

It chooses the first matching target portal, even when target IDs duplicate.
Missing targets/maps do not consume IDs. A found target consumes the retained
global ID before reference binding, even if no valid references remain and the
candidate is discarded. Valid references compact independently into each
country's four-entry array; ordinal1 selects the first actual runtime record,
not an obelisk whose base ID is1. Repeated references remain repeated. Open flags
start false, target-zone presence is explicit and each runtime portal ID retains
the original WORD publication projection. Runtime counts and pointer sources
are ready for the recovered update/publication/ownership functions.

The unsafe original inclusive one-past record lookup is explicitly rejected;
negative map accesses are safely skipped rather than dereferencing before the
map table. Those malformed memory accesses are not equivalence claims. Missing
country backing for an otherwise found portal fails explicitly. No required
world service is replaced with a no-op.

64 retained full original LoadObelisk executions compare source ID state, runtime
link order, IDs, first-match target country, map routing/zone state, both counts,
open flags and all eight reference identities. Six ordinary obelisks are loaded
first with base IDs100/101, proving ordinal resolution is independent of base ID.
The fixtures consume120 portal IDs but retain101 links, with14 empty resulting
configurations. Cases include missing maps/portal IDs, sparse/duplicate references,
references above count, zero ordinals and global ID wrap. Initial map/portal and
completed native input projections are explicit; original text parsing executes
but native portal text parsing is not claimed. Moving native configuration is
also checked. A separate native boundary case verifies one-past rejection after
ID consumption.

Validation:

- `python3 tools/dev.py build`: exit0, no warnings
  (`work/obelisk-portal-config-build.log`).
- `python3 tools/dev.py test`: exit0, **135/135 passed**, 28.29 seconds
  (`work/obelisk-portal-config-tests.log`).
- Independent original regeneration/`cmp`: exit0, byte-identical64 cases
  (`work/obelisk-runtime-portal-config-repeat.tsv`).
- Locked focused Ghidra export: exit0 (`work/obelisk-portal-config-ghidra.log`).
- All original EXE/PDB SHA256SUMS passed
  (`work/obelisk-portal-config-hashes.log`); `git diff --check`: clean.

Integration: retain maps/portal country fields and ordinary obelisk configuration
before binding portals, pass the same next-ID state used by ordinary obelisks,
and retain this table throughout nested ownership/death/reset/publication calls.
Use `target_map(index)` to route each map packet to the configured target map.
Keep each runtime zone_present projection current as target-map zones are
created/destroyed; binding does not substitute the actual map lifecycle owner.
Next independent work is native portal-header/light/fury parsing into these
completed inputs and recovered asset-wide comparison. Existing shared bObelisk
setter/raw pool preparation and root-world/KC/calendar/transport dependencies
remain open; canonical mobs/mob_pool belong exclusively to worker03 and were not
edited. No canonical interface change is required for this binder. SQL is
unchanged; final isolated SQL validation remains coordinator-owned. The server
is not playable.

## Portal/light/fury text parsing and binding composition

Implementation commit: `204047671f562ae7f1b8183d10a6d297dceeacdd`.

`parse_obelisk_runtime_portal_configuration` completes the native portal parser
from **ps_game.exe, CMap::LoadObelisk 00427300**. It uses the same recovered
numeric/token helpers as the ordinary obelisk parser and outputs the existing
binder's completed portal inputs. Headers decorate the wrapped base ID with
0x10000000 and commit only after all three fields. A truncated header retains
the previous active portal; later light/fury writes can therefore still change
that earlier completed record. Each reference writes its map before validating
the Obelisk_ label, preserving the old ordinal and trailing entries after a
failed/incomplete replacement. Lists retain at most four pairs per side.

The original compares six characters for `[fury]` but advances seven characters
before tokenization; `[fury]12,Obelisk_9` consequently reads map2. Light advances
seven after its seven-character label and reads map12. Mixed-case labels, decimal
integer wrapping, ignored extra references, mode changes, CRLF, Ctrl-Z, embedded
NUL and 8191-byte chunks are preserved. EOF retries cannot commit another portal
header or reference and only revisit the same first field. An explicit bound
rejects a65th raw portal row instead of reproducing out-of-array writes.

50 retained original parser scenarios stop at **00427eef**, before the loader
compacts or mutates temporary configuration. Original strtok/atoi execute;
completed raw stPortalLink records (76 bytes each) are compared. Cases include
partial/repeated headers/references, invalid labels, previous-active mutation,
all64 rows, section gating and the fury offset difference. The initial stack is
explicitly zeroed for deterministic empty input. Native text->parser->actual
binder additionally compares all64 existing full original portal-binding cases,
including ID gaps/wrap, duplicate target IDs, compacted ordinal references and
actual target-map routing. Native bounds rejection is checked separately; no
claim is made about original malformed-memory behavior.

The first full run failed135/137 tests (31.09 seconds): parser scenario3 and
binding scenario1 rejected valid references. The native prefix helper normalizes
input to lowercase; its new Obelisk_ comparison incorrectly supplied an uppercase
pattern. Using the lowercase pattern corrected native recognition. Original
fixtures were unchanged. The initial build also reported a misleading-indentation
warning, fixed before final validation. Both original logs remain retained.

Validation:

- Corrected `python3 tools/dev.py build`: exit0, no warnings
  (`work/obelisk-portal-parser-final-build.log`).
- Corrected `python3 tools/dev.py test`: exit0, **137/137 passed**, 28.28 seconds
  (`work/obelisk-portal-parser-final-tests.log`). Initial failure/warning logs:
  `work/obelisk-portal-parser-{build,tests}.log`.
- Independent original regeneration/`cmp`: exit0, byte-identical50 cases
  (`work/obelisk-runtime-portal-config-parser-repeat.tsv`). Ordinary parser and
  prior full portal-binding fixtures remain byte-identical
  (`work/obelisk-runtime-portal-parser-{ordinary,binding}-regression.tsv`).
- All original EXE/PDB SHA256SUMS passed (`work/obelisk-portal-parser-hashes.log`);
  `git diff --check`: clean. Existing focused Ghidra LoadObelisk export reused.
- No SQL behavior changed or SQL lifecycle action ran.

Integration: feed the same complete Obelisk.ini bytes to the ordinary and portal
parsers, bind ordinary records first, then portal references with the shared
next-ID state and stable actual map/portal storage. Retain the map routing table
and keep runtime zone-presence projections current through real map lifecycle.
The runtime update/ownership/publication providers can now consume the native
parsed-and-bound portal table. Next independent validation is comparison against
the recovered complete asset and live map/template owner assembly. Shared
bObelisk flag mutation, raw pool preparation and root-world/KC/calendar/transport
providers remain dependencies from prior blocks. Canonical files and SQL were
unchanged; final isolated SQL validation remains coordinator-owned. The server
is not playable.

## Complete recovered Obelisk.ini parser comparison

Evidence/test commit: `048ead20c5993c8ab9aa1de5477f2f9a4f23fe71`.

The new `obelisk-runtime-asset-unit` compares both native parsers against
**ps_game.exe, CMap::LoadObelisk 00427300**, stopping at **00427eef** before
materialization changes temporary country fields or filters records by maps.
This block adds complete recovered-asset evidence; production code is unchanged.
The same original CRT token/numeric execution and explicitly zeroed initial
stack contract from the parser fixtures are used. Original file/CRT/TLS/lock/
cookie and exact temporary-array reset boundaries remain observed ports.

Input is the private `.local/server-data/Obelisk.ini`, **20,295 bytes**, SHA256
`e3c72654021dddf318b6a2cdee48055ddf2e58ed5cef2b29c44d1f2b8db62164`.
The tracked fixture contains normalized numeric tables and asset identity, not
private file bytes. The test requires the private file, checks byte count/FNV64
against the original fixture and fails explicitly when missing or different.
Original generation separately records its SHA256. No asset was altered.

The original parser produces three temporary zone buckets, mapped to0/18/30,
nine obelisks (7/1/1), 160 companion groups and one portal input. These are the
complete byte stream's results, including uncommented example/documentation
lines; the native path does not clean or reinterpret the recovered asset.
All1,368 ordinary-table DWORDs and20 portal-table DWORDs are compared, including
record order, base IDs, country values, float bit patterns, all three mob IDs,
group counts/radii/member counts and all portal ordinal/map pairs. This closes
the prior synthetic-only parser validation gap, not runtime world assembly.

Validation:

- `python3 tools/dev.py build`: exit0, no compiler warnings
  (`work/obelisk-asset-build.log`).
- `python3 tools/dev.py test`: exit0, **138/138 passed**, 27.20 seconds
  (`work/obelisk-asset-tests.log`), including complete private asset comparison.
- Independent original generation/regeneration and `cmp`: exit0, byte-identical
  (`work/obelisk-runtime-asset-repeat.tsv`).
- All original EXE/PDB SHA256SUMS passed (`work/obelisk-asset-hashes.log`);
  `git diff --check`: clean. No SQL lifecycle action ran.

Integration: this test now requires the private recovered asset in its normal
`.local/server-data` location. Keep it outside version control. The verified
parsers can feed the existing retained ordinary/portal configuration binders;
production must still supply actual map sizes/portal targets, shared live mob
definitions and bObelisk mutation, shared next-ID/KC state, correct map lifecycle
routing, raw pool preparation and world services. Complete asset parsing does
not prove those records are spawnable with production data or establish client
playability. Existing focused Ghidra LoadObelisk evidence is reused. Canonical
files and SQL are unchanged; final isolated SQL validation remains coordinator-
owned. The server is not playable.

## Concrete shared definition-record flag provider

Implementation commit: `9feae1092ccc6842c4f85ebb2436ae3d464537e2`.

`ObeliskRuntimeDefinitionRecords` implements the configuration binder's required
`ObeliskRuntimeDefinitionOwner` using a mandatory live mutable-record accessor.
It resolves the actual retained 268-byte stMOBInfo by the admitted template ID
and sets byte0x7c to1. Evidence: **ps_game.exe, CMap::LoadObelisk 00427300**,
writes **004281b1 / 004281f0 / 0042822e**. It changes no other byte, creates no
second catalog and never substitutes an independent marking bitmap.

The real canonical catalog already exposes `mutable_mob_record(id)`, so production
can bind `ObeliskRuntimeDefinitionRecords([&catalog](uint32_t id) { return
catalog.mutable_mob_record(id); })` without a canonical field/setter change.
The empty accessor fails at construction. Wrong record length, zero/mismatched
ID or invalid raw level/HP fail before mutation, exposing mismatched slots or
invalid admission. The caller still guarantees actual owner identity; equal IDs
in an unrelated copied catalog cannot be distinguished by these checks. Access and
configuration must be serialized with catalog reload/publication; retain the
actual owner/backing records for all users. This raw flag does not require
republishing unrelated targeting/skill views.

The existing configuration comparison now uses this production provider over
explicit mutable full-record inputs instead of its old observed marking bitmap.
All64 original full loader materialization cases and64 text->parser->binder
compositions compare the source flag results. They include valid/invalid template
IDs and marking before an invalid-position record clears runtime definitions.
Sentinel bytes additionally verify that all267 non-flag bytes stay unchanged;
required-owner absence and stale ID rejection are checked separately. The
original fixture is unchanged and independently regenerated. Actual canonical
catalog construction/SQL loading remains outside this worker's older checkout;
the new provider's direct live-record mutation is verified, not a claim of
production root catalog assembly.

Validation:

- Final `python3 tools/dev.py build`: exit0, no warnings
  (`work/obelisk-definition-records-final-build.log`); shared-slot wait respected.
- Final `python3 tools/dev.py test`: exit0, **138/138 passed**, 32.18 seconds
  (`work/obelisk-definition-records-final-tests.log`), including the added
  required-owner/stale-ID guards. Earlier suite before those extra guard checks
  passed138/138 in43.24 seconds (`work/obelisk-definition-records-tests.log`).
- Independent original full loader regeneration/`cmp`: exit0, unchanged retained
  fixture (`work/obelisk-runtime-definition-records-original.tsv`).
- All original EXE/PDB SHA256SUMS passed
  (`work/obelisk-definition-records-hashes.log`); `git diff --check`: clean.
- Existing focused Ghidra LoadObelisk evidence reused; no SQL behavior changed
  or SQL lifecycle action ran.

Integration: the earlier requested canonical bObelisk setter is no longer a
blocker because the retained normalized catalog's existing mutable-record API
can supply this required provider directly. Ordinary templates and raw records
must describe the same admitted definitions; mismatches fail explicitly. Bind
this provider before startup configuration, retain the shared catalog, then
bind portals over stable runtime records. Actual ServerMaps currently exposes
only const map definitions; mutable retained portal-country access/real map
lifecycle routing still needs coordinator-owned assembly. Raw allocation/
preparation remains worker03's exclusive canonical task. Root-world/KC/calendar/
transport providers remain open. No canonical files or SQL changed; final
isolated SQL checks remain coordinator-owned. The server is not playable.

## Initial portal persistence list — fixed country and live chunk reads

Implementation commit: `528926fc31e9be32313b5e0167a8fa28e9f25ea2`.

`publish_obelisk_portal_database_list` recovers **ps_game.exe,
CMap::SendDBPortalLinkList 0042b460**. It sends opcode0206 with a BYTE record
count and nine bytes per row: live DWORD base ID, **constant country byte3**
(**0042b497**) and zero DWORD timestamp. This must not reuse the individual
portal change serializer, whose country is live. No target-country/portal/map
zone/open-flag read or guard belongs to the initial list function.

Rows are sent in groups of100 (903-byte packets), followed by a nonempty partial
packet; empty input sends nothing and an exact100 multiple has no empty tail.
Base IDs after a full-chunk transport callback are read live. Caller/provider
must preserve the table/storage and count while publishing; replacement of the
original global table or live count changes are outside this stable-span API.
The mandatory DB service remains supplied by ObeliskRuntimePortalWorld.

96 original execution fixtures compare168 packets and11,768 records. Counts
cycle0/1/2/12/99/100/101/199/200/201/255/301; alternate blocks mutate all later
base IDs during the first DB callback. Original target pointers include null,
0xdeadbeef and0xffffffff without faults, proving the source never dereferences
them. Native cases use absent targets and invalid country values and reject
unexpected map/world service calls. Large table cases stress this serializer
only: they do not remove UpdatePortalLink's separate twelve-flag safety bound.
Only DB transport is observed; the complete original serializer/loop executes.

Validation:

- `python3 tools/dev.py build`: exit0, no warnings
  (`work/obelisk-portal-database-build.log`).
- `python3 tools/dev.py test`: exit0, **139/139 passed**, 34.77 seconds
  (`work/obelisk-portal-database-tests.log`).
- Independent original regeneration/`cmp`: exit0, byte-identical96 cases
  (`work/obelisk-runtime-portal-database-repeat.tsv`).
- Locked focused Ghidra export: exit0 (`work/obelisk-portal-database-ghidra.log`).
- All original EXE/PDB SHA256SUMS passed
  (`work/obelisk-portal-database-hashes.log`); `git diff --check`: clean.

Integration: **CWorld::SendDBObeliskList 00413b90** calls the portal list first,
then each map's ordinary obelisk list. Preserve this order when connecting the
two recovered publishers to the real DB connection. Do not normalize initial
portal country3 to current country or suppress it based on map zone presence.
No canonical interface change is needed. Worker03 now owns the concrete actual
pool/init/zone ObeliskRuntimeWorld adapter assignment, so that work was not
duplicated. Mutable retained ServerMaps portal-country/lifecycle routing and
root-world/KC/calendar/transport assembly remain dependencies from prior blocks.
Canonical files and SQL were unchanged; final isolated SQL validation remains
coordinator-owned. The server is not playable.

## Direct user scene list — mixed ordinary/portal chunks

Implementation commit: `dd8daaeca4c7b56adb852ecbba27ac57b7202a46`.

`publish_obelisk_user_list` recovers **ps_game.exe, CMap::SendObeliskList
0042b040**. It sends opcode2101 directly to a required selected-user transport,
with BYTE count and13-byte rows: full DWORD runtime ID, BYTE display country,
raw float X/Z words. Ordinary map records come first (country0->1,1->2,other0),
then global portal links whose target-map identity equals the calling map
(country0->103,1->101,2->102,other100). Y is not published. No map-zone-presence
check gates this function.

Both phases share the same100-row chunk buffer: ordinary rows are not flushed
merely because the portal phase begins. Full chunks are1303 bytes; only a
nonempty remainder is sent. Empty scenes send nothing. Native portal scene
views borrow the actual target-map identity, stable runtime link/country and
live target position. Nonmatching maps are filtered before dereferencing target
backing; matching rows without required backing fail explicitly. Callback
mutations of later values/map selection are read live. Storage and table counts
must remain stable; replacing the original global table/count during sends is
outside this stable-span API.

192 original executions compare334 packets and21,835 rows;92 packets mix both
record kinds. Cases cover0/1/2/99/100/101/199/200 ordinary records and
0/1/2/12/100/101 portals, all versus alternating target-map matches, signed/extreme
country values, full DWORD IDs, distinct X/Y/Z values and exact chunk boundaries.
The first user-send callback in alternate blocks changes later ordinary rows
and live portal IDs/countries/coordinates/map selection. The original function
runs with a null map-zone pointer, proving that the separate change publication
zone gate must not be applied here. Only the selected user's SConnection::Send
**004d4d20** is observed, and its connection identity is checked. Large portal
cases stress this serializer, not UpdatePortalLink's separate12-flag bound.
A native filtered-null-target case also verifies filtering precedes backing
validation. No actual user-zone entry dispatch or network delivery is claimed.

Validation:

- `python3 tools/dev.py build`: exit0, no warnings (`work/obelisk-user-list-build.log`);
  shared resource waits respected.
- `python3 tools/dev.py test`: exit0, **140/140 passed**, 30.68 seconds
  (`work/obelisk-user-list-tests.log`).
- Independent original regeneration/`cmp`: exit0, byte-identical192 cases
  (`work/obelisk-runtime-user-list-repeat.tsv`).
- Locked focused Ghidra export: exit0 (`work/obelisk-user-list-ghidra.log`).
- All original EXE/PDB SHA256SUMS passed (`work/obelisk-user-list-hashes.log`);
  `git diff --check`: clean. No SQL lifecycle action ran.

Integration: at the original user scene publication point, bind send_user to
that user's real connection. Pass ordinary records from the retained calling
map; construct portal views using each configuration target_map(index) identity
and actual MapPortal.position, plus its stable runtime link. Keep the real map/
portal/connection owners alive across synchronous transport callbacks. This
requires no canonical interface change, but actual ServerMaps mutable portal
country/lifecycle and root user/world routing remain coordinator assembly work.
Worker03's pool/init/zone adapter assignment was not duplicated. KC/calendar and
other mandatory world services remain open. Canonical files and SQL were
unchanged; final isolated SQL validation remains coordinator-owned. The server
is not playable.
