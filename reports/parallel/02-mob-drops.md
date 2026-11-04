# Worker 02: mob drops

Status: loot algorithms, catalogs, incremental typed SQL reload, creation and
eligibility are reconstructed. Delivery connects actual pool/Actor/ZoneItems owners. Production
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
Milestone 8 UID implementation commit: `5f940275104dc1dfd6d38ca4d91c9ed7a50ffa5f`.

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

Historical milestone 7 boundary (resolved for MobItemsR2 in milestone 10):
SqlServer::query buffers its entire result, so a
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

## UID continuation checkpoint

Re-read current main continuity/parallel instructions and reviewed commits
5f94027/c2879e3. Main read-only snapshot `3e9e1a4` still has the pre-fix ItemIds
header/source; the delivered generator has not yet been integrated there. The
repeated assignment adds no unimplemented generator behavior identified by this
review. No new runtime code or execution results are claimed. Previous milestone
validation remains the applicable evidence; documentation diff check passes.

Next concrete task proposal: coordinator integrates the delivered shared owner,
then explicitly authorizes the CharacterCommands clock/UID call-site migration
and retained quest/drop provider assembly. CharacterCommands currently owns the
snapshot-clock caller; changing it exceeds the current item_ids.hpp/cpp/tests
ownership. Inject the same retained ItemIds into all those providers, use next()
for server-inclusive IDs and raw_next() for the existing drop creation boundary,
and validate complete producer composition. This is owner wiring, not another
UID generator. Awaiting assignment; no main/other-owner changes or repeated tests.


## Milestone 9: CharacterCommands uses the retained live UID owner

Implementation commit: `9d6d13499e74133e360991df620146d966534056`.

Read latest main continuity/parallel instructions. UID fix is already integrated
centrally in frozen source 5e8d1cb via b916c7f; historical checkpoint 783f9ba's
pre-integration snapshot is superseded. This assignment explicitly authorizes
character_commands.hpp/cpp and their tests. No main.cpp, Actor/inventory or other
worker source changed.

Original ProcessCharacterCreate 00480e90 evidence, especially 00481750..004818ce,
shows locked UID generation first, then separate GetLocalTime for each item's
make time, regardless of whether UID calendar refresh was needed. Ghidra's retained
focused export was reviewed alongside complete original instruction execution.
The starter DB packet is 0404, length 75+23*count, with packed per-item fields
bag/slot/UID/item ID/type/type-ID/quality/count/make time. All twelve immutable
EXE/PDB hashes verified again.

CharacterCommands now calls the retained identifiers_->next(), using that owner's
live clock under its lock; the separately injected timestamp clock remains only
for persistence metadata and each starter make-time sample. Its existing initial
metadata sample is unchanged and is not represented as an original UID-loop
sample. Each item still samples UID before make time. The obsolete overflow-error
mapping was removed: original counter wrap is ordinary generation, and failures
from required clock services are not disguised as a capacity result. Constructor
signature/shared_ptr ownership remain unchanged; no generator was added.

Deterministic callers must inject both independent services: ItemIdClock into the
retained ItemIds, and the existing timestamp callback into CharacterCommands.
The existing creation-network integration fixture now does this, with its clock
outliving the commands/generator. An injected timestamp callback no longer
silently controls UID sampling. Quest/start/drop providers must receive the SAME
retained ItemIds; server-inclusive providers use next(), while drop creation's
raw_unique_id boundary uses raw_next() and adds server once. Real world assembly
remains outside this caller assignment.

Validation:

- New original-execution oracle runs the complete starter UID/make-time loop and
  final original DB packet send, with the real local packer and GenID/server
  bodies before and after each creation on the SAME original global. 64 flows,
  380 generated starter records, cached/refreshed calendars, deadline/counter wrap,
  server 0/3/255 and changing independent make times. Only OS clock/lock and final
  DB transport are ports; full character-stat/header preparation is not claimed.
- Native test executes actual CharacterCommands, CreationRules, Characters create
  transaction/parameter construction, roster reload and client replies. SQL methods
  are explicitly test-only link-time scripted ports; no native repository method
  is substituted. All 146 submitted starter parameter records match the original
  23-byte packet records, including signed UID bits and separate timestamps.
  16 successes and 48 scripted create/item-save failures verify commit/rollback,
  stop-at-failed-row, cached account flags, 0102 result and ordered 0101 list
  packets. Invalid admission consumes neither clock nor SQL. External producer
  calls before/after prove the same live owner and advancement on SQL failure.
- Native build exit 0, no warnings: work/mob-drops-uid-caller-build.log.
  Full suite: 131/131 passed in 47.52 seconds, exit 0;
  work/mob-drops-uid-caller-tests.log. Additional test-only failure-prefix/count
  assertions were then compiled successfully in the final build;
  work/mob-drops-uid-caller-final-build.log.
- Final focused check after the additional assertions: 3/3 passed in 0.04 seconds,
  exit 0 (creation-unit, item-ids-unit, mob-drops-uid-creation-unit);
  work/mob-drops-uid-caller-focused.log.
