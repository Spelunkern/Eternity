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
