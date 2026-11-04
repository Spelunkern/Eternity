# Worker 02: mob drops

Status: loot algorithms, catalogs, selective reload, creation and eligibility are
reconstructed. Delivery connects actual pool/Actor/ZoneItems owners. Production
death/world routing and required services remain open; the server is not playable.
Only owned files and private CMake registrations were changed.

## Evidence and prior blocks

Original `ps_game.exe` SHA256:
`0fb7c9f13b5bcf186118f8bfe73ab214226b93e8aa502b8e6fded4a8312bc547`.
All twelve EXE/PDB hashes verified again. Focused Ghidra exports use locked
`--process --output work/exports/02-mob-drops`; generated evidence stays ignored.

| Implementation | Original source / completed behavior |
| --- | --- |
| ad3247b | ItemDrop 004a5ea0/004a65a0 selection/distribution; LoadConstMobItem 004c61f0 |
| 3b1e120 | CItemDrop::Init/InitLimit 004bde50/004be3e0, ordered grade population/limits |
| 52ab268 | Sequence init/reload 004be520/004be6a0, retained groups and 32-bit grades |
| 058df95 | Creation 004a662a..004a66cc / money 004a645b..004a64cc, actual pool/init/craft |
| b2f12a9 | Range/zone 004a64f0/004a6560, Distance2 0041b1f0, distinct money gate and NaN acceptance |
| 050179d | Concrete delivery through real Actor::receive_item 0045df40 and ZoneItems::enter 00417320 |

Milestone 7 implementation: `940a1f986fa4e92de56e9f0f169bbbf5e0fa4a04`.
Current UID implementation commit: `5f940275104dc1dfd6d38ca4d91c9ed7a50ffa5f`.

## Milestone 7: selective MobItems reload and live slot binding

Evidence: `CGameData::ReLoadConstMobItem` 004c2e70, write block
004c3043..004c3105, original `usp_Read_MobItems_R2` query. Focused Ghidra export
and immutable EXE/PDB hash verification completed again.

`mob_drops_reload.hpp/cpp` updates existing MobDropDefinitions in place. All four
columns must be read before a row is published. Missing rows do not clear slots;
invalid mob/order rows report diagnostics and continue. Grade then rate each
increment the caller's cumulative change counter only when changed, preserving
DWORD signed wrap. Empty/idempotent results succeed without resetting that
counter. Earlier writes survive later read/conversion failure. Storage is never
resized and initial-load rejection metadata is retained.

`reload_mob_drop_sql` uses the supplied native SqlServer owner and original
four-column selective procedure. It performs no database/pool lifecycle operations,
reconnect/reset, transaction changes or schema writes. Diagnostics are mandatory.
A failed query reports failure with unchanged state; result rows convert lazily,
so conversion failure preserves earlier rows. NULL/unsupported conversions are
explicitly rejected instead of guessing original uninitialized-buffer behavior.

`MobLootContext::live_slots` borrows the retained original table record. The loot
coordinator rereads that storage between callbacks; retaining a copied slot array
would hide successful reloads. The old value projection remains available for
explicit standalone fixtures. Production should bind the borrowed record.

Validation:

- 512 complete original reload executions: exact row comparisons/writes, cumulative
  counter, diagnostics and final table. Includes empty/duplicate/invalid rows,
  all four GetData failure positions, query failures, signed boundaries and 383
  partial/query failures. Profile/credential/DB-pool/query/GetData/logger/cookie
  responses are observed ports; actual reload control/write bodies execute.
- Original full ItemDrop callback installs a known two-field reload effect;
  native callback invokes the actual reload engine. Later slot reads select
  grade nine and suppress slot three, with matching CRT consumption and stable
  borrowed storage. This is composition evidence, not a claim that the original
  callback executed a nested database query.
- Read-only isolated SQL/original/native comparison: five selective requests,
  45 rows, 2,747 retained definitions / 27,470 slots. Cumulative counter 17->107;
  all five full-table/counter hashes match original row-write execution. A second
  identical reload per request leaves the counter unchanged. Shared SQL lock held;
  all snapshots stay ignored under work/. Only hash references are tracked.
  Log: `work/mob-drops-reload-sql-final.log`.
- Final native build: exit 0, no warnings; `work/mob-drops-reload-final-build.log`.
  Initial build rejected a mixed-type auto declaration in the SQL test; split
  declarations corrected it before validation. Initial failure is not a pass.
- `python3 tools/dev.py test`: exit 0, 129/129 passed in 32.77 seconds.
  Log: `work/mob-drops-reload-test.log`.
- New Python oracles pass `py_compile`; `git diff --check` passes.