- Read-only isolated SQL under shared lock: exit 0; existing original/native UID
  stream and 64 bigint parameter round trips pass again;
  work/mob-drops-uid-caller-sql.log. This is not a full creation SQL/TCP run.
- Existing creation-network-integration builds with the deterministic live clock.
  Its complete stored-procedure/TCP execution remains for the coordinator's full
  isolated integration: the harness creates/drops a disposable catalog, and SQL
  lifecycle is explicitly outside this worker's authorization. No such lifecycle
  operations were attempted in this block.
- New oracle passes py_compile; git diff --check passes. Initial original fixture
  attempt read the transport size from the buffer argument position; corrected
  to size at stack+8 and RET8 before successful regeneration and fixture copy.

Remaining: coordinator full creation SQL/TCP validation and real quest/drop/world
provider assembly with this retained shared owner and authoritative clock. This
block removes the snapshot UID caller from CharacterCommands; it does not claim
all runtime item producers are assembled or that the server is playable.


## CharacterCommands continuation checkpoint

Re-read current main continuity/parallel instructions and reviewed 9d6d134/1b78fc8.
The repeated caller-migration assignment is implemented and verified by milestone
9 above. Read-only main inspection still finds identifiers_->next(clock_()) in
CharacterCommands, so this delivery has not yet reached that caller. No additional
independent caller behavior was identified within the authorized paths. No new
runtime changes or test results are claimed; documentation diff check passes.

Next task proposal: coordinator integrates 9d6d134 and runs the updated existing
creation-network-integration in its disposable-catalog full SQL pipeline. Then
assign retained quest/drop provider assembly explicitly, with the SAME ItemIds
shared_ptr already supplied to CharacterCommands. SQL lifecycle and production
world assembly remain outside this assignment. Waiting for integration/new scope;
no main edits, duplicate implementation or repeated validation performed.


## Milestone 10: actual retained typed SQL cursor / MobItemsR2 streaming

Implementation commit: `bc0b0b8e13cc8302c3b6769cebd8cedf583e6eba`.

Latest continuity explicitly authorizes existing storage/sql_server.hpp/cpp and
new mob_drops_stream files. CharacterCommands migration is already integrated in
main's frozen 27df258 batch; its historical checkpoint is superseded. No main,
other-worker, credential, schema or database-lifecycle files were edited.

Evidence: ps_game.exe ReLoadConstMobItem 004c2e70, completed-row write block
004c3043..004c3105, SDatabase::GetData 004e15b0 and Fetch 004e2090. Locked focused
Ghidra export completed in work/exports/02-mob-drops; log
work/mob-drops-stream-ghidra.log. All twelve immutable EXE/PDB hashes pass.
Original column reads use SQL_C_SHORT/SQL_C_TINYINT/SQL_C_SHORT/SQL_C_LONG,
widths 2/1/2/4. Original wrappers normalize SQL_SUCCESS_WITH_INFO to success.

Existing SqlServer now exposes cursor(sql, parameters, column_projection) with
TypedSqlCursor::next(Row/End/Error). It reads the actual ODBC statement incrementally;
only a completed typed row replaces the output vector. End/error are sticky and
leave output untouched. SQL NULL remains explicit; the MobItems projection rejects
it rather than guessing original uninitialized buffer values. Numeric signed
reads are projected back to original WORD/DWORD bits by the existing reload engine.
Unexpected extra rowsets fail explicitly; trailing update-count results are skipped.

The cursor retains statement, copied input parameters, Unicode binding buffers
and the existing connection/environment handles. It survives destruction of the
SqlServer facade and supports move ownership. Statement disposal precedes binding
buffer destruction and connection disposal. An active transaction is rolled back
when the final retained connection is disposed, after statement release. No new
connection/credential owner is created by opening a cursor: the supplied existing
connection is shared through an internal lifetime lease. Facade diagnostics update
while it is alive; the cursor retains structured SqlError independently afterward.

Connections remain externally serialized. While a cursor exists, other query,
cursor or transaction operations on its facade fail explicitly instead of allowing
the driver to discard a live result. Destroy the cursor before reusing the
connection, including after End/Error. This is a new cursor lifetime rule; buffered
callers need no API changes. Existing query() still buffers all sets with the same
NULL, diagnostic, transaction and parameter contracts. Its original execution/
parameter binding code is shared with the cursor: UTF-16 text, signed bigint,
double and explicit NULL bindings retain their original widths/types.

reload_mob_drop_sql now opens the actual typed cursor and calls the existing
reload_mob_drop_rows through the concrete mob_drops_stream projection. Completed
rows publish in original order; a later fetch/GetData failure preserves earlier
writes and counter increments. The failing row is never partially published.
Initial statement/query failure reports query failure; later errors report row
failure. Stable definition storage, absent slots and cumulative signed counter
behavior remain unchanged. The buffered-result compatibility helper is retained.

Validation:

- 512 existing complete original ReLoadConstMobItem fixtures now pass through
  actual native SqlServer binding, statement/connection ownership, numeric cursor
  and reload engine with controlled ODBC boundary responses. Exact full tables,
  counters, diagnostics and pointer stability match, including 383 partial/query
  failures and all four GetData failure columns. No abstract-only SQL substitute.
- Controlled additional cases verify SQLFetch failure, unchanged output on every
  partial-row failure, sticky status, explicit NULL, trailing-result error,
  blocked overlapping query, move/destruction after facade release, statement->
  connection->environment cleanup order and deferred transaction rollback once.
  Buffered Unicode/bigint/double/NULL binding types are checked independently.
- New original wrapper oracle executes complete GetData/Fetch instructions over
  12 raw ODBC return cases (0,1,100,-1,-2,2). Raw ODBC results/diagnostic lookup are
  ports. Native cursor outcomes compare against original normalization, including
  success-with-info; this does not claim a byte-identical SDatabase object layout.
- Full native suite: exit 0, 132/132 passed in 51.10 seconds;
  work/mob-drops-stream-tests.log. Original-wrapper fixture/assertions were added
  afterward without production changes; final focused check passes 3/3 in 0.09
  seconds, exit 0, including all 12 original wrapper normalization outcomes;
  work/mob-drops-stream-focused.log.
- Successful build/retry and final storage/SQL test build exit 0:
  work/mob-drops-stream-build-retry.log and work/mob-drops-stream-final-build.log.
  No warnings in changed storage/stream paths. Broad header rebuild exposed
  existing misleading-indentation warnings in unchanged guild files; not edited.
  Initial build failed on assigning optional<SqlError> (const diagnostic members);
  changed to emplace before successful builds. Initial failure is not a pass.
  Wrapper-only final test rebuild: work/mob-drops-stream-wrapper-build.log.
- Real isolated read-only SQL, shared SQL lock, exit 0:
  work/mob-drops-stream-sql.log. Each of the four actual GetData conversions fails
  on a later malformed row, preserving the completed first reload row/counter.
  Signed/byte extremes, explicit NULL, extra result rejection, buffered multi-set
  API, Unicode/INT64_MIN/double/NULL parameters, read-only transaction commit and
  reading after SqlServer facade destruction all pass.
- The same locked run compares five real MobItemsR2 reloads: 45 rows, 2747 retained
  definitions/27470 slots, cumulative counter 17->107. All full-table/counter hashes
  match original instruction execution; repeated reload is idempotent. Numeric
  snapshot stays ignored at work/mob-drops-stream-sql.json. No database writes,
  DDL, resets, provisioning or lifecycle operations.
- New oracle passes py_compile and git diff --check passes. Prior original fixtures
  were reused rather than regenerating/repeating completed selection evidence.

Remaining: coordinator shared full-SQL integration and actual serialized definition/
connection owner assembly; production death/world/quest/drop services still need
real providers and the same retained ItemIds. Streaming mid-row publication is
implemented and verified here; uncontrolled network loss/timing equivalence and
original undefined NULL buffers are not claimed. No gameplay completion claim.

## Streaming continuation checkpoint

Re-read current main continuity/parallel instructions and reviewed bc0b0b8/6cb4b94.
The repeated streaming assignment is complete in milestone 10. Read-only main
snapshot f0bafb5 does not yet expose TypedSqlCursor or bind the streaming reload.
No additional independent behavior was identified within this four-column cursor/
MobItemsR2 assignment. Existing validation remains applicable; no new build, SQL
execution or original fixture regeneration is claimed. Documentation diff check
passes and only this report changed.

Next concrete proposal: coordinator integrates bc0b0b8 and runs shared full SQL
compatibility checks, then assigns production MobItems definition/connection lease
assembly. Use the retained stable MobDropDefinitions, serialize reload/read access,
and dedicate the existing connection while its cursor lives. Required root-world
services and quest/drop shared UID assembly remain explicit separate dependencies.
No duplicate cursor, extra credential owner or other worker implementation was
introduced. Waiting for integration review/new scoped assignment.


## Milestone 11: actual Actor retained inventory primitives for quest owners

Implementation commit: `c02b69c227951a9d1961170385176b443237e863`.

Read current main continuity/parallel instructions and worker06 QuestAddItem /
worker07 QuestFindItem reports. Expanded exclusive scope authorizes actor.hpp,
inventory.cpp and matching tests; prior storage/UID/caller commits are preserved.
No main checkout, other owner source, SQL fixtures or lifecycle files changed.

Evidence: ps_game.exe QuestFindItem 0047f790 / 0047f8e0, especially 0047f812,
0047fc80..0047fccd and 0047feb6..0047ff0d; QuestAddItem 0047e620 / 0047e6f0;
FreeItem 0040a590. Complete original Ghidra exports and worker reports show:
partial count changes precede DB/client, multi-item whole-stack release retains
old count, and raw pointer publication precedes default craft and DB delivery.
All twelve immutable EXE/PDB hashes pass again; log
work/mob-drops-inventory-hashes.log. Prior original fixtures were reused.

