# Worker 07: quest end requests and item requirements

## Evidence and implementation

Source: immutable `originals/ps_game/ps_game.exe` (SHA256
`0fb7c9f13b5bcf186118f8bfe73ab214226b93e8aa502b8e6fded4a8312bc547`).
Ghidra's complete `work/reverse/ps_game.exe/{decompiled.c,disassembly.txt}`
exports in the coordinator checkout were read only. Assembly resolves the
misidentified register/stack arguments in the decompilation.

- `CUser::QuestEndRequest` **0047f2a0**: bounds/active lookup and end-kind dispatch.
  Click completion requires a configured end NPC type. Unsupported kinds return.
- `QuestEndChkClickNPC` **0047f630**, `QuestEndChkItemPick` **0047f670**,
  `QuestEndChkKillMOB` **0047f6e0**, `QuestEndChkKillPC` **0047f740**:
  exact NPC type/ID checks; item and kill checks allow zero NPC; kills forward
  NPC argument zero, while click/item checks forward the request NPC value.
  Item completion consumes unless both 0x15d and end NPC type are nonzero.
- `QuestFindItem` **0047f790**: first matching slot in unlocked bags 1..N,
  slots 0..23; optional single-byte decrement (including zero-to-255 wrap),
  DB 0x0702 and client 0x0206 delivery, then release/clear on zero.
- `QuestFindItem` **0047f8e0**: first-match classification among three requirements;
  all totals checked before mutations; original counter overwrite/truncation;
  descending reward-dependent consumption override (0x163/164 + 32*i),
  full removal of matching effect-27 stacks, ordinary consumption in slot order,
  DB-before-client-before-release ordering. Failed checks preserve counters/items.

Public functions and required providers are in `quest_requests.hpp`.
No QuestEnd rewards, removal, selection or success-result implementation is
included. No database connection or world-entry behavior is added.

## Canonical integration needed

No shared canonical header was edited. `QuestRequestInventory` is an explicit
projection of unlocked bags excluding equipment, with 24 slots per bag. The
production adapter must retain original item ownership and project presence,
type/ID/count and item-definition effect, route account ID to DB packets, route
client packets to the requesting user, and release the actual item at the required
boundary. Keep the projection and owning inventory synchronized during callbacks.
`QuestRequestWorld` must resolve the positional definition, active quest and
current-zone NPC, and call the coordinator's real QuestEnd with the provided
NPC argument and success flag. The immutable definition owner must outlive the call.
All service methods are required virtual providers, with no default no-ops.

Worker 06 may use `quest_find_item` for its item-give/pick checks. Coordinator
QuestEnd may use `quest_find_items`; pass the real runtime counter array because
counter updates and reward checks depend on its initial contents. Both original
call sites alias the output to quest+0x1c (0047ec12, 0047f6be); the oracle
retains this alias, including during reward-dependent counter overwrites.

## Validation

- `python3 tools/dev.py build`: exit 0, final log
  `work/quest-requests-build-final2.log`, no compiler warnings.
- `python3 tools/dev.py test`: exit 0, **121/121 passed**, 44.61 seconds;
  `work/quest-requests-test-final.log`. Both new native comparisons pass:
  768 item cases and 1,024 request cases (1,792 original executions).
- Both original-execution generators: exit 0 using `.local/oracle-env/bin/python`.
- `python3 -m py_compile` for both new generators: exit 0.
- `git diff --check`: exit 0.
- Original executable/PDB manifest: `sha256sum -c SHA256SUMS`, exit 0,
  all entries OK (`work/quest-requests-hashes.log`).

`tools/quest_requests_oracle.py` executes original item routines: 768 cases of
unlocked/empty bags, slot scanning, duplicate requirements, byte boundaries,
consumption, effect-27 stacks, reward overrides, packets and final inventory.
`tools/quest_requests_end_oracle.py` executes original dispatch/check instructions:
1,024 cases, observing QuestEnd and injecting NPC/active/item-result providers.
Native request tests compose the actual new item routine with equivalent live
inventory requirements and verify the request consumption flag, rather than substituting a native item-check stub.
Fixtures are generated under ignored `work/` before copying into tests/data.

SQL integration and production adapters belong to the coordinator; no SQL contracts
or fixtures change. This subsystem has no SQL connection and does not make the
server playable.

