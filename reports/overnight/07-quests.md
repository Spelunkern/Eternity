# Overnight 07: quest and NPC integration

The existing `VerticalQuests` adapter binds quest acceptance, progress, NPC
completion and timer expiration to the retained Actor, QuestRuntime, ZoneNpcs,
NpcPool and required service ports. This milestone extends its integration test
with a timed quest accepted from a real zone NPC. It verifies the deadline just
before and at expiry, actual runtime and quest replica removal, mob-quest count,
client failure delivery, database and log ordering, and balanced quest locks.
No service callback is replaced with a successful fallback.

Original evidence: `ps_game.exe`, `CUser::QuestStartRequest` 0047f220,
`CUser::QuestAdd` 0047e010 and `CUser::UpdateQuest` 0048d1a0, `CUser::QuestEnd`
0047ec40, `CUser::QuestRemove` 0047e150. Prior original-execution timer and
completion comparisons are described in `reports/parallel/07-quest-requests.md`.
The timer is driven by the supplied tick; production still needs the original
zone/world scheduling boundary and actual service providers.

SQL integration was not run: this worktree has no `.local/runtime.env` or
`.local/sqlserver.env`, and SQL lifecycle is centrally coordinated. No database
was started or modified. Original-client end-to-end play has not been validated.

Next work: bind the adapter to the live user executor and actual NPC interaction,
combat/item events and minute scheduler; supply the real transport, reward,
logging, clock and recursive lock providers. Those root integration interfaces
are `VerticalQuestPorts` and the existing `VerticalQuests` entry points. Preserve
owner lifetimes and serialization as documented in `vertical_quests.hpp`.

Initial checkpoint validation: `git diff --check` passed. `python3 tools/dev.py build`
waited over six minutes for the shared build lock and was interrupted without
acquiring it; the new test has not been compiled or run. On resume, run
`python3 tools/dev.py build` followed by
`python3 tools/dev.py test -R '^vertical-quests-unit$'`, fix any failures, and
record the results. This is a checkpoint, not a validated milestone.

## Packet quest integration

`ps_game.exe` `CUser::PacketQuest` **0046be10**, instructions
**0046be10..0046be8e**, cancels exchange/action and ends the shop before reading
the opcode. Requests **0902/0904** use target DWORD at offset 2 and positional
quest WORD at offset 6; **0903** uses the same fields for end; **0907** uses a
quest WORD at offset 2 and reward byte at offset 4; **0908** looks up the active
quest and invokes `CUser::QuestEnd` **0047ec40** with NPC zero and failure.
Unknown quest-family opcodes close with reason 9. The adapter now routes these
requests into the existing `VerticalQuests` instance, including real runtime
lookup and failure completion for abandonment. Packet length checks are a native
framing boundary, not recovered original validation. Field reads occur after the
required synchronous action and shop callbacks, matching the original order.

Coordinator integration: route the quest opcode family on the same serialized
user executor to `VerticalQuests::receive`; provide `VerticalQuestPacketActions`
with the real action/exchange cancellation, shop end, and connection close
(reason 9) operations. Keep the actor, packet bytes, zone, NPC pool, quest owners
and service providers alive through callbacks. The method reports malformed
frames to the caller; the transport owns its framing policy. Attach the same
`VerticalQuests` instance to mob-kill, player-kill, item pickup and zone timer
events, and provide the real `VerticalQuestPorts` services. No callback is
supplied as a synthetic success.

The focused syntax check `docker run --rm --mount
type=bind,source="$PWD",target=/src,readonly --workdir /src
eternity-dev:bookworm g++ -std=c++20 -Iinclude -fsyntax-only
src/game/vertical_quests.cpp tests/vertical_quests_unit.cpp` exited 0. An earlier
host `g++` syntax check failed because host `sql.h` is not installed; the
container check supplies the project's development headers.

Packet bridge validation: `git diff --check` and the container syntax check above
passed. `python3 tools/dev.py build > work/overnight-07-build.log 2>&1` waited
more than seven minutes for a shared slot and was interrupted (exit 130) before
it acquired the lock; no native build or executable test completed in that
block. Run the build and focused test commands listed above on resume. The
packet-level tests cover both start opcodes, NPC completion, abandonment,
malformed and unknown frames, other opcode families, and callback-time opcode
mutation; those assertions were unexecuted at that checkpoint. SQL integration remains unavailable
without the centrally managed runtime configuration. This commit is a scoped
checkpoint, not a claim of validated or playable behavior.

## Original packet dispatch execution

`tools/vertical_quests_packet_oracle.py` runs original `CUser::PacketQuest`
**0046be10** and its jump table from `ps_game.exe` SHA256
`0fb7c9f13b5bcf186118f8bfe73ab214226b93e8aa502b8e6fded4a8312bc547`.
Only the called action, shop, quest and connection boundaries are intercepted
in the emulator's private image; the disk binary remains unchanged. Thirteen
cases assert the exact user, target, quest and selection arguments, action/shop
ordering, absent and present active quest abandonment, close reason 9 and an
opcode mutated by the shop callback. Output is retained in
`work/vertical-quests-packet-oracle.log`. The vertical integration fixture also
checks the failure client packet is already delivered at the DB-removal callback.

Validation: `.local/oracle-env/bin/python tools/vertical_quests_packet_oracle.py`
exited 0 for all thirteen original execution cases; `python3 -m py_compile
tools/vertical_quests_packet_oracle.py` and `git diff --check` exited 0. These
oracle assertions validate the original dispatch contract; they do not replace
the native integration test or provide live services.

The live-owner fixture additionally drives a selectable NPC-gated reward through
acceptance, mob progress, completion request, invalid choice and valid choice,
then checks actual reward/removal and replica persistence. The timed-failure
fixture checks the client packet at the DB removal callback. These test additions passed in the subsequent focused native test run.

The resumed full native build `python3 tools/dev.py build` exited 0; its complete
log is `work/overnight-07-build-resume.log`. It emitted existing warnings in
unrelated sources, including guild files. SQL integration remains unavailable
because this worktree lacks the required centrally managed environment files.

The focused native suite `python3 tools/dev.py test -R
'^(vertical-quests-unit|quest-requests.*-unit|quest-start.*-unit|mob-quests-unit)$'`
exited 0: **18/18 passed**, including the packet, timer, NPC, mob and reward
selection integration case. Full output is in
`work/overnight-07-quest-tests.log`. No SQL integration or original-client
end-to-end session was run. The next work remains the live user executor and
actual service/event bindings described above.

## Mob death to live quest owner

`VerticalQuestMobDeath` wraps the existing `MobDeathWorld` and forwards its
actual death, reward, drop, party, obelisk and cleanup services. Its quest-kill
boundary resolves the recipient's retained `VerticalQuests`, verifies that the
actor ID matches the credited `MobDeathUser`, and invokes `kill_mob` with the
live `MobRuntime::type` and real `MobQuestLinks`. It does not call the wrapped
quest-kill provider as well. The resolver must retain each user and quest owner
through synchronous death callbacks, including party member iteration; use this
same wrapper as the `MobDeathWorld` passed to `mob_death` on the serialized
world executor. A missing or mismatched owner must fail rather than claim success.

Evidence: `ps_game.exe` `CMob::Death` **004910f0**, including solo and group
reward calls around **004912xx..004914xx**, invokes
`CUser::QuestUpdateKillMOB` **00480140**; instructions **0048015b..00480166**
read the mob WORD at +0x220, represented by `MobRuntime::type`. The existing
`mob_death` coordinator owns recipient eligibility and original callback order.
The integration fixture executes real `mob_death` with the real damage ledger,
quest runtime, actor, quest replica and NPC-gated quest; it checks solo reward,
drop, progress and cleanup order, distinct mob type versus definition ID, and
rejection of a mismatched recipient. Existing original-execution mob death and
mob quest tests provide broader party and progress coverage.

The development-container syntax check `g++ -std=c++20 -Iinclude -fsyntax-only
src/game/vertical_quests.cpp tests/vertical_quests_unit.cpp` exited 0. The full native build `python3 tools/dev.py build` exited 0; see
`work/overnight-07-mob-bridge-build.log`. The focused command
`python3 tools/dev.py test -R
'^(vertical-quests-unit|mob-death-unit|mob-quests-unit|quest-requests.*-unit)$'`
exited 0, **10/10 passed**, including the new real mob-death integration;
see `work/overnight-07-mob-bridge-tests.log`. SQL integration remains unavailable without the centrally managed
runtime configuration; no database lifecycle action was taken.

## Party mob-death integration coverage

The `vertical-quests-unit` fixture now runs `mob_death` through the real
`VerticalQuestMobDeath` bridge with three separate Actors, QuestRuntimes, quest
replicas and client scenes. Two in-range members receive their own progress;
one out-of-range member remains unchanged. The fixture checks party lock,
per-member experience/progress ordering, group drop and cleanup, and verifies
that a mismatched resolver throws while the death coordinator releases the party
lock and does not advance the wrong recipient. This uses the recovered
`ps_game.exe` `CMob::Death` **004910f0** group dispatch and
`CUser::QuestUpdateKillMOB` **00480140**; `mob-death-unit` retains the separate
original-execution comparison of that coordinator. No production services are
replaced with a success fallback.

`python3 tools/dev.py build` exited 0; log:
`work/overnight-07-party-build.log`. The focused test command
`python3 tools/dev.py test -R
'^(vertical-quests-unit|mob-death-unit|mob-quests-unit|quest-requests.*-unit)$'`
exited 0 with **10/10 passed**; log `work/overnight-07-party-tests.log`.
SQL integration cannot run because the centrally managed
runtime configuration files are absent in this worktree; no database was started
or modified. Actual root user/party resolver and service wiring remain required.

## Item and player-kill progress through actual owners

The `vertical-quests-unit` fixture now accepts an item quest from the actual zone
NPC, delivers two item-progress events through `VerticalQuests::pick_item`, and
turns it in at that NPC. It verifies the real quest runtime and replica counters,
rejection of a wrong NPC, pooled Actor inventory ownership, consumption and
inventory replica deletion, DB packet order and quest reward/removal. A separate
NPC-gated player-kill quest advances twice through `kill_player` and verifies
both quest owners, completion, accounting and persistence order. These are
in-process integration cases, not real combat or pickup dispatch.

Original evidence: `ps_game.exe` `CUser::QuestUpdatePickItem` **00480390**,
`CUser::QuestEndChkItemPick` **0047f670**, multi-item `QuestFindItem`
**0047f8e0**, `CUser::QuestUpdateKillPC` **00480020** and
`CUser::KillPCTo` **0045aed0**. The existing original-execution comparisons in
`quest-requests-progress-unit` and `quest-requests-actor-unit` cover progress
and inventory callback behavior in more scenarios. Root still needs the actual
item and player-death event owners before these entry points can be wired; no
pickup event timing or synthetic success was invented here.

Validation: `python3 tools/dev.py build` exited 0, log
`work/overnight-07-progress-build.log`. `python3 tools/dev.py test -R
'^(vertical-quests-unit|quest-requests.*-unit|quest-start.*-unit|mob-quests-unit)$'`
exited 0, **18/18 passed**, log `work/overnight-07-progress-tests.log`.
`git diff --check` passed. Isolated SQL integration was not run because the
centrally managed runtime configuration is absent; no SQL lifecycle action was
taken. Original-client end-to-end play remains unverified.

## Zone quest timer integration

`vertical_quests_update_zone` now connects the real `ZoneUsers::visit_all` live
membership traversal to each member's retained `VerticalQuests::update_timers`.
The required `VerticalQuestZoneUsers::retain_quests(Actor&)` provider returns an
aliasing/shared lease that retains that exact actor, scene, quest adapter and
user-owned state through callbacks; it must not mutate membership while resolving.
The adapter rejects a mismatched Actor object rather than updating another user.
The caller retains the zone and executes this on the serialized world executor.

Original evidence: `ps_game.exe` `CZone::UpdateQuest` **00422320** walks the
shared user-list cursor under its recursive lock and invokes `CUser::UpdateQuest`
**0048d1a0** with one supplied tick. `CWorldThread::Update` **00407390** at
**0040771d..0040775a** gates the zone pass on its DWORD deadline and sets it to
current tick + 60000 after the pass. The root must call this adapter at that
existing cadence for every active primary and dynamic zone under the actual
world-list lock. This adapter does not create a scheduler or omit dynamic zones
on the root's behalf.

The native integration fixture starts three timed quests with distinct real
Actors, QuestRuntimes and replicas. It checks mismatch rejection, no expiration
before the deadline, actual expiration at the deadline, client/DB owner updates,
and a callback that detaches the current zone user and appends a new one while
preserving live traversal order and owner lifetimes. The full build
`python3 tools/dev.py build` exited 0; see
`work/overnight-07-zone-timers-build.log`. The focused command
`python3 tools/dev.py test -R
'^(vertical-quests-unit|quest-requests-timers-unit|zone-users.*-unit|quest-requests-actor-unit)$'`
exited 0 with **4/4 passed**; see `work/overnight-07-zone-timers-tests.log`.
`git diff --check` passed. SQL integration remains unavailable without the centrally managed runtime
configuration; no SQL lifecycle action was taken. Original-client end-to-end play
is unverified.

## NPC interaction request

`vertical_npc_request` implements `ps_game.exe` `CUser::PacketNpc` **00467510**:
request **0e04** looks up the target DWORD at offset 2 in the real `ZoneNpcs`;
a found, different NPC replaces the same `VerticalNpcInteraction` used by
movement and zone leave, decrementing/incrementing the actual NPC use counters
with DWORD wrapping. Missing or already-selected NPCs preserve state. Other
NPC-family opcodes call the required connection-close provider with reason 9.
Short-frame reporting is the native transport boundary, not original length
validation. No action/shop callbacks are added to this original handler.

Root interface: dispatch NPC-family packets to `vertical_npc_request` with the
actual Actor, current `ZoneNpcs`, retained per-user `VerticalNpcInteraction`,
and a real `VerticalNpcPacketActions::close_connection` implementation. The
same interaction object must go to `vertical_move_interactions` and
`zone_leave_interactions`. Serialize requests, movement and NPC retirement;
retain any currently selected NPC until this interaction is cleared or changed.
The packet handler does not own that NPC's lifetime beyond the call.

`tools/vertical_npc_packet_oracle.py` executes the original instructions from
`ps_game.exe` SHA256
`0fb7c9f13b5bcf186118f8bfe73ab214226b93e8aa502b8e6fded4a8312bc547` in a
private emulator image, intercepting only `CZone::FindNpc` **00416d10** and
`SConnection::Close` **004d43a0**. Six cases assert target IDs, acquisition,
switching, repeated selection, absent NPCs, DWORD count wrapping and close
reason; the original binary is unchanged. Command
`.local/oracle-env/bin/python tools/vertical_npc_packet_oracle.py` exited 0,
log `work/vertical-npc-packet-oracle.log`; `python3 -m py_compile tools/vertical_npc_packet_oracle.py` exited 0. The native fixture exercises
real ZoneNpcs/NpcPool and the existing movement cleanup on the same interaction.
`python3 tools/dev.py build` exited 0, log
`work/overnight-07-npc-packet-build.log`. The focused command
`python3 tools/dev.py test -R
'^(vertical-quests-unit|vertical-movement.*-unit|npc-patrol-unit|zone-npcs-unit)$'`
exited 0 with **6/6 passed**; log `work/overnight-07-npc-packet-tests.log`.
`git diff --check` passed. SQL integration remains unavailable without centrally managed
runtime configuration; no SQL lifecycle action was taken.

## NPC use and patrol integration

The vertical fixture now composes the real `vertical_npc_request`, retained
`VerticalNpcInteraction`, `vertical_move_interactions`, `update_npc_patrol`,
`ZoneNpcs::move`, NPC pool, and zone broadcast service. A selected NPC does not
move, consume random state or broadcast while in use. Moving the user away
clears the same interaction and decrements the live NPC counter; the next
patrol tick updates its position and deadline, advances the real random stream,
and emits the actual 0e03 movement broadcast. This is an in-process integration
case, not live session wiring.

Original evidence: `ps_game.exe` `CUser::PacketNpc` **00467510**,
`CUser::PacketPC` **0046a55f..0046a64d**, `CNpc::UpdateMove` **004bbeb0**,
and `CZone::MoveNpc` **00416f30**. The existing `npc-patrol-unit` compares
independent original-execution cases. `python3 tools/dev.py build` exited 0,
log `work/overnight-07-npc-patrol-build.log`; `python3 tools/dev.py test -R
'^(vertical-quests-unit|vertical-movement.*-unit|npc-patrol-unit|zone-npcs-unit)$'`
exited 0, **6/6 passed**, log `work/overnight-07-npc-patrol-tests.log`.
`git diff --check` passed. SQL integration was unavailable because centrally
managed configuration is absent; no database lifecycle action was taken. The
root still needs to route NPC packets and retain the selected NPC as described
above; original-client end-to-end play is unverified.

## Shared NPC and zone-leave cleanup

The vertical fixture now selects the same actual NPC for two distinct Actors,
then runs the existing `zone_leave_interactions` and `zone_shutdown_shop` bodies
with each user's own `VerticalNpcInteraction`. It verifies that the first leave
clears only that user's pointer and decrements the NPC count before shop shutdown;
patrol stays paused and consumes no random state while the second user remains.
After the last leave, the count reaches zero and the actual zone patrol resumes,
updates its path/deadline and emits the real 0e03 movement broadcast. This is an
in-process composition with recorded service boundaries, not live session wiring.

Original evidence: `ps_game.exe` `CUser::PacketNpc` **00467510**,
`CUser::LeaveZone` **0044aee0** (NPC decrement and clear **0044aee0..0044aef4**,
shop shutdown **0044af35..0044af37**) and `CNpc::UpdateMove` **004bbeb0**.
The native build `python3 tools/dev.py build` exited 0; log
`work/overnight-07-npc-zone-leave-build.log`. The focused command
`python3 tools/dev.py test -R
'^(vertical-quests-unit|vertical-disconnect-unit|vertical-movement.*-unit|npc-patrol-unit|zone-npcs-unit)$'`
exited 0, **7/7 passed**; log `work/overnight-07-npc-zone-leave-tests.log`.
`git diff --check` passed. Isolated SQL integration was unavailable because the
centrally managed runtime configuration is absent; no database lifecycle action
was taken. Root still needs actual NPC packet routing and retained NPC/user
lifetimes. Original-client end-to-end play remains unverified.

## Character quest restoration into the live runtime

`vertical_quests_restore_runtime` materializes the character loader's existing
`RestoredQuests` into the actual `QuestRuntime` and `QuestRuntimePool`; it does
not re-sample time, send packets or create substitute services. Active and
finished rows retain their original order; a failed CQuest allocation is skipped
and reported in separate active/finished counts. Active rows must refer to the
same retained `QuestDefinitions` object. Finished rows outside the definitions
are silently skipped, as in the original handler. The 0507 path changes only
quest definition, ID and success on a recycled object, leaving its other payload
intact. The returned shared runtime lease retains both pool and definitions; the
pool lease supplied by the caller must itself retain `QuestRuntimePoolServices`.

`vertical_quests_entry_rows` exposes the actual allocated active and finished
runtime lists for the existing `character_quests` and `character_finished_quests`
serializers. This matters when allocation fails: sending the initial loader's
unfiltered rows would show the client a quest the live runtime does not own.

Coordinator integration: after character restoration and before quest entry
packets, bind `VerticalSessionCharacter::pipeline().result().quests`, account ID,
the canonical shared quest pool and the same shared definitions to
`vertical_quests_restore_runtime`; retain its runtime in the session and use
that exact runtime in `QuestStartItemsActorWorldOwners`. Feed entry quest packets
from `vertical_quests_entry_rows(*runtime)` rather than from the loader's
pre-allocation projection. Preserve the existing loader's `invalid_active`
diagnostics, and retain the real pool services and definition owners. Other
entry phases and real packet transport remain the coordinator's responsibility.

Original evidence: `ps_game.exe` `CUser::PacketUserDBSetInfo` **0046d780**,
active 0506 and finished 0507 branches; finished writes only definition, ID and
success at **0046e5b8..0046e5d3**. `CUser::SendCharacterQuest` **004835f0** and
`CUser::SendCharacterQuestFinish` **00483730** use the live lists. Existing
`quests-unit` compares the underlying restoration arithmetic against original
execution. New integration cases cover real runtime/replica completion after
restoration, entry packet contents, mixed allocation failure, definition-owner
mismatch, dependency lifetimes and retained recycled finished payload.

The development-container C++ syntax check and `python3 tools/dev.py build`
exited 0; full build log `work/overnight-07-restore-build.log`. The focused command
`python3 tools/dev.py test -R
'^(vertical-quests-unit|quests-unit|quest-replica-unit|quest-removal-unit|quest-start.*-unit|quest-requests-actor-unit)$'`
exited 0 with **14/14 passed**; log `work/overnight-07-restore-tests.log`.
`git diff --check` passed. No SQL lifecycle action was taken; isolated SQL
integration still requires centrally managed runtime configuration.

## Entry packets from the live quest runtime

`vertical_quests_send_entry_step` handles the quest and finished-quest
`CharacterEntryStep` values directly from the actual allocated `QuestRuntime`.
It samples the active-quest clock once before locking, sends full 255-record
batches under the appropriate active or finished list lock, unlocks, and then
sends any final partial batch. It tests the live definition's maximum-time field
rather than trusting a stale projection flag. Other entry steps return false
without callbacks. The provider must bind `VerticalQuestEntryPorts` to the same
user's real clock, active and finished recursive locks and client transport;
retain the runtime, definitions and user through delivery, and do not mutate
either quest list from delivery callbacks. This restriction is explicit because
the current native finished-list owner does not expose the original live cursor.

For coordinator wiring, restore and retain the canonical runtime before the
quest entry steps as described above. In the `complete_character_selection` step
callback currently built by `deliver_vertical_entry`, call
`vertical_quests_send_entry_step(*runtime, step, quest_entry_ports)` before
`EntryDelivery::send(step)`, falling through only when it returns false. This
requires the coordinator's session owner to retain the runtime and actual lock,
clock and send services; no entry step may be reported successful by a fallback.

Original evidence: `ps_game.exe` `CUser::SendCharacterQuest` **004835f0** and
`CUser::SendCharacterQuestFinish` **00483730**. The active clock read is at
**00483607**, its 255-record send at **004836c2..004836da**, and unlock/final
send at **004836e9..00483712**; the corresponding finished boundaries are
**004837ad..004837e3**. `tools/vertical_quests_entry_oracle.py` executes both
original routines in a private emulator image, observing only clock, locks and
client transport. Twelve cases cover 0, 1, 254, 255, 256 and 510 rows for each
list, including exact opcodes, payload IDs, counters, lengths and lock/send
ordering. `.local/oracle-env/bin/python tools/vertical_quests_entry_oracle.py`
and `python3 -m py_compile tools/vertical_quests_entry_oracle.py` exited 0;
log `work/vertical-quests-entry-oracle.log`. Original binaries are unchanged.

The native fixture uses actual pooled quests, including allocation omissions,
256 active and 256 finished entries, stale timer flags, empty lists and a
throwing transport to check lock cleanup. `python3 tools/dev.py build` exited 0;
log `work/overnight-07-entry-send-build.log`. The focused command
`python3 tools/dev.py test -R
'^(vertical-quests-unit|quests-unit|quest-replica-unit|quest-requests-actor-unit|character-entry-unit|entry-packets-unit)$'`
exited 0 with **6/6 passed**; log `work/overnight-07-entry-send-tests.log`.
`git diff --check` passed. Isolated SQL integration was unavailable because this
worktree lacks the centrally managed runtime configuration; no SQL lifecycle
action was taken. Original-client end-to-end play is unverified.

## Live quest to DB-agent save plan

The vertical fixture now accepts an NPC-gated timed quest, advances it through
the actual `VerticalQuests` owner, runs the real `quest_runtime_save` phase, and
applies its 0a04 packet to the same `QuestReplica`. It checks the partial batch
is sent after unlocking, its exact account, delay and counters, and the resulting
`usp_Save_Char_Quest_Add_E` plan. After actual NPC completion it checks that the
same replica produces `usp_Save_Char_FinishedQuest_E` with the expected owner,
quest, success and insert flag. This is an in-process plan check: no SQL
procedure, commit or save-success acknowledgment is simulated or claimed.

Original evidence: `ps_game.exe` `CUser::SendDBAgentCharGetInfo` **0046c980**,
quest phase **0046ccd4..0046ce4a**; `ps_dbAgent.exe` `DBCharacter::SaveCharacter`
**004242c2..00424617**; `ps_game.exe` `CUser::QuestEnd` **0047ec40** and
`CUser::QuestRemove` **0047e150**. Existing `quest-replica-unit` independently
compares DB-agent transitions and SQL plans to original execution. Root must
still connect the same retained runtime and account to its real serialized save
and DB-agent delivery; SQL execution remains owned by the full character
transaction.

`python3 tools/dev.py build` exited 0; log
`work/overnight-07-save-chain-build.log`. The focused command
`python3 tools/dev.py test -R
'^(vertical-quests-unit|quest-replica-unit|quest-requests-actor-unit|vertical-disconnect-unit|quest-rewards-unit|quest-removal-unit)$'`
exited 0, **6/6 passed**; log `work/overnight-07-save-chain-tests.log`.
`git diff --check` passed. Isolated SQL integration was unavailable because the
centrally managed runtime configuration is absent; no database lifecycle action
was taken. Original-client end-to-end play remains unverified.

## Quest runtime ownership guard

`QuestRuntime::bound_to` exposes a read-only identity check for its immutable
account and actual `QuestRuntimePool`. `VerticalQuests` now rejects a runtime
bound to a different Actor account or different pool at construction. Its quest
entry points and database delivery recheck the binding, so an account changed
between calls cannot advance quests or route a persistence command under the
wrong account. These are native integration guards; the original used fields of
one CUser and did not have independent host projections to reject. The account,
pool and retained owners must still remain stable through every synchronous
callback. This guard applies to `VerticalQuests`; direct users of lower-level
quest adapters must bind their own matching owners.

Original evidence: `ps_game.exe` `CUser::QuestAdd` **0047e010** allocates from
the global quest heap at **0047e019..0047e026**, inserts into the same user and
reads that user's account at **0047e0f9**; `CUser::QuestRemove` **0047e150**
reads the same user account at **0047e1e6** and returns repeatable quests to
that heap at **0047e20c..0047e216**. The fixture checks both mismatched
construction cases, account changes before packet callbacks, and an account
change with an active quest before mob progress. It restores the account and
continues through the real mob-death, replica and NPC-completion flow.

The development-container C++ syntax check and `python3 tools/dev.py build`
exited 0; full build log `work/overnight-07-binding-build.log`. The focused
command `python3 tools/dev.py test -R
'^(vertical-quests-unit|quest-start.*-unit|quest-requests.*-unit|quest-removal-unit|mob-quests-unit)$'`
exited 0 with **19/19 passed**; log `work/overnight-07-binding-tests.log`.
`git diff --check` passed. Isolated SQL integration was unavailable without the
centrally managed runtime configuration; no SQL lifecycle action was taken.
Original-client end-to-end play remains unverified.