New methods on EXISTING Actor / retained_inventory_ / inventory_:

- inventory_slot_empty(bag,slot): both retained ownership and value-only mirror
  rows occupy a slot; does not invent a pooled object for legacy constructor rows.
- retained_inventory_item(bag,slot): optional borrowed GroundItem* AND its actual
  GroundItemPool*. Returned pointer values do not refer to movable vector entries.
- claim_inventory_item(bag,slot,item,pool): claims the already allocated detached
  pointer. No reset, decode, allocation, stack merge or pickup side effects. Native
  row coordinates/definition ID normalize to the retained slot. Occupied slot or
  duplicate pointer in this Actor returns false without taking ownership.
- set_inventory_item_count: synchronizes BYTE count in pooled object and value
  mirror, without packets or replica mutation. Optional expected pointer rejects
  a different current allocation.
- sync_inventory_item: copies the authoritative payload into the value mirror
  after post-store craft/default changes, before forwarding the DB packet. Does
  not decode/rewrite craft tails or reverse authority from mirror to pointer.
- release_inventory_item: direct bound-pool release and removal of both Actor
  records. No count overwrite/reset, ItemRemove, exchange cleanup, equipment,
  DB/client/log or pickup effects. Optional expected identity and empty-slot
  results permit revalidation. Pool validation failure leaves Actor records intact.

Ownership/serialization contract: execute under the same serialized Actor owner
through all callbacks. Pool and definitions outlive Actor; borrow is not an owning
lease and expires on release/reuse, even if a future allocation has the same address.
Caller must retain Actor and relookup/revalidate after callbacks. Raw claim requires
an exclusive live allocation from the supplied pool, not owned by another Actor/
zone; pool membership is a caller precondition because existing pool exposes no
membership probe. Invalid coordinates, linked/undefined allocations or broken
mirrors fail explicitly. Capacity matches existing inventory APIs (bags0..9,
slots0..23); quest adapters search bags1..open, never equipment or locked bags.
Existing Actor APIs/callers and teardown remain unchanged.

Exact quest adapter recipe:

- 06: slot_empty -> inventory_slot_empty; create_item -> SAME pool.allocate;
  store -> claim_inventory_item on that allocation. Keep pointer authoritative
  across the following defaultcraft write; send_database first syncs full mirror,
  then forwards the required actual 0701 transport. Do not call receive_item or
  load_inventory_item. The caller retains an unclaimed allocation on failed claim.
- 07: construct/refresh its required count projection from borrowed live items;
  before forwarding DB, set_inventory_item_count to the projection's current
  count. Do not blindly subtract packet count: multi-item whole-stack projection
  deliberately retains old count until release. Forward DB then client, then
  release_inventory_item through the borrowed bound pool. Replica changes only
  through real packets. Do not implement release with remove_item.

These methods remove the canonical interface blocker. Real quest world transports,
Actor leases/locks, root inventory load and shared UID assembly remain mandatory;
this block does not fabricate them or install a second inventory/pool.

Validation:

- Build exit0, no compiler warnings: work/mob-drops-inventory-build.log and final
  test/SQL-mode rebuild work/mob-drops-inventory-sql-build.log. No failed build/test
  run in this block. Full suite exit0: 135/135 passed in30.55seconds;
  work/mob-drops-inventory-tests.log.
- Existing original request fixtures now compose actual Actor primitives, actual
  GroundItemPool and InventoryReplica: 512 flows,209 completions,680 direct Actor
  releases. Full event traces, packets, final counts, deletion journal, pointer/
  mirror/cache state, equipment and locked-bag sentinels match original.
- Existing original QuestAddItem fixtures now compose actual raw Actor claim and
  DB-boundary mirror sync:384 cases,152 grants. Complete CItem/craft-tail payloads,
  lookup/allocation/claim order,0701 and0205 packets and real DB replica match.
  Store observes empty craft; DB callback observes synchronized default craft.
  No extra pool allocation occurs during claim, and no implicit transport occurs.
- Guard/lifetime test covers nested serialized claim causing vector growth while
  the borrowed pooled pointer stays stable, stale expected pointer, duplicate/
  occupied claim, value-only rows, full payload mirror, count sync, direct release
  without exchange cleanup, linked/invalid slot rejection, LIFO/reset and exactly
  one teardown release. No retained pointer is used after pool reset as an owner.
- Read-only isolated SQL under shared SQL lock, exit0:
  work/mob-drops-inventory-sql.log. Actual synchronized Actor mirror's high-bit UID,
  count, WORD quality and post-defaultcraft string round-trip through explicit
  bigint/tinyint/smallint/varchar casts. This is a scalar type/bit compatibility
  check, NOT full character persistence. No writes, DDL or lifecycle operations;
  shared full SQL remains coordinator-owned.