Implementation commit: `60be7af9ad8e99411e8fb06b39ce59870096481d`.

## Milestone 2: complete request/item/replica flow

New evidence block: `tools/quest_requests_flow_oracle.py` executes the original
`QuestEndRequest` **0047f2a0**, completion checks and **real** `QuestFindItem`
**0047f8e0** together. Unlike the initial dispatch oracle, the item routine is not
replaced. Required external boundaries remain observed providers: active lookup,
zone NPC lookup, transport, locks, item release and coordinator QuestEnd.
The inline effect-27 release is observed at **0047fccd** before the original pool
insertion; ordinary whole-stack release is observed at `CObjectMgr::FreeItem`
**0040a590**. Both expose the retained stack count before ownership release.

512 deterministic scenarios produce 209 QuestEnd calls, 899 DB/client packet
pairs and 680 item releases. The fixture captures counter aliasing, consumption
suppression for pending user selection, ordinary and effect-27 stack removal,
reward-dependent consumption, missing/mismatched NPCs, inactive/bounded quests,
zero NPC, zero unlocked bags and all five unlocked bags. Counts are positive;
zero-to-255 single-item behavior remains covered by milestone 1.

`tests/quest_requests_flow_unit.cpp` compares the entire original event stream,
final inventory and live quest counters. It composes the actual native request
and item routines with `GroundItemPool` and `InventoryReplica`, including UID
preservation, ordered deletion journals and exact pool release counts. Equipment
and a locked bag contain matching sentinel items and must remain unchanged.
The completion provider observes synchronized live inventory and DB cache.
This is an in-process cache/pool integration test, not SQL or production wiring.

### Integration dependency clarified

`Actor::inventory()` exposes value rows, while actual objects/pools are private
`Actor::retained_inventory_` (`actor.hpp`). `Actor::load_inventory_item`
(`inventory.cpp`) retains both copies. A production quest adapter must update both
counts before forwarding DB/client callbacks and release the retained ownership
only after those callbacks, preserving the old count for multi-item whole-stack
release. Evidence: **0047f812**, **0047fc80..0047fccd** and
**0047feb6..0047ff0d**. The new fixture checks that release-visible count exactly.

Do not implement the quest release provider with `Actor::remove_item`: that method
also cleans exchange offers, sends its own 0702, logs removal and may send 0206
(`inventory.cpp`, ItemRemove **0045f260** / ItemRemoveFree **0045f470**).
QuestFindItem directly releases through the pool and does not call ItemRemove.
Doing so would duplicate packets and introduce original-unobserved side effects.

Needed canonical interface (coordinator-owned): serialized access to the retained
`GroundItem` and its pool by bag/slot, a count update that synchronizes both the
value row and retained object without packets, and a direct release/unlink that
removes both copies without exchange/log/transport side effects. An equivalent
world inventory owner is acceptable. Then a `quest_requests` adapter can bind the
existing required providers. No shared Actor header or implementation was edited.

Milestone 2 validation:

- `.local/oracle-env/bin/python tools/quest_requests_flow_oracle.py`: exit 0;
  fixture generated under `work/` before copying into tests/data.
- `python3 -m py_compile tools/quest_requests_flow_oracle.py`: exit 0.
- `python3 tools/dev.py build`: exit 0, no warnings;
  `work/quest-requests-flow-build.log`.
- `python3 tools/dev.py test`: exit 0, **122/122 passed**, 43.82 seconds;
  `work/quest-requests-flow-test.log`. New test output confirms 512 original
  flows, 209 completions and 680 pool releases with matching DB replica/journals.
- `sha256sum -c SHA256SUMS`: exit 0, all entries OK;
  `work/quest-requests-flow-hashes.log`.
- `git diff --check`: exit 0. No SQL contracts/fixtures or lifecycle changed;
  shared SQL validation remains coordinator-owned.

Milestone 2 implementation/evidence commit: `6ea92573fe00c7822f2696a425fa74e4187bf08e`.

Remaining task proposal: coordinator supplies the serialized canonical live-item
access/count/direct-release interface above; then worker 07 implements the
`quest_requests` production inventory adapter. All currently assigned original
request/check/item bodies are reconstructed and compared; core QuestEnd/Select
and canonical Actor ownership remain outside this worker's write scope.