## Canonical reward removal binding

`VerticalQuestRewardBinding` implements `QuestRewardWorld` using the actual
Actor, `QuestRuntime` and `QuestRuntimePool`. Reward completion now can route
`remove_quest` directly through that runtime and its real DB delivery instead
of delegating removal to an independently selected reward owner. Constructor
and callback checks reject account/pool mismatches; the runtime rejects a quest
belonging to another user. The binding forwards experience, database packets,
item receipt, client delivery and log operations to required
`VerticalQuestRewardServices`; there are no default or successful placeholder
callbacks. The caller must bind those services to the same Actor and retain the
Actor, runtime, pool, services, quest payload and definitions across callbacks.

Coordinator interface: construct one `VerticalQuestRewardBinding` with the same
Actor, runtime and pool supplied in `QuestStartItemsActorWorldOwners`; have
`VerticalQuestPorts::reward_world(Actor&)` return that retained binding. Supply
actual Actor experience, real quest item receipt, client/DB transport and logging
through `VerticalQuestRewardServices`. The item receipt must return the actual
bag, slot and definition only after a real grant; this binding does not invent
that service or complete the root wiring.

Original evidence: `ps_game.exe` `CUser::QuestSuccessResult` **0047ee40** sends
the client result at **0047f036**, calls `CUser::QuestRemove` **0047e150** at
**0047f041**, then sends the log at **0047f056**. The existing
`quest-rewards-unit` compares 512 original reward traces. The vertical fixture
completes a restored quest with a real runtime and replica while explicitly
forbidding the underlying reward provider's removal callback; it also verifies
wrong account/pool, wrong runtime quest and changed-account rejection.

`python3 tools/dev.py build` exited 0; final log
`work/overnight-07-reward-binding-build-verified.log`. The focused command
`python3 tools/dev.py test -R
'^(vertical-quests-unit|quest-rewards-unit|quest-requests-completion-unit|quest-requests-actor-unit|quest-removal-unit|quest-replica-unit)$'`
exited 0, **6/6 passed**; log `work/overnight-07-reward-binding-tests.log`.
`git diff --check` passed. Isolated SQL integration was unavailable because the
centrally managed runtime configuration is absent; no SQL lifecycle action was
taken. Original-client end-to-end play remains unverified.

## Actual quest reward item receipt

`QuestStartItemsActor::add` exposes the existing real Actor/inventory/pool/UID
implementation of the inner QuestAddItem overload, returning its actual
`QuestStartItemsResult` without the outer start-item wrapper's 0205 packet.
`VerticalQuestRewardItems` converts a successful inner add into the real
`QuestRewardItemReceipt`; failure returns no receipt and does not invent a
successful grant. It checks that the item adapter belongs to the supplied Actor
and requires an item-name provider because `ItemDefinition` does not retain the
original name bytes.

Original evidence: `ps_game.exe` `CUser::QuestSuccessResult` **0047ee40** calls
inner `CUser::QuestAddItem` **0047e6f0** at **0047ef7e..0047efa0**, then uses
its definition, bag and slot in the 0903 result and 0502 log. The separate
wrapper **0047e620** sends a 0205 notice and is not called by reward completion.
Existing `quest-start-items` and `quest-rewards` original-execution comparisons
cover these routines independently.

Coordinator interface: retain the real `QuestStartItemsActor` constructed with
the reward Actor's actual definitions, item pool, shared ItemIds and transports;
construct `VerticalQuestRewardItems` with that Actor and an item-name provider
backed by the actual catalog (for example the existing
`InventoryWorld::inventory_item_name` service). Implement
`VerticalQuestRewardServices::receive_item` by returning `reward_items.receive`
for this same Actor, and keep the canonical reward/runtime binding described
above. No 0205 packet or synthetic receipt should be added. Names, item
allocation, UID, DB delivery and all owner lifetimes remain real services.

The vertical fixture completes an NPC-gated quest through the actual quest
runtime, reward binding, Actor inventory, GroundItemPool and InventoryReplica;
it checks the 0701/0a02 DB order, retained item/UID, 0903 bag/slot/type fields,
0502 item fields, absence of an extra client packet, wrong-Actor rejection, and
failed full-bag/unknown-item grants. The focused development-container syntax
check and `python3 tools/dev.py build` exited 0; final build log
`work/overnight-07-reward-item-build-final.log`. The focused command
`python3 tools/dev.py test -R
'^(vertical-quests-unit|quest-rewards-unit|quest-start-items.*-unit|quest-requests-actor-unit|quest-requests-completion-unit|quest-replica-unit)$'`
exited 0 with **10/10 passed**; log `work/overnight-07-reward-item-tests.log`.
`git diff --check` passed. Isolated SQL integration was unavailable because the
centrally managed runtime configuration is absent; no SQL lifecycle action was
taken. Original-client end-to-end play is unverified.

### Quest reward experience owner integration

`VerticalQuestRewardExperience` forwards the quest award to the retained
`Actor::gain_experience` and its actual `PlayerCore`, `StatusRules`, `Experience`
curve, and required `ExperienceAwardPorts`. It checks character/account identity
at construction and each award. Its required options provider must sample the
live party and skill owners; the quest caller's modifier-bypass flag overrides
only `skip_modifiers`. Original evidence: `ps_game.exe`
`CUser::QuestSuccessResult` **0047ee40** calls `CUser::AddExpFromUser`
**00458240** with source zero and modifier bypass enabled. The existing award
implementation retains the party bonus even with modifier bypass.

Coordinator integration: bind `VerticalQuestRewardServices::add_experience` to
`VerticalQuestRewardExperience::award`; provide the canonical actor/player,
experience tables, real database/client/level-up ports, and a synchronous live
`ExperienceAwardOptions` provider including `normal_party_seven`. All owners
must outlive the binding. No successful transport or level-up callback is
invented here. This branch still needs integration into the coordinator's real
session and party owner and original-client end-to-end validation.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-reward-experience-build-final.log`);
`python3 tools/dev.py test -R '^(vertical-quests-unit|experience-award-unit|mob-targeting-lifecycle-experience-unit|quest-rewards-unit|quest-start-items.*-unit|quest-requests-actor-unit|quest-requests-completion-unit|quest-replica-unit)$'`
passed 12/12 (log `work/overnight-07-reward-experience-tests-final.log`). The
first run failed because its experience fixture omitted three growth modes;
the fixture was corrected. The new real quest completion test checks actual
player experience, 0602 database and 0207 client packets, seven-member bonus,
modifier bypass, quest completion, and changed-account rejection. `git diff
--check` passed. Isolated SQL integration was not run: `.local/runtime.env` and
`.local/sqlserver.env` are unavailable; no database lifecycle action was taken.

### Quest item pickup eligibility

`VerticalQuestPickupWorld` binds the inventory pickup world's quest-eligibility
query to the same Actor's retained `VerticalQuests` and its `QuestRuntime`;
all other pickup, inventory, money, party, and transport methods forward to the
real supplied `ItemPickupWorld`. It rejects a different Actor and uses the
runtime's shared-cursor original `QuestFind` implementation, including the
account/pool identity guard. Original evidence: `ps_game.exe`
`CZone::GetPickItem` **004174a0** calls `CUser::QuestFind` **0047e4b0** at
**004175ec** for linked quest items.

Coordinator interface: wrap the actual user's `ItemPickupWorld` with
`VerticalQuestPickupWorld(actor, quests, world)` and pass it to the existing
`vertical_inventory_request`; all three owners and the zone/item/link data must
remain live for the synchronous call. This connects pickup *eligibility* only.
The caller still needs an evidence-backed integration of item acquisition with
`QuestUpdatePickItem` **00480390** and the coordinator's real session dispatch;
this change does not assert that every pickup advances a quest.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-pickup-quest-build-final.log`); the initial build failed on a
duplicate test-local variable, which was renamed. `python3 tools/dev.py test -R
'^(vertical-quests-unit|vertical-inventory-unit|item-pick-unit|quest-runtime-unit|quest-requests-progress-unit|quest-replica-unit)$'`
passed **5/5 selected tests** (no `quest-runtime-unit` test matched); log
`work/overnight-07-pickup-quest-tests.log`. The vertical test uses the real
`ZoneItems::pick`, item links, quest runtime and Actor: it verifies inactive
rejection, active acceptance, finished ineligibility, and wrong-Actor rejection.
`git diff --check` passed. Isolated SQL integration was unavailable because the
centrally coordinated `.local/runtime.env` and `.local/sqlserver.env` are absent;
no SQL lifecycle action was taken. Original-client end-to-end play is unverified.

### Player-kill quest progression boundary

`vertical_quests_kill_pc_to` binds the killer's retained `VerticalQuests` to
an explicitly required `VerticalQuestKillPcContinuation`. It rejects a quest
owner belonging to another Actor, runs real `QuestUpdateKillPC` first, and
propagates failures from the remaining bookkeeping without rolling back already
published quest progress. Original executable evidence: `ps_game.exe`
`CUser::KillPCFrom` **0045b050** calls `CUser::KillPCTo` **0045aed0** at
**0045b055**; `KillPCTo` calls `CUser::QuestUpdateKillPC` **00480020** at
**0045aeeb**, before its game log, kill counters and DB/client notices.

Coordinator interface: the real `KillPCFrom`/`KillPCTo` owner must split its
`KillPCTo` implementation immediately after its quest call, invoke
`vertical_quests_kill_pc_to(killer, victim, killer_quests, continuation)`, and
implement `after_quest` with the actual original remainder, including logs,
kill counts and notices. Do not pass a full `KillPCTo` implementation as the
continuation, which would repeat quest progress. The existing solo death reward
owner calls a required `kill_from` service after experience and before item
loss; its concrete KillPCFrom/KillPCTo backend is still an external dependency.
Both Actor owners, quest runtime and continuation must remain live for the call.

The item-progress dispatch remains unresolved: an `objdump -d -Mintel` scan
of the original executable found no direct call to **00480390**. This does not
rule out indirect invocation; do not add a pickup callback without evidence.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-player-kill-quest-build.log`); `python3 tools/dev.py test -R
'^(vertical-quests-unit|quest-requests-progress-unit|quest-replica-unit|mob-targeting-lifecycle-experience-unit|quest-requests-actor-unit)$'`
passed **5/5** (log `work/overnight-07-player-kill-quest-tests.log`). The
vertical test checks killer identity, runtime and replica progression before the
continuation, continuation exception propagation, and subsequent NPC turn-in.
`git diff --check` passed. Isolated SQL integration was unavailable because
`.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL lifecycle
action was taken. Original-client end-to-end play remains unverified.

## Stop session packet dispatch on failure

The session packet router now stops and clears its retained FIFO when a
location-3 or location-4 packet dispatch throws, then reports the captured
exception to the selected session's existing failure service and requests the
actual channel disconnect. The session exposes `report_dispatch_failure` on
the world strand for this purpose. This prevents further queued quest or NPC
actions after a possibly partial replica mutation. Disconnect completion and
the existing admission-pass, IO lease and registry fences remain owned by the
existing session lifecycle. This is native failure containment, not a claim
that the original server used exceptions. The recovered FIFO and dispatch
owners are `ps_game.exe` `CUser::UpdatePackets` **00466cc0** and
`CUser::PacketProcessing` **00466da0**.

Coordinator integration: port the `report_dispatch_failure` method in
`vertical_world_session.hpp` and the `update` catch in
`vertical_world_session_packet_router.hpp`, preserving the coordinator's newer
session and registry state. Its real channel disconnect must initiate normal
close/drain and its failure callback must record the exception; the router
does not roll back game state or decide final-save policy. Timer, mob-credit
and other quest callbacks outside this packet FIFO still need equivalent
real-owner failure containment.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-dispatch-failure-build.log`); `python3 tools/dev.py
test --timeout 30 -R '^(vertical-quests-unit|vertical-session-registry-unit|vertical-world-unit|world-protocol-unit|quest-requests-actor-unit)$'`
passed **5/5** (log `work/overnight-07-quest-dispatch-failure-tests.log`). The
retained-world fixture queues a failing dispatch followed by an NPC action,
checks the original exception reaches the failure service, confirms one
disconnect, and verifies the second action is discarded and future enqueue
fails. `git diff --check` passed. Isolated SQL integration remains unavailable
because `.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL
lifecycle action was taken. Original-client end-to-end play remains unverified.

### Pickup quest lookup locking correction

`VerticalQuests::active` now delegates to the existing
`QuestRequestsActorWorld::find_quest`, which acquires and releases the actual
user quest lock around `QuestRuntime::find_active`. The prior pickup adapter
used the right list and cursor but bypassed that lock. The account/pool guard
still runs before lock acquisition. Original evidence: `ps_game.exe`
`CUser::QuestFind` **0047e4b0** enters the quest critical section at
**0047e4be** and leaves it at **0047e501** or **0047e510**, for missing and
matching quests respectively. `CZone::GetPickItem` **004174a0** calls this
lookup at **004175ec**.

Coordinator integration remains the `VerticalQuestPickupWorld` interface above;
its `VerticalQuestPorts` lock methods must bind to the same user's actual quest
critical section. The new test verifies lock/unlock on missing, active and
finished lookups and rejection of an account change before lock acquisition.
It retains real `ZoneItems::pick`, quest runtime and Actor owners.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-pickup-lock-build.log`); `python3 tools/dev.py test -R
'^(vertical-quests-unit|vertical-inventory-unit|item-pick-unit|quest-requests-actor-unit|quest-requests-progress-unit)$'`
passed **5/5** (log `work/overnight-07-pickup-lock-tests.log`). `git diff
--check` passed. Isolated SQL integration was unavailable because
`.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL lifecycle
action was taken. Original-client end-to-end play remains unverified.

### Timer phase actor identity guard

The current coordinator has a `WorldPrimaryQuestPhase` which pins a visited
Actor and then independently resolves `VerticalQuests&`; its existing direct
`update_timers(now)` call does not check that both refer to the same Actor.
`VerticalQuests::update_timers_for(Actor&, tick)` now rejects a mismatch before
any timer side effect, then uses the original timer owner. The existing
`vertical_quests_update_zone` also uses this checked entry point.

Original evidence: `ps_game.exe` `CWorldThread::Update` **00407390** invokes
`CZone::UpdateQuest` **00422320** at **0040771d..0040775a**; the zone invokes
`CUser::UpdateQuest` **0048d1a0** for the actual traversed user. The native
identity guard ensures a resolver does not substitute another session; it is
not an extra original packet or callback.

Coordinator integration: in `world_primary_quest_phase.hpp`, replace
`quests.update_timers(now)` with `quests.update_timers_for(actor, now)` while
retaining the existing actor/session/runtime/transport lease. Continue handling
primary and dynamic zones using their actual owners; this branch does not edit
the coordinator checkout or implement dynamic-zone traversal.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-timer-actor-build.log`); `python3 tools/dev.py test -R
'^(vertical-quests-unit|quest-requests-timers-unit|quest-requests-actor-unit|zone-users.*-unit)$'`
passed **4/4** (log `work/overnight-07-timer-actor-tests.log`). The vertical
fixture checks a mismatched Actor leaves both real runtimes and DB replicas
untouched, an unexpired matching Actor, and the existing multi-user expiration
with live membership mutation. `git diff --check` passed. Isolated SQL
integration was unavailable because `.local/runtime.env` and
`.local/sqlserver.env` are absent; no SQL lifecycle action was taken.
Original-client end-to-end play remains unverified.

### Admitted-session quest and NPC packet adapter

`VerticalQuestNpcPackets` routes quest-family packets to the existing
`VerticalQuests::receive` owner and NPC-family packets to
`vertical_npc_request`, preserving handled, malformed, closed and unhandled
results. Construction rejects a different Actor object, including one with the
same numeric character ID, or a different `ZoneNpcs` than the quest owner.
The retained NPC interaction is the same object required by movement and
zone-leave cleanup. Both real action providers are mandatory. Original evidence:
`ps_game.exe` `CUser::PacketQuest` **0046be10** and `CUser::PacketNpc`
**00467510**; their original execution oracles are documented above. The
adapter is a native composition boundary, not a new original packet.

Coordinator interface: create one `VerticalQuestNpcPackets` for the admitted
session using its actual Actor, `VerticalQuests`, current zone's `ZoneNpcs`,
`VerticalNpcInteraction`, and action/close providers. In the required
`VerticalWorldSessionGameplay::Other` callback, pass the retained packet to
`receive`; delegate `unhandled` to the remaining real opcode dispatcher and
apply the session's existing malformed/closed policy to those results. Retain
all owners during callbacks, serialize them with the world strand, invalidate
and rebuild the adapter on zone transfer, and retain selected NPCs until
interaction cleanup. Do not count a handler return as client end-to-end play.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-npc-session-build.log`); `python3 tools/dev.py test -R
'^(vertical-quests-unit|quest-requests-actor-unit|quest-requests-completion-unit|quest-start-items-actor-world-unit|zone-npcs-unit|vertical-move-interactions-unit)$'`
passed **5/5 selected tests** (no `vertical-move-interactions-unit` matched),
log `work/overnight-07-quest-npc-session-tests.log`. The vertical fixture routes
NPC selection, quest acceptance, mob progress and NPC turn-in through retained
owners, checks replica/database and action order, movement cleanup, unrelated
and malformed packets, close forwarding, and mismatched actor/zone rejection.
`git diff --check` passed. Isolated SQL integration was unavailable because
`.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL lifecycle
action was taken. Original-client end-to-end play remains unverified.

### Retain the selected NPC for session interactions

`VerticalQuestNpcPackets` now pins the selected `Npc` in the existing `NpcPool`
while its shared `VerticalNpcInteraction` points to that object. NPC selection
and switching synchronize the lease automatically. The required session owner
must call `synchronize_interaction()` immediately after movement or zone-leave
cleanup changes the shared interaction, and must clear the interaction before
destroying the packet adapter. All calls and NPC retirement remain serialized.
This is a native lifetime guarantee: original `ps_game.exe`
`CUser::PacketNpc` **00467510** retains the selected NPC pointer and updates its
use count; `CUser::PacketPC` **0046a55f..0046a64d** and `CUser::LeaveZone`
**0044aee0..0044b04c** clear that same interaction. Original NPC allocation
uses `SSyncHeap<CNpc>::Alloc` **0040acc0**; the existing native pool defers reuse
of pinned objects rather than changing original packets or counters.

Coordinator integration: retain the same packet adapter across quest/NPC
packets and movement, use its exact `VerticalNpcInteraction` for movement and
leave, and synchronize after either cleanup. On transfer/disconnect, perform
actual interaction cleanup and synchronization before releasing the adapter,
actor, zone or NPC pool. The adapter obtains the NPC pool already bound to the
same `VerticalQuests`; no synthetic NPC owner or success callback is added.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-npc-interaction-lease-build.log`); `python3 tools/dev.py test -R
'^(vertical-quests-unit|zone-npcs-unit|quest-start-items-actor-world-unit|quest-requests-actor-unit|zone-leave-interactions-unit)$'`
passed **4/4 selected tests** (no `zone-leave-interactions-unit` matched), log
`work/overnight-07-npc-interaction-lease-tests.log`. The vertical fixture selects
a real zone NPC, removes and releases it, verifies the pool does not recycle it
while selected, runs real movement cleanup, synchronizes, and verifies reuse.
`git diff --check` passed. Isolated SQL integration was unavailable because
`.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL lifecycle
action was taken. Original-client end-to-end play remains unverified.

### Session NPC movement and leave lifecycle

`VerticalQuestNpcPackets::move_interactions` and `leave_interactions` execute the
existing real `vertical_move_interactions` and `zone_leave_interactions` steps
using the exact retained Actor and NPC interaction, then synchronize the pool
lease. If a later required callback throws after the interaction has been
cleared, the old NPC lease is still released and the exception propagates; if
the interaction remains set, its lease stays retained. No shop, exchange, PvP
or other service is replaced with a success stub.

Original evidence: `ps_game.exe` `CUser::PacketPC` **0046a55f..0046a64d**
clears an out-of-range NPC before the later PvP check; `CUser::LeaveZone`
**0044aee0..0044b04c** clears the NPC before shop and exchange cleanup. The
existing recovered handlers implement these original steps; this change binds
them to the packet adapter's native NPC lease.

Coordinator integration: replace the corresponding single existing interaction
step with `packets.move_interactions(wide, actual_move_services)` or
`packets.leave_interactions(actual_shop, actual_leave_services)` using the
same session adapter; do not run the original step a second time. The remaining
movement, leave, party, vehicle and other lifecycle steps remain with their
actual owners. If using the lower-level handlers directly, retain the prior
explicit `synchronize_interaction()` contract. Clear the interaction before
destroying the adapter and retain its pool and actor throughout.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-npc-lifecycle-build.log`); `python3 tools/dev.py test -R
'^(vertical-quests-unit|zone-npcs-unit|quest-requests-actor-unit|quest-start-items-actor-world-unit|zone-leave-exchange-unit)$'`
passed **4/4 selected tests** (no `zone-leave-exchange-unit` matched), log
`work/overnight-07-npc-lifecycle-tests.log`. The vertical fixture uses real
movement and leave handlers, checks cleanup and pool reuse, and injects a PvP
service failure after NPC removal to verify exception propagation and release.
`git diff --check` passed. Isolated SQL integration was unavailable because
`.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL lifecycle
action was taken. Original-client end-to-end play remains unverified.

### Quest money reward and DB replica integration

The live quest reward fixture now delivers opcode `0603` to the actual
`MoneyReplica`, alongside the existing actual quest and inventory replicas.
An NPC-gated completion awards both money and a real retained item; it verifies
Actor money, DB money, `0603` before item `0701` and quest removal `0a02`, and
the original `0903` success packet and `0502` game-log reward fields. A second
real quest completion starts within ten units of `UINT32_MAX` and verifies the
actual money cap, replica update, and original uncapped advertised reward value.
No production owner or callback was replaced.

Original evidence: `ps_game.exe` `CUser::QuestSuccessResult` **0047ee40**,
`CUser::ChkAddMoney` **00478360**, `CUser::SendDBMoney` **0046c700** and
`ps_dbAgent.exe` `CUser::PacketUserSetStatus` **00405b00** case `0603`.
The existing original-executable quest reward oracle covers the reward routine
independently; this adds a live Actor/runtime/replica integration check.

Coordinator interface: route quest reward `VerticalQuestRewardServices::send_database`
`0603` to the same account's actual DB-agent character replica, for example the
existing `CharacterReplicaDelivery` / `CharacterReplica::apply` path, in order
with quest and inventory messages. Retain the actual Actor money field and
preserve delivery failure; do not synthesize a successful DB acknowledgement.
This still requires real session wiring and SQL-backed persistence.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-money-build.log`); `python3 tools/dev.py test -R
'^(vertical-quests-unit|quest-rewards-unit|quest-requests-actor-unit|quest-replica-unit|money-unit|inventory-replica-unit)$'`
passed **6/6** (log `work/overnight-07-quest-money-tests.log`). `git diff --check`
passed. Isolated SQL integration was unavailable because `.local/runtime.env`
and `.local/sqlserver.env` are absent; no SQL lifecycle action was taken.
Original-client end-to-end play remains unverified.

### Compose real quest reward owners

`VerticalQuestRewardComposition` implements `VerticalQuestRewardServices` by
calling the same Actor's actual `VerticalQuestRewardExperience` and
`VerticalQuestRewardItems`, the actual Actor client transport, and the required
`VerticalQuestPorts` database and game-log methods. Construction rejects an
experience or item adapter belonging to a different Actor. The experience
adapter also rechecks player/actor identity after its live options callback,
before applying any award, so a callback cannot redirect the award to a changed
account. All actual service failures propagate; no success is synthesized.

Original evidence: `ps_game.exe` `CUser::QuestSuccessResult` **0047ee40** calls
`CUser::AddExpFromUser` **00458240**, `CUser::ChkAddMoney` **00478360**, inner
`CUser::QuestAddItem` **0047e6f0**, client delivery and `CUser::QuestRemove`
**0047e150**. The recovered reward routine controls their order; the new class
only binds its required calls to existing owners.

Coordinator interface: construct the actual experience adapter from the
session's Actor, PlayerCore, live party/skill options, curve and experience
ports; construct the item adapter from the same Actor, actual item pool/UIDs
and item-name provider; pass both with the session's real `VerticalQuestPorts`
to `VerticalQuestRewardComposition`, and use it as the services argument of
`VerticalQuestRewardBinding` for the same runtime and pool. The ports and
experience transports must belong to that session and route real DB, client,
level-up and game-log operations. Retain all owners through synchronous calls.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-reward-composition-build.log`); `python3 tools/dev.py test -R
'^(vertical-quests-unit|quest-rewards-unit|experience-award-unit|quest-start-items-actor-unit|quest-requests-actor-unit|quest-replica-unit|money-unit)$'`
passed **7/7** (log `work/overnight-07-reward-composition-tests.log`). The
vertical fixture completes one NPC-gated quest through real experience, money,
item, quest runtime and replicas; it checks client and DB ordering, rejects
mixed actor components, and verifies options-time account changes have no award
side effects. `git diff --check` passed. Isolated SQL integration was unavailable
because `.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL
lifecycle action was taken. Original-client end-to-end play remains unverified.

### Bind live quest runtime save to the session owner

`VerticalQuests::save_runtime(tick)` invokes the existing `quest_runtime_save`
with its own retained `QuestRuntime`, Actor account, actual `VerticalQuestPorts`
quest lock and actual DB transport. It validates the existing account/pool
binding before acquiring the lock and again immediately after acquisition; if
the binding changes during acquisition it releases the lock and propagates the
failure without sending a save. Full and partial batch timing remains owned by
the existing original-derived save routine. Its documented restriction remains:
callbacks must not mutate the active quest list while a save is in progress.

Original evidence: `ps_game.exe` `CUser::SendDBAgentCharGetInfo` quest phase
**0046ccd4..0046ce4a** sends `0a04` batches, with full batches inside the
quest lock and the final partial batch after unlock; the DB-agent quest replica
and recovered stored-procedure plan are described in the previous save section.
The new method is an owner binding, not a SQL acknowledgement or new packet.