- Worker base predates canonical quest request/start-item sources. Read-only
  tools/mob_drops_inventory_owners.py snapshots four source/header files plus two
  original fixtures into ignored work/mob-drops-inventory-owners. Private CMake
  uses those only when canonical headers are absent; central integration links
  normal canonical owners. No tracked alternate quest implementation is created.
  Snapshot manifest: work/mob-drops-inventory-owners.json; SHA256 below.
  - `include/eternity/game/quest_requests.hpp`: `c5527089335a826292932ce5bb08eeb0f5a2e41d8d530c424b5d745a327e4aa9`
  - `src/game/quest_requests.cpp`: `59c197bc5261f5a0935e517146a0ecd6ca73bef681391bd35b30e65abf1a8d16`
  - `include/eternity/game/quest_start_items.hpp`: `c2c91c9490299897941af4e402dbba349502e61c236c397ecf573602a79cbdb5`
  - `src/game/quest_start_items.cpp`: `eb4dd81f628c057322f22e1fad409d75a7a39214a88b3513fd173b3b56cf8fd1`
  - `tests/data/quest-requests-flow-oracle.tsv`: `c33e2bb5cff41ae9ac87c4dadf0fc674e783859bab707e3763925ee0e8122b25`
  - `tests/data/quest-start-items-oracle.tsv`: `fabb479105023441b102e607f0e0adcd42800e212e15596b92e5a7ed88de37d1`
- Snapshot tool passes py_compile; git diff --check passes.

Remaining: workers06/07 can now bind their existing required providers to these
exact Actor primitives, with real retained scene/transport/UID owners. Coordinator
integrates and runs shared full SQL; no global playability completion is claimed.

## Retained inventory continuation checkpoint

Re-read current main continuity/parallel instructions and reviewed c02b69c/302dcb2.
Read-only main snapshot 48f2c32 still lacks the delivered retained-inventory API;
its eleventh-batch validation freeze is active. The repeated assignment is already
implemented and validated in milestone 11. No additional independent behavior was
identified in the authorized primitive scope. No new implementation/test results
or shared-source changes are claimed; documentation diff check passes.

Next concrete proposal: coordinator integrates c02b69c, then assigns workers06/07
the actual Actor-backed QuestAddItem/QuestFindItem adapters described above with
mandatory transports and shared UID. Worker01 can also bind its retained weapon
view using retained_inventory_item(0,5); the borrow is not a lifetime lease. Root
Actor serialization/lifetime must cover each whole callback sequence. Avoid
replacing the retained owner with a copied inventory or second pool. Waiting for
integration review or another explicitly scoped task.


## Milestone 12: immediate Actor effect-removal DB delivery

Implementation commit: `c964d62b52b6ca64403b1850c78807aa36b30845`.

Read latest main continuity/parallel notes and worker01 d9bb675/8fce79c report.
Expanded ownership covers actor.hpp/actor.cpp and matching tests. Inventory API
and prior commits are preserved; worker06/07 adapters, zone/users/mobs, skill and
SQL owners were not edited. No SQL lifecycle operations or credential changes.
All twelve original EXE/PDB hashes pass; work/mob-drops-effect-transport-hashes.log.

Original evidence (ps_game.exe, complete Ghidra exports / instruction execution):

| Site | Immediate backend / client behavior |
| --- | --- |
| SendSkillRemove 00482a70 | 9-byte0902(account DWORD,skill WORD,level BYTE), then6-byte050e(instance DWORD); send return ignored |
| RemApplySkillToMOB00489560 / Sleep004893a0 / Hide00489290 | unlink,0902,050e,original state reversal/free order |
| ChkEndTimeSkill004865b0 / map check00484680 | completed unlink before0902/050e, then unapply/free and deferred party notices |
| PvPClearDebuff0047c610 |0902 before cancellation/free; no050e |
| ItemEquipmentRem004557ed..00455996 / AddApplySkillBuff00484f86..004850a2 | selected element removals0902/050e before unapply/free |
| ClearApplySkillByDeath00488000 | selected reversal then0902/free; no050e |
| PacketUserDBSetInfo0046d780 case0505 / AddApplySkill00487ce0 | filtered transient and offline-expired rows send0902; skipped unknown/non-effect/map-blocked rows do not |
| ResetCharacter0044a050 | runtime release only, no persistence erase/0902/050e |

Therefore current EffectCycle::erase_persisted hooks correspond to original0902
sites, including restoration cleanup. EffectReplica::erase itself is NOT globally
changed. release_all/shutdown and direct replica/save operations remain non-sending.
Application0901 delivery is outside this removal assignment and remains unchanged.

Actor now preserves its previous first-match replica erase, then sends exactly
one0902 through a required ActorEffectDatabase binding before its existing client
hook. It sends even when the mirror has no matching row; no acknowledgment gates
client reversal/release. Packet account comes from account_id(), not Actor::id();
instance ID is only in050e. No second cache, runtime pool or generic outbox added.

Binding/construction:

- New constructor overload accepts ActorEffectDatabase& explicitly. The existing
  constructor remains source-compatible and detects an explicitly composed
  ActorScene+ActorEffectDatabase adapter. This binds before ActorLoadPipeline's
  restoration callbacks without editing that separately owned loader.
- A legacy unbound actor can be inspected or shut down. Removal requires binding;
  public Actor action preflight rejects an unbound active actor before mutation.
  Direct EffectObserver callbacks are noexcept and fail fast if unbound. There is
  no fallback send_self route, no silent DB no-op and no default-success sink.
- Remote deployment supplies its real DB-agent queue/connection, retaining/copying
  bytes before return. For cohosted native services ActorEffectReplicaDelivery is
  a concrete route to the EXISTING SkillReplica (CharacterReplica::skills()). It
  uses the SAME supplied recursive cache mutex as character saves, calls actual
  apply(), and requires diagnostics for rejected/failed requests. It constructs
  no replica/pool/connection and does not invent retry/rollback semantics.
- Actor, scene, transport, receiver, cache mutex and diagnostics must be retained
  through callbacks. Transport handles failures without throwing or blocking the
  original reversal; root owner chooses the real route before loading/actions.
  Do not feed the packet back into Actor::erase_persisted (double erase/recursion).

Actual DB-agent evidence: ps_dbAgent.exe PacketUserSkillApply00406250 case0902
unlinks the first matching cached pair WITHOUT recycling. The concrete delivery
uses existing SkillReplicaPool::strand and preserves later skill_save_plan /
SaveCharacter00423f55..004242c2 behavior. Runtime applied-skill ID release remains
an existing required ActorScene service; no new native runtime heap is claimed.

Validation:

- Complete original SendSkillRemove execution:512 cases, alternating send return
  values, account/skill/level boundaries and separate instance IDs. Exact0902 and
  050e bytes/order match actual Actor removal. Only DB/client send boundaries are
  observed ports; the original construction/control instructions execute.
- Existing2304 original shape/sleep traces now pass through actual Actor observer,
  transport and replica. Full DB/client/world/ID-release traces match, with unlink
  and first-match mirror erase observed before DB, then client, then release.
  EffectWorld operations remain explicit original-fixture ports, not new gameplay
  implementations. Runtime original heap release is in the existing oracle;
  native release boundary remains the mandatory Scene provider.
-512 concrete receiver compositions also execute actual SkillReplica and its
  existing pool: one duplicate removed/stranded, survivor timer9 retained,
  available count unchanged, and canonical delete-all/add-survivor save plan
  unchanged. Required receiver diagnostics observe malformed/wrong-owner requests.
- Native checks cover explicit constructor and composed-scene binding, unbound
  action rejection without mutation, restoration filters/expiry versus skipped
  rows, death/duel without050e, shutdown withoutDB and direct replica semantics.
  Existing Actor/action/hide/equipment/duel/HP fixtures now record explicit backend
  bindings; their established client/world traces remain unchanged.
- Build exit0, no compiler warnings in this block:
  work/mob-drops-effect-transport-owner-build.log. Python oracle py_compile and
  git diff --check pass. Original fixture generated under work/ before copy.
- FULL SUITE IS NOT PASSED:136/137 pass in40.61seconds; ctest exit8 / dev wrapper
  exit1. Only failure is zone-users-unit: "Actor effect removal requires a
  DB-agent transport binding". Log work/mob-drops-effect-transport-tests.log;
  detailed successful new outputs in build/Testing/Temporary/LastTest.log.
- That existing zone fixture is worker01-owned and still constructs actors with
  hide effects but no DB transport. Its source was left untouched. A minimal
  recording-provider patch is prepared at
  tests/data/mob-drops-effect-transport-zone-fixture.patch; git apply --check passes.
  This check is NOT a claim that the patched fixture was executed. Explicit
  authorization was requested asynchronously because file ownership is outside
  this scope; no reply/approval has been inferred from elapsed time.
- No SQL was executed in this transport/cache block. Actual canonical save-plan
  composition is verified; coordinator full isolated SQL/character integration
  remains required under its disposable-catalog lifecycle ownership.

Integration/remaining: worker01/coordinator must migrate the zone fixture and any
real scenes with removal-capable actors to the required transport binding. Apply
and run the prepared patch only with that owner's authorization. Root must select
actual DB queue or cohosted receiver on the same cache/save lock and retain runtime
ID services; immediate packet construction/cache delivery is implemented, full
root network/world assembly and global playability are not claimed.

## Milestone 13: authorized ZoneUsers effect-transport fixture migration

Fixture-only implementation commit:
`4a1ebd6b976f8c1a329c96f1b737c6e8ca618d00`.

Read current continuity/parallel instructions and reviewed c964d62/5546d0b.
Coordinator explicitly authorized the narrow tests/zone_users_unit.cpp exception.
Applied exactly the prepared recording-provider patch: fixture Scene implements
ActorEffectDatabase and captures emitted packet bytes. Actor binds this explicit
capability during construction. No production no-op, zone_users implementation,
other worker01 source or main/frozen source was changed. No runtime contract fix
was needed. This commit must follow the immediate effect-transport implementation
c964d62; it does not replace that implementation or its mandatory root bindings.