## Milestone 3: player-kill and item-pick progress

Coordinator-assigned new prefix: `quest_requests_progress`. Source evidence remains
immutable `ps_game.exe`, SHA256 above, Ghidra full assembly/decompilation exports.

- `CUser::QuestUpdateKillPC` **00480020**: positive have-PC-quests guard;
  recursive active-list lock, reset/advance live cursor, select the first type-3
  quest only. A saturated first match stops the search. No victim identity,
  minimum-time check or clock call. Increment counter 0 up to 255, send DB 0a05
  then self 0905; reread the live definition/counter and call QuestEnd(0,true)
  only with no end NPC and a satisfied player-kill count (definition+0x155).
  Completion runs while the list lock is held; no later quest is advanced.
- `CUser::QuestUpdatePickItem` **00480390**: type/ID each strictly 1..255;
  existing `ItemQuestLinks` end list, including order and duplicates. Valid empty
  lists still read the clock once. Real QuestFind resolves each ID against the
  current active list. Only type-1 quests pass, and a nonzero minimum-time field
  requires `now <= success_tick` (unsigned comparison, including wrap boundaries).
  Each matching unsaturated counter slot increments by ONE, not stack quantity.
  All three slots are independent, even for identical item targets. Reload live
  definition/counters after callbacks and continue current-object slots even
  when a previous completion unlinks it. Future duplicate links use fresh lookup.
- Auto item completion executes `QuestEndChkItemPick` **0047f670** with NPC zero:
  no NPC lookup; mandatory live inventory query with consume=true, then mandatory
  QuestEnd(0,true) only when the inventory provider succeeds. The provider may
  update the live counter array, just as QuestFindItem does. Pending selection
  does not suppress consumption here because the end NPC type is zero.
- DB progress packet is 10 bytes: 0a05, account DWORD, quest WORD, slot byte,
  counter byte. Self packet is 6 bytes: 0905, quest WORD, slot byte, counter byte.
  ID/count are loaded again after the DB send, so callback counter changes can
  intentionally make the self packet differ from the preceding DB packet.

`quest_requests_progress.hpp/cpp` uses live `RestoredQuest` bindings and required
clock/active-list/lookup/transport/inventory/completion providers, with no default
no-ops. Definitions and the current quest payload must survive the entire update,
including unlink; link storage must remain valid through callbacks. Account
binding remains stable for the serialized operation. Missing definitions throw
rather than silently discarding a broken live binding. Mob-kill progress and
QuestStart/QuestEnd core were not edited or duplicated.

Original comparison: `tools/quest_requests_progress_oracle.py` executes both whole
original routines, the original QuestFind **0047e4b0** and automatic item check
**0047f670**. Ports observe locks, clock, packets, inventory query and QuestEnd.
512 cases per routine cover guards, saturated first PC quest, multiple matching
PC quests, duplicate/missing item links, all three slots, deadline boundaries,
DB counter mutation, client definition replacement, query counter updates,
completion definition replacement, optional unlink and cursor adjustment.
Native comparisons also apply every real packet to `QuestReplica`; callback-only
game mutations are deliberately not invented as additional persistence packets.

Production integration: bind the required active-list lock/cursor providers to
the canonical quest owner, and retain current quest payload after removal until
the update returns. Item-pick callers invoke this once per successful pickup/
receipt event, not once per unit in its stack. The inventory query provider can
compose milestone 1's `quest_find_items` with the actual live counters and the
coordinator's real inventory owner. Actual PC combat and item receipt dispatch
remain coordinator-owned. The separate Actor direct-release/count-sync dependency
from milestone 2 is unchanged; this block does not solve it.

Milestone 3 validation:

- `.local/oracle-env/bin/python tools/quest_requests_progress_oracle.py`: exit 0;
  generated under `work/` before fixture copy. 512 PC-kill + 512 item-pick cases.
- `python3 -m py_compile tools/quest_requests_progress_oracle.py`: exit 0.
- `python3 tools/dev.py build`: final exit 0, no warnings;
  `work/quest-requests-progress-build-final.log`. The initial test declaration
  compile error was corrected before this successful final build.
- `python3 tools/dev.py test`: exit 0, **123/123 passed**, 45.54 seconds;
  `work/quest-requests-progress-test.log`. New suite confirms 1,152 DB/client
  updates, 596 inventory queries and 363 completions with exact callback/unlink/
  cursor behavior and real QuestReplica delivery.