Coordinator interface: invoke `session_quests.save_runtime(current_tick)` at
the original serialized character save/disconnect quest phase, retaining the
same Actor, runtime, quest pool, lock, transport and DB-agent owner. Route its
`VerticalQuestPorts::send_database` packets in order to the actual session DB
transport/replica and let any failure propagate. Do not also call the lower-level
`quest_runtime_save` for the same phase. Keep subsequent DB-agent SQL save and
fencing with their real persistence owner.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-runtime-save-binding-build.log`); `python3 tools/dev.py test -R
'^(vertical-quests-unit|vertical-disconnect-unit|quest-replica-unit|quest-save-unit|quest-requests-actor-unit)$'`
passed **4/4 selected tests** (no `quest-save-unit` matched), log
`work/overnight-07-runtime-save-binding-tests.log`. The vertical fixture saves
a real active timed quest to the actual QuestReplica and SQL procedure plan,
checks partial-send lock order, and rejects account changes before or during
lock acquisition without leaking the lock. `git diff --check` passed. Isolated
SQL integration was unavailable because `.local/runtime.env` and
`.local/sqlserver.env` are absent; no SQL lifecycle action was taken.
Original-client end-to-end play remains unverified.

### Bind character-entry quest packets to the live session

`VerticalQuests::send_entry_step` invokes the existing original-derived quest
entry sender with the same retained Actor, `QuestRuntime` and pool used for
subsequent quest gameplay. It takes the active-list clock, lock and client
transport from the real `VerticalQuestPorts` and requires a
`VerticalQuestEntryFinishedLock` for the same user's separate finished-list
lock. It rejects a changed account/pool before delivery and releases either
lock if the account changes during acquisition. Other entry steps return false
without invoking quest services. Existing batching and callback mutation
restrictions remain unchanged.

Original evidence: `ps_game.exe` `CUser::SendCharacterQuest` **004835f0** and
`CUser::SendCharacterQuestFinish` **00483730**; the original clock, locking and
255-row packet boundaries are documented and oracle-tested in the earlier entry
section. This method binds those recovered routines to real session owners.

Coordinator interface: retain the `VerticalQuests` constructed with the
restored runtime before character-entry quest steps. Implement
`VerticalQuestEntryFinishedLock` with that session's actual finished-list
critical section; bind `VerticalQuestPorts::quest_tick_ms`, `lock_quests`,
`unlock_quests` and `send_client` to that same user's real clock, active lock
and client transport. In the entry step handler, call
`session_quests.send_entry_step(step, finished_lock)` and delegate false only
to the real handlers for other steps. Do not call the lower-level quest sender
again. All owners must remain live and serialized through callback delivery.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-bound-quest-entry-build.log`); `python3 tools/dev.py test -R
'^(vertical-quests-unit|character-entry-unit|entry-packets-unit|quest-requests-actor-unit|quest-replica-unit)$'`
passed **5/5** (log `work/overnight-07-bound-quest-entry-tests.log`). The real
restored-runtime fixture checks active and finished packets, separate locks,
unrelated-step fallthrough, and account changes during both lock acquisitions
without leaked locks or client sends. `git diff --check` passed. Isolated SQL
integration was unavailable because `.local/runtime.env` and
`.local/sqlserver.env` are absent; no SQL lifecycle action was taken.
Original-client end-to-end play remains unverified.

### Guard reward composition against stale actor objects

`VerticalQuestRewardBinding` now rejects a `VerticalQuestRewardComposition`
whose exact Actor object differs from the binding's Actor, even if both objects
have identical account and character IDs and the service is supplied through
`VerticalQuestRewardServices&`. The existing account/pool checks cannot
otherwise distinguish those stale-session objects. The guard applies to the
concrete composed service; custom `VerticalQuestRewardServices` implementations
still require the caller to bind every callback to the correct Actor.

Original evidence: `ps_game.exe` `CUser::QuestSuccessResult` **0047ee40**
updates the calling user's experience, money and items and calls
`CUser::QuestRemove` **0047e150** for that same user. The native object identity
check enforces that owner relationship without introducing a protocol change.

Coordinator interface: construct `VerticalQuestRewardComposition` and
`VerticalQuestRewardBinding` from the same retained Actor object, runtime,
pool and real services. Treat construction failure as an invalid/stale session;
do not substitute an unbound reward service. Retain the whole session through
synchronous reward delivery.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-reward-binding-actor-build.log`); `python3 tools/dev.py test -R
'^(vertical-quests-unit|quest-rewards-unit|quest-requests-completion-unit|quest-requests-actor-unit|quest-start-items-actor-unit|experience-award-unit)$'`
passed **6/6** (log `work/overnight-07-reward-binding-actor-tests.log`). The
vertical fixture verifies rejection before side effects for two distinct Actors
with identical account and character IDs, then completes the legitimate quest
through actual experience, money, item and quest owners. `git diff --check`
passed. Isolated SQL integration was unavailable because `.local/runtime.env`
and `.local/sqlserver.env` are absent; no SQL lifecycle action was taken.
Original-client end-to-end play remains unverified.

### Combined quest flow through the cohosted character replica

The vertical integration fixture now routes quest, experience, money and item DB
packets through one actual `CharacterReplicaDelivery` and `CharacterReplica`.
It exercises empty quest entry, NPC selection and acceptance packets, live mob
quest progress, actor-bound quest save, NPC completion packet, real experience,
money and item reward owners, quest removal and movement interaction cleanup.
It verifies the shared DB sequence `0a01`, `0a05`, `0a04`, `0602`, `0603`,
`0701`, `0a02`, the live and replica state, and the resulting finished-quest
stored-procedure plan. This is a synchronous in-process fixture; action/transport
providers are test fixtures, and the procedure plan is not a SQL execution.

Original evidence: `ps_game.exe` `CUser::PacketNpc` **00467510**,
`CUser::PacketQuest` **0046be10**, `CUser::QuestUpdateKillMOB` **00480140**,
`CUser::SendDBAgentCharGetInfo` quest phase **0046ccd4..0046ce4a**,
`CUser::QuestSuccessResult` **0047ee40**, and `ps_dbAgent.exe`
`CUser::PacketUserSetStatus` **00405b00**. Earlier report sections cite the
separately executed original oracles for these recovered paths.

Coordinator interface: retain one session's `VerticalQuests`, packet adapter,
reward composition and real `CharacterReplicaDelivery`, sharing its save mutex
with the actual character save owner. Route quest and experience DB sends in
original order; do not acknowledge them independently of actual delivery. The
fixture does not wire the coordinator's session, network, mob death scheduler,
SQL execution or original client; those remain integration work.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-character-quest-flow-build.log`); `python3 tools/dev.py test -R
'^(vertical-quests-unit|character-replica-unit|quest-rewards-unit|quest-requests-actor-unit|quest-replica-unit|vertical-disconnect-unit|experience-award-unit)$'`
passed **6/6 selected tests** (no `character-replica-unit` matched), log
`work/overnight-07-character-quest-flow-tests.log`. `git diff --check` passed.
Isolated SQL integration was unavailable because `.local/runtime.env` and
`.local/sqlserver.env` are absent; no SQL lifecycle action was taken.
Original-client end-to-end play remains unverified.

### Restore quests from retained DB-agent rows

`vertical_quests_restore_replica` projects the actual `CharacterReplica` quest
cache into the existing `restore_quests` and pooled `QuestRuntime` owners,
binding the runtime to the cache's account. It preserves retained active and
finished row order, delay, counters and completion status; invalid active
quest IDs are returned to the caller, and native game-server allocation skips
remain reported by `VerticalQuestRestoreResult`. It does not alter the DB cache.

This addresses an existing integration gap: `vertical_persistence_restore`
skips a quest row when the DB-agent `QuestReplicaPool` allocation fails, but
the raw `PlayerSnapshot` still contains that row and the current
`ActorLoadPipeline::result().quests` projects that snapshot. Using that latter
projection directly can resurrect a quest which the DB-agent never retained.
Original evidence: `ps_dbAgent.exe` `DBCharacter::LoadCharacter` **00420610**
skips failed quest allocations; `ps_game.exe` `CUser::PacketUserDBSetInfo`
**0046d780** cases `0506/0507` restores the rows actually delivered by DB-agent.

Coordinator interface: after actual DB-agent selection/restoration and before
quest entry, hold the same recursive mutex used by `CharacterReplicaDelivery`
and character saves while calling `vertical_quests_restore_replica` with that
session's retained `CharacterReplica`, the original load tick, actual pooled
quest heap and the same quest-definition owner used by the actor loader. Retain
the returned runtime and pool/definition leases for entry, gameplay and save;
construct `VerticalQuests` with the same selected Actor and that runtime. Do not
also materialize `ActorLoadPipeline::result().quests` from the unfiltered
snapshot. Preserve caller handling of invalid IDs and allocation omissions.
This cohosted bridge does not substitute SQL loading or a DB-agent response.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-replica-quest-restore-build-final.log`); `python3 tools/dev.py
test -R '^(vertical-quests-unit|vertical-persistence-unit|quests-unit|quest-replica-unit|quest-requests-actor-unit|character-entry-unit)$'`
passed **6/6** (log `work/overnight-07-replica-quest-restore-tests-final.log`).
The first run failed because the test snapshot lacked the required empty
`PointLog` catalog; it was corrected. The fixture exercises actual persistence
restoration with a failed DB quest allocation, a retained invalid quest ID,
entry packets, and completion back to the same `CharacterReplica` through its
real delivery owner. `git diff --check` passed. Isolated SQL integration was
unavailable because `.local/runtime.env` and `.local/sqlserver.env` are absent;
no SQL lifecycle action was taken. Original-client end-to-end play is unverified.

### Bind replica quest restoration to the selected session Actor

`vertical_quests_restore_session` takes the actual `VerticalSessionCharacter`,
a session owner lease, the shared recursive character-replica/save mutex, the
original load tick and actual quest pool/definitions. It validates that the
loaded Actor's account and character ID match the retained `CharacterReplica`,
projects the replica under that mutex through `vertical_quests_restore_replica`,
and returns an aliasing `shared_ptr<Actor>` tied to the supplied session lease
alongside the actual restored runtime and invalid/allocation results. A stale
account is rejected before quest allocation. Pool/definition lifetime and
DB-agent allocation omission behavior remain those of the previous milestone.

Original evidence: `ps_dbAgent.exe` `DBCharacter::LoadCharacter` **00420610**
retains successfully allocated quest rows; `ps_game.exe`
`CUser::PacketUserDBSetInfo` **0046d780** cases `0506/0507` loads those rows
into the selected user before `CUser::SendCharacterQuest` **004835f0** and
`CUser::SendCharacterQuestFinish` **00483730**. The alias and mutex are native
lifetime/serialization bindings, not original protocol additions.

Coordinator interface: after the `VerticalWorldSession` has its selected
`VerticalSessionCharacter` and before quest entry, call
`vertical_quests_restore_session(session_shared_ptr, *session.character(),
replica_save_mutex, original_load_tick, quest_pool, quest_definitions)` on the
world strand. The supplied lease must actually retain that exact character,
Actor, quest-use state, scene and all referenced rules/pools; the helper cannot
prove arbitrary `shared_ptr<void>` ownership. Use the returned Actor alias and
runtime in `QuestStartItemsActorWorldOwners`, retain the result through entry,
gameplay and save, and do not release/move the `ActorLoadPipeline` Actor while
the alias exists. Retain the DB pool and the same save mutex through DB delivery
and fencing. Construct only once per selected session and preserve invalid and
allocation-skip results. Required real quest ports and zone owners remain with
the coordinator; no callback or database success is supplied here.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-session-quest-restore-build.log`); `python3 tools/dev.py test
-R '^(vertical-quests-unit|vertical-persistence-unit|quests-unit|quest-replica-unit|actor-load-unit|character-entry-unit)$'`
passed **5/5 selected tests** (no `actor-load-unit` matched), log
`work/overnight-07-session-quest-restore-tests.log`. The vertical fixture uses
an actual `VerticalSessionCharacter`, DB cache allocation omission, actor-bound
entry and quest completion through its `CharacterReplicaDelivery`; it verifies
stale-account rejection and session retention/release by the actor alias.
`git diff --check` passed. Isolated SQL integration was unavailable because
`.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL lifecycle
action was taken. Original-client end-to-end play remains unverified.

### Typed selected-session quest restoration

`vertical_quests_restore_selected_session` obtains the selected character from
the retained session's own `character()` accessor, checks its loaded Actor and
`account_id()` against that session before quest allocation, then invokes the
existing mutex-protected replica restoration. The Actor alias retains that same
session owner. Null sessions, sessions without a selected character, and stale
session accounts fail before runtime allocation. This removes the need for the
coordinator to separately pair an arbitrary erased lease and character when
using the current `VerticalWorldSession` interface.

Original evidence: `ps_dbAgent.exe` `DBCharacter::LoadCharacter` **00420610**
and `ps_game.exe` `CUser::PacketUserDBSetInfo` **0046d780** cases `0506/0507`
restore quest rows for the selected user; original entry calls
`CUser::SendCharacterQuest` **004835f0** and `CUser::SendCharacterQuestFinish`
**00483730**. The typed session and identity checks are native owner bindings.

Coordinator interface: call `vertical_quests_restore_selected_session(session,
replica_save_mutex, original_load_tick, quest_pool, quest_definitions)` after
selection and before quest entry on the world strand. `session` must be the
retained `shared_ptr<VerticalWorldSession>` whose `character()` and
`account_id()` are the real selected owner. Retain the returned runtime and
Actor alias, actual quest services and zone, and do not move/release the
pipeline Actor while aliases exist. If the session itself stores the quest
owner, the alias creates a strong ownership cycle: perform actual NPC/quest
cleanup and release all session-held quest owners during teardown before
expecting the session to be destroyed. The session must also retain scene,
use-state, rule, pool and DB dependencies as described in the previous section.
This helper does not wire the coordinator or supply DB/transport callbacks.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-typed-quest-session-build.log`); `python3 tools/dev.py test -R
'^(vertical-quests-unit|vertical-persistence-unit|quest-replica-unit|quests-unit|character-entry-unit|vertical-disconnect-unit)$'`
passed **6/6** (log `work/overnight-07-typed-quest-session-tests.log`). The
fixture uses a session-shaped owner around a real `VerticalSessionCharacter`,
checks missing session/character and stale account before allocation, then
verifies actual quest entry/completion and owner retention and release. It does
not instantiate the coordinator's `VerticalWorldSession`. `git diff --check`
passed. Isolated SQL integration was unavailable because `.local/runtime.env`
and `.local/sqlserver.env` are absent; no SQL lifecycle action was taken.
Original-client end-to-end play remains unverified.

### Compose the live quest session owners

`VerticalQuestSession` now constructs one session-scoped composition of the
existing `VerticalQuests`, `VerticalQuestNpcPackets`, real inner quest-item
grant owner, actor experience adapter, reward composition and runtime-bound
reward removal. Its private port bridge forwards the required clock, DB, client,
lock and log operations to the supplied real `VerticalQuestPorts`; its reward
lookup returns the same session's actual `VerticalQuestRewardBinding` instead
of requiring a circular external callback. The bridge checks Actor identity and
never substitutes successful delivery. Construction checks the existing runtime,
account and player bindings and requires real item-name and experience-options
providers.

Original evidence: `ps_game.exe` `CUser::PacketNpc` **00467510**,
`CUser::PacketQuest` **0046be10**, `CUser::QuestStartRequest` **0047f220**,
`CUser::QuestSuccessResult` **0047ee40**, `CUser::QuestAddItem` **0047e6f0**,
and `CUser::QuestRemove` **0047e150**. Their behavior remains in the existing
original-derived owners and oracle tests; this is a native ownership composition.

Coordinator interface: restore the session's Actor alias and quest runtime as
above, then construct `VerticalQuestSession` with those exact owners, actual
quest-use state, current `ZoneNpcs` and pool, real `VerticalQuestPorts`, live
`PlayerCore`, status/experience rules, experience ports and live options,
original item-name provider, and the same NPC interaction and packet action
providers used by movement and leave. Use `session.quests()` for quest entry,
progress, timer and save, and `session.packets()` for quest/NPC dispatch and
interaction cleanup. The upstream `reward_world` callback is not called by
this composition; its internally bound reward performs actual removal and uses
the supplied real services. Retain every referenced owner and serialize calls.
On transfer/disconnect run real interaction cleanup before destruction; if the
session retains an Actor alias back to its world session, explicitly release
quest session owners during teardown to break the strong ownership cycle.
The coordinator's network, scheduler, DB and SQL services remain external.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-session-composition-build.log`); `python3 tools/dev.py
test -R '^(vertical-quests-unit|quest-rewards-unit|quest-requests-actor-unit|quest-start-items-actor-world-unit|quest-replica-unit|experience-award-unit|vertical-disconnect-unit)$'`
passed **7/7** (log `work/overnight-07-quest-session-composition-tests.log`). The
combined fixture exercises entry, NPC selection and quest packets, mob progress,
active save, all actual reward owners, `CharacterReplicaDelivery`, completion
and movement cleanup through the composed session; it rejects a mismatched
player and forbids use of the upstream reward callback. `git diff --check`
passed. Isolated SQL integration was unavailable because `.local/runtime.env`
and `.local/sqlserver.env` are absent; no SQL lifecycle action was taken.
Original-client end-to-end play remains unverified.

### Retain composed quest sessions during zone timer traversal

`VerticalQuestSessionZoneUsers` adapts a required real
`VerticalQuestSessionResolver` to the existing `VerticalQuestZoneUsers`
interface. The resolver returns the live composed session for the traversed
Actor; the adapter rejects null or a different Actor object and returns an
aliasing `shared_ptr<VerticalQuests>` that retains the whole
`VerticalQuestSession` through `ZoneUsers::visit_all` and timer callbacks.
Referenced external scene, ports, pools and use-state must still be retained by
the real session owner. The adapter does not provide a registry or fake timer
success.

Original evidence: `ps_game.exe` `CZone::UpdateQuest` **00422320** visits the
actual zone users and invokes each `CUser::UpdateQuest` **0048d1a0**; the
`CWorldThread::Update` **00407390** quest cadence is at
**0040771d..0040775a**. The native alias preserves the owner lifetime across the
existing original-derived traversal and callbacks.

Coordinator interface: implement `VerticalQuestSessionResolver::retain(Actor&)`
with the real session registry, returning that exact actor's retained
`VerticalQuestSession` without mutating zone membership. Supply a
`VerticalQuestSessionZoneUsers` to `vertical_quests_update_zone` for each
actual primary and dynamic zone at the original cadence. The coordinator's
current `WorldPrimaryQuestPhase` has its own traversal; if it remains the
caller, its `retain` and `quests` services must resolve the same retained
session and call `quests.update_timers_for(actor, now)` rather than the
unvalidated `update_timers(now)`. Do not run both traversals for the same
phase. Retain the world/session services and release session-held quest owners
at teardown to break any actor-alias ownership cycle.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-session-zone-build.log`); `python3 tools/dev.py test -R
'^(vertical-quests-unit|quest-requests-timers-unit|zone-users-unit|quest-requests-actor-unit|vertical-disconnect-unit)$'`
passed **5/5** (log `work/overnight-07-quest-session-zone-tests.log`). The
vertical fixture uses a composed quest session and real `ZoneUsers` timer pass,
checks mismatch rejection and alias retention, then completes the quest through
real reward and replica owners. `git diff --check` passed. Isolated SQL
integration was unavailable because `.local/runtime.env` and
`.local/sqlserver.env` are absent; no SQL lifecycle action was taken.
Original-client end-to-end play remains unverified.

### Retained quest-session credit from mob death

`VerticalQuestSessionMobCredit` binds the existing `MobDeathWorld::quest_kill`
callback to a required actual credited-Actor resolver and the real composed
quest-session resolver. It retains both owners through `kill_mob`, verifies the
death record's user ID before and after session resolution, and checks exact
Actor object identity against that session. Missing or mismatched owners throw
before quest progress; experience, drops, party locking and all other death
services remain with the existing `mob_death` owner.

Original evidence: `ps_game.exe` `CMob::Death` **004910f0** invokes
`CUser::QuestUpdateKillMOB` **00480140** for eligible credited users. Existing
mob-death and quest original-execution tests cover eligibility, party ordering
and quest packet behavior; the new class only binds the callback to retained
native session owners.

Coordinator interface: implement `VerticalQuestMobActorResolver::retain` using
the actual live Actor represented by each `MobDeathUser` projection, with a
lease retaining that Actor and its scene; implement
`VerticalQuestSessionResolver::retain` using the exact Actor's real quest
session. In the real `MobDeathWorld::quest_kill(user, mob)` callback invoke
`credit.apply(user, mob)` once, with the actual `MobQuestLinks`. Keep the mob,
party, user projections, resolvers, session dependencies and links alive across
the serialized death call. Do not also wrap that same callback with
`VerticalQuestMobDeath` or apply a second quest credit. The resolver must itself
establish that the projection represents the returned Actor; equal numeric IDs
alone cannot prove that relationship.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-session-mob-credit-build.log`); `python3 tools/dev.py test -R
'^(vertical-quests-unit|mob-death-unit|mob-quests-unit|quest-requests-actor-unit|quest-replica-unit|vertical-disconnect-unit)$'`
passed **6/6** (log `work/overnight-07-session-mob-credit-tests.log`). The
combined fixture runs the real `mob_death` coordinator and retained quest
session through `CharacterReplicaDelivery`, rejects a same-ID different Actor
and a missing session, and checks original reward/drop/quest ordering. Its
unrelated experience and drop callbacks are test observers, not production
services. `git diff --check` passed. Isolated SQL integration was unavailable
because `.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL
lifecycle action was taken. Original-client end-to-end play remains unverified.

### Make NPC interaction switching safe on lease failure

`VerticalQuestNpcPackets` now pins a different `0e04` target in the actual
session `NpcPool` before invoking the existing `vertical_npc_request` switch.
If native pinning fails, the previous interaction pointer and both NPC use
counts remain unchanged. After a successful pin, the existing handler still
performs the original switch, and the adapter transfers the prepared lease.
Malformed, unknown, missing-target and already-selected handling remains with
the original-derived handler. The caller must still serialize NPC membership,
retirement and packet handling and use the same NPC pool for the zone.

Original evidence: `ps_game.exe` `CUser::PacketNpc` **00467510** implements the
`0e04` selected-NPC and use-count transition; the successful branch has no
intervening service callback. The previously documented original packet oracle
covers that handler. Pool pinning and its failure handling are native lifetime
safety measures, not an original packet or a fabricated successful callback.

Coordinator interface: construct the session adapter with the actual zone's
canonical NPC pool and keep that pool and session alive through selection,
movement and leave cleanup. A pin failure indicates inconsistent or exhausted
native ownership and propagates; do not report the switch as successful. Keep
using the same `VerticalNpcInteraction` and the movement/leave integration
contracts documented above.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-npc-switch-pin-build.log`); `python3 tools/dev.py test -R
'^(vertical-quests-unit|zone-npcs-unit|quest-start-items-actor-world-unit|quest-requests-actor-unit|quest-requests-completion-unit)$'`
passed **5/5** (log `work/overnight-07-npc-switch-pin-tests.log`). The vertical
fixture selects a real NPC, attempts to switch to an NPC owned by a different
real pool, verifies the failure preserves the pointer and counts, and runs real
movement cleanup. `git diff --check` passed. Isolated SQL integration was
unavailable because `.local/runtime.env` and `.local/sqlserver.env` are absent;
no SQL lifecycle action was taken. Original-client end-to-end play is unverified.

### Party mob credit through composed quest sessions

The vertical fixture now runs the existing `mob_death` party coordinator through
`VerticalQuestSessionMobCredit` and three distinct actual
`VerticalQuestSession` / Actor / QuestRuntime owners. It checks the original
party lock and range order, credits only the two eligible members, verifies
runtime counters and DB-agent quest replicas, and rejects a resolver returning
another member's session. On rejection the existing death coordinator releases
the party lock; previously published credit remains published. A progress
callback drops the last external session reference and confirms that the bridge's
lease retains the credited session until that callback returns, then releases it.
This milestone adds integration coverage; it does not change production code.

Original evidence: `ps_game.exe` `CMob::Death` **004910f0** performs party
eligibility/reward dispatch and invokes `CUser::QuestUpdateKillMOB` **00480140**
for eligible recipients. Existing mob-death and quest original-execution
oracles separately cover the recovered branch and packet behavior.

Coordinator interface remains `VerticalQuestSessionMobCredit::apply` in the
real `MobDeathWorld::quest_kill` callback. Actor and quest-session resolvers must
resolve each actual party member under the existing party lock and retain its
owners across callbacks. Preserve existing range, experience and drop services;
do not invoke a second quest-credit adapter for the same recipient.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-party-quest-session-build.log`); `python3 tools/dev.py test -R
'^(vertical-quests-unit|mob-death-unit|mob-quests-unit|quest-requests-actor-unit|quest-replica-unit|vertical-disconnect-unit)$'`
passed **6/6** (log `work/overnight-07-party-quest-session-tests.log`). The test
uses the real death coordinator, quest runtime and replica; unrelated party
experience/drop callbacks are test observers, not production implementations.
`git diff --check` passed. Isolated SQL integration was unavailable because
`.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL lifecycle
action was taken. Original-client end-to-end play remains unverified.

### Retain the quest session through player-kill credit

`vertical_quests_session_kill_pc_to` resolves and retains the killer's actual
`VerticalQuestSession`, rejects a missing or different Actor object, and invokes
the existing `vertical_quests_kill_pc_to` boundary. That boundary applies the
real quest progress first and then calls the required real `KillPCTo`
bookkeeping continuation. The session lease remains alive across both stages;
no kill log, counter, DB/client notice or successful continuation is invented.
The caller must retain killer, victim and continuation for the synchronous call.

Original evidence: `ps_game.exe` `CUser::KillPCFrom` **0045b050** calls
`CUser::KillPCTo` **0045aed0** at **0045b055**; `KillPCTo` calls
`CUser::QuestUpdateKillPC` **00480020** at **0045aeeb** before its remaining
log, counter and notice work. Existing progress/original-execution tests cover
the recovered quest operation.

Coordinator interface: implement `VerticalQuestSessionResolver::retain` using
the killer's actual session registry entry and invoke
`vertical_quests_session_kill_pc_to(killer, victim, resolver, continuation)`
at the original `KillPCTo` boundary. The continuation must implement the real
remaining `KillPCTo` bookkeeping and must not call a full `KillPCTo` entry
again. The caller must retain both Actors and their scenes and serialize the
call; the session resolver must retain all referenced quest services. The real
KillPCFrom/KillPCTo backend is still an external integration dependency.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-player-kill-session-build.log`); `python3 tools/dev.py test -R
'^(vertical-quests-unit|quest-requests-progress-unit|quest-replica-unit|quest-requests-actor-unit|mob-targeting-lifecycle-experience-unit|vertical-disconnect-unit)$'`
passed **6/6** (log `work/overnight-07-player-kill-session-tests.log`). The
vertical fixture uses an actual composed session, quest runtime and replica,
rejects a missing and same-ID wrong Actor, drops external session references in
the progress callback, and checks retention through the bookkeeping observer.
The observer is a test continuation, not a production bookkeeping service.
`git diff --check` passed. Isolated SQL integration was unavailable because
`.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL lifecycle
action was taken. Original-client end-to-end play remains unverified.

### Quest-item drop eligibility from retained sessions

`VerticalQuestSessionLoot` supplies the two quest-specific callbacks needed by
`MobLootWorld`: `end_quests` reads the actual `ItemQuestLinks` for the live item
type/ID, and `has_quest` retains the actual Actor and composed quest session,
checks their identity against the loot-user projection, and calls the existing
locked `VerticalQuests::active` lookup. It returns no links for an unknown item
and propagates missing/mismatched owners rather than inventing eligibility.
Selection, random rolls, party ownership, item allocation, delivery, money and
logs remain with the existing required mob-loot services.

Original evidence: `ps_game.exe` `CMob::ItemDrop` **004a5ea0** checks linked
quest IDs against party members via `CUser::QuestFind` **0047e4b0** at
**004a6016** and the solo user at **004a6056**. The existing mob-loot and
quest-find original-execution fixtures cover those recovered routines; this
change binds their callbacks to the live native quest owners.

Coordinator interface: retain the real `ItemQuestLinks`, implement
`VerticalQuestLootActorResolver::retain(MobLootUser&)` with the exact live
Actor represented by each loot projection, and implement the existing
`VerticalQuestSessionResolver` with that Actor's actual session. Route the real
`MobLootWorld::end_quests` and `has_quest` callbacks to this bridge, retaining
all actors, sessions, item definitions, links and party state through the
serialized loot call. Continue using the real mob-loot item/delivery, RNG,
party and logging owners; this bridge does not manufacture a dropped item.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-loot-session-build.log`); `python3 tools/dev.py test -R
'^(vertical-quests-unit|mob-drops-loot-unit|mob-drops-unit|mob-quests-unit|quest-requests-actor-unit|item-pick-unit)$'`
passed **6/6** (log `work/overnight-07-quest-loot-session-tests.log`). The
vertical fixture uses the real `mob_loot` coordinator, item links, two composed
sessions and locked quest runtimes; it checks solo ineligibility, eligibility
from another party member, unknown links and mismatched actor/session rejection.
Its item-delivery callback is a test observer, not a production drop service.
`git diff --check` passed. Isolated SQL integration was unavailable because
`.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL lifecycle
action was taken. Original-client end-to-end play remains unverified.

### Quest drop through actual zone and inventory owners

The quest-loot fixture now continues an eligible solo quest-item drop through
the existing `deliver_mob_drop` creation/delivery owner, actual shared
`GroundItemPool`, shared `ItemIds`, `ZoneItems`, locked quest pickup
eligibility, `Actor::pickup_item`, retained Actor inventory and actual
`InventoryReplica`. It verifies the same item UID moves from the zone to the
Actor and DB-agent replica, with the `0a01` quest-start and `0701` inventory
updates and the pickup log. The original quest counter remains unchanged in
this path: the original call site for `QuestUpdatePickItem` **00480390** is
still unproven, so the fixture does not invent that callback. This milestone
adds in-process integration coverage; it does not modify production behavior.

Original evidence: `ps_game.exe` `CMob::ItemDrop` **004a5ea0** checks quest
eligibility, its item-delivery overload **004a65a0** creates and publishes the
item, `CZone::GetPickItem` **004174a0** checks pickup eligibility, and
`CUser::ItemGet` **0045d2e0** admits the selected item. Existing original
mob-loot, drop-delivery and item-pick oracles cover these recovered routines
separately.

Coordinator interface: route the real mob-loot quest callbacks through
`VerticalQuestSessionLoot`, use the actual drop-delivery services and shared
item pool/UID owner, and route pickup through `VerticalQuestPickupWorld` with
the same Actor and quest session. Real zone broadcast, inventory DB transport,
item logs and session ownership remain required. This fixture calls the Actor
pickup method directly and uses test observers for external broadcasts and logs;
it does not validate network dispatch, SQL execution or an original client.

Validation: `python3 tools/dev.py build` passed (final log
`work/overnight-07-quest-drop-pickup-build-final.log`); `python3 tools/dev.py
test -R '^(vertical-quests-unit|mob-drops-loot-unit|mob-drops-delivery-bridge-unit|item-pick-unit|quest-requests-actor-unit|inventory-replica-unit)$'`
passed **6/6** (final log `work/overnight-07-quest-drop-pickup-tests-final.log`).
The fixture uses the real mob-loot, drop-delivery, zone, pickup, Actor and
replica owners and checks ownership, UID, persistence and the unchanged quest
counter. `git diff --check` passed. Isolated SQL integration was unavailable
because `.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL
lifecycle action was taken. Original-client end-to-end play remains unverified.