Validation:

- `python3 tools/dev.py build`: exit0; only the fixture recompiled/linked, no
  warnings. Log work/mob-drops-effect-fixture-build.log.
- `python3 tools/dev.py test`: exit0, **137/137 passed in42.82seconds**, including
  zone-users-unit and both immediate effect transport comparisons. Log
  work/mob-drops-effect-fixture-tests.log. No failed test remains in this worker
  suite at this source; no duplicate full-suite run performed after this pass.
- Initial milestone12 failure remains unchanged at
  work/mob-drops-effect-transport-tests.log:136/137,ctest8/dev1,40.61seconds.
  SHA256 `187329daa20579be18109daa62caf0ba48f7cc8811f57dc210f682a3011d0321`.
  It is preserved as a failed run, not retroactively counted as passed. The
  historical milestone12 validation section above describes that initial state;
  its fixture-authorization dependency is resolved by this explicit assignment.
- git diff --check passes. No binary, SQL, schema, credential or lifecycle edits;
  this fixture-only block did not repeat unrelated SQL/original execution checks.

Remaining: coordinator integrates the implementation plus fixture commit, runs
shared complete checks and supplies actual production DB queue/cohosted cache,
shared save mutex, Actor lifetime and runtime ID services. A recording test
transport establishes no root-world assembly or global gameplay completion.

## Fixture continuation checkpoint

Re-read current main continuity/parallel instructions and reviewed4a1ebd6/1293d5e.
The repeated narrow exception is already complete and its137/137 validation is
recorded above. Read-only main snapshotff9f044 remains under fourteenth-batch
freeze; its reviewed-awaiting-fixture-fix note predates this completed delivery.
Reverse patch check confirms the prepared migration is present in this worker;
no patch was reversed, no runtime edits or duplicate tests were performed.
Documentation diff check passes.

Next concrete proposal: coordinator releases the held c964d62 delivery together
with fixture4a1ebd6 after its freeze, runs shared complete checks and assigns any
remaining real DB route/Actor-load root binding. Actual services must remain
required. No independent work remains in this fixture-only assignment; awaiting
integration or another explicitly scoped task.

## Rebirth subowner API need (recorded before modification)

The new Actor/Rebirth assignment requires exact silent replacement of HP/MP/SP.
ActorResources alone owns current_; its existing current() is const, and clamp /
heal / recover mutate values or invoke sinks. No correct Actor-only forwarding
implementation exists without bypassing that owner. Per the assignment's
conditional resource-change instruction, the precise minimal need is one inline
resources.hpp method: replace_raw(PlayerResources value) noexcept {current_=value;}.
No resources.cpp implementation edit, const_cast, second resource field or
clamping/notification workaround will be used. Actor will forward to that method.
Presentation already has the real ZoneLifecycleState::motion (+1434) owner;
attack_state.motion (+1448) and attack_state.mode (+142c) remain separate.


## Milestone 14: real Actor Rebirth resource/presentation ownership

Implementation commit: `a7d46ab2805f38d1369eb25dd556a74f7d856bf0`.

Read current main continuity/parallel notes and worker08 Rebirth bridge/report.
Original evidence: ps_game.exe Rebirth00459ec0 raw copy00459ef6..00459f1a
(maxima17c/180/184 -> current1238/123c/1240) and presentation clear0045a148;
EnterZone0044ae30 presentation store0044ae3d; SendUserShape00482ad0 presentation
read00482cbc (also CZone::SendUserShape0041ff3a). These use CUser+1434, distinct
from attack motion+1448. Existing shared life owner remains attack_state.mode
(+142c). Complete Ghidra exports and original instructions were inspected; all
12 EXE/PDB checksums pass, work/mob-drops-actor-rebirth-hashes.log.

Minimal actual owner changes:

- ActorResources::replace_raw(PlayerResources) noexcept assigns its existing
  current_ directly. Exact need was recorded BEFORE the two-line resources.hpp
  addition, following the assignment's conditional resource-change instruction.
  resources.cpp is untouched. No const_cast, placement reconstruction, extra
  resource cache, clamp, heal/recover callback, notification or damage reset.
- Actor::replace_resources_raw forwards to that SAME resource owner. Existing
  resources() remains const and its borrowed reference stays stable. It does not
  change maxima, recovery, life or effect/credit state.
- Actor::presentation_motion() const and presentation_motion(BYTE) noexcept use
  the ALREADY PRESENT ZoneLifecycleState::motion. No new storage field was added.
  attack_state.motion remains the distinct +1448 scheduler byte; mode remains
  the existing +142c life owner, without a duplicate lifecycle life field.