- `sha256sum -c SHA256SUMS`: exit 0, all entries OK;
  `work/quest-requests-progress-hashes.log`.
- `git diff --check`: exit 0. No database/schema/SQL fixture changes; central SQL
  integration remains coordinator-owned.

Milestone 3 implementation commit: `3408c7824d00fb0c1edb0d04a1a3dd46106e0411`.

Next integration proposal: coordinator binds the canonical active quest cursor/
retained payload and connects actual PC-death/item-receipt dispatch to these
entry points, or assigns a further independent quest progress routine. The Actor
inventory adapter still awaits the separately specified canonical ownership API.
No additional assigned progress body remains unreconstructed in this block.

## Milestone 4: timed quest expiration

Coordinator-assigned prefix: `quest_requests_timers`. PDB-derived Ghidra function
catalog resolves **CUser::UpdateQuest 0048d1a0** (103 bytes) as the timed lifecycle
routine; there is no separate named CUser quest-expiry routine in that catalog.
Evidence: immutable `ps_game.exe`, full `functions.tsv`, `disassembly.txt` and
`decompiled.c` exports; executable SHA256 remains the value above.

Recovered behavior in `quest_requests_timers.hpp/cpp`:

- Always acquire the user's active-list lock, even when the list is empty. Reset
  the shared active cursor to its first quest. No clock call inside UpdateQuest:
  the caller's supplied DWORD tick stays fixed across all callbacks.
- Definition DWORD **+0x128** (maximum time) must be nonzero. Its magnitude is not
  used; the absolute quest DWORD **+0x14** (`RestoredQuest::packet.end_tick`) is
  compared with the supplied tick. Expire on **unsigned end_tick <= tick**,
  including equality, zero and unsigned wrap boundaries. Do not use signed
  elapsed arithmetic, minimum/success time, or `packet.timed` as this guard.
- Call the required real QuestEnd provider with **NPC=0, success=false**, under
  the active-list lock. The timer itself emits no packet, log or direct removal.
  The original false branch of QuestEnd **0047ec40** sends client **0903**
  (22 bytes), calls QuestRemove **0047e150**, then game-log **0502** (150 bytes).
  QuestRemove sends DB **0a02** (9 bytes) after finished insertion or FreeQuest
  **0040a820**. Repeatable removals use DB success=2; other failures use 0.
- After that provider returns, read the owner's **current shared cursor** and
  advance it. Do not prefetch a successor or reread the removed quest. Removal
  moves the current cursor to predecessor/head; a reentrant lookup in transport
  can replace it again, deliberately changing/skipping later timer visits.

Caller evidence (read-only, not new scheduling code): `CZone::UpdateQuest`
**00422320** holds its user-list lock and invokes the whole CUser routine per
member. `CWorldThread::Update` **00407390**, **0040771d..0040775a**, gates that zone
pass on DWORD deadline +0x90 and stores supplied tick + **60000** afterward.
Thus integrate at the original minute scheduling boundary; do not invent a
per-quest clock read or per-frame timer cadence from this small routine.

### Complete original execution and callback evidence

`tools/quest_requests_timers_oracle.py` executes whole original UpdateQuest,
**real QuestEnd failure**, **real QuestRemove** and **real FreeQuest** in 512
scenarios. Only locks, transports, SetGameLogMain and the cookie helper are ports.
The log oracle compares initialized 0502 opcode/quest ID, not uninitialized
log-main bytes. Traces include exact client/DB packets, recursive lock sequence,
finished/recycled order and shared cursor. Explicit transport callbacks modify a
future quest's deadline/definition (M), move the shared cursor to first/last live
quest (J), or externally unlink a future quest without a persistence command (X).
That last case is a declared callback mutation, not an invented QuestEnd action.

Reference coverage: 609 failure completions, 609 client/DB/log deliveries,
294 real heap releases, 202 cursor-changing callbacks, 145 future timer changes
and 38 external unlinks. The native timer composes an explicit observed
QuestEnd/owner **test port**, not a production duplicate of the coordinator core.
It compares every trace and final list/deadline/definition/cursor state and applies
all DB failure transitions to the real `QuestReplica`. Test `packet.timed` flags
are deliberately the opposite of the definition guard to catch wrong binding.
No canonical QuestRuntime or Actor file was edited.