## Quest eligibility on the existing mob-loot world

`VerticalQuestMobLootWorld` wraps the existing `MobLootWorld` and supplies quest
item links and retained-session eligibility through `VerticalQuestSessionLoot`.
All other callbacks, including actual item delivery and money services, forward
to the existing owner; exceptions propagate. The integration fixture uses the
wrapper for solo and party eligibility, verifies the underlying quest callbacks
are not used, propagates a delivery failure, and continues an eligible drop
through the real drop, zone, Actor pickup and inventory replica owners.

Original evidence: `ps_game.exe` `CMob::ItemDrop` **004a5ea0** calls
`CUser::QuestFind` **0047e4b0** at **004a6016** for party members and
**004a6056** for the solo user.

Coordinator interface: construct the wrapper with the real `MobLootWorld` and
`VerticalQuestSessionLoot`, then pass it to `mob_loot`. Retain those owners,
the item links and actor/session resolvers for the serialized loot pass. Supply
real delivery, inventory, persistence and broadcast services; this adapter does
not install itself into the coordinator's world session. The item pickup quest
progress call site remains unproven, and original-client play remains unverified.

Validation: `python3 tools/dev.py build` passed (final log
`work/overnight-07-quest-loot-world-build-retry.log`). `python3 tools/dev.py
test -R '^(vertical-quests-unit|mob-drops-loot-unit|mob-drops-delivery-bridge-unit|mob-drops-unit|item-pick-unit|quest-requests-actor-unit)$'`
passed **6/6** (final log `work/overnight-07-quest-loot-world-tests-retry.log`).
The first focused run failed because the new intentional failed delivery added
an attempt to a fixture count; the corrected expectation passed on retry.
`git diff --check` passed. Isolated SQL integration remains unavailable because
`.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL lifecycle
action was taken. Next, integrate the adapter with the coordinator's actual
world/session ownership and validate dispatch and persistence when those owners
and the centrally coordinated SQL environment are available.

## Quest-item pickup through the inventory packet handler

The quest drop fixture now sends the original `0205` pickup request to
`vertical_inventory_request` with the quest-aware `VerticalQuestPickupWorld`,
actual zone and retained Actor. It verifies that a truncated request has no
side effects, then checks exchange cancellation, shop closure and pickup-check
ordering, item UID transfer into the Actor and inventory replica, database
packet sequence and pickup logging. Its external action and log callbacks are
test observers. This milestone extends integration coverage; it does not wire
the live network dispatcher.

Original evidence: `ps_game.exe` `CUser::PacketMaininterface` **00468420**,
pickup handler **004686c9..004686f2**, calls the existing item pickup path;
`CZone::GetPickItem` **004174a0** checks quest eligibility and
`CUser::ItemGet` **0045d2e0** admits the item.

Coordinator interface: dispatch the retained packet to
`vertical_inventory_request` with the same Actor, zone, item quest links and
pool, wrapping the real pickup services with `VerticalQuestPickupWorld` bound
to that Actor's retained quest session. `VerticalInventoryActions` must call
the real exchange cancellation and shop shutdown owners and expose the retained
pickup-check setting. The original item progress call site remains unproven.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-pickup-packet-build.log`); `python3 tools/dev.py test
-R '^(vertical-quests-unit|vertical-inventory-unit|mob-drops-loot-unit|mob-drops-delivery-bridge-unit|item-pick-unit|inventory-replica-unit)$'`
passed **6/6** (log `work/overnight-07-quest-pickup-packet-tests.log`).
`git diff --check` passed. Isolated SQL integration was unavailable because
`.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL lifecycle
action was taken. Network dispatch, SQL execution and original-client play are
not verified. Next work is live session dispatch and real owner wiring when
coordinator interfaces are available.

## Retained quest session for pickup packets

`vertical_quests_session_pickup_request` resolves and retains the matching
`VerticalQuestSession` throughout the original `0205` inventory handler and
constructs its quest-aware pickup world over the caller's actual services. It
rejects absent or mismatched sessions before action callbacks, and malformed or
unrelated packets do not resolve a session. The integrated quest-drop fixture
checks mismatched and missing sessions, releases the resolver's last external
session references inside the exchange callback, verifies the session remains
alive through pickup, and verifies release when the request returns. External
shop/exchange and transport callbacks in that fixture are observers.

Original evidence: `ps_game.exe` `CUser::PacketMaininterface` **00468420**,
pickup branch **004686c9..004686f2**, and `CZone::GetPickItem` **004174a0**;
`CUser::QuestFind` **0047e4b0** supplies the existing eligibility check. A
read-only `objdump -d -M intel` search for direct calls to
`CUser::QuestUpdatePickItem` **00480390** found none; this does not rule out
indirect calls, so no pickup progress callback was added.

Coordinator interface: route `0205` to
`vertical_quests_session_pickup_request` with the retained Actor, zone, item
quest links, actual `ItemPickupWorld`, shared ground pool, real
`VerticalInventoryActions`, and a `VerticalQuestSessionResolver` bound to the
same Actor. The resolver must retain the live session; network dispatch,
exchange and shop services, SQL and real logging remain coordinator-owned.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-pickup-lease-build.log`); `python3 tools/dev.py test
-R '^(vertical-quests-unit|vertical-inventory-unit|item-pick-unit|quest-requests-actor-unit|inventory-replica-unit)$'`
passed **5/5** (log `work/overnight-07-quest-pickup-lease-tests.log`).
`git diff --check` passed. Isolated SQL integration was unavailable because
`.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL lifecycle
action was taken. Network dispatch, SQL execution and original-client play are
unverified. Next, connect this interface to the coordinator's real session and
world owners and validate the complete path when those dependencies are ready.

## Retained quest/NPC packet dispatch

`vertical_quests_session_receive` filters the quest and NPC packet families,
resolves and validates the Actor's matching `VerticalQuestSession`, and retains
it through the existing packet handler and synchronous callbacks. Short and
unrelated packets return without resolving a session. The integrated fixture
uses the entry point for NPC selection, quest acceptance and completion; it
checks missing and wrong sessions, and releases external session references
inside the shop callback to verify the dispatch holds a live owner. Existing
external transport and action callbacks are test observers.

Original evidence: `ps_game.exe` `CUser::PacketQuest` **0046be10** and
`CUser::PacketNpc` **00467510**; the adapter delegates their recovered
behavior to the existing packet owners rather than implementing callbacks.

Coordinator interface: route admitted Actor packets in families `09` and `0e`
to `vertical_quests_session_receive` using a resolver that retains the same
Actor's live quest session. Preserve the existing session, zone, NPC pool,
interaction and real exchange, shop, client, database and log owners; movement
and leave must still clean up the same NPC interaction. This does not install a
live network dispatcher or implement SQL transport.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-packet-session-build.log`); `python3 tools/dev.py test
-R '^(vertical-quests-unit|quest-requests-actor-unit|quest-runtime-unit|npc-request-unit|vertical-inventory-unit)$'`
passed **3/3 matching tests** (log
`work/overnight-07-quest-packet-session-tests.log`; the two other names do not
match registered tests). `git diff --check` passed. Isolated SQL integration
was unavailable because `.local/runtime.env` and `.local/sqlserver.env` are
absent; no SQL lifecycle action was taken. Network dispatch, SQL execution and
original-client play remain unverified. Next integrate the entry point into the
coordinator's live session ownership when its dispatcher is available.

## Retained session for NPC movement and leave cleanup

`vertical_quests_session_move_interactions` and
`vertical_quests_session_leave_interactions` resolve and retain the matching
Actor's quest session while delegating to its existing NPC interaction cleanup
owners. They reject missing or mismatched sessions before cleanup. The combined
fixture verifies movement and leave clear the selected NPC and decrement its
use count, checks failure paths, and releases external session references from
the PvP and shop callbacks while confirming the session stays alive. Those
callbacks are test observers; no live services were substituted in production.

Original evidence: `ps_game.exe` `CUser::PacketPC` **0046a55f..0046a64d**
contains movement interaction cleanup, and `CUser::LeaveZone` **0044aee0**
clears the NPC at **0044aee0..0044aef4** before shop and exchange cleanup.

Coordinator interface: replace the corresponding existing interaction step
with the matching new helper, passing the admitted Actor, its real movement or
leave services, actual shop state for leave, and the same retained-session
resolver used for NPC packets. These helpers do not perform the remainder of
movement or zone leave; do not run the replaced interaction step twice. NPC
interaction cleanup must precede session destruction.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-session-cleanup-build.log`); `python3 tools/dev.py test
-R '^(vertical-quests-unit|vertical-move-interactions-unit|zone-leave-interactions-unit|quest-requests-actor-unit)$'`
passed **2/2 matching tests** (log
`work/overnight-07-quest-session-cleanup-tests.log`; the movement and leave
names in the filter did not match registered tests). `git diff --check` passed.
Isolated SQL integration was unavailable because `.local/runtime.env` and
`.local/sqlserver.env` are absent; no SQL lifecycle action was taken. Actual
network dispatch, SQL execution and original-client play remain unverified.
Next integrate the helpers into the coordinator's world/session lifecycle.

## Retained quest session during character entry and save

`vertical_quests_session_send_entry_step` and
`vertical_quests_session_save_runtime` retain and verify the matching Actor's
quest session while delegating to the existing entry and save owners. Unrelated
entry steps return without resolving a session. The integrated fixture rejects
mismatched sessions and releases external session references in the entry lock
and save database callbacks while confirming the operation retains the session.
External delivery callbacks in the fixture are observers.

Original evidence: `ps_game.exe` `CUser::SendCharacterQuest` **004835f0**
sends active quest entry, and `CUser::SendDBAgentCharGetInfo` **0046c980**,
quest phase **0046ccd4..0046ce4a**, saves active quest batches.

Coordinator interface: use the new helpers for the corresponding real
character-entry quest steps and save phase, passing the admitted Actor, actual
finished-list lock and `VerticalQuestSessionResolver`. The coordinator's newer
`PrimaryQuestPhaseServices` additionally requires `retain(Actor&)` to pin the
Actor, session, runtime, definitions and transport through the visitor, and
`quests(Actor&)` to return that same pinned session's `VerticalQuests&`; reject
identity mismatches. Its primary-only phase does not cover dynamic zones. The
coordinator source containing that interface is newer than this branch's
snapshot and was inspected read-only; this milestone does not wire it.

Validation: `python3 tools/dev.py build` passed (final log
`work/overnight-07-quest-session-lifecycle-build-retry.log`); `python3 tools/dev.py
test -R '^(vertical-quests-unit|quest-requests-actor-unit|quest-runtime-save-unit|character-entry-unit)$'`
passed **3/3 matching tests** (final log
`work/overnight-07-quest-session-lifecycle-tests-retry.log`; the save test name
did not match a registered test). The first test run failed because the new
fixture callback replaced an existing database-order observer; the retry chains
and restores it. `git diff --check` passed. Isolated SQL integration was
unavailable because `.local/runtime.env` and `.local/sqlserver.env` are absent;
no SQL lifecycle action was taken. Live coordinator wiring, SQL execution and
original-client play remain unverified.

## Pickup attack-state gate before quest-session resolution

The retained-session pickup adapter previously resolved the session before the
inventory handler's attack-state gate. A valid `0205` request from an Actor in
attack mode 1 could therefore throw for a missing session instead of being
ignored. The adapter now applies the same gate after packet framing and before
session resolution. The real inventory handler still performs the pickup. The
resolver must not mutate the Actor's attack state or packet while resolving;
the serialized owner must keep those values stable across the handoff.

Original evidence: `ps_game.exe` `CUser::PacketMaininterface` **00468420**,
`0205` handler **004686c9..004686f2**, gates pickup before exchange and shop
callbacks. The existing `vertical-inventory-unit` compares its handler with
512 original-request oracle cases. The combined quest fixture additionally
checks attack mode 1 with a missing session, verifies an ignored result and no
item or action effects, and still rejects a missing session when the gate is
open.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-pickup-gate-build.log`); `python3 tools/dev.py test -R
'^(vertical-quests-unit|vertical-inventory-unit|item-pick-unit)$'` passed
**3/3** (log `work/overnight-07-quest-pickup-gate-tests.log`). `git diff
--check` passed. Isolated SQL integration remains unavailable because
`.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL lifecycle
action was taken. The coordinator must wire the retained-session pickup helper
and real services as described above. Live network dispatch, SQL execution and
original-client play remain unverified.

## Coordinator integration dependency

Read-only inspection of coordinator commit `9d6799e` and its current source
found `WorldPrimaryQuestPhase` and `WorldRoot::bind_primary_quest_phase`, but
no concrete `PrimaryQuestPhaseServices` implementation, no retained
`VerticalQuestSession` on `VerticalWorldSession`, and no production routing of
the quest/NPC and quest-aware pickup adapters. The newer world-phase header is
not present in this branch snapshot. The existing primary phase calls
`quests.update_timers(now)` after asking its service for `quests(actor)`;
the integration must ensure that is the exact pinned Actor's quest session.
Prefer `update_timers_for(actor, now)` or equivalently validate
`quests.owns_actor(actor)` before updating. Its `retain(actor)` token must pin
the session, Actor, quest runtime, definitions and transports throughout the
visitor, and `quests(actor)` must return the quest owner from that same token.
Dynamic instance zones need their own traversal; the primary phase visits only
primary zones.

Required coordinator-owned interfaces: attach the constructed
`VerticalQuestSession` to the admitted `VerticalWorldSession` and its Actor;
provide a serialized `VerticalQuestSessionResolver` with exact Actor identity;
route packet families `09`/`0e` and `0205`, character quest entry/save,
movement/leave cleanup, mob credit and quest loot through the corresponding
adapters documented above; supply actual exchange, shop, zone, item, DB-agent,
logging and transport owners. Implement and bind the primary phase service,
then extend the real scheduler to active instance zones. Original evidence is
`ps_game.exe` `CWorldThread::Update` **0040771d..00407760** and
`CZone::UpdateQuest` **00422320**; quest packet evidence is
`CUser::PacketQuest` **0046be10** and `CUser::PacketNpc` **00467510**.

No code changed or tests were run in this inspection. Previous milestone tests
and exact commands are recorded above. Both `.local/runtime.env` and
`.local/sqlserver.env` remain absent; no SQL lifecycle action was taken. Actual
SQL execution, network dispatch and original-client end-to-end play remain
unverified. Further production integration in this block depends on the
coordinator-owned session/dispatcher and actual service bindings; changing
shared cross-subsystem files or fabricating those services would violate the
assigned scope.

WAITING_FOR_ASSIGNMENT

## World-session quest ownership: initial integration

The expanded assignment supersedes the earlier `WAITING_FOR_ASSIGNMENT`.
This branch now contains the coordinator's `VerticalWorldSession` and required
current supporting contracts. During its actual preparation callback,
`install_quests` accepts a single `unique_ptr<VerticalQuestSession>` bound to
the exact restored `WorldUser::actor`; a mismatched Actor, missing preparation,
closing session, duplicate install, or Actor lease aliasing the owning world
session is rejected. `quests_lease` aliases the world session's ownership so the
Actor, quest session and its references survive callbacks and disconnect drain.
The session still retains the reservation, IO lease, admission and DB-work
fences from the coordinator. Cleanup remains the real disconnect service's
responsibility before final destruction.

`VerticalWorldQuestSessions` resolves the actual Actor pointer through the
existing world registry and returns the pinned quest session, including while a
closing Actor remains registered for zone-leave cleanup.
`VerticalWorldPrimaryQuestServices` supplies the coordinator's real primary
quest phase, and the copied phase calls `update_timers_for(actor, now)` to
reject a wrong Actor. No successful gameplay service was fabricated. The
coordinator's preparation service still must construct and install the quest
session with real DB, log, quest, NPC, inventory, experience and transport
owners; bind the primary service to `WorldRoot`, route packets and teardown,
and cover dynamic zones. The Actor lease used for construction must be the
independently owned `character()->pipeline().result().actor`, not the
session-aliasing Actor returned by `vertical_quests_restore_session`.

Coordinator source ported to this isolated branch: `vertical_world_session.hpp`,
`vertical_owned_selection.hpp`, `world_primary_quest_phase.hpp`, current
`vertical_session_registry.hpp`, `vertical_world.hpp`, session registry header
and implementation, world protocol and socket headers and implementations,
framing header and implementation, `outbound_budget.hpp`,
`world_reply_schedule.hpp`, entry delivery header and implementation, and
matching `world_protocol_unit.cpp`. These carry the coordinator's existing
reservation, IO, social/guild entry and drain contracts; they are not a claim
that all coordinator changes have been ported. The quest-specific coordinator
diff is the attachment/lease in `vertical_world_session.hpp`,
`vertical_quests_world_session.hpp`, `actor_owner()` on the quest session, and
the exact-Actor call in the primary phase. Reconcile against current
coordinator source rather than replacing newer shared files wholesale.

Original evidence: `ps_game.exe` `CWorldThread::Update` **0040771d..00407760**
and `CZone::UpdateQuest` **00422320**, plus `CUser::PacketQuest` **0046be10**
and `CUser::PacketNpc` **00467510**. Native build passed with
`python3 tools/dev.py build` (final log
`work/overnight-07-world-quest-build-retry.log`); `python3 tools/dev.py test -R
'^(vertical-quests-unit|vertical-session-registry-unit|vertical-world-unit|world-protocol-unit|vertical-social-replica-unit)$'`
passed **5/5** (log `work/overnight-07-world-quest-tests-retry.log`). The new
quest-specific check covers absent Actor resolution; it does **not** cover a
successful real world-session install, resolver lease across teardown, or the
primary phase with a real quest session. The first focused run failed because
this older branch's world-protocol test expected the old duplicate policy; the
retry uses the coordinator's matching test. Earlier compatibility builds failed
while copying required coordinator dependencies; their logs are retained.
`git diff --check` passed. Isolated SQL integration remains unavailable because
`.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL lifecycle
action was taken. Live network dispatch, SQL execution and original-client
play remain unverified. Next: add a real prepared-session fixture covering
install, duplicate/wrong/cyclic rejection, registry identity, primary ticking,
disconnect drain and reservation/IO lease lifetime, then construct quest
services from the real runtime preparation owner and wire entry/packet paths.

## Correct Actor ownership for world-session quest preparation

The preceding milestone's instruction to use `character()->pipeline().result().actor`
as an independently shared owner was incorrect: that member is a
`unique_ptr<Actor>`. Passing it as a `shared_ptr` is impossible, while an alias
of the owning `VerticalWorldSession` would form a cycle once the quest session
is installed. The world session now holds its `VerticalSessionCharacter` by
`shared_ptr` and exposes `quest_actor_lease()` only during preparation. The
lease aliases the actual pipeline Actor through an owner that retains both the
character and its actual scene, without retaining the world session. Its member
order destroys the character and Actor before the scene. The original session
still owns the character and scene, and the existing quest-install identity and
cycle checks remain in place. Other Actor rules, pools, services and transports
must still outlive their real session and quest owners.

The new test creates a real `VerticalSessionCharacter` and loaded Actor,
verifies exact Actor identity, drops external character/scene references, and
confirms the lease retains both until released. It also rejects missing owners.
It does not yet exercise `quest_actor_lease()` through a live prepared world
session, successful installation, primary quest ticking, or disconnect drain.
The coordinator's current `VerticalSessionRegistry::bind_network_registry` has
since gained an explicit realm binding; retain that newer contract when
reconciling this branch and do not overwrite the coordinator's registry with
this branch's earlier copied snapshot.

Original evidence for the owning lifecycle remains `ps_game.exe`
`CUser::PacketUserDBChar` **0046cf60**, `CWorld::WaitUser` **0040ce70**, and
`CWorld::LeaveUser` **0040ca20**; this change concerns native ownership around
those recovered paths and does not claim additional original protocol behavior.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-world-quest-actor-lease-build.log`); `python3 tools/dev.py
test -R '^(vertical-quests-unit|vertical-session-registry-unit|vertical-world-unit|vertical-social-replica-unit|world-protocol-unit)$'`
passed **5/5** (log `work/overnight-07-world-quest-actor-lease-tests.log`).
`git diff --check` passed. Isolated SQL integration remains unavailable because
`.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL lifecycle
action was taken. Next, construct the quest runtime and real services from the
prepared session using `quest_actor_lease()`, install them, and test the actual
registry/phase/closing path with reservation, IO and DB drain fences. Original
client end-to-end play remains unverified.

## Prepared world-session quest attachment fixture

The integration fixture now starts an actual `VerticalWorldSession` using the
real account claim, selection owner, world registry, Actor load pipeline and
preparation callback with a synthetic snapshot. During preparation it obtains
`quest_actor_lease()`, constructs and installs the actual
`VerticalQuestSession`, rejects a cyclic session-owned Actor alias and a second
installation, resolves the installed quest owner through
`VerticalWorldQuestSessions` and `VerticalWorldPrimaryQuestServices`, and
rejects a different Actor object with the same ID. It calls the retained quest
owner's timer update for that exact Actor. While closing, the disconnect callback
can still resolve the quest owner; after disconnect the fixture checks the
registry, admission-list and IO lease drain, verifies an outstanding quest lease
keeps the session alive, and verifies final release leaves no ownership cycle.
Unused external callbacks throw; test-only preparation and disconnect callbacks
provide the synthetic lifecycle, not real DB or gameplay services.

Original evidence: `ps_game.exe` `CUser::PacketUserDBChar` **0046cf60**,
`CWorld::WaitUser` **0040ce70**, `CWorld::LeaveUser` **0040ca20**,
`CWorldThread::Update` **0040771d..00407760** and `CZone::UpdateQuest`
**00422320**. This validates native owner composition, not original-client
protocol execution. The fixture uses the copied registry contract in this
branch; the coordinator's newer realm-bound network registry must be preserved
when reconciling. It does not run the actual `WorldPrimaryQuestPhase` traversal,
real quest preparation services, full admission, SQL, social/guild teardown or
an original client. Next connect the preparation service and packet/entry/save
paths to real runtime owners and add a populated primary-zone timer fixture.

Validation: `python3 tools/dev.py build` passed (final log
`work/overnight-07-world-quest-session-fixture-build-final.log`); `python3
tools/dev.py test -R '^(vertical-quests-unit|vertical-session-registry-unit|vertical-world-unit|world-protocol-unit|vertical-social-replica-unit)$'`
passed **5/5** (final log
`work/overnight-07-world-quest-session-fixture-tests-final.log`). `git diff
--check` passed. The `vertical-quests-unit` target now links `eternity_world` to
exercise the real session and reservation owners. Isolated SQL integration was
unavailable because `.local/runtime.env` and `.local/sqlserver.env` are absent;
no SQL lifecycle action was taken. Original-client play remains unverified.

## Restore the live world's retained quest replica

`vertical_world_quests_restore` connects selected-session preparation to the
existing quest replica restoration owner without creating a session ownership
cycle. It acquires `VerticalWorldSession::quest_actor_lease()`, locks the
caller-supplied actual character-replica mutex, verifies exact session account,
Actor and replica character ID, restores through
`vertical_quests_restore_replica`, and rechecks identity before returning the
Actor and pooled runtime. The returned Actor lease retains the real selected
character and scene rather than aliasing the owning world session.