- Actor::enter_zone_lifecycle now clears +1434's owner before its first clock,
  preserving attack motion. Actor::user_shape_changed serializes presentation
  from that owner. Actor::send_shape_self was audited: original SendShapeMe00482510
  sends the effective shape and does not read this motion byte, so it is unchanged.
  Existing Actor/UserShape fixtures now set presentation through its real API and
  preserve an independent attack-motion sentinel, keeping original packet bytes.

Exact worker08/root adapter bindings now available:

    presentation_motion(const Actor& a) -> a.presentation_motion()
    presentation_motion(Actor& a,BYTE v) -> a.presentation_motion(v)
    replace_resources(Actor& a,PlayerResources v) -> a.replace_resources_raw(v)

These are silent, exact, non-reentrant stores on the serialized Actor owner. Keep
Actor/scene/definitions and admission subject alive through synchronous callbacks.
Do not change the existing bridge's mode/recovery publication or point its motion
setter at attack_state.motion. Real corpse/movement/admission, recover-change,
view/self/log/clock services remain required; pure SendRecoverChange is still not
Actor::recover. No worker08 bridge or world/lifecycle implementation was edited.

Validation:

- New original execution fixture:512 full-DWORD boundary combinations (zero,
  1,65535,65536,INT_MAX,INT_MIN bits,UINT_MAX and123456789). Executes the actual
  six raw-copy instructions and isolated presentation store, preserving life and
  attack byte. These are instruction slices, NOT a new whole-Rebirth claim;
  worker08 owns the existing complete693-byte coordinator evidence.
- Native comparison uses actual ActorResources via Actor forwarding, even with
  deliberately different maxima to expose clamps. Exact current HP/MP/SP, stable
  reference, no resource-sink/party/clock calls, unchanged max/recovery/life/attack
  fields and silent presentation reset all match. Separate damage-credit test
  proves raw full HP does not clear the ledger. EnterZone's first clock observes
  presentation0 with attack0xa5 and shared life unchanged; normal two-clock/auth
  sequencing and movement checks remain intact.
-8 actual canonical worker08 Actor-bridge compositions bind the new methods:
  restore/no-restore, corpse/no-corpse, dead/nondead, false movement and callback
  changes to live resources. Exact restore-before-FindDead and presentation-before-
  recovery/life tail are observed; callback resources survive. No scratch resource/
  presentation/life owner is substituted. This does not duplicate worker08's
  success/cross-map/nested-Rebirth fixtures or supply missing root action services.
- Worker base predates the canonical bridge. Read-only snapshot tool copies only
  its source/header into ignored work/mob-drops-actor-rebirth-owners; central CMake
  uses normal canonical owners when present. Manifest:
  work/mob-drops-actor-rebirth-owners.json. Exact SHA256 values:
  - `include/eternity/game/mob_targeting_lifecycle.hpp`: `05ceeab56cf60279513de5e20b55d66faba3349da6e4926df24f41739408469a`
  - `src/game/mob_targeting_lifecycle.cpp`: `e15c5bf74ef9d50446d1fea6f7e00caae397f5a3d8c3230487cdc2c983eca5de`
- Native build exit0, no warnings: work/mob-drops-actor-rebirth-build.log;
  final test-header rebuild exit0:work/mob-drops-actor-rebirth-final-build.log.
- Full suite exit0: **139/139 passed in42.11seconds**;
  work/mob-drops-actor-rebirth-tests.log. No failed build/test run in this block;
  no repeat after the complete pass. Passing output details remain in
  build/Testing/Temporary/LastTest.log.
- Both new Python tools pass py_compile; git diff --check passes. Generated
  original fixture was written under work/ before copy, with no active readers.
- No SQL execution/lifecycle/schema changes in this silent Actor-state block.
  Rebirth itself has no direct SQL call; actual admission/shared SQL checks remain
  with the coordinator's integration pipeline. No main/frozen source changes.

Remaining: worker08/root can replace their fixture presentation projection and
required raw-store implementation with these real owner calls. Preserve existing
life/recovery order and actual serialized subject/zone lifetime. Root world,
corpse/admission/transport services still need assembly; no gameplay completion
is inferred from native comparisons or test counts.

## Actor Rebirth continuation checkpoint

Re-read current main continuity/parallel instructions and reviewed a7d46ab/cca5d9e.
The repeated raw-resource/presentation ownership assignment is implemented and
validated in milestone14. Read-only main408826d does not yet expose those Actor
methods; fifteenth retry validation is still active. No new implementation or
test execution is claimed; documentation diff check passes.

Next concrete proposal: integrate a7d46ab, let worker08/root bind its three required
ports to these actual owner calls, then explicitly assign the remaining pure
SendRecoverChange00482230 notification API (Actor/ActorResources scope if needed).
That provider must emit original current-energy/party packets without mutation,
clamp, heal or damage reset; Actor::recover remains an invalid substitute. Real
corpse/admission/transport/subject leases remain separate root dependencies. No
further independent raw-store/presentation change was identified inside the
current assignment, and no worker08 or main source was edited. Awaiting review
or another scoped task rather than repeating completed validation.