### Canonical adapter contract and remaining work

Bind `QuestRequestsTimerWorld::first_active_quest/next_active_quest` directly to
the same canonical `QuestRuntime::first_active/next_active` used by QuestFind and
QuestRemove, with the user's recursive quest-list lock covering the whole pass.
Do not snapshot active() into a separate iteration vector or maintain a second
cursor. Bind `end_quest` to the coordinator's real QuestEnd failure path: it must
own client failure, actual removal, DB persistence and game-log delivery, without
additional timer packets. The supplied tick comes from the zone/world scheduler.
The existing canonical cursor API is sufficient; no new canonical field is
requested. The actual recursive lock, end provider and scheduler wiring remain
required. The Actor inventory adapter dependency from milestone 2 is separate
and unchanged. This does not implement core QuestEnd/Select or playable gameplay.

Milestone 4 validation:

- `.local/oracle-env/bin/python tools/quest_requests_timers_oracle.py`: exit 0;
  generated first under `work/`, then copied into tests/data.
- `python3 -m py_compile tools/quest_requests_timers_oracle.py`: exit 0.
- `python3 tools/dev.py build`: exit 0, no compiler warnings;
  `work/quest-requests-timers-build.log`.
- `python3 tools/dev.py test`: exit 0, **124/124 passed**, 44.33 seconds;
  `work/quest-requests-timers-test.log`. New suite confirms 512 complete original
  lifecycles and 609 failure completions, matching cursor/removal/callback traces
  and real QuestReplica failure transitions.
- `sha256sum -c SHA256SUMS`: exit 0, all entries OK;
  `work/quest-requests-timers-hashes.log`.
- `git diff --check`: exit 0. No SQL contracts, schema or fixtures changed;
  final isolated SQL integration remains coordinator-owned.

Milestone 4 implementation commit: `7669d2c64e367629270c35e29340585ba039acb2`.

Next task proposal: coordinator binds this timer to canonical QuestRuntime and
real QuestEnd/zone scheduler, or assigns a new independent quest subsystem. No
additional named CUser timed lifecycle body was found in the supplied catalog.
The Actor inventory ownership dependency remains separately pending.

## Milestone 5: QuestEnd and explicit selection

The coordinator explicitly reassigned the formerly excluded core functions to
worker 07 under `quest_requests_completion`. Historical exclusions above describe
earlier scope; this block now implements `CUser::QuestEnd` **0047ec40** and
`CUser::QuestEndSelect` **0047ebe0**, preserving existing reward/removal owners.

`quest_requests_completion.hpp/cpp` recovers:

- Abandon/fail bypasses time gates. Client 0903 (22 bytes, context and initial
  quest ID, zero result fields), actual owner removal with success=false, log-main
  fill, then 0502 game log (150 bytes). The log ID is loaded AFTER log-main fill
  and may differ from the client ID. The name comes from the definition captured
  on entry, even when callbacks replace the quest's live definition. Removal may
  finish or recycle; its retained payload must remain readable through logging.
- Success minimum-time gate: capture the initial definition; when minimum minutes
  is nonzero read clock, then reload live start_tick and compare the wrapped DWORD
  `(minimum_minutes * 60000 + start_tick) <= now` unsigned. No deadline/success_tick
  substitution. A callback replacing the definition does not replace the captured
  gate/selection policy; the reward owner separately reads the live definition.
- Definition 0x15c == 1 immediately delegates variant 0, ahead of user selection.
  Otherwise nonzero 0x15d sets live success=1 before sending four-byte 0907 and
  retains the active quest for later selection.
- Automatic variant path performs the original additional, discarded clock read.
  Search variants 2,1,0 descending. Item quests use byte selector 0x163 and count
  0x164 (+32*i), selector 1..3. Mob quests use WORD selector 0x160 and count 0x162,
  selector 1..2. Require nonzero selector/count and enough live counter value.
  Variant 0 is unconditional fallback, even when its own requirements do not pass.
  Other end kinds fall through to variant 0. Reward context is forwarded unchanged.