The real prepared-session fixture now loads a synthetic quest into its actual
`CharacterReplica`, restores that quest and counter into the pooled runtime,
and installs the returned Actor and runtime in its `VerticalQuestSession`. It
also checks a changed Actor account is rejected before restoration; existing
cycle, duplicate, exact-Actor, closing, admission and IO checks still run. The
fixture uses a real mutex protecting its test replica and synthetic external
services. It does not execute SQL or live packet dispatch.

Original evidence: `ps_game.exe` `CUser::PacketUserDBSetInfo` **0046d780**,
quest load cases **0506/0507**, and `CUser::SendCharacterQuest` **004835f0**.
Coordinator interface: call `vertical_world_quests_restore` on the serialized
world strand during real session preparation, passing the selected character's
actual replica/save mutex, tick, retained quest pool and definitions. Construct
the quest session from its returned Actor and runtime with the real ports, then
install it via `install_quests`. Wire entry, packet, save, movement and leave
callbacks to the same owner and preserve the coordinator's newer realm-bound
registry. The coordinator now also has an `update_active` quest-phase path for
active zones; this branch has not exercised it with a populated live zone.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-world-quest-restore-build.log`); `python3 tools/dev.py test
--timeout 30 -R '^(vertical-quests-unit|vertical-session-registry-unit|vertical-world-unit|world-protocol-unit|vertical-social-replica-unit)$'`
passed **5/5** (log `work/overnight-07-world-quest-restore-tests.log`).
`git diff --check` passed. Isolated SQL integration remains unavailable because
`.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL lifecycle
action was taken. Live service construction, active-zone traversal, SQL and
original-client play remain unverified.

## Cohosted quest DB-agent replica ports

`VerticalWorldQuestReplicaPorts` routes quest/inventory DB packets to the actual
selected `CharacterReplica` through `CharacterReplicaDelivery`, using the same
recursive mutex supplied by the real save owner. It validates exact Actor and
replica account/character identity, propagates rejected, unsupported or
failed replica operations as failures, and forwards clocks, quest locking,
client delivery, reward and log operations to the supplied real world ports.
It creates no DB cache and reports no fake service success.

The prepared-world-session fixture now constructs this adapter over that
session's actual replica and the fixture's save mutex. It restores an active
quest, dispatches original quest-family opcode `0908` through the retained
session, and verifies the live runtime and the same character replica both
remove the quest without sending to an alternative fixture DB cache. It also
checks malformed DB packets and a different Actor object are rejected. Its
external clock, action, logging and transport owners remain test fixtures; this
is synchronous cohosted DB-agent delivery, not SQL execution or network
transport.

Original evidence: `ps_game.exe` `CUser::PacketQuest` **0046be10** dispatches
`0908` to failure completion through `CUser::QuestEnd` **0047ec40**;
`ps_dbAgent.exe` `CUser::PacketUserSetStatus` **00405b00** owns the character
cache packet family including quest removal `0a02`.

Coordinator interface: construct `VerticalWorldQuestReplicaPorts` with the
exact retained Actor, `session.character()->replica()`, the shared recursive
mutex used by `VerticalWorldSessionSaveBinding`/`CharacterSaveStore`, and real
world quest services; retain all those owners through quest-session teardown.
Use it as the `VerticalQuestSession` upstream only in a cohosted DB-agent
configuration. A remote DB-agent must use its real ordered transport and failure
policy instead. Install the constructed quest session during real preparation;
packet routing, save/entry, active-zone tick, and teardown still require live
wiring. Preserve the coordinator's newer realm-bound registry and fences.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-world-quest-replica-ports-build.log`); `python3 tools/dev.py
test --timeout 30 -R '^(vertical-quests-unit|quest-requests-actor-unit|vertical-session-registry-unit|vertical-world-unit|world-protocol-unit|vertical-social-replica-unit|character-replica-unit)$'`
passed **6/6 matching tests** (log
`work/overnight-07-world-quest-replica-ports-tests.log`; the character replica
name did not match a registered test). `git diff --check` passed. Isolated SQL
integration remains unavailable because `.local/runtime.env` and
`.local/sqlserver.env` are absent; no SQL lifecycle action was taken. SQL
execution, production service wiring and original-client play remain
unverified.

## Quest accept, mob progress and reward on the retained world session

The prepared-world-session fixture now takes a repeatable NPC quest through
actual `0902` acceptance, the actual `mob_death` coordinator and retained
`VerticalQuestSessionMobCredit`, and `0903` NPC completion. Quest progress,
quest removal, reward money and reward item DB packets go to the selected
character's real `CharacterReplica` through `VerticalWorldQuestReplicaPorts`;
experience goes through the same replica and save mutex using the existing
`CharacterReplicaDelivery`. The fixture checks live quest state and replica
counters, completed repeat-quest removal, Actor money and inventory UID, and
replica money, experience and inventory. It first abandons the restored quest,
then reaccepts it as a repeatable quest. Its mob experience/drop callbacks,
clock, action and log transports are test observers, and it uses a synthetic
snapshot and NPC; this is not an original-client or SQL test. No production
code changed in this milestone.

Original evidence: `ps_game.exe` `CUser::PacketQuest` **0046be10**,
`CUser::QuestStartRequest` **0047f220**, `CUser::QuestUpdateKillMOB`
**00480140**, `CUser::QuestEndRequest` **0047f2a0**, and
`CUser::QuestSuccessResult` **0047ee40**; `ps_dbAgent.exe`
`CUser::PacketUserSetStatus` **00405b00** receives the associated cache
packets. Real runtime preparation, packet routing, mob reward/drop services,
zone traversal and disconnect ownership still need coordinator wiring using the
interfaces documented above; preserve its newer realm-bound registry.

Validation: `python3 tools/dev.py build` passed (final log
`work/overnight-07-world-quest-lifecycle-build-retry.log`); `python3 tools/dev.py
test --timeout 30 -R '^(vertical-quests-unit|quest-requests-actor-unit|vertical-session-registry-unit|vertical-world-unit|world-protocol-unit|vertical-social-replica-unit)$'`
passed **6/6** (final log `work/overnight-07-world-quest-lifecycle-tests-retry.log`).
The first run failed at acceptance because the fixture tried to restart an
abandoned non-repeatable quest and had an incompatible country; the corrected
fixture uses the real repeat and faction rules. `git diff --check` passed.
Isolated SQL integration remains unavailable because `.local/runtime.env` and
`.local/sqlserver.env` are absent; no SQL lifecycle action was taken. Production
network dispatch, SQL execution and original-client play remain unverified.

## Owned cohosted quest installation during world preparation

`vertical_world_install_replica_quests` now constructs the actual
`VerticalWorldQuestReplicaPorts` and `VerticalQuestSession` from the selected
Actor, its actual `CharacterReplica`, the supplied save mutex, existing
quest/item/zone owners and the world's real upstream services. It installs both
on `VerticalWorldSession` during preparation. The session owns the replica ports
and destroys the quest session before those ports; it validates that an owned
ports object is exactly the one bound to that quest session. The existing
single-install, preparation, Actor identity and session-cycle checks remain in
force. Clocks, logs, quest locks, client routing, experience, actions, NPC
interaction and all shared pools still must be the actual world owners and
outlive quest cleanup; no successful service callback is substituted.

The prepared-session fixture uses this installation path for the existing
NPC-targeted quest accept, real mob-credit and completion/reward path through
its selected replica. It rejects a different ports object before installation,
retains the prior cycle/duplicate/Actor checks, and closes through the existing
registry, admission and IO fences. The snapshot, NPC and external services are
synthetic test fixtures; the test does not execute SQL or a network client.

Original evidence remains `ps_game.exe` `CUser::PacketQuest` **0046be10**,
`CUser::QuestStartRequest` **0047f220**, `CUser::QuestUpdateKillMOB`
**00480140**, `CUser::QuestEndRequest` **0047f2a0**, `CWorld::LeaveUser`
**0040ca20**, and `ps_dbAgent.exe` `CUser::PacketUserSetStatus` **00405b00**.
The installation helper concerns native composition and makes no new protocol
claim.

Coordinator integration: after real preparation opens the actual save resource,
call `vertical_world_quests_restore` using its replica mutex, then populate
`VerticalWorldQuestInstall` with the returned Actor/runtime and retained real
catalog, pool, zone, NPC, clock, log, action and experience owners; invoke
`vertical_world_install_replica_quests` before preparation completes. Route
entry/packets/save, mob credit, timers and leave to the installed owner. Keep
the coordinator's newer realm-bound registry, admission, social, guild and IO
fences rather than replacing current shared headers with this branch's older
copies. This helper is for a cohosted replica; remote DB-agent operation still
needs its actual ordered transport. Real runtime wiring and active-zone timer
traversal remain unfinished.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-world-quest-install-build.log`); `python3 tools/dev.py test
--timeout 30 -R '^(vertical-quests-unit|quest-requests-actor-unit|vertical-session-registry-unit|vertical-world-unit|world-protocol-unit|vertical-social-replica-unit)$'`
passed **6/6** (log `work/overnight-07-world-quest-install-tests.log`).
`git diff --check` passed. Isolated SQL integration remains unavailable because
`.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL lifecycle
action was taken. SQL execution and original-client play remain unverified.

## Retain the cohosted quest/save mutex

`VerticalWorldQuestReplicaPorts` now owns a `shared_ptr` lease to the actual
recursive mutex used for character replica saves instead of borrowing a bare
reference. `VerticalWorldQuestInstall::replica_mutex` takes that same shared
mutex; missing leases and a lease directly aliasing the enclosing world session
are rejected. The prepared-session fixture drops its independent mutex owner
before quest accept, mob progress and rewards, checks that the installed quest
ports retain the mutex through those operations and a closed-session quest
lease, and checks it is released when the quest/session lease is released.
The fixture also checks rejection of a direct session-owned mutex alias. It
uses a synthetic snapshot and test service observers; it does not open SQL.

Coordinator integration: supply the *same independently owned account mutex*
that is passed to the real social/save preparation and `CharacterSaveStore`;
keep the rest of the real clock, log, quest lock, NPC, experience and transport
owners alive through session teardown. A `VerticalWorldSessionSqlSave::mutex()`
alias also retains the SQL save resource itself: do not substitute that alias
without explicitly coordinating its later worker disposal and drain, or claim
the resource has been destroyed while quest leases retain it. The direct alias
check cannot detect arbitrary indirect ownership cycles, so the supplied mutex
owner must not retain the world session. Preserve the coordinator's newer
realm-bound reservation and social/guild/IO fences.

Original evidence for the cohosted packets and save contract is
`ps_dbAgent.exe` `CUser::PacketUserSetStatus` **00405b00** and
`ps_game.exe` `CUser::SendDBAgentCharGetInfo` quest phase
**0046ccd4..0046ce4a**. The mutex lease is native lifetime integration, not a
claim about original in-process synchronization.

Validation: `python3 tools/dev.py build` passed (final log
`work/overnight-07-world-quest-mutex-lease-build-final.log`); `python3 tools/dev.py
test --timeout 30 -R '^(vertical-quests-unit|quest-requests-actor-unit|vertical-session-registry-unit|vertical-world-unit|world-protocol-unit|vertical-social-replica-unit)$'`
passed **6/6** (final log
`work/overnight-07-world-quest-mutex-lease-tests-final.log`). `git diff --check`
passed. Isolated SQL integration remains unavailable because `.local/runtime.env`
and `.local/sqlserver.env` are absent; no SQL lifecycle action was taken. Live
save-resource teardown, production quest wiring and original-client play remain
unverified. Next integrate this shared mutex with the actual preparation and
save owner, then exercise its full cancellation and disconnect drain.

## Admitted-world quest and NPC packet dispatch

`VerticalWorldQuestPacketDispatch` supplies the coordinator packet router's
`OwnedDispatch` signature for families `09` and `0e`. Before invoking the
installed quest/NPC handler it requires location 4, the registered exact Actor,
its actual zone user membership and the same `ZoneNpcs` owner bound to the quest
session. It retains the quest session during dispatch, returns false for
unrelated families, propagates malformed requests, and treats a real handled or
connection-closed result as consumed. It does not implement or fake any action,
shop, close, DB or logging callback.

The prepared-session fixture now uses an actual `RuntimeZone` user list and
NPC owner to run abandonment, NPC-targeted acceptance, `0e04` NPC selection,
real mob quest credit, NPC completion/rewards, and movement interaction cleanup
through this hook and existing owners. It checks non-admitted and wrong-NPC-zone
rejection, unrelated packet fallback, replica state and NPC use counts. Its
admission location/zone are set synthetically and it calls the hook directly;
it does not exercise the coordinator's queued router, a socket, SQL, or a
real original client. External services and snapshot remain test fixtures.

Original evidence: `ps_game.exe` `CUser::UpdatePackets` **00466cc0**,
`CUser::PacketProcessing` **00466da0**, `CUser::PacketQuest` **0046be10**,
`CUser::PacketNpc` **00467510**, and movement interaction branch
**0046a55f..0046a64d**. Coordinator integration: bind a retained
`VerticalWorldQuestPacketDispatch` as the real
`VerticalWorldSessionPacketRouter::OwnedDispatch` on the world strand, compose
it with any other owned-family dispatchers, and keep it alive through router
stop/drain. The router must propagate failures to its real disconnect policy.
The quest/NPC session remains bound to its original `ZoneNpcs`; on zone transfer
its ownership must be reconciled with the destination before dispatch. The
`0205` pickup family still needs its real inventory world and actions wired.
Keep the coordinator's newer realm-bound registry, admission, social, guild,
reservation and IO fences.

Validation: `python3 tools/dev.py build` passed (final log
`work/overnight-07-world-quest-dispatch-build-final.log`); `python3 tools/dev.py
test --timeout 30 -R '^(vertical-quests-unit|quest-requests-actor-unit|vertical-session-registry-unit|vertical-world-unit|world-protocol-unit|vertical-social-replica-unit)$'`
passed **6/6** (final log
`work/overnight-07-world-quest-dispatch-tests-final.log`). `git diff --check`
passed. Isolated SQL integration remains unavailable because `.local/runtime.env`
and `.local/sqlserver.env` are absent; no SQL lifecycle action was taken. Live
router wiring, SQL execution and original-client play remain unverified.

## Quest lifecycle through the real world packet FIFO

The prepared-session fixture now binds `VerticalWorldQuestPacketDispatch` as the
actual `VerticalWorldSessionPacketRouter::OwnedDispatch`, attaches the selected
session to its real bounded FIFO, queues quest/NPC packets and invokes its
world-strand update. It verifies queued `0908` abandonment, a two-packet `0902`
acceptance / `0e04` NPC selection pass, actual mob quest credit, queued `0903`
completion and rewards in the selected character replica. It stops the router
in the disconnect callback before registry release and retains the existing IO,
quest-session and mutex lifetime checks. The required non-quest gameplay
factory throws if reached; it does not report fake success.

This branch copies the coordinator's current
`vertical_world_session_packets.hpp`, `vertical_world_session_gameplay.hpp`,
and `vertical_world_session_packet_router.hpp` to exercise those real owners;
there are no production changes to them in this milestone. Reconcile rather than
replace the coordinator's newer shared sources. The snapshot, admission
location/zone, NPC and external clock, action, logging and mob reward/drop
services are synthetic fixtures. The test does not run a network socket,
production router binding, actual social/guild teardown or SQL. The NPC-zone
binding must still be reconciled on real zone transfer; the `0205` pickup path
still requires its actual inventory world and actions.

Original evidence: `ps_game.exe` `CUser::UpdatePackets` **00466cc0**, FIFO
push **004e0740** / pop **004e07a0**, `CUser::PacketProcessing` **00466da0**,
`CUser::PacketQuest` **0046be10** and `CUser::PacketNpc` **00467510**.
Coordinator integration: supply a retained `VerticalWorldQuestPacketDispatch`
as the router's `OwnedDispatch`, compose other real owned-family dispatchers,
attach and stop with the actual session lifecycle, and preserve the newer
realm-bound registry, reservation, admission, social, guild and IO fences.

Validation: `python3 tools/dev.py build` passed (final log
`work/overnight-07-world-quest-fifo-build-final.log`); `python3 tools/dev.py test
--timeout 30 -R '^(vertical-quests-unit|quest-requests-actor-unit|vertical-session-registry-unit|vertical-world-unit|world-protocol-unit|vertical-social-replica-unit)$'`
passed **6/6** (log `work/overnight-07-world-quest-fifo-tests-final.log`).
`git diff --check` passed. Isolated SQL integration remains unavailable because
`.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL lifecycle
action was taken. Production wiring, SQL execution and original-client play
remain unverified.

## Admitted-world quest-aware item pickup dispatch

`VerticalWorldQuestPickupDispatch` is an `OwnedDispatch`-compatible `0205`
handler. It requires the exact registered, admitted Actor and actual zone item
owner, preserves the original attack-state gate before owner lookup, and asks
the world for a retained bundle containing the real item links, pickup world,
item pool and inventory actions. Missing owners or a changed Actor/zone/attack
state during lookup fail instead of reporting success. The existing
`vertical_quests_session_pickup_request` performs the actual pickup and quest
eligibility against the retained quest session; unrelated packets return false.

The prepared-session fixture composes quest/NPC and pickup dispatch through the
actual world packet FIFO. It verifies non-admitted rejection, incomplete and
mutating owner rejection, no owner lookup while the attack gate is closed,
missing-item rejection, and successful pickup of a synthetic pooled quest item
from the actual `ZoneItems` into the Actor inventory and selected character
replica with the same UID and real loot-log path. The existing quest counter is
unchanged by pickup because the original call site for
`CUser::QuestUpdatePickItem` **00480390** remains unproven. The fixture creates
the ground item directly; it does not establish an original-client mob-drop
path. Its admission, transport and service callbacks are synthetic observers.
The fixture also now retains `MapGeometry` for the full `ZoneItems` lifetime.

Original evidence: `ps_game.exe` `CUser::PacketMaininterface` **00468420**,
pickup branch **004686c9..004686f2**, `CZone::GetPickItem` **004174a0**,
`CUser::QuestFind` **0047e4b0**, and `CUser::ItemGet` **0045d2e0**. Coordinator
integration: compose this handler with `VerticalWorldQuestPacketDispatch` in
the real packet router's `OwnedDispatch`; provide a lookup retaining the exact
zone item services, shared item pool, item-quest links and actual exchange/shop
and pickup actions across the synchronous call. The lookup must not mutate the
Actor, packet, zone membership, item/zone owners or attack state. Propagate
lookup and handler failures to the real disconnect/error policy; preserve
realm-bound reservation, social/guild, save-mutex and IO fences. Real runtime
lookup wiring, full admission, SQL and original-client play remain unverified.

Validation: `python3 tools/dev.py build` passed (final log
`work/overnight-07-world-quest-pickup-dispatch-build-complete.log`); `python3
tools/dev.py test --timeout 30 -R '^(vertical-quests-unit|vertical-inventory-unit|item-pick-unit|vertical-session-registry-unit|vertical-world-unit|world-protocol-unit)$'`
passed **6/6** (final log
`work/overnight-07-world-quest-pickup-dispatch-tests-complete.log`). Earlier
runs exposed two fixture expectations: the original missing-item response is
`inventory_failure_packet(3)`, and the loot log requires a nonzero drop type;
logs are retained under `work/overnight-07-world-quest-pickup-*`. `git diff
--check` passed. Isolated SQL integration remains unavailable because
`.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL lifecycle
action was taken.

## Retain quest runtime across NPC-zone transfer

The quest and NPC packet owners can now rebind their NPC lookup together after
real old-zone interaction cleanup and destination admission, without replacing
the Actor's quest runtime or replica. `vertical_world_quests_rebind_zone`
requires the registered exact Actor to be admitted in the supplied destination
and retains its installed quest session. Rebinding rejects a stale source,
active NPC interaction/lease, or an in-flight quest, packet, movement or leave
callback. Native call-depth guards preserve nested existing quest calls while
preventing reentrant zone replacement during their callbacks. The old and new
zone, NPC pool and actual session services must remain retained through the
transition; this helper does not perform zone leave, admission, gameplay-owner
invalidation or packet queue management.

The prepared-session fixture checks rejection before admission, with an active
NPC, with a stale source, and from reentrant packet and timer callbacks. It
then detaches the synthetic source Actor, attaches it to an actual destination
`ZoneUsers`, rebinds the same quest runtime, verifies old-zone NPC selection no
longer resolves, selects a destination NPC through the real FIFO and cleans up
its interaction/use count. Zone admission and external services are synthetic;
no real teleport, socket or SQL execution is claimed.

Original evidence: `ps_game.exe` `CUser::LeaveZone` **0044aee0**, NPC interaction
cleanup **0044aee0..0044aef4**, `CUser::PacketNpc` **00467510**, and
`CUser::PacketPC` movement interaction branch **0046a55f..0046a64d**. The native
rebind guard is an ownership integration boundary, not a claim that the original
executable had this helper. Coordinator integration: after the actual old-zone
leave has cleared the NPC interaction and the destination Actor is admitted,
invoke `vertical_world_quests_rebind_zone` on the world strand before enabling
new gameplay packets; retain both zone owners and use the same NPC pool. Keep
the existing router gameplay invalidation, admission, save, social/guild,
realm-reservation and IO fences. Real transfer orchestration remains unwired.

Validation: `python3 tools/dev.py build` passed (final log
`work/overnight-07-world-quest-zone-transfer-build-final.log`); `python3
tools/dev.py test --timeout 30 -R '^(vertical-quests-unit|quest-requests-actor-unit|vertical-session-registry-unit|vertical-world-unit|world-protocol-unit|vertical-social-replica-unit)$'`
passed **6/6** (final log
`work/overnight-07-world-quest-zone-transfer-tests-final.log`). `git diff
--check` passed. Isolated SQL integration remains unavailable because
`.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL lifecycle
action was taken. Real zone transfer, SQL execution and original-client play
remain unverified.

## Detect silent quest and reward replica failures

`VerticalWorldQuestReplicaPorts` now verifies the cohosted replica result for
quest insertion `0a01`, quest removal `0a02`, quest state update `0a03`, quest
counter update `0a05`, and new inventory item `0701` while holding the same
save mutex. It rejects missing quest targets and occupied item destinations,
checks the expected quest row/counter or item UID after applying, and throws on
silent allocation/no-op failures rather than presenting them as successful
service delivery. The underlying `QuestReplica` and `InventoryReplica` retain
their recovered original behavior; this validation is the native cohosted
integration boundary. It does not roll back already applied game or DB-agent
state. In particular, original quest insertion may consume a finished row even
when allocation fails; the actual world must stop or recover the session on the
propagated failure rather than continue as though the mutation succeeded.

Focused fault tests use real `CharacterReplica`, `QuestReplicaPool` and
`InventoryReplicaPool` owners with allocation-failure fixtures: missing quest
and item allocations are detected, missing progress/removal targets fail, and
an occupied replica item slot is left unchanged. Existing prepared-world quest
accept/progress/completion and reward tests continue through this adapter.
Original evidence: `ps_dbAgent.exe` `CUser::PacketUserQuest` **00406590** and
`CUser::AddDeleteQuest` **00405380**, plus
`CUser::PacketUserSetStatus` **00405b00** and the existing DB-agent inventory
packet owner. The original allocation behavior remains covered by the separate
replica unit tests.

Coordinator interface: use these updated replica ports with the exact selected
Actor, actual character replica and independently retained shared save mutex;
propagate exceptions to the real session failure/drain policy. The coordinator
currently has its own guarded inventory-pickup owner, which should retain its
existing allocation/identity checks. Bulk quest-save `0a04`, inventory count
and removal packets still need equivalent missing-row postcondition review;
this milestone does not claim those paths are protected. Keep the newer
realm-bound registry and social/guild/reservation/IO fences.

Validation: `python3 tools/dev.py build` passed (final log
`work/overnight-07-world-quest-replica-allocation-build-final.log`); `python3
tools/dev.py test --timeout 30 -R '^(vertical-quests-unit|quest-requests-actor-unit|vertical-session-registry-unit|inventory-replica-unit|quest-replica-unit|vertical-world-unit)$'`
passed **6/6** (final log
`work/overnight-07-world-quest-replica-allocation-tests-final.log`). `git diff
--check` passed. Isolated SQL integration remains unavailable because
`.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL lifecycle
action was taken. Production failure recovery, SQL execution and original-client
play remain unverified.

## Validate bulk quest and inventory replica mutations

The retained quest replica ports now prevalidate `0a04` bulk saves against the
active replica rows in their original ordered, consuming-match sequence. This
preserves duplicate IDs while rejecting an incomplete batch before it can
partially update the cache. After delivery the adapter checks each matched
row's ID, delay and counters. Inventory `0702` removal and `0707` count changes
now require a cached item and verify its UID and resulting count or removal.
These checks operate under the existing shared save mutex and leave the replica
owners' recovered behavior unchanged. Original evidence: `ps_dbAgent.exe`
`CUser::PacketUserQuest` **00406590** and `CUser::PacketUserSetStatus`
**00405b00** (inventory dispatch **00405f4e..00405fea**).

Coordinator integration: port the changes in
`include/eternity/game/vertical_quests_world_session.hpp` to its retained
`VerticalWorldQuestReplicaPorts`, together with the earlier allocation checks
from `d32da3b`. Keep the coordinator's newer session registry and admission,
IO lease, social, guild and reservation fences. Route exceptions through the
real session failure/drain policy; these checks do not roll back mutations
already made by the world actor. Next work is to integrate that failure policy
and validate the real DB-agent/SQL and original-client paths.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-world-quest-replica-bulk-build.log`); `python3 tools/dev.py
test --timeout 30 -R '^(vertical-quests-unit|quest-requests-actor-unit|vertical-session-registry-unit|inventory-replica-unit|quest-replica-unit|vertical-world-unit)$'`
passed **6/6** (log `work/overnight-07-world-quest-replica-bulk-tests.log`).
The focused test exercises ordered duplicate IDs, incomplete bulk packets,
missing inventory targets, partial removal and full removal using real replica
owners. `git diff --check` passed. Isolated SQL integration was unavailable:
`.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL lifecycle
action was taken. Original-client end-to-end play remains unverified.

## Fence retained sessions after quest timer failure

Primary quest timer dispatch now catches per-actor callback exceptions and
reports them to `PrimaryQuestPhaseServices::failure`. The retained-world
implementation resolves the exact Actor's session and invokes the real
failure/disconnect policy. A session latches this failure before the callback,
rejects subsequent packet enqueue, clears its queued packets at the next router
update, and does not run further primary quest callbacks while awaiting close.
Repeated reports do not request a second disconnect. The retained owner and
existing admission, IO lease and teardown fences remain in force. Original
phase evidence: `ps_game.exe` `CWorldThread::Update` **0040771d..00407760** and
`CZone::UpdateQuest` **00422320**; exception handling is native containment.

Coordinator integration: retain its newer `WorldPrimaryQuestPhase::dispatch`
and `update_active` traversal, add the `failure(Actor&, std::exception_ptr)`
service contract and wrap both owner lookup and timer call with
`vertical_primary_quest_update_actor`, passing a callback for `world.running()`.
Port the exact retained-session failure resolution, session failure latch and
router checks from this commit after `b021673`. The real failure service and
channel disconnect remain mandatory; this does not undo a partial quest
mutation or determine final-save policy. Mob-credit and other callbacks outside
the primary phase still require integration with their actual caller's failure
policy.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-timer-failure-build.log`); `python3 tools/dev.py test
--timeout 30 -R '^(vertical-quests-unit|vertical-session-registry-unit|vertical-world-unit|world-protocol-unit|quest-requests-actor-unit)$'`
passed **5/5** (log `work/overnight-07-quest-timer-failure-tests.log`). The
retained-world fixture injects a timer callback exception with an NPC request
already queued and checks failure reporting, one disconnect, immediate enqueue
rejection and discarded pending work. `git diff --check` passed. Isolated SQL
integration remains unavailable because `.local/runtime.env` and
`.local/sqlserver.env` are absent; no SQL lifecycle action was taken.
Original-client end-to-end play remains unverified.

