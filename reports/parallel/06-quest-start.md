# Quest admission and start

Status: implemented admission/start, complete start-log serialization and QuestAdd
state transitions; production quest/world
ownership adapters remain required. This does not make the server playable.

## Evidence

Read-only Ghidra exports: `/home/franco/Projects/Eternity/work/reverse/ps_game.exe/`
(`decompiled.c`, `disassembly.txt`, `types.txt`). Executable: `ps_game.exe`.

- `CUser::QuestEnableStart` **0047f080**: enabled byte `+1cc`, inclusive level
  bounds `+102/+104`, country selector `+106`, job flags `+10a`, grow `+107`.
  Country selectors 3 and 4 mean user countries 3 and 2 respectively. Unknown
  selectors bypass country checks. Grow 2 rejects only requirement 3; grow >=3
  bypasses grow checks. Sex, party and party-job fields are not checked.
- `CUser::QuestFindFinishSuccess` **0047e520**: any nonzero successful entry
  matching any nonzero ID from `+118`, `+110`, `+112` satisfies prerequisites.
  Despite their recovered names HG/VG, the last two fields are unsigned quest IDs
  at these call sites (`MOVZX`). Zero IDs do not match.
- `CUser::QuestStartRequest` **0047f220**: unsigned 16-bit, one-based positional
  definition lookup, then dispatch types 0..4; unknown types silently stop.
- Checks **0047f390**, **0047f400**, **0047f490**, **0047f500**, **0047f570**:
  NPC existence/type/ID; active/finished rejection; admission; item requirements;
  allocation; start. Item-give consumes before allocation without rollback;
  item-pick checks without consuming. Pick/use compare the full 32-bit request
  target with the definition's byte item ID. Use requires matching last item and
  unsigned `tick <= deadline`, including equality. Previous-quest starts require
  an NPC even with wildcard NPC type, check prerequisites before duplicates, and
  deliberately pass no NPC to QuestStart.
- `CUser::QuestStart` **0047ea70**: exact eight-byte client packet
  `0902, npc runtime handle (u32), definition ID (u16)`, followed by required
  start-log delivery. Common log data are captured before the client send. `QuestGetStartItem` **0047eb70** calls each nonzero type in
  three ordered item/type/count triples, ignores failures and continues.

## Implementation and boundaries

`quest_start.hpp/cpp` uses an immutable 464-byte definition projection and a user
projection. Valid job domain is 0..5; invalid jobs throw instead of reproducing
out-of-record reads. Definitions must remain alive while an owner retains their
references. No existing canonical header was modified.

Required `QuestStartWorld` methods have no default bodies: NPC lookup, active and
finished lookup, inventory requirement/consumption, clock, quest allocation/add,
client send, start logging, and start-item grants. Block 2 below implements full
`0x0501`/144-byte log serialization; the production adapter must supply the
context, snapshot it before client delivery and send the captured packet.
Remaining world providers are explicit unimplemented production dependencies,
not successful gameplay services.

Canonical integration changes needed:

- Project user level (`+136`), country (`+12e`), grow (`+12f`), job (`+134`), last
  use deadline (`+57d0`) and item (`+57d4`), plus finished ID/success entries.
- Supply positional quest records and live NPC runtime handle/type/ID.
- Connect `QuestStartWorld::add` through Block 3 and the canonical owner. Evidence
  `QuestAdd` **0047e010**: allocation failure returns false; initializes success
  and three counters to zero, start tick, optional end tick using wrapping
  `minutes*60000+tick`, optional success tick, active-list insertion, kill-PC/MOB
  counters for end types 3/2, and DB packet `0x0a01` (billing u32, ID u16,
  truncated maximum minutes u16). Do not infer an admission capacity limit here.
- Connect inventory and logging providers and production dispatch to this module.
  Core QuestEnd/removal/ownership remain the coordinator's domain.

## Validation

`python3 tools/dev.py build`: exit 0 (379 build steps). No warnings in owned
sources; the full baseline emits existing guild-source warnings.
`python3 tools/dev.py test`: exit 0; **120/120** tests passed in **35.25 s**.
`quest-start-unit`: **1,536** exact original admission/request traces and **479**
successful starts matched, including the exact client packet and provider order.
Logs: ignored `work/quest-start-build.log`, `work/quest-start-test.log`, and
`build/Testing/Temporary/LastTest.log`.
`git diff --check`: exit 0.

The original-execution generator completes
1,536 deterministic cases, executing original admission, prerequisite-list
traversal, request/check functions, packet construction and start-item loops.
World lookup, inventory, allocation and transport are observed ports. Cases
include failure ordering, all start types, boundary ticks/levels, prerequisite
success values 0/1/2, alternate prerequisites, failed allocation after consumption,
unknown country/start selectors and zero-count item grants.