- Explicit selection does a fresh active lookup (sharing the original cursor),
  accepts any nonzero live success flag and requires selection < definition 0x15d.
  Only nonzero end NPC type triggers mandatory inventory query/consumption with
  the actual quest counter array. Failed query stops; successful query delegates
  the selected variant with context ZERO. It does not repeat minimum-time or
  deadline gates, and does not recheck flags/definition after the inventory call.

### Actual canonical owner composition

`QuestCompletionOwners` is a typed live adapter: canonical
`QuestRuntime::find_active/remove` own lookup, cursor and removal, while its Award
callable invokes canonical `award_quest_reward` with live user/money/reward-world
bindings. It never copies reward, pool, finished-list or persistence logic. Ports
for clock, inventory, self transport and game logs remain mandatory. Earlier
request/progress/timer providers can now call `quest_requests_complete` directly.

This worker's prepared base predates the canonical owner files. To test actual
composition without modifying or importing a second checkout, the read-only
`tools/quest_requests_completion_owners.py` snapshots those four canonical source/
header files into ignored `work/quest-requests-completion-owners`. Private CMake
compiles that snapshot only when canonical headers are absent locally. Central
integration instead links its normal `eternity_game` owners. Nothing is copied
into tracked canonical paths and no main checkout file is written. Snapshot
manifest: `work/quest-requests-completion-owners.sha256`; source owner contents:

- quest_runtime.hpp: `4fa31cdfb4dcaad96094359db94e704f033e43ee14515476206835f60c04ce7d`
- quest_runtime.cpp: `8f029694af640ab9bf38faea4828a4e8aa0991ba582da11f68efc3dd621fee25`
- quest_rewards.hpp: `bf64bad65f367b405f9f144ec5bcd0b5851f38dfcdf69f0192a7fab102933d25`
- quest_rewards.cpp: `5760073a3783c45df23989c0dc52bddb147815c83a9a769de2f965390616ed86`

`tools/quest_requests_completion_oracle.py` executes complete original End/Select
with REAL QuestSuccessResult **0047ee40**, QuestFind **0047e4b0**, QuestRemove
**0047e150** and FreeQuest **0040a820**. Observed external ports are clock, locks,
transports/log main, actor experience, reward item receipt and inventory query.
The native test composes the actual canonical owners and real QuestReplica, not
reward/removal stubs. 768 End + 768 Select paths cover wrapped minimum gates,
all variants, pending flags, absent/out-of-range selection, inventory failures,
clock definition/start mutation, client definition/success mutation, inventory
counter/definition mutation, post-removal ID mutation and DB callback cursor
lookup. Undefined game-log string tails after their first NUL are normalized to zero;
all initialized fields and string bytes compare exactly. QuestFind stack frames
can populate those undefined tails even when the entry stack was zeroed.
Reference outcomes: 377 rewards (295 variant 0, 49 variant 1, 33 variant 2),
231 pending-selection notices, 86 abandonment failures and 72 inventory queries.

### Minimal canonical/production requirements

No new QuestRuntime or Actor field is needed for core completion. Bind a serialized
quest owner (or the same recursive quest lock for cross-thread callers), its DB
sink, and the actual reward-world implementation. Retained/pool-reused quest
payload and captured definition storage must survive the complete callback/log
sequence. Bind inventory query to the prior actual `quest_find_items`, passing
live counters, consuming=true. Keep live money and original account/user routing
for `award_quest_reward`; fill_game_log_main must preserve bytes 0..1 (opcode).
All transports, experience, item receipt and log context remain required services.
Valid definition selections fit the three stored variants; original Select only
checks the data-defined selection count, while canonical award owner rejects a
variant >=3 rather than reading outside its definition. No new clamp is added.

The separate Actor retained inventory/count/direct-release interface from
milestone 2 still blocks the production inventory adapter. This block supplies
core completion and actual owner composition, not combat dispatch, world entry or
playability. No SQL lifecycle/schema changes are included.

Milestone 5 validation:

- `python3 tools/quest_requests_completion_owners.py`: exit 0; read-only canonical
  snapshots match the four content hashes above, under ignored work/ only.
- `.local/oracle-env/bin/python tools/quest_requests_completion_oracle.py`: exit 0;
  generated under work/ before copying; 1,536 complete original owner paths.
- `python3 -m py_compile` for both new tools: exit 0.
- `python3 tools/dev.py build`: exit 0, no compiler warnings;
  `work/quest-requests-completion-build.log`.