## Resolve mob quest credit against the retained world actor

`VerticalWorldQuestMobActors` now implements the existing mob-credit Actor
resolver using the real character registry and admitted zone user owner. It
requires a caller-supplied `Match(VerticalWorldSession&, MobDeathUser&)` from
the actual mob-death owner to verify the exact live death-user object; the
record's character ID alone cannot establish identity. The resolver checks
session failure/closing state, location, Actor/account identity, exact zone
membership and registry ownership before and after that callback, and returns
an alias retaining the session and Actor. The existing
`VerticalQuestSessionMobCredit` then uses the exact quest session and real
`MobQuestLinks`. Original evidence: `ps_game.exe` `CMob::Death` **004910f0**
dispatches solo and party quest credit; the identity callback is a native
lifetime fence around the reconstructed death-user projection.

Coordinator integration: port this new resolver from
`vertical_quests_world_session.hpp` after the session failure latch in
`9e1ca22`. Construct it with the coordinator's actual world registry and a
callback that verifies the death-user object's identity against the retained
mob/party death owner. Invoke the existing `VerticalQuestSessionMobCredit` from
the actual `MobDeathWorld::quest_kill` callback. The callback must use the real
death/party owner and must not return success solely because IDs match. The
current mob-death projection does not itself expose such an identity owner;
production wiring and its exception policy remain to be integrated. Keep the
coordinator's realm-bound reservation, IO, social and guild fences.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-mob-identity-build.log`); `python3 tools/dev.py test
--timeout 30 -R '^(vertical-quests-unit|vertical-session-registry-unit|vertical-world-unit|mob-death-unit|quest-requests-actor-unit)$'`
passed **5/5** (log `work/overnight-07-quest-mob-identity-tests.log`). The
retained-world mob-death fixture exercises real quest credit, rejects a stale
same-ID death record, rejects mutation during identity lookup, and checks the
exact selected Actor is retained. `git diff --check` passed. Isolated SQL tests
remain unavailable because `.local/runtime.env` and `.local/sqlserver.env` are
absent; no SQL lifecycle action was taken. Original-client end-to-end play
remains unverified.

## Route real mob death through retained quest credit

`VerticalQuestMobDeath` now accepts the existing
`VerticalQuestSessionMobCredit` owner as an alternative to its legacy quest
resolver. Its `quest_kill` callback uses that retained Actor/session path;
all other death, reward, party-lock and cleanup callbacks still forward to
the actual `MobDeathWorld`. The original constructor and behavior remain
available. Original evidence: `ps_game.exe` `CMob::Death` **004910f0** invokes
`CUser::QuestUpdateKillMOB` **00480140** for eligible solo and party members.

Coordinator integration: port the constructor and dispatch changes in
`vertical_quests.hpp` and `vertical_quests.cpp`, then construct
`VerticalQuestMobDeath(real_death_world, retained_session_credit)` for the
actual `mob_death` call. Retain both owners through the synchronous death
callback. The identity provider for `VerticalWorldQuestMobActors` still must
verify the real death-user object as described in the preceding section; no
production death-user owner has been added here. Exceptions propagate to the
real death caller, which must implement the appropriate failure policy;
partial rewards are not rolled back.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-mob-bridge-build.log`); `python3 tools/dev.py test
--timeout 30 -R '^(vertical-quests-unit|vertical-session-registry-unit|vertical-world-unit|mob-death-unit|quest-requests-actor-unit)$'`
passed **5/5** (log `work/overnight-07-quest-mob-bridge-tests.log`). Solo,
party and retained-world fixtures invoke `mob_death` through the new wrapper;
the party fixture checks failure unlocks and each member's own quest owner.
`git diff --check` passed. Isolated SQL integration remains unavailable because
`.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL lifecycle
action was taken. Original-client end-to-end play remains unverified.

## Resolve quest loot against retained world actors

`VerticalWorldQuestLootActors` implements the existing loot Actor resolver
using the registered selected session and exact Actor in its admitted zone.
It requires a caller-supplied `Match(VerticalWorldSession&, MobLootUser&)`
from the real loot owner to identify the live loot-user projection, and
revalidates the user ID, session, Actor, account, zone and registry after the
callback. Closing or failed sessions are refused. The returned Actor alias
retains the real session while `VerticalQuestSessionLoot` checks its actual
quest runtime. Original evidence: `ps_game.exe` `CMob::ItemDrop` **004a5ea0**
checks linked quest eligibility; the identity fence is native integration.

Coordinator integration: port `VerticalWorldQuestLootActors` from
`vertical_quests_world_session.hpp` after the session failure latch in
`9e1ca22`. Construct it with the actual registry and the loot owner's exact
record-identity callback, then bind it and the retained quest-session resolver
into the existing `VerticalQuestSessionLoot` and `VerticalQuestMobLootWorld`.
The loot owner must prove the specific projection belongs to the selected
session, not merely compare IDs. Production loot owner construction and its
failure policy are still required; no success callback is supplied here.
Preserve the coordinator's realm-bound reservation and session lifecycle fences.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-loot-identity-build.log`); `python3 tools/dev.py test
--timeout 30 -R '^(vertical-quests-unit|vertical-session-registry-unit|vertical-world-unit|mob-drops-unit|quest-requests-actor-unit)$'`
passed **5/5** (log `work/overnight-07-quest-loot-identity-tests.log`). The
retained-world fixture queries active and absent quests, rejects a stale same-ID
loot projection and detects identity mutation during lookup. `git diff --check`
passed. Isolated SQL tests remain unavailable because `.local/runtime.env` and
`.local/sqlserver.env` are absent; no SQL lifecycle action was taken.
Original-client end-to-end play remains unverified.

## Guard quest item replica identity

The cohosted quest ports now check that a `0702` removal or `0707` count
update targets the same retained Actor item UID, type and type ID as the
replica row before applying it. For count changes and partial removals they
also check the Actor's already-updated count against the packet and prior
replica count. Full removal deliberately permits the original retained old
count until release. This catches a stale replica slot before it can mutate a
different item; it does not roll back an Actor mutation that already happened.
Original evidence: `ps_game.exe` `CUser::QuestFindItem` **0047f790** and
**0047f8e0**, and `ps_dbAgent.exe` `CUser::PacketUserSetStatus` **00405b00**
(inventory dispatch **00405f4e..00405fea**).

Coordinator integration: its current `VerticalWorldQuestReplicaPorts` already
contains the previous missing-row and postcondition checks. Port only the new
retained Actor UID/type/count checks in its existing remove/count prevalidation
block. Continue to use the exact selected Actor and shared save mutex; route
exceptions through the real failure and disconnect policy. The slot projection
must retain its actual GroundItem until the database callback returns.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-item-identity-build.log`); `python3 tools/dev.py test
--timeout 30 -R '^(vertical-quests-unit|inventory-replica-unit|quest-requests-actor-unit|vertical-session-registry-unit|vertical-world-unit)$'`
passed **5/5** (log `work/overnight-07-quest-item-identity-tests.log`). The
fixture uses real retained Actor and replica items, rejects mismatched UID and
count without touching the replica, and exercises count update, partial and
full removal. `git diff --check` passed. Isolated SQL tests remain unavailable
because `.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL
lifecycle action was taken. Original-client end-to-end play remains unverified.

## Validate quest item insertion against the Actor

Before applying quest item insertion `0701`, the cohosted quest ports now
require the exact selected Actor to retain the item in the target slot with
the packet's UID, item ID, type, type ID and count. The existing occupied-slot
and replica-allocation checks remain in place. Invalid slot envelopes still go
to the original replica validator. This is a native consistency fence; Actor
mutation is already performed before the callback and is not rolled back.
Original evidence: `ps_game.exe` `CUser::QuestAddItem` **0047e6f0** and
`ps_dbAgent.exe` `CUser::PacketUserSetStatus` **00405b00** (inventory dispatch
**00405f4e..00405fea**).

Coordinator integration: its existing `VerticalWorldQuestReplicaPorts`
contains the earlier occupied-slot and allocation guards. Port the additional
Actor item checks in the `add_item` prevalidation block, using the exact
selected Actor and its real save mutex. Ensure the real failure/disconnect
policy handles exceptions, and retain the GroundItem until the DB callback
returns. This composes with the removal/count checks in `dbe7b2b`.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-item-insert-build.log`); `python3 tools/dev.py test
--timeout 30 -R '^(vertical-quests-unit|inventory-replica-unit|quest-requests-actor-unit|vertical-session-registry-unit|vertical-world-unit)$'`
passed **5/5** (log `work/overnight-07-quest-item-insert-tests.log`). Focused
fixtures reject missing and mismatched Actor items before replica allocation,
then exercise actual allocation failure and the retained-world reward flow.
`git diff --check` passed. Isolated SQL tests remain unavailable because
`.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL lifecycle
action was taken. Original-client end-to-end play remains unverified.

## Quest consumption integration fixture (test pending)

Added a focused fixture that connects the actual `QuestRequestsActorInventory`
consumer to `VerticalWorldQuestReplicaPorts`, a retained Actor item and the
real `CharacterReplica`. It exercises a one-item partial consumption followed
by a two-item full-stack quest requirement. This specifically checks the
original full-removal behavior where the Actor item retains its old count
until release. The fixture has compiled but its assertions have not yet run.
Evidence: `ps_game.exe` `CUser::QuestFindItem` **0047f790** and **0047f8e0**;
`ps_dbAgent.exe` `CUser::PacketUserSetStatus` **00405b00**.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-consumption-replica-build.log`). The command `python3
tools/dev.py test --timeout 30 -R '^(vertical-quests-unit|inventory-replica-unit|quest-requests-actor-unit|vertical-session-registry-unit|vertical-world-unit)$'`
waited several minutes for the shared build lock and was interrupted without
running tests (log `work/overnight-07-quest-consumption-replica-tests.log`).
Run that exact command once the shared lock is available; do not treat this
fixture as validated until it passes. No coordinator code change is required
for this test beyond the prior item guards in `dbe7b2b` and `cb94d65`.
Isolated SQL configuration was previously absent and no SQL lifecycle action
was taken. Original-client end-to-end play remains unverified.

## Quest consumption fixture validation

The previously pending fixture now passes after isolating its client scene from
unrelated quest fixtures. Its first retry ran **4/5**: `vertical-quests-unit`
failed the later `database/client ordering differs` assertion because the new
fixture had added its expected client packets to the shared scene (log
`work/overnight-07-quest-consumption-replica-tests-retry.log`). No production
code was changed to address this fixture isolation error.

The corrected `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-consumption-replica-build-final.log`), and `python3
tools/dev.py test --timeout 30 -R '^(vertical-quests-unit|inventory-replica-unit|quest-requests-actor-unit|vertical-session-registry-unit|vertical-world-unit)$'`
passed **5/5** (log `work/overnight-07-quest-consumption-replica-tests-final.log`).
The actual quest consumer, retained Actor and replica remain synchronized for
partial and full-stack item consumption. Evidence remains `ps_game.exe`
`CUser::QuestFindItem` **0047f790**, **0047f8e0** and `ps_dbAgent.exe`
`CUser::PacketUserSetStatus` **00405b00**. `git diff --check` passed. The prior
section's test-pending status is superseded. Coordinator integration still
requires the guards in `dbe7b2b` and `cb94d65`; isolated SQL and original-client
end-to-end play remain unverified.

## Validate quest reward money replica updates

The cohosted quest ports now check that a `0603` money packet matches the
selected Actor's already-updated balance before delivery and verify the
character replica balance afterward. A mismatch throws before mutating the
replica; prior Actor/reward mutations are not rolled back. Original evidence:
`ps_game.exe` `CUser::QuestSuccessResult` **0047ee40** and `ps_dbAgent.exe`
`CUser::PacketUserSetStatus` **00405b00** (`0603` balance update).

Coordinator integration: port the `set_money` precondition and postcondition in
`VerticalWorldQuestReplicaPorts::send_database`, retaining the exact selected
Actor and the same save mutex. The real session failure/disconnect policy must
handle exceptions. Existing reward services must still produce and deliver the
real money packet; this check does not synthesize a reward or persistence result.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-money-replica-build.log`); `python3 tools/dev.py test
--timeout 30 -R '^(vertical-quests-unit|inventory-replica-unit|quest-requests-actor-unit|vertical-session-registry-unit|vertical-world-unit)$'`
passed **5/5** (log `work/overnight-07-quest-money-replica-tests.log`). The
fixture rejects a mismatched balance before replica mutation and verifies a
matching update; the retained-world reward flow continues to pass. `git diff
--check` passed. Isolated SQL tests remain unavailable because
`.local/runtime.env` and `.local/sqlserver.env` are absent; no SQL lifecycle
action was taken. Original-client end-to-end play remains unverified.

## Retain the exact quest runtime for disconnect save

`VerticalQuests::runtime_for(Actor&)` validates the exact Actor and existing
account/pool binding before returning its runtime. The new
`vertical_world_quests_runtime_for_save(session, registry)` validates the
registered session and Actor and returns a `shared_ptr<QuestRuntime>` alias
that pins the actual quest session, runtime and pool. It remains available
while the session is closing, before registry release. Original save evidence:
`ps_game.exe` `CUser::SendDBAgentCharGetInfo` **0046c980** and quest-save phase;
this lease is native lifetime and identity enforcement.

Coordinator integration: port `runtime_for` in `vertical_quests.hpp/.cpp` and
the lease helper in `vertical_quests_world_session.hpp`. In
`VerticalWorldSessionZoneServices::prepare_save`, obtain and retain the lease
for the entire synchronous `vertical_save_character` call and pass `*lease` as
its `QuestRuntime&`; do not return a reference from a temporary lease. The
save service's quest lock and database callback must be the real matching quest
and cohosted replica owners, and `0302` must only follow successful preparation.
Preserve its admission-pass, IO lease, social, guild and reservation fences.
The external real save/teardown composition and SQL execution remain required.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-save-runtime-lease-build.log`); `python3 tools/dev.py
test --timeout 30 -R '^(vertical-quests-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit|quest-runtime-save-unit)$'`
passed **4/4 matching tests** (log
`work/overnight-07-quest-save-runtime-lease-tests.log`; no test named
`quest-runtime-save-unit` exists in this tree). The retained-world fixture
rejects a same-ID foreign Actor, resolves the exact runtime during preparation
and closing, and verifies the lease pins the session and mutex until released.
`git diff --check` passed. Isolated SQL tests remain unavailable because
`.local/runtime.env` and `.local/sqlserver.env` were absent in the prior check;
no SQL lifecycle action was taken. Original-client end-to-end play remains
unverified.

## Compose session character save with retained quest runtime

`vertical_world_quests_save_character` now obtains the identity-checked quest
runtime lease and holds it throughout the existing `vertical_save_character`
pipeline, using the selected session's actual Actor, zone-entry state and quick
slots. It verifies the supplied skill runtime belongs to that Actor and checks
the registered session, Actor and character again after synchronous delivery.
The caller supplies the actual ordered save service and locks; the helper does
not issue the DB exit request or replace any required service. Original
ordering evidence: `ps_game.exe` `CUser::SendDBAgentCharGetInfo` **0046c980**,
quest phase **0046ccd4..0046ce4a**.

Coordinator integration: after `5c08ea3c`, port this helper and the
`vertical_save_core_skills.hpp` include from `vertical_quests_world_session.hpp`.
In `VerticalWorldSessionZoneServices::prepare_save`, replace the manually
assembled `vertical_save_character` call with
`vertical_world_quests_save_character(*owner_, registry_, ports_.skills(*owner_),
ports_.save(*owner_), scratch)`. The actual save service must route quest
`0a04` to the matching cohosted replica and hold the real quest lock; retain
its existing non-quest delivery and teardown policies. Request `0302` only
after successful save preparation. Do not remove the coordinator's admission,
IO, social, guild or reservation fences. This branch's helper does not itself
prove those production providers are wired.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-session-save-build.log`); `python3 tools/dev.py test
--timeout 30 -R '^(vertical-quests-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit|vertical-disconnect-tcp-unit)$'`
passed **5/5** (log `work/overnight-07-quest-session-save-tests.log`). The
retained-world fixture runs the helper with real Actor, quest and character
replica owners, routes `0a04` through the cohosted quest adapter, and verifies
the saved quest counter. `git diff --check` passed. Isolated SQL configuration
was absent at the prior check; no SQL lifecycle action was taken. Actual
coordinator save composition, SQL execution and original-client end-to-end play
remain unverified.

## Route session save quest batches through the quest owner

The retained session save helper now wraps the supplied real save services:
non-quest phases continue to use the caller's transport and effect lock, while
quest locking and `0a04` batch delivery use the installed `VerticalQuests`
owner and its actual cohosted replica port. Public save-only methods validate
the exact Actor and runtime binding; lock acquisition revalidates and releases
the real lock if binding changes. Delivery failures propagate and the helper
does not send the DB exit request. Original evidence: `ps_game.exe`
`CUser::SendDBAgentCharGetInfo` **0046c980**, quest phase
**0046ccd4..0046ce4a**, and `ps_dbAgent.exe` `CUser::PacketUserQuest`
**00406590**.

Coordinator integration: port the save-only methods in
`vertical_quests.hpp/.cpp` and the `QuestSaveServices` routing in
`vertical_quests_world_session.hpp` after `4b7f5144`. In its real session zone
service call `vertical_world_quests_save_character` before `0302`; provide the
real non-quest save services and ensure the installed quest port is the same
selected character replica with the same save mutex. Do not also send `0a04`
through the generic save transport. Existing admission, IO, social, guild,
reservation and teardown fences remain required. SQL execution and the real
coordinator's final wiring are not established by this fixture.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-save-routing-build.log`); `python3 tools/dev.py test
--timeout 30 -R '^(vertical-quests-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit|vertical-disconnect-tcp-unit)$'`
passed **5/5** (log `work/overnight-07-quest-save-routing-tests.log`). The
retained-world fixture changes a live quest counter, verifies the real quest
lock was called, observes the replica update and confirms the generic transport
never received `0a04`. `git diff --check` passed. Isolated SQL configuration
was absent in the prior check; no SQL lifecycle action was taken. Original-client
end-to-end play remains unverified.

## Validate quest save failure ordering

The retained-world fixture now removes an active quest's replica row and runs
`vertical_world_quests_save_character` against the still-active runtime. The
cohosted `0a04` guard reports the missing row; the save exits without sending
the later `0609` quick-slot phase and the real quest lock is released. The
fixture restores its replica through the actual quest adapter before continuing.
This tests failure propagation, not transactional rollback: earlier save phases
may already have reached the replica. Original evidence: `ps_game.exe`
`CUser::SendDBAgentCharGetInfo` **0046c980**, quest phase
**0046ccd4..0046ce4a**, and `ps_dbAgent.exe` `CUser::PacketUserQuest`
**00406590**.

Coordinator integration remains as described for `dbb5befd`: call the retained
session save helper before issuing `0302`, route failures to the real lifecycle
policy, and retain the coordinator's admission, IO, social, guild and
reservation fences. Production teardown must not treat an exception as a
successful prepared save.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-save-failure-build.log`); `python3 tools/dev.py test
--timeout 30 -R '^(vertical-quests-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit|vertical-disconnect-tcp-unit)$'`
passed **5/5** (log `work/overnight-07-quest-save-failure-tests.log`).
`git diff --check` passed. Isolated SQL configuration was absent in the prior
check; no SQL lifecycle action was taken. Actual coordinator wiring, SQL
execution and original-client end-to-end play remain unverified.

## Revalidate retained session during save callbacks

The retained quest save service now revalidates the exact session, character,
Actor account and registry ownership before and after each synchronous save
transport, clock and position callback. It also revalidates after acquiring the
real effect or quest lock and releases that lock if validation fails. A callback
that changes ownership therefore stops later save phases; already-delivered
packets are not rolled back. The quest session lease remains held throughout.
Original ordering evidence: `ps_game.exe` `CUser::SendDBAgentCharGetInfo`
**0046c980**, quest phase **0046ccd4..0046ce4a**. Reentry validation is native
lifetime protection, not a claim about original exception behavior.

Coordinator integration: port the updated local `QuestSaveServices` wrapper in
`vertical_world_quests_save_character` after `dbb5befd`; call that helper from
the real zone save path before `0302` and propagate exceptions through the
existing failure policy. Continue to supply real non-quest services and retain
all admission, IO, social, guild and reservation fences.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-save-reentry-build.log`); `python3 tools/dev.py test
--timeout 30 -R '^(vertical-quests-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit|vertical-disconnect-tcp-unit)$'`
passed **5/5** (log `work/overnight-07-quest-save-reentry-tests.log`). The
retained-world fixture changes the Actor account during the real position
callback and verifies that only the earlier `0606` packet was delivered before
failure; subsequent normal and failure save cases still pass. `git diff
--check` passed. Isolated SQL configuration was absent in the prior check; no
SQL lifecycle action was taken. Production coordinator wiring, SQL execution and
original-client end-to-end play remain unverified.

## Bind the coordinator's split quest-save phase

The current coordinator zone service now offers a `save_quests(session, tick,
VerticalSaveCoreSkillServices&)` alternative after its real core/skill/effect
phases. `vertical_world_quests_save_phase(session, registry, tick)` supplies
that quest-only operation using the retained session's exact runtime, quest
lock and cohosted quest replica port. It holds the runtime/session lease and
revalidates identity after delivery. It does not repeat other save phases or
send `0302`. Original evidence: `ps_game.exe`
`CUser::SendDBAgentCharGetInfo` **0046c980**, quest phase
**0046ccd4..0046ce4a**, and `ps_dbAgent.exe` `CUser::PacketUserQuest`
**00406590**.

Coordinator integration: port the phase helper in
`vertical_quests_world_session.hpp` after `5c08ea3c` and bind its current
`Ports::save_quests` to a callback that calls
`vertical_world_quests_save_phase(session, registry, tick)`. Keep `quests`
unset, as the coordinator requires exactly one alternative. Before using this
callback, the composition must prove that the installed quest replica port
and `VerticalWorldSessionSaveBinding` address the same selected character
replica and the same `save_mutex()`. The callback uses the installed quest
port directly; it must not also send the batch through the supplied BoundSave
service. Preserve the existing BoundSave handling for other phases and the
admission, IO, social, guild, reservation and teardown fences. This supersedes
the earlier suggestion to replace the coordinator's entire `prepare_save`;
production same-mutex composition and the callback are still unverified.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-save-phase-build.log`); `python3 tools/dev.py test
--timeout 30 -R '^(vertical-quests-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit|vertical-disconnect-tcp-unit)$'`
passed **5/5** (log `work/overnight-07-quest-save-phase-tests.log`). The
retained-world fixture invokes the phase with the captured tick, checks that
the actual quest lock is taken and observes the real replica counter update.
`git diff --check` passed. Isolated SQL configuration was absent in the prior
check; no SQL lifecycle action was taken. Original-client end-to-end play
remains unverified.

## Verify the split quest save binding

The split quest-save helper now requires the coordinator binding's actual
recursive mutex as an argument. It inspects the retained quest session's actual
upstream port and requires a `VerticalWorldQuestReplicaPorts` bound to both the
selected session's exact `CharacterReplica` and that mutex before taking the
quest lock or saving. `VerticalQuestSession::upstream_ports()` exposes the
existing retained upstream for this identity check. No substitute service is
accepted. The helper still retains the session/runtime and uses the captured
tick. Original save evidence: `ps_game.exe` `CUser::SendDBAgentCharGetInfo`
**0046c980**, quest phase **0046ccd4..0046ce4a**, and `ps_dbAgent.exe`
`CUser::PacketUserQuest` **00406590**; this binding validation is native safety.

Coordinator integration: after `3cca8349`, port the upstream-port accessor,
`matches_save_binding` and revised phase helper signature. Its real
`Ports::save_quests` callback must retain the actual
`VerticalWorldSessionSaveBinding`, check `binding->matches(session)`, then call
`vertical_world_quests_save_phase(session, registry, binding->save_mutex(), tick)`.
Use the installed quest replica port for `0a04` only once and preserve BoundSave
for other phases. The callback and binding acquisition remain production wiring;
keep existing admission, IO, social, guild, reservation and teardown fences.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-save-binding-build.log`); `python3 tools/dev.py test
--timeout 30 -R '^(vertical-quests-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit|vertical-disconnect-tcp-unit)$'`
passed **5/5** (log `work/overnight-07-quest-save-binding-tests.log`). The
retained-world fixture proves a foreign mutex is rejected before quest locking
or replica mutation, then saves successfully with the real mutex. `git diff
--check` passed. Isolated SQL configuration was absent in the prior check; no
SQL lifecycle action was taken. Production coordinator wiring, SQL execution
and original-client end-to-end play remain unverified.

## Compose the coordinator's split quest-save callback

`vertical_world_quests_save_phase_callback<Binding>` now produces the exact
signature expected by the coordinator's `Ports::save_quests`. It requires a
real binding lookup, retains the returned binding through the synchronous
operation, checks `binding->matches(session)`, then passes its actual
`save_mutex()` into the existing replica/mutex-verified quest save phase. The
supplied generic save service remains responsible for non-quest phases; the
quest phase uses the installed cohosted port once. No callback returns success
without executing the retained quest owner. Original ordering evidence:
`ps_game.exe` `CUser::SendDBAgentCharGetInfo` **0046c980**, quest phase
**0046ccd4..0046ce4a**; binding checks are native composition safeguards.

Coordinator integration: port the callback factory after `70a17e8e` and bind
`Ports::save_quests` to `vertical_world_quests_save_phase_callback<Binding>(
registry, actual_binding_lookup)`, where `Binding` is its real
`VerticalWorldSessionSaveBinding<Store>` and lookup retains the same live
binding used by the zone service. Leave `Ports::quests` empty. Preserve the
existing BoundSave service for all other phases and the admission, IO, social,
guild, reservation and teardown fences. This branch does not instantiate the
coordinator's real zone-service/binding composition; that remains required.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-save-callback-build.log`); `python3 tools/dev.py test
--timeout 30 -R '^(vertical-quests-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit|vertical-disconnect-tcp-unit)$'`
passed **5/5** (log `work/overnight-07-quest-save-callback-tests.log`). The
retained-world fixture supplies an identity-checking binding projection, rejects
missing lookup and foreign session, and verifies the real quest replica update
with the matching mutex. `git diff --check` passed. Isolated SQL configuration
was absent in the prior check; no SQL lifecycle action was taken. Real
coordinator wiring, SQL execution and original-client end-to-end play remain
unverified.