`sha256sum -c SHA256SUMS`: exit 0, all original EXE/PDB hashes OK.
Generator: `.local/oracle-env/bin/python tools/quest_start_oracle.py`, exit 0;
fixture generated under ignored `work/` then copied to the owned fixture path.
A second generation compares byte-for-byte identical (`cmp`: exit 0).
`python3 -m py_compile tools/quest_start_oracle.py`: exit 0.
SQL lifecycle/final isolated SQL integration are reserved for the coordinator;
this module introduces no SQL contract/schema changes.

Implementation commit: `f10cfb7b8b64e9cfbd32b5600a5a9f62f5e47872`.

## Block 2: complete quest-start log serialization

`quest_start_log.hpp/cpp` reconstructs `TP_GAMELOG_QUEST_START` (144 bytes,
`0x0501`) and its common header. Evidence: `ps_game.exe`,
`CUser::SetGameLogMain` **0045b0d0**, `CUser::QuestStart` **0047ea70**, and
Ghidra `TP_GAMELOG_MAIN` / `TP_GAMELOG_QUEST_START` types.

Offsets: user ID 2, account name 6 (19 bytes), character ID 25, character name
29 (21 bytes), level 50, experience 52, map 56, position bit patterns 58/62/66,
system time 70, quest ID 74, quest name 76 (32 bytes), NPC type 108, NPC name
112 (32 bytes). Absent NPC writes type zero and only the name's first NUL.
The NPC runtime handle is exclusive to the separate client packet, not the log.
String writes retain initialized bytes after the first NUL. The writer validates
all string bounds before modifying storage; fresh packet construction explicitly
starts with zeroed storage rather than leaking original stack contents.

Integration: production `prepare_start_log` can now build/capture the full packet
with `quest_start_log_packet`, and `log_start` must send that captured packet.
`QuestStartLogContext` requires user/account/character names and IDs, level,
experience, map, raw position bits and the original system-time value. This is
not admission's monotonic clock. Actual time acquisition and log transport remain
required world providers. No shared canonical header changes were made.

Original-execution fixture: `quest_start_log_oracle.py` executes both complete
original functions. Only OS GetLocalTime, transports and security check are
ports. Client send mutates live user state, proving that the log captured data
before that callback. The native test composes the existing `quest_start`
coordinator with the real new writer and compares both packets exactly.

Validation: `python3 tools/dev.py build`: exit 0; final 131-step build has no
warnings. `python3 tools/dev.py test`: exit 0, **121/121** tests in **37.48 s**.
Both `quest-start-unit` (1,536 traces / 479 starts) and `quest-start-log-unit`
(256 complete log/client pairs plus original calendar packing) pass. Final logs:
`work/quest-start-log-build-final.log`, `work/quest-start-log-test-final.log`.
The obsolete pre-calendar test was cancelled while still waiting for its lock;
it was not counted as validation. `git diff --check`: exit 0.

256 original cases generated successfully
under ignored `work/`; repeated generation is byte-for-byte identical (`cmp`,
exit 0). Original hashes and Python compilation pass. Cases cover all string
lengths through each original maximum, nonzero retained tails, absent/present
NPC, arbitrary 32-bit IDs/types and raw floating-point bit patterns.
Focused Ghidra re-export: `work/exports/06-quest-start/ps_game.exe` (locked
`tools/reverse.py --process`, exit 0). `GetSystemTime` **004cb8e0** calls Windows
GetLocalTime and packs local year/month/day/hour/minute/second using wrapping
`(((((year-16)*16+month)*32+day)*32+hour)*64+minute)*64+second`.
`quest_start_log_time` implements that arithmetic. The expanded oracle executes
this original helper too; only GetLocalTime itself is a clock port. Native
calendar acquisition must supply local calendar fields rather than Unix/UTC time.
Calendar cases include years 0/15/16/1999/2000/2026/65535 and overflow behavior.
SQL lifecycle/integration remains the coordinator's responsibility; no SQL code
or contracts changed in this block.

Block 2 implementation commit: `6c607bfc8267a7e644ec0681cbe1b7e020806e8b`.

Next dependency: canonical `QuestRuntime` is present only in the read-only main
checkout and has no admission/add method yet. The coordinator must integrate the
worker's code with that owner and the item-requirement/grant owners. Proposed
next assignment: a production `QuestStartWorld` adapter under `quest_start`, once
canonical runtime, inventory and log transport interfaces are available in this
worker's base. Do not duplicate QuestRuntime ownership or QuestAddItem here.