Important native SQL boundary: SqlServer::query buffers its entire result, so a
transport/fetch error occurs before any row publication. `MobDropReloadCursor`
already supports the original streaming failure semantics, but exact production
mid-fetch publication requires a canonical statement/cursor interface delivering
Row/End/Error after four typed column reads while retaining statement lifetime.
That shared storage interface was not edited. Normal restored procedure results
and lazy conversion failures are verified; no streaming error equivalence claimed.

## Retained delivery/creation evidence

Concrete delivery uses one executor CRT source across position/craft/party draw,
a real shared GroundItemPool, canonical ItemDefinition and actual Actor admission
or ZoneItems publication. Sequence mark='V' precedes required logging. Per-call
handles survive recursive delivery. Results distinguish none/allocation failure,
inventory, ground and retained unlinked allocation; inventory receipts include
stored/merged/returned and expose no caller-owned item pointer after admission.

Prior comparison: 512 complete original drops, 41 real inventory admissions,
431 ground publications; real DB/client/log packet effects, nested delivery,
merged/recycled storage, 0707 count update, full-inventory return, money placement,
expiry/reuse and rejected geometry. Restored SQL craft comparison retains 16,047
definitions / 9,430 eligible / 133,142 draws, hash `14191292995060048443`.
Earlier commit reports retain detailed fixture limits and corrected harness runs.

## Required production integration and remaining work

- Bind MobDeathWorld::drop_items to mob_loot, then deliver_mob_drop / ground-money
  placement. Keep the outer party lock, mob-owned ServerRandom and shared CRT.
- Bind `live_slots` to the retained `definitions.mobs[mob_id]` record; serialize
  reload/read access under the definition owner and never resize borrowed storage.
  Use MobMaxID+1. Other live mob-template fields still require their owner bindings.
- Mob templates need money +28/+2a and quest type/type-ID +2c/+2d; level +22,
  object ID +5c and mob type +220 feed sequence/drop metadata. User/party views
  need mode +104, leader +0c, cursor +108, type +114, members +18, zone +e0,
  instance +164, state +5758, dead +142c, X/Z +d0/+d8, money +58b0/+58b4.
- Resolve retained Actor/ItemDefinition owners. InventoryWorld must capture and
  revalidate the same live party with compatible recursive locking. Actual
  transports/return owners/log sinks remain required; exceptions imply no rollback.
- Shared original UID production ownership is still a coordinator dependency;
  the generator algorithm is corrected in milestone 8 below. All producers must
  use the same instance. Add server ID only once.
- Zone identity means live CZone identity, not map number; never truncate host
  pointers. Retain catalog entries and all eight sequence backing groups; publish
  after every sequence reload attempt, including failure. Unsupported INI syntax,
  original negative-grade indexing and SQL NULL buffers are not guessed.
- Real quest links/ownership, guild ranking/revalidation, party money grants,
  top-level dispatch and logging remain required. Both-mob events 7/8 and sequence
  0a06 stay with the coordinator's logging owner. No required service defaults to
  a no-op. Milestone 8 changes only the explicitly authorized shared ItemIds
  header/source/tests in this worker. No SQL fixture or main checkout was modified.

## Historical continuation checkpoint (superseded by UID assignment)

Checkpoint `5652e56` recorded production dependencies against main `b19f469`.
The coordinator then explicitly assigned existing canonical ItemIds/header/tests
plus new mob_drops_uid evidence to this worker. This resolves the algorithm/API
part of that dependency; no main checkout or another owner's source was edited.

## Milestone 8: original shared ItemIds / GenID semantics

Evidence: ps_game.exe `SUniqueIDGen::SUniqueIDGen` 00402550,
`SUniqueIDGenByServer` constructor 004011c0, global startup 0051fb60,
`SetStartID` 004494d0, `GenID` 004cbe60, server adapter 00457a50 and local
calendar packer (misleading symbol GetSystemTime) 004cb8e0. Global item state
is 00547ce0: counter +4, refresh baseline +8, DWORD deadline +0c, packed local
calendar +10, critical section +14. All twelve immutable binary/PDB hashes pass.
Focused locked Ghidra export completed; log `work/mob-drops-uid-ghidra.log`.

Existing ItemIds now owns that state directly. Construction zeroes counter,
baseline and deadline and samples local calendar once. GenID locks before tick
sampling, refreshes iff unsigned tick >= absolute DWORD deadline, sets wrapped
`tick+2000`, restores baseline, then samples packed local calendar. Every call
increments the DWORD counter; the returned low DWORD is counter shifted left 16,
so no exception occurs at 65535 or full DWORD overflow. Zero stamp does not force
refresh. SetStartID only changes the next-refresh baseline. Original local WORD
calendar arithmetic wraps modulo DWORD; it does not use the narrower database
world-calendar year validation. Constructor/refresh years 1999/2032/2064 and
injected zero calendar state are covered. Zero SYSTEMTIME fixture is an explicit
OS-response projection, not a claim that the platform emits invalid dates.