## Bind the zone service's quest save provider

`vertical_world_bind_quest_save_phase<Binding>(ports, registry)` now installs
the retained quest-save callback directly into the coordinator-compatible zone
ports using their existing binding lookup. It refuses missing binding lookup,
an existing runtime provider or an already installed quest callback. The
callback retains and validates the actual binding and checks the selected
replica and shared mutex before the quest save. Original ordering evidence:
`ps_game.exe` `CUser::SendDBAgentCharGetInfo` **0046c980**, quest phase
**0046ccd4..0046ce4a**. The binder is native composition, not a recovered
original service.

Coordinator integration: port this binder after `a4c3f78f`, then, while
constructing `VerticalWorldSessionZoneServices<Store>::Ports`, configure its
real `binding` lookup and call
`vertical_world_bind_quest_save_phase<typename VerticalWorldSessionZoneServices<Store>::Binding>(zone_ports, registry)`
before constructing the zone service. Leave `quests` unset and do not replace
other save, admission, IO, social, guild, reservation or teardown providers.
The configured binding lookup must continue to return the actual binding used
by the zone service; the installed quest port must share its replica/mutex.
This branch tests an interface-compatible ports projection, not the actual
coordinator zone service or SQL-backed binding.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-zone-save-bind-build.log`); `python3 tools/dev.py test
--timeout 30 -R '^(vertical-quests-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit|vertical-disconnect-tcp-unit)$'`
passed **5/5** (log `work/overnight-07-quest-zone-save-bind-tests.log`). The
retained-world fixture checks missing lookup, duplicate provider and foreign
binding rejection before exercising the matching quest replica and mutex.
`git diff --check` passed. Isolated SQL configuration was absent in the prior
check; no SQL lifecycle action was taken. Actual coordinator wiring, SQL
execution and original-client end-to-end play remain unverified.

## Fence the active zone-save binding identity

The split quest-save callback now requires its supplied save service to expose
`VerticalQuestZoneSaveBindingIdentity` and verifies that its retained binding
object is the exact object returned by the quest binding lookup. This closes a
reentrant/dynamic lookup gap where two different bindings could otherwise be
used for non-quest and quest phases even if both referred to the same session
and mutex. The actual quest port still verifies replica and mutex identity.
Original save-order evidence: `ps_game.exe` `CUser::SendDBAgentCharGetInfo`
**0046c980**, quest phase **0046ccd4..0046ce4a**. The identity interface is a
native composition safeguard.

Coordinator integration: port `VerticalQuestZoneSaveBindingIdentity` and the
callback check from `vertical_quests_world_session.hpp`. In its
`VerticalWorldSessionZoneServices::BoundSave`, also derive from that interface
and implement `const void* quest_save_binding_identity() const noexcept override
{return binding_.get();}`. Include the quest integration header where needed.
Then configure the actual ports with `vertical_world_bind_quest_save_phase`
after the real binding lookup is established. The lookup must return the same
retained binding instance used by `BoundSave`, not merely one with the same
session or mutex. Preserve all admission, IO, social, guild, reservation and
teardown fences. The coordinator's actual `BoundSave` modification is not
included in this isolated branch and must be applied before enabling the hook.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-zone-binding-identity-build.log`); `python3 tools/dev.py
test --timeout 30 -R '^(vertical-quests-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit|vertical-disconnect-tcp-unit)$'`
passed **5/5** (log `work/overnight-07-quest-zone-binding-identity-tests.log`).
The retained-world fixture rejects an active-binding mismatch before quest
replica mutation and then saves successfully with the same binding instance.
`git diff --check` passed. Isolated SQL configuration was absent in the prior
check; no SQL lifecycle action was taken. Production coordinator integration,
SQL execution and original-client end-to-end play remain unverified.

## Revalidate the split save binding after quest delivery

The split quest-save callback now rechecks that the retained binding still
matches the session, the active zone-save identity still points to that exact
binding and its mutex address is unchanged after quest delivery. This stops
later save phases when a synchronous quest callback changes the binding. The
quest lock is still released on the tested reentry path; already-applied quest
replica writes are not rolled back. Original save-order evidence: `ps_game.exe`
`CUser::SendDBAgentCharGetInfo` **0046c980**, quest phase
**0046ccd4..0046ce4a**. The reentry check is native integration protection.

Coordinator integration: after `6af36208`, port the post-delivery checks in
`vertical_world_quests_save_phase_callback`. The real `BoundSave` must implement
`VerticalQuestZoneSaveBindingIdentity` as described in the prior section and
the zone service must propagate exceptions before quick slots or `0302`.
Preserve the coordinator's admission, IO, social, guild, reservation and
teardown fences; this branch has not modified its actual zone service.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-binding-reentry-build.log`); `python3 tools/dev.py test
--timeout 30 -R '^(vertical-quests-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit|vertical-disconnect-tcp-unit)$'`
passed **5/5** (log `work/overnight-07-quest-binding-reentry-tests.log`). The
retained-world fixture changes the binding during the quest-lock callback and
checks that the operation throws and the lock is released. `git diff --check`
passed. Isolated SQL configuration was absent in the prior check; no SQL
lifecycle action was taken. Production coordinator wiring, SQL execution and
original-client end-to-end play remain unverified.

## Route character-entry quest lists from the retained owner

`VerticalProtocolServices` now accepts an explicit `quest_entry` provider. If
present, `deliver_vertical_entry` invokes it for the original quest and
finished-quest steps instead of sending those lists from the load snapshot;
other entry steps and their order are unchanged. `VerticalWorldQuestEntry`
implements that provider using the registered selected Actor, retained quest
session, real quest lock and caller-supplied finished-list lock. It validates
ownership around delivery and does not turn a missing owner into success. The
legacy snapshot entry path remains for compositions without the provider.
Original evidence: `ps_game.exe` `CUser::PacketUserDBChar` **0046cf60**,
`CUser::SendCharacterQuest` **004835f0** and
`CUser::SendCharacterQuestFinish` **00483730** use the live quest lists.

Coordinator integration: port the `quest_entry` field in
`vertical_protocol.hpp`, the two-step dispatch in `vertical_protocol.cpp`, and
`VerticalWorldQuestEntry` in `vertical_quests_world_session.hpp`. After the
actual quest session is installed and before character-entry delivery, bind
`protocol.quest_entry` to a retained `VerticalWorldQuestEntry` using the exact
selected session, registry and actual finished-quest lock owner. The provider and
lock must outlive synchronous delivery; it must not be bound to a stale or
synthetic lock. Preserve the coordinator's existing protocol, admission, IO,
social, guild and reservation services. Production protocol wiring and its
real lock provider remain required; the optional legacy path alone does not
prove retained quest entry is active.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-entry-routing-build.log`); `python3 tools/dev.py test
--timeout 30 -R '^(vertical-quests-unit|vertical-protocol-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit)$'`
passed **5/5** (log `work/overnight-07-quest-entry-routing-tests.log`). The
protocol fixture checks the two owned steps and their order; the retained-world
fixture checks the active quest packet and actual finished-list lock. These are
separate fixtures, not an original-client end-to-end test. `git diff --check`
passed. Isolated SQL configuration was absent in the prior check; no SQL
lifecycle action was taken. Production wiring, SQL execution and original-client
end-to-end play remain unverified.

## Bind retained quest entry to protocol services

`vertical_world_bind_quest_entry` installs the existing retained quest-entry
provider into a `VerticalProtocolServices` instance. It requires the registered
selected Actor, installed quest session and an independently retained actual
finished-list lock; it rejects missing or duplicate providers and a lock lease
that retains the enclosing session. The callback retains the lock and uses a
weak session reference, then resolves and validates the exact session at each
quest entry step. Original entry evidence: `ps_game.exe`
`CUser::PacketUserDBChar` **0046cf60**, `CUser::SendCharacterQuest` **004835f0**
and `CUser::SendCharacterQuestFinish` **00483730**.

Coordinator integration: port this binder after `fff06fae`. After installing
the selected quest owner and before calling entry delivery, obtain the actual
retained finished-list lock and call
`vertical_world_bind_quest_entry(protocol, session, registry, finished_lock)`.
Keep the returned protocol services and lock alive through synchronous entry;
do not replace the other protocol, admission, IO, social, guild or reservation
providers. The coordinator still needs a real finished-list lock owner and must
supply it; this helper does not invent one or enable itself automatically.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-entry-bind-build.log`); `python3 tools/dev.py test
--timeout 30 -R '^(vertical-quests-unit|vertical-protocol-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit)$'`
passed **5/5** (log `work/overnight-07-quest-entry-bind-tests.log`). The
retained-world fixture exercises the bound callback and actual quest runtime,
rejects missing and duplicate providers and rejects a session-cyclic lock lease.
`git diff --check` passed. Isolated SQL configuration was absent in the prior
check; no SQL lifecycle action was taken. Production wiring, SQL execution and
original-client end-to-end play remain unverified.

## Own the finished-quest lock in the retained runtime

`QuestRuntime` now owns a distinct recursive finished-list mutex. Finished-row
insertion during restore and nonrepeatable removal uses that mutex; quest-start
finished-list reads and entry-row snapshots use it as well. The lock is released
before quest removal sends its DB packet. `VerticalQuestRuntimeFinishedLock`
adapts this actual runtime mutex to the existing entry interface, and
`vertical_world_quests_finished_lock` resolves the exact selected Actor and
runtime while rejecting a session-owned runtime alias. The runtime and other
quest operations still require the serialized world executor; this is not a
claim that the entire quest heap is thread-safe. Original evidence:
`ps_game.exe` `CUser::QuestRemove` **0047e150** and
`CUser::SendCharacterQuestFinish` **00483730**. The runtime-owned mutex is the
native implementation of the distinct finished-list lock.

Coordinator integration: port the `QuestRuntime` mutex and finished-list read
and insertion changes, `VerticalQuestRuntimeFinishedLock`, runtime-owner
accessor and `vertical_world_quests_finished_lock`. After the actual quest
session is installed, call `vertical_world_quests_finished_lock(session,
registry)` and pass that retained lock to `vertical_world_bind_quest_entry` from
`5ef0d6bb`. Preserve existing world serialization and admission, IO, social,
guild, reservation and teardown fences. The real coordinator protocol binding
is still required; the original-client entry flow has not been run.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-finished-owner-build.log`); `python3 tools/dev.py test
--timeout 30 -R '^(vertical-quests-unit|vertical-protocol-unit|quest-runtime-unit|quest-requests-actor-unit|vertical-session-registry-unit|vertical-world-unit)$'`
passed **5/5 matching tests** (log
`work/overnight-07-quest-finished-owner-tests.log`; no `quest-runtime-unit` test
is registered). The retained-world fixture verifies the entry adapter holds
the runtime's exact mutex against another thread and sends the quest entry
packet, while existing completion and restore cases pass. `git diff --check`
passed. Isolated SQL configuration was absent in the prior check; no SQL
lifecycle action was taken. Production coordinator wiring, SQL execution and
original-client end-to-end play remain unverified.

## Bind installed quest entry during session preparation

`VerticalWorldSession::prepared` now binds its installed quest session into the
protocol services before entry delivery. `bind_installed_quest_entry` creates the
finished-list lock adapter from that exact quest runtime, retains the runtime
without retaining the enclosing session in a cycle, and validates the selected
Actor and registry before and after each quest entry step. Existing protocol
services and the legacy path for a session without an installed quest owner
remain in place; an already populated quest provider is rejected rather than
silently replaced. Original evidence: `ps_game.exe` `CUser::PacketUserDBChar`
**0046cf60**, `CUser::SendCharacterQuest` **004835f0** and
`CUser::SendCharacterQuestFinish` **00483730**.

Coordinator integration: after `fff06fae` and `48a83004`, port
`bind_installed_quest_entry` and the call after `services_.protocol(*this)` in
`VerticalWorldSession::prepared`. The real preparation must install the quest
session before completing; protocol providers must not also install a separate
quest callback. Preserve newer admission, IO, social, guild and reservation
fences in the coordinator's session. This branch did not modify or run the
coordinator checkout.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-entry-session-build.log`); `python3 tools/dev.py test
--timeout 30 -R '^(vertical-quests-unit|vertical-protocol-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit)$'`
passed **5/5** (log `work/overnight-07-quest-entry-session-tests.log`). The
retained-world fixture calls the session binder with its actual quest runtime
and checks quest packets and duplicate rejection; the protocol fixture checks
entry-step dispatch separately. These do not constitute a combined production
preparation or original-client test. `git diff --check` passed. Isolated SQL
configuration was absent in the prior check; no SQL lifecycle action was taken.
Production coordinator wiring, SQL execution and original-client end-to-end play
remain unverified.

## Exercise retained quest state through character-entry delivery

The retained-world fixture now binds the installed quest-entry provider and runs
`deliver_vertical_entry` against the selected session's actual Actor load
pipeline, quest runtime and character replica. It changes a quest counter after
loading and verifies the emitted `0901` packet contains that live counter once,
then restores the fixture for the existing gameplay tests. The fixture supplies
local protocol callbacks for unrelated entry services; it is not a production
network session or original-client run. Original evidence: `ps_game.exe`
`CUser::PacketUserDBChar` **0046cf60** and `CUser::SendCharacterQuest`
**004835f0**.

Coordinator integration remains the `fff06fae`, `48a83004` and `b21f796e`
protocol/session changes: install the real quest session during preparation,
then bind its entry provider before the actual entry delivery. Preserve the
coordinator's admission, IO, social, guild and reservation fences. This test
calls the binder and dispatcher together but does not execute the session's
asynchronous `prepared` completion or the coordinator's real service wiring.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-entry-composed-build.log`); `python3 tools/dev.py test
--timeout 30 -R '^(vertical-quests-unit|vertical-protocol-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit)$'`
passed **5/5** (log `work/overnight-07-quest-entry-composed-tests.log`).
`git diff --check` passed. Isolated SQL configuration was absent in the prior
check; no SQL lifecycle action was taken. Production coordinator wiring, SQL
execution and original-client end-to-end play remain unverified.

## Fence character entry after a session failure

The installed and externally bound retained quest-entry providers now reject a
session already marked as dispatch-failed, both before and after delivery.
`VerticalWorldSession::prepared` also stops before entry delivery if a failure
was already latched while asynchronous preparation completed; it clears the
preparing state so normal disconnect does not cancel an already completed
preparation. This is native failure containment, not a recovered original
exception policy. Original entry evidence: `ps_game.exe`
`CUser::PacketUserDBChar` **0046cf60**, `CUser::SendCharacterQuest` **004835f0**
and `CUser::SendCharacterQuestFinish` **00483730**.

Coordinator integration: after the session/entry changes in `b21f796e` and the
failure latch in `9e1ca22`, port the `dispatch_failed` checks in the session,
`VerticalWorldQuestEntry` and entry binder. Preserve the coordinator's existing
asynchronous preparation cancellation and all admission, IO, social, guild,
reservation and teardown fences. The production entry binding remains to be
integrated; this change does not add a substitute success callback.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-entry-failed-session-build.log`); `python3 tools/dev.py
test --timeout 30 -R '^(vertical-quests-unit|vertical-protocol-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit)$'`
passed **5/5** (log `work/overnight-07-quest-entry-failed-session-tests.log`).
The retained-world fixture induces a quest timer failure and verifies that both
pre-bound entry providers and a new bind reject it without sending quest
packets. The asynchronous `prepared` early-return branch is compiled but is not
independently exercised by this fixture. `git diff --check` passed. Isolated
SQL configuration was absent in the prior check; no SQL lifecycle action was
taken. Production wiring, SQL execution and original-client end-to-end play
remain unverified.

## Exercise preparation completion after a quest failure

The retained-world fixture now posts its real preparation completion callback
onto the world strand after inducing the existing quest timer failure. It
verifies that `VerticalWorldSession::prepared` does not attempt entry or issue a
second disconnect, and that later close does not cancel an already completed
preparation. Existing session registry, IO lease, quest lease and cleanup
assertions still run. This exercises the previously untested early-return branch
from `ca45f829`; it does not exercise successful production preparation or an
original client. Original entry evidence: `ps_game.exe`
`CUser::PacketUserDBChar` **0046cf60**, `CUser::SendCharacterQuest` **004835f0**
and `CUser::SendCharacterQuestFinish` **00483730**. The asynchronous failure
fence itself is native lifecycle protection.

Coordinator integration remains the `ca45f829` preparation and quest-entry
failure checks, composed with the coordinator's real preparation cancellation,
admission, IO, social, guild, reservation and teardown fences. No coordinator
source was modified.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-preparation-failure-build.log`); `python3 tools/dev.py
test --timeout 30 -R '^(vertical-quests-unit|vertical-protocol-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit)$'`
passed **5/5** (log `work/overnight-07-quest-preparation-failure-tests.log`).
`git diff --check` passed. Isolated SQL configuration was absent in the prior
check; no SQL lifecycle action was taken. Production coordinator wiring, SQL
execution and original-client end-to-end play remain unverified.

## Exercise successful asynchronous quest entry preparation

A second reserved world-session fixture now completes selection and posts the
real preparation completion on the world strand. Its preparation restores the
quest runtime from the selected character replica and installs the actual quest
session; `VerticalWorldSession::prepared` automatically binds and runs the
retained quest-entry provider. The test observes the live `0901` quest counter,
ready state, waiting-list registration and normal registry/IO cleanup on close.
Its unrelated protocol/admission services are local test providers; this is not
an original client, SQL-backed preparation or production coordinator runtime.
Original evidence: `ps_game.exe` `CUser::PacketUserDBChar` **0046cf60**,
`CUser::SendCharacterQuest` **004835f0** and
`CUser::SendCharacterQuestFinish` **00483730**.

Coordinator integration remains the session and entry changes in `fff06fae`,
`48a83004` and `b21f796e`: the real preparation must install the selected quest
owner before completing, and existing admission, IO, social, guild, reservation
and teardown fences must remain intact. This fixture executes the successful
asynchronous session branch that was previously tested only in separate pieces,
but does not instantiate the coordinator's actual runtime providers.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-entry-prepared-build.log`); `python3 tools/dev.py test
--timeout 30 -R '^(vertical-quests-unit|vertical-protocol-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit)$'`
passed **5/5** (log `work/overnight-07-quest-entry-prepared-tests.log`).
`git diff --check` passed. Isolated SQL configuration was absent in the prior
check; no SQL lifecycle action was taken. Production coordinator wiring, SQL
execution and original-client end-to-end play remain unverified.

## Compose replica restore and quest installation

`vertical_world_prepare_replica_quests` now restores the selected session's
actual quest replica under its retained save mutex, invokes a required factory
for the real world services, verifies the factory returned the exact restored
Actor/runtime, quest definitions, pool and mutex, then installs the quest owner.
It returns skipped-allocation counts and invalid active quest IDs to the caller;
it does not substitute success for missing services or discard diagnostics.
Original evidence: `ps_game.exe` `CUser::PacketUserDBSetInfo` **0046d780**
quest load cases **0506/0507**, and `CUser::PacketUserDBChar` **0046cf60**.

Coordinator integration: port this helper after its existing restore/install
owners. During actual session preparation call it with the real retained save
mutex, quest pool and definitions, and a factory that binds the supplied
restored owners to the real zone, NPC, transport, reward and action services.
Handle returned skipped/invalid diagnostics according to the real preparation
policy before reporting completion. Preserve the coordinator's admission, IO,
social, guild, reservation and teardown fences. The helper does not itself
construct those external services or establish SQL/original-client readiness.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-prepare-composition-build.log`); `python3 tools/dev.py test --timeout 30 -R '^(vertical-quests-unit|vertical-protocol-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit)$'`
passed **5/5** (log `work/overnight-07-quest-prepare-composition-tests.log`).
The successful asynchronous session fixture now uses the composed helper,
rejects a factory returning another mutex without installing a quest owner,
then restores, installs and emits the live quest entry packet. `git diff
--check` passed. Isolated SQL configuration was absent in the prior check; no
SQL lifecycle action was taken. Production coordinator wiring, SQL execution
and original-client end-to-end play remain unverified.

## Preserve restored quest ownership during preparation

The composed restore/install helper now requires both the same address and the
same `shared_ptr` ownership for the factory-returned Actor, quest runtime,
definitions, pool and replica mutex. This rejects same-address non-owning or
unrelated control-block aliases before installing the quest session, preserving
the actual owners through later entry and gameplay. Original preparation
context: `ps_game.exe` `CUser::PacketUserDBSetInfo` **0046d780** quest cases
**0506/0507** and `CUser::PacketUserDBChar` **0046cf60**. Shared ownership
validation is a native lifetime safeguard.

Coordinator integration: port the `same_lease` check in
`vertical_world_prepare_replica_quests` after `25dfcf1d`. The real preparation
factory must return copies or aliases sharing the restored owners' control
blocks; a no-op-deleter wrapper is not a retained owner. Preserve the
coordinator's admission, IO, social, guild, reservation and teardown fences and
propagate preparation failures through its real policy.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-prepare-lease-build.log`); `python3 tools/dev.py test --timeout 30 -R '^(vertical-quests-unit|vertical-protocol-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit)$'`
passed **5/5** (log `work/overnight-07-quest-prepare-lease-tests.log`). The
successful asynchronous preparation fixture rejects a same-address non-owning
Actor alias without installing a session, then restores and enters with genuine
leases. `git diff --check` passed. Isolated SQL configuration was absent in
the prior check; no SQL lifecycle action was taken. Production coordinator
wiring, SQL execution and original-client end-to-end play remain unverified.

## Validate all retained quest preparation leases

The successful asynchronous preparation fixture now tries same-address,
non-owning aliases for each checked dependency: Actor, quest runtime, quest
pool, definitions and save mutex. Each is rejected before quest installation;
the fixture then uses genuine leases and completes entry. This broadens the
coverage of the owner checks from `ac3d9c23` without changing production code.
Original preparation context: `ps_game.exe` `CUser::PacketUserDBSetInfo`
**0046d780** quest cases **0506/0507** and `CUser::PacketUserDBChar`
**0046cf60**. Shared ownership checks are native lifetime protection.

Coordinator integration remains the composed preparation helper and matching
shared ownership checks in `25dfcf1d` and `ac3d9c23`. The factory must bind the
real retained services and preserve the original control blocks for all five
owners. The coordinator's admission, IO, social, guild, reservation and
teardown fences remain required.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-prepare-all-leases-build.log`); `python3 tools/dev.py test --timeout 30 -R '^(vertical-quests-unit|vertical-protocol-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit)$'`
passed **5/5** (log `work/overnight-07-quest-prepare-all-leases-tests.log`).
`git diff --check` passed. Isolated SQL configuration was absent in the prior
check; no SQL lifecycle action was taken. Production coordinator wiring, SQL
execution and original-client end-to-end play remain unverified.

## Coordinator save-resource lifetime during quest installation

The current coordinator's `VerticalWorldSessionSqlSave::mutex()` returns an
alias whose control block owns the entire SQL save resource, including its
connections and recovery writer. Passing that alias to
`vertical_world_prepare_replica_quests` would retain the SQL resource in the
session's quest ports past `VerticalWorldSessionSaveTeardown::release_binding`;
the resource could then be destroyed on the world thread after the DB disposal
fence, rather than on the DB executor. The coordinator's teardown explicitly
excludes external resource aliases. The quest helper's same-owner check also
means its factory must return the exact mutex lease passed to the helper.

For integration, create and retain the actual account mutex independently,
pass that same `shared_ptr<std::recursive_mutex>` to
`vertical_world_session_sql_save_opener` and to
`vertical_world_prepare_replica_quests`, and return that same lease in
`VerticalWorldQuestInstall::replica_mutex`. The SQL save resource and social
services must use that same mutex address. Do not substitute the alias returned
by `VerticalWorldSessionSqlSave::mutex()` for the independently retained mutex
in quest ports. Preserve the preparation cancellation, save binding drain,
account reservation, IO, social, guild and world-removal fences. An alternative
coordinator change must explicitly release quest ports and every resource alias
before disposing the SQL binding, with no later quest callbacks.

Original context remains `ps_game.exe` `CUser::PacketUserDBSetInfo` **0046d780**
quest cases **0506/0507**, `CUser::PacketUserDBChar` **0046cf60**, and
`CWorld::LeaveUser` **0040ca20**. This is a native resource-lifetime constraint,
not a claim about original thread ownership.

Validation: read-only inspection of current coordinator
`vertical_world_session_sql_save.hpp`, `vertical_world_session_preparation.hpp`,
`vertical_world_session_save_teardown.hpp` and `vertical_world_session_runtime.hpp`,
and this branch's `vertical_quests_world_session.hpp` and
`vertical_world_session.hpp`. No code changed, so no build or test was rerun.
The previous focused native build and 5/5 tests remain recorded above. Actual
coordinator composition, SQL execution, and original-client play are unverified.
Next: integrate using the independent mutex lease and test real preparation and
teardown across the retained session and save binding.

## Bind quest restoration to the retained save owner

Added `vertical_world_prepare_bound_replica_quests`, which requires the world
strand, checks `save.matches(strand, registry, session)` and checks the
independently retained mutex against `save.save_mutex()` before restoring and
installing the real quest owner. The factory must still return the same five
restored owner leases checked by `vertical_world_prepare_replica_quests`. A
foreign save owner or mutex fails before quest installation. Original loading
context: `ps_game.exe` `CUser::PacketUserDBSetInfo` **0046d780** quest cases
**0506/0507** and `CUser::PacketUserDBChar` **0046cf60**. Binding checks are
native lifetime and synchronization safeguards.