## Block 3: actual QuestAdd state transitions

New ownership prefix: `quest_start_add`. `quest_start_add.hpp/cpp` implements
`ps_game.exe`, `CUser::QuestAdd` **0047e010** (focused locked Ghidra re-export:
`work/exports/06-quest-start/ps_game.exe`). Allocation, clock, insertion/ownership
and DB delivery are required ports; none has a default no-op implementation.

On allocation failure, returns false with no clock, list, accounting or DB work.
On success, binds the supplied immutable `QuestDefinition`, sets ID, success and
three counters, reads the clock exactly once, and computes start/end/success
states using wrapping 32-bit `minutes*60000+tick`. Zero maximum/minimum minutes
produce zero end/success ticks respectively; the portable timed flag depends on
maximum minutes, even if the calculated end tick wraps to zero. Admission,
duplicate checks, capacity checks, client sends and items do not belong here.

The original inserts at the active list tail and increments its list count
inside the active-list critical section (**0047e098..0047e0d7**). It increments
player-kill count for end type 3 or mob-kill count for type 2 only after leaving
that section (**0047e0dd..0047e0f3**). Other types do not change either count.
The exact 10-byte DB packet is `0x0a01`, current billing ID (u32), definition ID
(u16), and low 16 bits of maximum minutes. It is sent after insertion/accounting
(**0047e0f9..0047e12c**); return true does not depend on transport acknowledgement.

`QuestStartAddUser` contains references to live billing/counters. Tests include
insertion-boundary callbacks changing these fields; the later count increment
and DB packet observe the changed values. Counts wrap without C++ signed UB.
The insertion port owns append/claim and active-list count only. Pool and
immutable definitions must outlive the inserted quests.

### Exact canonical adapter contract

Read-only main `quest_runtime.hpp/cpp` was inspected. Its current
`QuestRuntime::add_active` BOTH claims/appends AND increments end-type counters.
**Do not call it directly from `QuestStartAddWorld::insert_active`: that would
increment twice and expose incorrect values at the insertion boundary.**

Coordinator change needed: expose a primitive such as
`QuestRuntime::add_active_unaccounted(RestoredQuest&)` that validates/binds pool
ownership and appends once, without changing mob/player counters. Keep the
existing `add_active` accounting behavior for restoration/other callers by
having it call the primitive and then its existing counter logic. Adapter:

- `allocate` -> canonical `QuestRuntimePool::allocate`.
- `insert_active` -> that new unaccounted insertion primitive.
- `QuestStartAddUser` references -> live billing ID,
  `runtime.mob_kill_quests()`, `runtime.player_kill_quests()`.
- `send_database` -> actual DB-agent transport; all visible state is already
  updated when it is called.
- `QuestStartWorld::add` -> bind the positional definition to its canonical
  `QuestDefinition`, then invoke `quest_start_add` once. No copied runtime owner
  or compensating counter decrement is needed or recommended.

This worker did not modify/copy the canonical owner. The insertion port remains
a production dependency until that coordinator change and adapter are supplied.

Validation: `python3 tools/dev.py build`: exit 0, 132 steps, no warnings.
`python3 tools/dev.py test`: exit 0; **122/122** tests in **40.01 s**.
`quest-start-add-unit`: **512** original cases and **438** allocations/real DB
replica adds match. Logs: `work/quest-start-add-build.log`,
`work/quest-start-add-test.log`, `build/Testing/Temporary/LastTest.log`.

The new oracle executes the COMPLETE
original **0047e010** including actual linked-list writes and count increments.
512 fixtures cover retained payload, allocation failure, existing active nodes,
all end-type branches, tick/minute overflow, signed counter boundaries, IDs
0/32768/65535, DB minute truncation and live insertion-boundary mutations.
Snapshots verify initialization before the clock and accounting around insertion
and DB delivery. Native DB packets also apply to the real `QuestReplica`.
Repeated fixture generation is byte-for-byte identical; original EXE/PDB hashes,
Python compilation and `git diff --check` pass. SQL lifecycle/full integration
remain reserved for the coordinator; this block changes no SQL contract/schema.

Block 3 implementation commit: `0e2cb1bfd044ee6ca20e27522c93f1f599de784f`.

Concrete next work within `quest_start_add`: compose the already-recovered
request checks with the actual new add function in original/native fixtures,
removing the previous QuestAdd observation port. This can validate DB delivery
before client/log/start-item effects and consumed-item allocation failure without
copying canonical ownership or changing the earlier milestone implementations.

## Block 4: request/add/start composition evidence