- `python3 tools/dev.py test`: final exit 0, **125/125 passed**, 45.18 seconds;
  `work/quest-requests-completion-test-final.log`. The initial run passed all 124
  existing suites but exposed undefined original log-string tails in Select case
  48; after explicit tail normalization the full suite passes. That initial run
  is not counted as passed. No native behavior was changed for undefined bytes.
- New test uses actual canonical QuestRuntime/QuestReward code, plus QuestReplica,
  and verifies full packet/log traces, callbacks, money, retained quest payload,
  active/finished/free ownership and shared cursor.
- `sha256sum -c SHA256SUMS`: exit 0, all originals OK;
  `work/quest-requests-completion-hashes.log`.
- `git diff --check`: exit 0. No schema/SQL fixtures or lifecycle changed;
  final isolated SQL integration stays coordinator-owned.

Milestone 5 implementation commit: `f40c155cae0b5e628b43cb34dea9c59e3a481261`.

Next task proposal: instantiate the typed completion adapter on the canonical
serialized user owner and connect prior end-request/progress/timer providers to
it. The remaining mandatory inventory bridge depends on the Actor interface
specified separately in milestone 2; real reward item receipt and transport/log
context must also be bound. No independent assigned End/Select body remains
unreconstructed; further canonical integration requires coordinator ownership.

## Milestone 6: concrete Actor consumption and live quest world

Coordinator-assigned prefix: `quest_requests_actor`. The prior Actor ownership
blocker is resolved by worker 02's pinned **c02b69c227951a9d1961170385176b443237e863**
(`302dcb2` report): borrowed retained item/pool, synchronized count and direct
release primitives on the existing Actor. Shared Actor/inventory files remain
read-only; this block does not duplicate their ownership logic.

`QuestRequestsActorInventory` now runs the existing actual `quest_find_item` /
`quest_find_items` routines against Actor's unlocked bags 1..max_bags and 24 slots.
It borrows the actual pooled allocations and rejects value-only inventory rows
instead of fabricating pooled ownership. Definition **nRealType +0xa0**, verified
in Ghidra `stItemInfo`, maps to canonical `ItemDefinition::real_type`; value 27 is
the full matching quest-stack removal case (not an Effect1/Special field).

For every 0702, it synchronizes BOTH retained count and Actor value mirror to the
routine's current projection BEFORE the mandatory actual DB transport. It does
not subtract packet amount a second time. Multi-item whole-stack projection
retains its old count; single-item decrement exposes its decremented count.
After DB returns it revalidates pointer/pool/UID/definition/count, routes 0206 through the
actual Actor scene, revalidates again, then uses direct
`Actor::release_inventory_item(expected_pointer)` after client delivery. There is
no Actor::remove_item, Actor::receive_item, allocation, exchange cleanup, removal log, second
inventory owner or implicit DB replica change in this adapter.

`QuestRequestsActorWorld` concretely binds the same Actor, canonical QuestRuntime,
immutable definitions and required root ports to existing request, PC/item
progress, timer and core completion interfaces. Lookup and traversal use the SAME
QuestRuntime cursor. Progress and timer completion call actual QuestEnd; explicit
selection and reward delegation use actual canonical `award_quest_reward` with
Actor's live money and account routing. Actual QuestRuntime owns removal,
finished/recycle storage and its DB command. No fake completion/query provider
is installed in this production class.

### Dependency preparation and lifetime contract

`tools/quest_requests_actor_owners.py` snapshots pinned Actor header/inventory
source from the exact worker 02 commit using read-only git show, plus canonical
quest_runtime/quest_rewards from main read-only, under ignored
`work/quest-requests-actor-owners`. Manifest records each SHA256/source. Private
CMake replaces the OLD inventory.cpp source with that exact dependency and uses
the matching Actor header overlay only when retained_inventory_item is absent.
Central builds use their normal canonical Actor/inventory and quest owners.
No main/other-worker checkout or tracked shared canonical path is edited.