Coordinator integration: port the adapter with `25dfcf1d` and `ac3d9c23`.
Invoke it on the world strand only after `VerticalWorldSessionPreparation`
retains and validates its actual `VerticalWorldSessionSaveBinding`, and before
successful completion is published; pass the independently owned account mutex
also passed to the SQL opener. The current coordinator preparation has no such
post-bind hook: add one inside `opened`, after `binding_` assignment and its
`matches` validation, so exceptions use its existing failed-binding DB-worker
disposal path. Do not compose this by wrapping `Bind` after it creates a binding:
an exception before the wrapper returns can destroy the last binding on the
world strand. Preserve cancellation, admission, IO, social, guild, reservation,
and teardown fences. The adapter itself does not install that coordinator hook.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-bound-prepare-build.log`); `python3 tools/dev.py test
--timeout 30 -R '^(vertical-quests-unit|vertical-protocol-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit)$'`
passed **5/5** (log `work/overnight-07-quest-bound-prepare-tests.log`). The
asynchronous entry fixture supplies a test binding with the coordinator's
`matches` and `save_mutex` interface, rejects foreign owner and mutex, then
installs and sends the restored quest entry. It does not exercise the actual
coordinator preparation or SQL resource. `git diff --check` passed. Both
`.local/runtime.env` and `.local/sqlserver.env` were absent; no isolated SQL
integration test was run or SQL lifecycle changed. Original-client end-to-end
play remains unverified. Next: add and test the retained post-bind hook against
coordinator preparation and real teardown.

## Retained preparation post-bind quest integration

Copied the coordinator's retained preparation and its missing header dependencies
into this branch. `VerticalWorldSessionPreparation::create` now accepts an
optional `AfterBind(VerticalWorldSession&, Binding&)`, invoked on the world
strand after the owner has retained and validated its binding and before it
publishes success. Exceptions follow existing opener drain and database-worker
binding disposal. Checks after binding and after the hook stop publication if a
callback reenters cancellation. Existing callers remain source-compatible.
The hook receives a reference, so it need not retain the binding or SQL resource.

The quest entry fixture now uses the actual copied preparation and resource
opener: its post-bind hook runs `vertical_world_prepare_bound_replica_quests`
with the retained binding, verifies mismatched mutex and session rejection, and
installs the real restored quest owner before entry. A separate throwing hook
checks that completion reports failure only after binding destruction off the
world strand and after the account database fence drains. The resource and save
binding in this fixture are test objects; real SQL and production runtime
composition were not exercised. Original load context: `ps_game.exe`
`CUser::PacketUserDBSetInfo` **0046d780** quest cases **0506/0507** and
`CUser::PacketUserDBChar` **0046cf60**. The asynchronous hook and disposal
fences are native implementation details, not claims about original internals.

Coordinator integration: the coordinator already owns the copied dependency
headers, so apply only the `AfterBind` changes in
`vertical_world_session_preparation.hpp` and the earlier quest adapter changes;
do not replace its headers wholesale. Supply the hook from the real preparation
provider after constructing the independently retained account mutex; pass the
actual retained `Binding&`, that mutex, real pool and definitions and real
quest factory to `vertical_world_prepare_bound_replica_quests`. Handle its
restore diagnostics according to the real policy. Preserve existing social,
guild, admission, IO, reservation, cancellation and teardown fences; do not
retain the SQL resource via its aliasing mutex accessor in quest ports. The
coordinator's concrete SQL/social preparation convenience factories still need
to accept and forward the hook or construct the preparation directly.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-postbind-final-build.log`); `python3 tools/dev.py test
--timeout 30 -R '^(vertical-quests-unit|vertical-protocol-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit)$'`
passed **5/5** (log `work/overnight-07-postbind-tests.log`). `git diff --check`
passed. Isolated SQL configuration was absent at the preceding check; no SQL
lifecycle was changed. Production coordinator wiring, actual SQL teardown, and
original-client end-to-end play remain unverified. Next: forward the hook through
real SQL/social preparation and test cancellation and teardown with those owners.

## Forward quest preparation through save factories

The SQL, generic social, and SQL/social preparation factories now accept an
optional `AfterBind` callback and forward it to the retained preparation. The
existing overload shape is preserved by a default empty callback. This lets the
real provider supply `vertical_world_prepare_bound_replica_quests` after the
save binding and social owners have been retained. A new reentrant cancellation
fixture calls `cancel` from the hook and checks that no preparation completion
is delivered, the binding is destroyed on the database worker, and the account
database fence has drained before cancellation completion. The fixture uses a
test resource and binding; it does not connect to SQL.

Original context: `ps_game.exe` `CUser::PacketUserDBSetInfo` **0046d780**
quest cases **0506/0507**, `CUser::PacketUserDBChar` **0046cf60**, and
`CWorld::LeaveUser` **0040ca20**. The callback and cancellation barriers are
native integration safeguards.

Coordinator integration: apply the three factory parameter/forwarding changes
in `vertical_world_session_preparation.hpp` after the `AfterBind` change in
`1a571295`. The coordinator's
`vertical_world_session_sql_preparation_provider.hpp` still needs a real
per-session quest callback wired through `VerticalWorldSessionSqlPreparationServices`
and forwarded with the independently retained account mutex. The account
provider must continue deriving that mutex from the actual social account.
Preserve all existing admission, reservation, IO, social, guild and teardown
fences. No SQL or social callback is synthesized by these changes.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-postbind-factories-build.log`); `python3 tools/dev.py test
--timeout 30 -R '^(vertical-quests-unit|vertical-protocol-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit)$'`
passed **5/5** (log `work/overnight-07-postbind-factories-tests.log`).
`git diff --check` passed. The SQL/social factories were compiled but not
executed against real SQL; both `.local/runtime.env` and `.local/sqlserver.env`
were absent and SQL lifecycle was not changed. Production provider wiring,
actual SQL teardown and original-client end-to-end play remain unverified.
Next: connect the real runtime provider and validate its lifecycle.

## Build the per-session quest post-bind callback

Added `vertical_world_quest_after_bind<Binding>`, which constructs the actual
retained preparation callback from the independently owned account mutex,
quest pool and definitions, and mandatory live tick, quest factory and restore
policy callbacks. It checks the session strand, save binding identity and
mutex before calling any of those services, then uses
`vertical_world_prepare_bound_replica_quests`. The restore policy receives all
skipped/invalid diagnostics; it executes after quest installation and an
exception fails preparation through its normal cleanup path. The enclosing
session must retain its real quest services until disconnect even on failure.
No gameplay callback is synthesized.

The retained preparation entry fixture now constructs and executes this hook,
proves a foreign save owner does not invoke the tick or policy and installs no
quest owner, then restores the real quest session and calls the supplied policy.
Original context: `ps_game.exe` `CUser::PacketUserDBSetInfo` **0046d780**
quest cases **0506/0507** and `CUser::PacketUserDBChar` **0046cf60**. The
callback and binding checks are native integration and lifetime safeguards.

Coordinator integration: after porting the preceding preparation changes,
construct `vertical_world_quest_after_bind<VerticalWorldSessionSqlSave::Binding>`
with the same mutex used to open the SQL save resource and pass it as
`after_bind` to `make_vertical_world_session_sql_preparation` or the SQL/social
factory. The current `vertical_world_session_sql_preparation_provider.hpp` still
needs its real per-session providers for tick, pool, definitions, factory and
restore policy wired to that argument. Its account variant must retain the
existing social account mutex. Preserve admission, IO, social, guild, reservation
and teardown fences. The coordinator provider itself was not copied or changed.

Validation: `python3 tools/dev.py build` passed (log
`work/overnight-07-quest-after-bind-provider-build.log`). Focused test result is
recorded below. SQL configuration was absent in the preceding check; no SQL
lifecycle was changed. Production provider wiring, real SQL teardown and
original-client end-to-end play remain unverified.

`python3 tools/dev.py test --timeout 30 -R '^(vertical-quests-unit|vertical-protocol-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit)$'` passed **5/5** (log `work/overnight-07-quest-after-bind-provider-tests.log`). `git diff --check` passed.

## Compose the SQL provider's quest callback

Added `vertical_world_sql_quest_after_bind_provider` in
`vertical_quests_sql_preparation_provider.hpp`. It requires the real quest pool,
definitions, tick, install factory and restore policy, verifies the session is
retained on the supplied world strand, and constructs the SQL save binding's
quest callback with the independently retained mutex supplied by the provider.
The retained preparation fixture constructs this callback and rejects a missing
mutex; the prior fixture exercises the equivalent callback against a test save
binding. Neither fixture opens SQL or claims gameplay success.

The coordinator provider wiring is preserved as the reviewable patch
`reports/overnight/patches/07-quest-sql-provider.patch`: add an optional
per-session `after_bind` factory to `VerticalWorldSessionSqlPreparationServices`,
construct its callback with the actual selected session and exact account mutex,
reject an empty returned callback, and forward it to the SQL preparation factory.
The existing account variant continues to obtain its mutex from the real social
account. Set that service to `vertical_world_sql_quest_after_bind_provider(...)`
using the actual world owners. This patch is not installed in the coordinator.
`git apply --check` against its current checkout passed without writing there.
The callback is optional for compatibility, so the production caller must supply
it explicitly; admission, IO, social, guild, reservation and teardown fences
still apply.

Original context: `ps_game.exe` `CUser::PacketUserDBSetInfo` **0046d780**
quest cases **0506/0507** and `CUser::PacketUserDBChar` **0046cf60**. The
callback and shared-mutex lifetime checks are native integration safeguards.

Validation: an initial build including the full copied coordinator owner failed
(log `work/overnight-07-sql-quest-provider-build.log`): this older branch lacks
current guild membership/directory APIs, session username and network registry
interfaces. Those temporary copies were removed; the provider patch therefore
has not been compiled with the coordinator owner. The isolated quest helper
build passed with `python3 tools/dev.py build` (log
`work/overnight-07-sql-quest-provider-isolated-build.log`).
`python3 tools/dev.py test --timeout 30 -R '^(vertical-quests-unit|vertical-protocol-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit)$'`
passed **5/5** (log `work/overnight-07-sql-quest-provider-tests.log`).
`git diff --check` passed. Isolated SQL configuration was absent at the prior
check; no SQL lifecycle was changed. Concrete coordinator wiring, real SQL
teardown and original-client end-to-end play remain unverified. Next: compile
the provider patch against a coordinator-compatible integration fixture and run
the actual retained SQL/social lifecycle when centrally available.

## Reject restored quest rows before constructing session services

Quest restoration now supplies its skipped/invalid diagnostics to the real
restore policy before invoking the quest installation factory or installing an
owner. If the policy throws, no quest services are constructed and no quest
owner is attached to the retained session. On success, the factory still sees
its full restored diagnostics. Existing direct preparation callers keep the
optional default behavior; the composed post-bind callback requires its real
policy. This supersedes the earlier report statement that the policy executes
after installation.

The entry fixture rejects via the policy and verifies that neither its factory
nor the retained quest owner exists, then performs the normal successful
preparation. Original context: `ps_game.exe` `CUser::PacketUserDBSetInfo`
**0046d780** quest cases **0506/0507** and `CUser::PacketUserDBChar`
**0046cf60**. This ordering is native preparation failure handling, not a claim
about original internal callback order.

Coordinator integration: port the changes to
`vertical_world_prepare_replica_quests`,
`vertical_world_prepare_bound_replica_quests`, and
`vertical_world_quest_after_bind` after `2b27af6d`. Preserve the existing
post-bind error disposal and all admission, IO, social, guild, reservation and
teardown fences. SQL provider patch integration and real policy selection remain
outstanding; the provider patch was not applied to the coordinator. Real SQL
teardown and original-client end-to-end play remain unverified.

Validation: `python3 tools/dev.py build` passed (log `work/overnight-07-quest-restore-policy-build.log`); `python3 tools/dev.py test --timeout 30 -R '^(vertical-quests-unit|vertical-protocol-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit)$'` passed **5/5** (log `work/overnight-07-quest-restore-policy-tests.log`). `git diff --check` passed. Both `.local/runtime.env` and `.local/sqlserver.env` were absent; no SQL lifecycle action or SQL integration test was performed.

## Fence restored quest rows across preparation callbacks

Quest preparation snapshots the active and finished replica fields used by
`vertical_quests_restore_replica` while holding the retained save mutex. The
restore policy and quest service factory run without that mutex. After each
callback, preparation reacquires the mutex, checks the selected actor and
character, and compares the ordered quest projection to the saved snapshot.
The final check and owner installation hold the mutex together. Changed quest
rows fail before installation; a policy change also fails before the factory.
Save bookkeeping flags and deletion journals are not part of the restoration
projection and are not compared.

The retained preparation fixture mutates an active quest counter from both the
policy and factory and checks that each attempt rejects the stale projection
without installing quests, then restores the fixture and verifies successful
entry. Original context: `ps_game.exe` `CUser::PacketUserDBSetInfo` **0046d780**
quest cases **0506/0507**, `CUser::PacketUserDBChar` **0046cf60** and
`ps_dbAgent.exe` `PacketUserQuest` **00406590**. The snapshot and mutex checks
are native concurrency safeguards, not an assertion about original locking.

Coordinator integration: port the snapshot and revalidation changes in
`vertical_world_prepare_replica_quests` after `bfa7ffae`; the existing policy,
factory, shared account mutex, admission, IO, social, guild, reservation and
teardown fences are still required. The SQL provider patch remains unintegrated.
Production SQL lifecycle and original-client end-to-end play remain unverified.

Validation: `python3 tools/dev.py build` passed (log `work/overnight-07-quest-restore-snapshot-build.log`); `python3 tools/dev.py test --timeout 30 -R '^(vertical-quests-unit|vertical-protocol-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit)$'` passed **5/5** (log `work/overnight-07-quest-restore-snapshot-tests.log`). `git diff --check` passed. Both `.local/runtime.env` and `.local/sqlserver.env` were absent; no isolated SQL test or SQL lifecycle action was performed.

## Stop primary quest timers when a session closes

`VerticalWorldPrimaryQuestServices` now declines timer retention for a closing
or dispatch-failed session, and direct timer quest access rejects either state.
The shared `VerticalWorldQuestSessions` resolver still retains the owner during
closing, because NPC cleanup and final quest save need it. This prevents a
primary-zone visit from starting a new timer update while the real disconnect
path is still removing the actor.

The fixture checks both a dispatch-failed owner and a cleanly closing owner:
primary retention is denied while cleanup retention remains available; direct
primary access on close throws. The existing final-save pin check still runs on
close. Original scheduler context: `ps_game.exe` `CWorldThread::Update`
**0040771d..00407760** and `CZone::UpdateQuest` **00422320**. The retained
session close/failure fences are native concurrency safeguards, not a claim
about the original scheduler's internal lock ordering.

Coordinator integration: update `VerticalWorldPrimaryQuestServices::retain`
and `quests` in `vertical_quests_world_session.hpp`; preserve the shared resolver
for disconnect and final save. Keep admission, IO, social, guild, reservation
and actual zone-removal fences. SQL provider patch integration, real SQL teardown
and original-client end-to-end play remain unverified.

Validation: `python3 tools/dev.py build` passed (log `work/overnight-07-quest-timer-close-build.log`); `python3 tools/dev.py test --timeout 30 -R '^(vertical-quests-unit|vertical-protocol-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit)$'` passed **5/5** (log `work/overnight-07-quest-timer-close-tests.log`). `git diff --check` passed. Both `.local/runtime.env` and `.local/sqlserver.env` were absent; no SQL lifecycle action or isolated SQL test was performed.

## Preserve zone traversal leases while skipping closing quest timers

Corrected `934515ce`: `ZoneUsers::visit_all` requires a non-null retention lease
for each visited actor, so returning an empty lease for a closing or failed
session could abort the entire primary quest phase. Primary quest retention now
uses the same retained quest session as before; a separate `eligible` check
skips timer access for closing or dispatch-failed owners. Direct quest access
still rejects both. The default eligibility implementation preserves other
`PrimaryQuestPhaseServices` implementations. Disconnect cleanup and final save
still retain the same real quest owner.

The fixture runs the actual `ZoneUsers::visit_all` over a failed retained actor
and verifies traversal completes without another timer failure. It also checks
that a cleanly closing session remains retainable but ineligible, while direct
timer access is rejected. Original scheduler context: `ps_game.exe`
`CWorldThread::Update` **0040771d..00407760**, `CZone::UpdateQuest` **00422320**,
and `CZone::UpdateUserAttack` **00421930** / `RemoveUser` **00415ec0** for the
shared cursor/lock model. The eligibility fence is a native lifecycle safeguard.

Coordinator integration: port both `PrimaryQuestPhaseServices::eligible` and
the early eligibility check in `world_primary_quest_phase.hpp`, plus the
`VerticalWorldPrimaryQuestServices` changes. Do not apply `934515ce`'s null
retention behavior without this correction. Preserve all existing admission,
IO, social, guild, reservation, cleanup and save fences. Production SQL
provider wiring, actual SQL teardown and original-client play remain unverified.

Validation: `python3 tools/dev.py build` passed (log `work/overnight-07-quest-phase-retain-build.log`); `python3 tools/dev.py test --timeout 30 -R '^(vertical-quests-unit|vertical-protocol-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit)$'` passed **5/5** (log `work/overnight-07-quest-phase-retain-tests.log`). `git diff --check` passed. Both `.local/runtime.env` and `.local/sqlserver.env` were absent; no isolated SQL test or SQL lifecycle action was performed.

## Reconcile active-zone quest phase with close eligibility

Ported the coordinator's existing `WorldPrimaryQuestPhase::update_active`
entry point into this branch and routed both primary and active visits through
the same dispatch path. The retained service's eligibility check from
`56ae50f4` now applies to active instance visits as well as primary visits. The
two entry points share one 60000 ms deadline; an interrupted active traversal
does not advance it. Actual root active-list ownership remains supplied by the
coordinator; no fake production callback was introduced.

A native world fixture uses a running `VerticalWorld`, real `ZoneUsers` and a
retained Actor with a test active-list root. It verifies that an ineligible
actor remains retained but receives no timer/failure callback, that interrupted
visitation leaves the deadline unchanged, and that completed and skipped
visits use the shared deadline. This fixture does not exercise the coordinator's
real dynamic-zone owner. Original scheduler context: `ps_game.exe`
`CWorldThread::Update` **0040771d..00407760** and `CZone::UpdateQuest`
**00422320**. Retained-owner eligibility is a native lifecycle safeguard.

Coordinator integration: the current coordinator already has `update_active`
and the common dispatch path. Port only the `PrimaryQuestPhaseServices::eligible`
contract, early check in `vertical_primary_quest_update_actor`, and
`VerticalWorldPrimaryQuestServices` close/failure eligibility from `56ae50f4`;
do not replace the coordinator active-zone implementation. Preserve admission,
IO, social, guild, reservation, cleanup and final-save fences. Production SQL
provider wiring, real SQL teardown and original-client play remain unverified.

Validation: the initial `python3 tools/dev.py build` failed to link `vertical-world-unit` because that target lacked the quest implementation (log `work/overnight-07-active-quest-phase-build.log`). The private test target now links the existing quest and world libraries. The repeated `python3 tools/dev.py build` passed (log `work/overnight-07-active-quest-phase-rebuild.log`); `python3 tools/dev.py test --timeout 30 -R '^(vertical-quests-unit|vertical-protocol-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit)$'` passed **5/5** (log `work/overnight-07-active-quest-phase-tests.log`). `git diff --check` passed. Both `.local/runtime.env` and `.local/sqlserver.env` were absent; no isolated SQL test or SQL lifecycle action was performed.

## Fence direct quest integration after dispatch failure

The quest/NPC dispatcher, quest pickup dispatcher and quest-zone rebind now
reject a session whose dispatch has failed, even when called outside the packet
router. Pickup also rechecks the failure fence after its real owner lookup,
which can reenter world callbacks. Unrelated packet families still fall through;
closing-session cleanup and final quest save still retain their existing owners.

The retained-session fixture induces a real timer dispatch failure, then calls
each integration boundary directly. It verifies rejection before NPC selection,
pickup action gating or quest-zone rebind, with no NPC interaction left behind.
Original dispatch context: `ps_game.exe` `CUser::UpdatePackets` **00466cc0**,
`PacketProcessing` **00466da0**, `CWorldThread::Update`
**0040771d..00407760**, and `CZone::UpdateQuest` **00422320**. Failure fencing
is native lifecycle protection, not a claim about original failure recovery.

Coordinator integration: port the `dispatch_failed()` checks in
`VerticalWorldQuestPacketDispatch`, `VerticalWorldQuestPickupDispatch` before
and after owner lookup, and `vertical_world_quests_rebind_zone`. Preserve the
packet queue, zone ownership, admission, IO, social, guild, reservation,
cleanup and final-save fences. SQL provider wiring, real SQL teardown and
original-client end-to-end play remain unverified.

Validation: `python3 tools/dev.py build` passed (log `work/overnight-07-quest-direct-failure-build.log`); `python3 tools/dev.py test --timeout 30 -R '^(vertical-quests-unit|vertical-protocol-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit)$'` passed **5/5** (log `work/overnight-07-quest-direct-failure-tests.log`). `git diff --check` passed. Both `.local/runtime.env` and `.local/sqlserver.env` were absent; no isolated SQL test or SQL lifecycle action was performed.

## Require primary quest eligibility from real services

`PrimaryQuestPhaseServices::eligible` is now mandatory rather than implicitly
returning true. Every implementation must supply its own lifecycle decision;
the retained world-session service already checks the exact actor, session
close and dispatch-failure state. This prevents a newly bound service from
silently bypassing the close fence. The earlier `56ae50f4` note describing a
default eligibility implementation is superseded.

The active-zone fixture now also makes its eligibility callback throw and
checks that the phase invokes the supplied failure service exactly once instead
of entering quest timer execution. The production failure service remains the
real retained session's dispatch-failure path. Original scheduler context:
`ps_game.exe` `CWorldThread::Update` **0040771d..00407760** and
`CZone::UpdateQuest` **00422320**. Mandatory eligibility is a native service
contract, not a claim about original interfaces.

Coordinator integration: when porting `56ae50f4`, declare `eligible` pure
virtual and implement it in every `PrimaryQuestPhaseServices` provider. The
current coordinator's `VerticalWorldPrimaryQuestServices` must use its actual
retained session close/failure check; its `RootQuestServices` test fixture must
supply an explicit test policy. Preserve active-zone retention, admission, IO,
social, guild, reservation, cleanup and final-save fences. SQL provider wiring,
real SQL teardown and original-client end-to-end play remain unverified.

Validation: `python3 tools/dev.py build` passed (log `work/overnight-07-quest-eligibility-required-build.log`); `python3 tools/dev.py test --timeout 30 -R '^(vertical-quests-unit|vertical-protocol-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit)$'` passed **5/5** (log `work/overnight-07-quest-eligibility-required-tests.log`). `git diff --check` passed. Both `.local/runtime.env` and `.local/sqlserver.env` were absent; no isolated SQL test or SQL lifecycle action was performed.

## Compile coordinator-compatible quest integration

The current coordinator's `VerticalWorldPrimaryQuestServices::retain` returns an
empty lease for a closing or dispatch-failed actor, but `ZoneUsers::visit_all`
requires a nonempty lease for every visited actor. Added the reviewable patch
`reports/overnight/patches/07-quest-phase-eligibility.patch`: retain the real
quest owner for traversal, require an explicit eligibility callback, skip timer
work for closing/failed sessions, and update the coordinator's world test
fixture. This is the correction from `56ae50f4` and `d1a76546` adapted to the
current coordinator active-zone phase; it must accompany the earlier close
fence. Original scheduler context: `ps_game.exe` `CWorldThread::Update`
**0040771d..00407760**, `CZone::UpdateQuest` **00422320**,
`CZone::UpdateUserAttack` **00421930** and `RemoveUser` **00415ec0**. The
eligibility and lease checks are native lifecycle safeguards.

`tests/prepare_quest_coordinator_compat.py` copies the current coordinator
headers and its world test into ignored `work/quest-coordinator-compat`, applies
both the phase patch and `07-quest-sql-provider.patch` only to that copy, and
copies this branch's quest SQL callback helper. When the snapshot exists, two
private CMake object targets compile the actual patched coordinator SQL
provider/quest callback types and the patched coordinator world test source
against its own current headers. The snapshot is read-only with respect to the
coordinator and is reproducible with:
`python3 tests/prepare_quest_coordinator_compat.py /home/franco/Projects/Eternity-foundation`.
The object targets do not link or execute the coordinator runtime or SQL.
Both patch files passed `git apply --check` against the current coordinator;
neither patch was applied there. Real caller wiring must supply the quest
provider with the actual account mutex, tick, pool, definitions, factory and
restore policy. Preserve admission, IO, social, guild, reservation, cleanup and
final-save fences. Real SQL lifecycle and original-client play remain unverified.

Validation: the snapshot preparation command passed; `python3 tools/dev.py build`
compiled both coordinator compatibility object targets (log
`work/overnight-07-coordinator-world-compat-build.log`). The final repeat build
and focused test results follow below. The coordinator's test was compiled,
not run; branch fixtures exercise the corresponding quest eligibility and
retention behavior.

The repeat `python3 tools/dev.py build` passed (log `work/overnight-07-coordinator-compat-final-build.log`); `python3 tools/dev.py test --timeout 30 -R '^(vertical-quests-unit|vertical-protocol-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit)$'` passed **5/5** (log `work/overnight-07-coordinator-compat-tests.log`). `git diff --check` passed. Both `.local/runtime.env` and `.local/sqlserver.env` were absent; no isolated SQL test or SQL lifecycle action was performed.

## Exercise coordinator active-instance quest phase in isolation

Extended the coordinator compatibility snapshot with an optional `--runtime`
mode. It copies the coordinator's current C++ sources, headers, CMake file and
test sources (excluding test data) into ignored
`work/quest-coordinator-runtime`, applies the existing SQL-provider and phase
patches there, and enables a private CMake target that builds only its
`vertical-world-unit` through `tools/dev.py`'s shared build lock. Nothing is
written to the coordinator checkout. The snapshot is reproducible with
`python3 tests/prepare_quest_coordinator_compat.py /home/franco/Projects/Eternity-foundation --runtime`.

The phase patch now extends the coordinator's own real active-instance world
fixture: an explicitly ineligible retained actor is traversed without reaching
its quest timer, while the real active-zone ownership remains in use. Existing
coordinator fixture checks still cover timed quest completion, active instance
retention, and the common primary/active deadline. Test eligibility is a fixture
policy; real production eligibility remains the retained session service.
Original scheduler context: `ps_game.exe` `CWorldThread::Update`
**0040771d..00407760**, `CZone::UpdateQuest` **00422320**,
`CZone::UpdateUserAttack` **00421930**, and `RemoveUser` **00415ec0**.
The phase eligibility callback is a native lifecycle safeguard.

The actual production SQL provider caller still must supply the real quest
callback, and the patches remain unapplied in the coordinator checkout. Preserve
admission, IO, social, guild, reservation, cleanup and final-save fences. This
fixture does not execute SQL or an original client and does not establish global
playability.

Validation: `python3 tests/prepare_quest_coordinator_compat.py /home/franco/Projects/Eternity-foundation --runtime`
passed; `python3 tools/dev.py build` freshly compiled and linked the isolated
coordinator world test against its copied sources (log
`work/overnight-07-coordinator-runtime-build.log`). The focused test result
follows below. An initial `git apply --check` was accidentally run against this
branch, where the phase changes already exist, and failed as expected; the same
check against the read-only coordinator checkout passed.

`python3 tools/dev.py test --timeout 90 -R '^(quest-coordinator-world-runtime|vertical-quests-unit|vertical-protocol-unit|vertical-session-registry-unit|vertical-world-unit|quest-requests-actor-unit)$'` passed **6/6**, including the isolated coordinator world runtime fixture (log `work/overnight-07-coordinator-runtime-tests.log`). `git diff --check` passed. Both `.local/runtime.env` and `.local/sqlserver.env` were absent; no isolated SQL test or SQL lifecycle action was performed.