The repeated coordinator assignment's QuestAdd implementation was already
completed in `0e2cb1b` / `693916e`. This block completes its documented next step
without reconstructing it again: original/native full-flow comparison under the
exclusive `quest_start_add` prefix.

`quest_start_add_flow_oracle.py` executes `ps_game.exe`
`CUser::QuestStartRequest` **0047f220**, checks **0047f390..0047f570**, actual
`CUser::QuestAdd` **0047e010**, `CUser::QuestStart` **0047ea70**, and
`QuestGetStartItem` **0047eb70**. QuestAdd is NOT replaced by an observation port.
It initializes the real original CQuest, writes the actual intrusive active list,
updates the original accounting fields and constructs the original DB packet.
Ghidra evidence remains the original complete export and Block 3 focused export.

World lookup/duplicate checks, inventory, allocation, OS clock/locks, transport
and common log-header capture are explicit ports. The common log capture supplies
initialized zero header/storage; the complete original QuestStart log body is
compared against the real native `quest_start_log_packet`. The real common log
header and calendar helper were independently executed/verified in Block 2.
This fixture does not claim to execute the missing inventory/grant providers.

`quest_start_add_flow_unit.cpp` composes `quest_start_request`, `quest_start_add`
and `quest_start_log_packet` directly, applies DB adds to `QuestReplica`, and
compares all packets and the complete ordered event stream. It verifies:

- DB `0x0a01` after insertion/accounting, before client `0x0902`, then log
  `0x0501`, then all nonzero start-item grants (including continued failed grants).
- Consumed start requirements remain consumed if allocation subsequently fails;
  allocation failure emits no DB/client/log/grants.
- Item-pick checks do not consume; rejected active/finished quests skip allocation.
- Use-item admission and actual allocation each read the clock separately,
  including wrap from `0xffffffff` to zero between those calls.
- Canonical definition binding uses the positional record, even with embedded
  definition IDs 0/7/65535; counters, timers and truncated DB minutes match.

640 cases cover all five start types, 253 successful starts and 16 consumed-item
allocation failures in the original. Fixture generation is deterministic and
byte-identical on repetition. It is generated in ignored `work/` before copying.
Original EXE/PDB hashes, Python compilation and `git diff --check` pass.
Native validation: `python3 tools/dev.py build`: exit 0, no warnings.
`python3 tools/dev.py test`: exit 0; **123/123** tests in **39.85 s**.
The new suite matches **640** flows, **253** starts and **16** consumed-item
allocation failures. Final logs: `work/quest-start-add-flow-build-final.log`,
`work/quest-start-add-flow-test-final.log`.
The initial full test attempt failed because the new fixture format omitted an
empty-event token for requests rejected before any provider call. The generator
and parser now use `-`; regeneration and the complete rerun passed. No production
behavior change was needed.

The Block 3 adapter contract is unchanged: insertion must claim/append without
mob/player accounting, and live counter references must bind the canonical owner.
Read-only main still exposes only accounting `QuestRuntime::add_active`; no owner
was copied or modified. Required allocation/inventory/log/transport providers
remain production dependencies, and this evidence does not make the server playable.
SQL lifecycle/full isolated integration remain reserved for the coordinator.

Block 4 evidence commit: `874b9db17d0c9b853d4828dea3d626d212be773f`.

Next task proposal: once the coordinator supplies unaccounted canonical insertion
and integrates the shared runtime/interfaces into this worker's base, assign a
production `QuestStartWorld` adapter under `quest_start_add`. Actual QuestAddItem
and quest completion/removal remain outside this worker's ownership. Further
independent implementation of those providers would duplicate another owner.

## Block 5: actual quest item grants

Expanded exclusive prefix: `quest_start_items`. New native coordinator recovers
both `ps_game.exe`, `CUser::QuestAddItem` overloads: wrapper **0047e620**, inner
**0047e6f0**. Focused Ghidra exports were produced with the locked `--process`
wrapper under `work/exports/06-quest-start/ps_game.exe`. Additional evidence:
`CGameData::GetItemInfo` **00402710**, `CObjectMgr::CreateItem` **0040a510**,
`CItem::Init` **004b9f30**, and inline UID instructions **0047e854..0047e914**.

Recovered behavior:

- Definition lookup happens first. Original lookup accepts type/ID 1..255 and
  requires nonzero type/ID bytes in its table record. Missing definition writes
  null definition and clears bag/slot outputs. No clock/allocation/transport follows.
- Snapshot signed open-bag count after lookup. Search bags 1..count, slots 0..23,
  in order; never inspect equipment bag 0, merge a stack, enforce stack count,
  split quantity or clamp quantity. Full/zero/negative bag count leaves existing
  bag/slot outputs unchanged while still publishing the looked-up definition.