API/caller migration:

- Existing ItemIds(server), next(CreationTime), CreationTime and creation_time
  remain source compatible; all existing callers compile. Snapshot ticks use
  their low DWORD. Snapshot sampling already happened outside the lock, so that
  adapter cannot reproduce the original clock-sampling placement under contention.
- Runtime owners should inject one retained ItemIdClock into one ItemIds and use
  next() (server byte added once) or raw_next() (original GenID without server).
  The clock must outlive the generator. Both adapters share exactly one counter,
  baseline, deadline and recursive lock. Do not configure a second server-zero
  generator for drops. MobDropCreationWorld::raw_unique_id should delegate to the
  SAME owner's raw_next(); quest/start/character providers use next() or explicitly
  add server once to raw_next(). Existing character snapshot calls still work;
  migration to live clock ownership belongs with the coordinator's owner wiring.
- The default Linux clock uses low-DWORD CLOCK_BOOTTIME milliseconds and localtime_r
  calendar, sampled lazily on refresh. This chooses boot-time semantics, not
  process-relative elapsed time. Windows timer quantization/suspend behavior is
  not asserted identical; injected clock responses establish algorithm equality.
- Production main currently constructs the shared pointer for CharacterCommands;
  quest/drop services must receive that same retained owner. Actual world assembly
  is still required. Constructor initialization failures propagate explicitly.

Call-site audit: tracked `mob-drops-uid-calls.tsv` and reproducible audit tool cover
absolute global references and direct base/server calls, including billing,
user drop/create/remake, gem removal, shop, exchange, market, point, PvP exchange,
quest, character creation and mob drops. QuestionMgr::AddQuestion 00448bb4 uses
its own generator at manager+80; it must not be merged with item state. GenIDU
004cbec0 has a distinct 1000ms/full-counter algorithm and no direct calls in this
image; it is not silently implemented by raw_next. No direct SetStartID call was
found for the item global; its constructor baseline is zero. Indirect pointer
aliasing is not proven absent. Linear objdump misaligns at a preceding switch
table; an independent E8 displacement scan recovers the actual server-wrapper
call at 00457a51, also confirmed by function-entry decode and Ghidra.

Validation:

- Original execution fixture: 128 real constructors, 2304 retained interleaved
  operations, 73213 generated IDs. Complete GenID/server instructions and complete
  UID slices of QuestAddItem 0047e854..0047e917, CMob::ItemDrop
  004a6663..004a668e and ProcessCharacterCreate 00481750..00481810 execute.
  Local packing instructions execute; only OS critical-section/tick/calendar
  responses are ports. These are UID slices, not full quest/drop gameplay runs.
- One 65540-call retained sequence alternates actual quest/drop UID slices at a
  cached tick. FNV covers every output UID, not merely the final value. Other
  sequences cover baseline changes, zero stamp/tick, 65535/full-counter wrap,
  unsigned sign boundary, deadline wrap, exact expiry, server bytes 0/1/127/128/255,
  and local-calendar refresh cadence. Native comparisons match original UID hashes
  and observed tick/calendar cadence. Native lock exclusion is additionally
  covered by the existing eight-thread/8000-ID unit test, not OS port emulation.
- Final build exit 0, no warnings: `work/mob-drops-uid-final-build.log`.
  Final suite exit 0: 130/130 passed in 43.65 seconds, including existing callers;
  `work/mob-drops-uid-final-tests.log`. Earlier suite also passed 130/130 before
  additional adapter/SQL checks, but final results above cover the committed code.
- Focused isolated SQL exit 0 under shared SQL lock: same original/native stream
  plus 64 SELECT CONVERT(bigint, ?) round trips of actual generated IDs, including
  high-bit signed values. No database writes/schema/lifecycle operations. This
  verifies parameter bit preservation, not item persistence or gameplay.
  Log: `work/mob-drops-uid-sql.log`.
- Python py_compile and git diff --check pass. A requested wrapper help command
  unexpectedly ran two asset-only tests; terminated before SQL phases. That
  partial run is not counted as integration validation. Missing optional Capstone
  was handled by existing objdump/Ghidra tools; no dependency installed.

Remaining integration: retain one generator across all item-producing services,
provide the authoritative runtime boot/calendar clock and server byte, and connect
real death/inventory/world/log services. Earlier creation-header warning about
ItemIds timing/wrap describes the pre-fix implementation; coordinator can update
that comment during integration. SQL streaming publication remains a separate
storage-owner dependency. No production gameplay completion is claimed.