Callers retain Actor, quest payload, definitions, ports and actual pools through
all callbacks and execute on the same serialized owner/recursive quest lock.
Searched bag contents must not be released/reused or mutated by transport
callbacks; identity/count changes fail explicitly rather than sending a stale
client notice or releasing a replacement. Borrowed pointers are not owning
leases, and pointer+UID cannot prove absence of same-address/same-UID ABA reuse:
the retain/no-reuse contract is required. Reentrant inventory consumption through
the same adapter is explicitly rejected to prevent projection invalidation;
reentrant quest lookup/cursor movement is supported and tested. Unlocked-bag
configuration stays stable during the operation; all searched slots are revalidated
after DB/client callbacks. Actual root transports, actor
experience, reward item receipt, NPC/zone lookup, logs/calendar and locks remain
required ports; ActorScene remains the actual self-transport owner.

### Original validation coverage

- Reuse all 512 prior complete original request/consumption flows through the
  NEW production inventory adapter and actual Actor, pool and InventoryReplica,
  rather than a manually synchronized test projection. Exact bytes, retained
  release-visible count, live quest counters and equipment/locked-bag sentinels
  remain compared. Original reference has 680 releases.
- New `tools/quest_requests_actor_oracle.py` executes 384 full original chains:
  64 each of EndRequest, EndSelect, failure, UpdateQuest, item progress and PC
  progress. Item query, core End/Select, reward result, QuestFind, QuestRemove and
  FreeQuest are real original routines. External boundaries are clock/locks,
  transport, NPC, actor experience, reward item receipt and game-log header.
  Native comparison instantiates the production QuestRequestsActorWorld with
  ACTUAL Actor, QuestRuntime, reward owner, pooled items and both DB replicas.
- New traces contain 1,281 DB commands, 1,107 client deliveries, 415 inventory
  releases and 303 final result logs; 82 DB callbacks perform reentrant live
  quest lookup. Failure, pending selection, insufficient inventory, consumed
  inventory followed by an unmet minimum-time gate, repeatable recycling and
  post-removal cursor continuity are retained. Undefined original game-log name
  tails alone are normalized after NUL, as in milestone 5.
- DB callbacks observe synchronized Actor pointer/mirror before persistence;
  client callbacks observe already-updated InventoryReplica and the still-owned
  item with original full-stack count. Final UID deletion order matches pool
  releases, and Actor pointer/mirror/cache counts match. A native stale-identity
  callback test explicitly removes the affected allocation after DB and requires
  rejection before stale client delivery/release. No production fallback is used.

Milestone 6 validation:

- `python3 tools/quest_requests_actor_owners.py`: exit 0; exact pinned dependency
  plus canonical quest snapshots and manifest prepared under ignored work/.
- `.local/oracle-env/bin/python tools/quest_requests_actor_oracle.py`: exit 0;
  generated under work/ before fixture copy. Prior 512-flow fixture reused intact.
- `python3 -m py_compile` for both new tools: exit 0.
- `python3 tools/dev.py build`: both dependency-overlay and final guard rebuilds
  exit 0. Final `work/quest-requests-actor-build-final.log` has no warnings. The
  initial overlay rebuild contains pre-existing guild-file warnings from this
  old prepared base; no Actor adapter warning/error occurred.
- `python3 tools/dev.py test`: exit 0, **126/126 passed**, 39.26 seconds;
  `work/quest-requests-actor-test.log`. New suite reports 512 original consumption
  flows + 384 complete live owner chains and **1,095 direct pool releases**,
  with exact replicas/mirrors/callbacks/cursors. Stale identity rejection passes.
  No failed build or test run occurred in this milestone.
- `sha256sum -c SHA256SUMS`: exit 0, all originals OK;
  `work/quest-requests-actor-hashes.log`.
- `git diff --check`: exit 0. No SQL lifecycle/schema/fixtures changed. Actual
  packet application to InventoryReplica and QuestReplica is tested in process;
  shared full isolated SQL integration remains coordinator-owned.

Milestone 6 implementation commit: `cb7ebf58e5428ec713d25057a98dfcc81194414d`.

Next integration proposal: coordinator instantiates QuestRequestsActorWorld on
its retained user executor and connects request/progress/timer dispatch to this
same object, supplying real ports and reward-world receipt/experience services.
Root inventory restoration must create retained pooled Actor entries (existing
load_inventory_item or real raw claim), not constructor value-only rows. No
additional canonical inventory field/interface is requested. The historical
Actor blocker is resolved; root owner attachment and real external providers
remain open. This milestone does not claim world entry or server playability.