- Allocate exactly once at the first free slot. Failure clears bag/slot and stops;
  it does not retry later holes. Bind definition before the make-time clock.
- Initialize definition type/ID, low count byte, full quality WORD, zero six gems,
  first craft NUL, eleven zero craft additions, packed local make time and make
  type ASCII `Q`. Craft bytes after the first NUL survive pooled reuse.
- Obtain a required server-wide UID; claim/store the pointer in the selected slot
  and set bag/slot outputs before default craft assignment. For nonzero server
  and required wisdom 1..99, craft becomes twenty ASCII zeroes plus NUL; otherwise
  it stays empty. Send exact DB `0x0701` (57 bytes) after this state is published.
- The wrapper sends exact client `0x0205` (16 bytes) only on success, AFTER DB.
  Client type/ID/count are low bytes of the requested arguments; quality is the
  definition's full quality WORD, gems/trailing craft are zero. It does not send
  UID or actual crafted string. Neither function emits an item log, party notice,
  pickup side effect or failure packet. Transport acknowledgments do not govern
  the boolean success return.

### Shared-owner composition and production adapter

Native code reuses `GroundItem`, `GroundItemPool`'s allocation contract,
`initialize_ground_item`, `set_ground_item_craft`, `inventory_add_packet` and
`inventory_receipt_packet`; it does not copy inventory/pool/packet bodies.
Definitions, empty-slot access, allocation, make-time clock, shared UID generation,
raw ownership claim and transports are mandatory ports with no no-op defaults.

Coordinator Actor patch needed: serialized empty-slot access plus a method that
claims an ALREADY allocated `GroundItem` into retained inventory at bag/slot,
without stack merging, reallocation, pickup/party/log/DB/client effects. Keep the
pointer authoritative during `store` (which precedes default craft assignment),
then synchronize any Actor `inventory_` value mirror by the DB send boundary.
The pool must outlive the actor. Do not use `Actor::receive_item` (extra pickup
behavior) or `load_inventory_item` (allocates a second object); do not bypass
`retained_inventory_` ownership by only pushing its public value vector.
Read-only canonical Actor/inventory/pool/UID sources were inspected, not modified.

UID is deliberately a REQUIRED shared-owner port. The oracle executes the real
inline generator, including unsigned tick/deadline comparison, wrapping +2000,
reset to the global baseline counter, 32-bit increment and low-16-bit shift,
packed local-calendar refresh and server byte. Existing canonical `ItemIds::next`
uses a 64-bit deadline, resets counter to zero and throws at 65535; do not claim
it is equivalent at these boundaries or instantiate a separate quest-only UID
owner. The coordinator must reconcile the shared UID contract or supply its
accurate equivalent. Make time is sampled BEFORE UID acquisition; UID refresh
may sample a later local second. Do not collapse those two calendar observations.

### Validation

`python3 tools/dev.py build`: exit 0, 134 steps, no warnings.
`python3 tools/dev.py test`: exit 0; **124/124** tests in **35.11 s**.
`quest-start-items-unit`: **384** original cases / **152** grants matched.
Logs: `work/quest-start-items-build.log`, `work/quest-start-items-test.log`.
`git diff --check`: exit 0.

The new oracle executes BOTH complete original grant
functions, the actual item-definition lookup and original inline UID generator.
Only CreateItem, OS clocks/locks and transports are ports. 384 deterministic
cases include 152 successful grants, alias definition fields vs requested bytes,
quality 0/255/256/65535, counts -1/0/255/256/513, required-wisdom/server branches,
first/last/second-bag holes, full bags, negative/zero bag counts, lookup failure,
allocation failure, retained craft tails and UID baseline/deadline/counter wrap.
Original stack storage starts zeroed; DB packet string tails after the copied
NUL are unspecified stack bytes in the original and deterministic zeros here.
The native shared serializer intentionally zero-initializes those tails.
Native tests compare the complete grant CItem payload, scan/make-time/store
observations and UID results, full DB/client packets and lower-overload output parameters. They compose
the real GroundItemPool and apply each DB grant to real InventoryReplica.
Generator repeat is byte-identical; original EXE/PDB hashes and Python checks pass.
SQL lifecycle/full integration remain coordinator-owned; no SQL contract changed.

Block 5 implementation commit: `5d9e55c87a2ed7cba7f4bc553d377e2de86b4218`.

Next independent block: execute QuestGetStartItem **0047eb70** with all three
actual grant calls and persistent inventory/pool/cache, verifying that earlier
slot claims affect later grants and that failures do not stop remaining grants.
Production owner/interface dependencies above remain explicit.
