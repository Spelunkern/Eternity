# Eternity continuity — active 2026-11-03

The user resumed complete native server reconstruction. Continue server work.

## Objective and constraints

Reconstruct the complete EP4.5 server natively on Linux in C++20. Completion means
an original EP4 client can connect and play with original server behavior. Client
work is deferred; continue the server independently. Use Ghidra and SQL Server;
SQLite migration is deferred. Exclude ps_gamewar and Phoenix. Preserve originals.
Code/documentation in English, progress in Spanish. Rough global estimate remains
about 7%; the server is NOT playable. Test counts do not measure completion.
The user authorized parallel work and nine independent terminal workers on 2026-11-03. Read AGENTS.md and PARALLEL_WORK.md. Reasoning for the launcher is LOW (explicit user correction), not high.

Repository: https://github.com/Spelunkern/Eternity, branch main.
Checkout: /home/franco/Projects/Eternity.

## Completed checkpoints

- e14c0fa: actor experience, level notifications, maximum resource delivery and
  corrected user-ID routing.
- ff37918: published damage ownership. Each pooled MobRuntime owns its ledger;
  Init/recycling retain credit; Reset returns records; destruction deletes active
  records. 128 original pools / 2,432 operations and eight destruction scenarios.
  Resumed normal build, all 117 tests and complete isolated SQL integration exit 0.
  Earlier full 117-test ASAN/UBSAN log is also clean.
- 90690a4: published CMob::Death coordinator, original experience
  rates, credited solo/party/raid reward ordering, exactly-once obelisk/linked
  accounting, logging boundary, member removal, damage reset before skill cleanup.
  World reward/drop/quest/skill bodies are required providers, not default no-ops.
  Native actor test composes a real level transition; SQL test uses restored mob
  1 Exp=704, base reward=844 and final solo award=1,012.

- New validated mob-quest implementation: definition bindings, mob-target index,
  stable minimum-level ordering and duplicate suppression; live kill counters,
  exact DB/client packets and required QuestEnd boundary. 128 original index
  fixtures and 512 complete original kill updates compose native mob death and
  the real QuestReplica. Full SQL/asset comparison matches 2,747 mob slots,
  1,720 quests and the complete 11,984-byte index (498 references).

## Latest validation

Current mob-quest checkpoint:

- `.local/mob-quests-final-build.log`: exit 0, no compiler warnings.
- `.local/mob-quests-focused-final.log`: all four relevant tests pass.
- `.local/mob-quests-test.log`: exit 0, all 119 native tests pass (35.95 s).
- `.local/mob-quests-sanitized.log`: exit 0, all 119 ASAN/UBSAN tests pass (70.56 s).
- `.local/mob-quests-integration.log`: complete isolated SQL integration exit 0,
  including the full original mob/quest index and disposable guild cleanup.
- Original EXE/PDB hashes verified again. No checks remain pending.

Previous death checkpoint:

- `.local/mob-death-final-build.log`: exit 0, no compiler warnings.
- `.local/mob-death-final-test.log`: exit 0, all 118 tests passed (34.16 s).
- `.local/mob-death-final-focused.log`: exact 1,536-case original death comparison
  and real Actor integration passed, including final rates and reward callbacks.
- `.local/mob-death-final-sanitized.log`: exit 0; ASAN/UBSAN death/actor tests pass.
- `.local/mob-death-integration.log`: complete isolated SQL integration exit 0,
  including mob reward composition and final disposable guild-catalog checks.
- All original EXE/PDB hashes verified. No credentials or generated assets tracked.

## Current evidence and next work

`mob_death.hpp/cpp` implements the complete recovered coordinator, not its missing
reward services. Death does not set the FSM state or remove/recycle the corpse.
The actual combat caller must own those transitions.

Original-execution fixture: `tools/mob_death_oracle.py` and
`tests/data/mob-death-oracle.tsv`. Real selection/reset, GetExp and party level
share execute; world/reward bodies are observed ports. Cases include original and
changed static rates, 0..31 members, callback-driven member/party updates, tied and
aggregate credit, missing users, signed boundaries and ownership precedence.

Next: reconstruct QuestEnd (0047ec40), then complete drop/skill/log providers
and connect production combat/world owners. Kill counters/indexing are now
implemented, but QuestEnd reward/removal and the production quest owner are not.
The original kill-progress minimum-time gate is now <= success_tick, not >=;
QuestEnd has its own minimum-time check. Preserve both source contracts.
Do not mistake the coordinator's tests for finished rewards or playable gameplay.

Ghidra exports (ignored):

- `work/mob-death-lifecycle/ps_game.exe`: CMob::Death 004910f0. Assembly guards
  prevent duplicate principal transitions; misleading decompiler output does not.
- `work/mob-death-party/ps_game.exe`: GetPartyExpByLevel 00444e40 and
  EnableApplyRange 0045de60. Original count globals at 005423a8 / 005423e8 overlap
  after eight normal entries. Death retains its initial party-kind branch.
- `work/party-experience-init/ps_game.exe`: ChkPartyExpByLevel 00444d80.
- `work/mob-kill-quests/ps_game.exe`: QuestUpdateKillMOB 00480140. Original lookup
  is QuestFind 0047e4b0; completion is QuestEnd 0047ec40 (still unreconstructed).
- `work/reverse/ps_game.exe` has the full original decompilation/disassembly:
  CMobQuestData::Init 004c9ed0 builds positional quest links and guards the
  second target by the FIRST target's nonzero ID. AddEnd 004c9310 deduplicates
  quest IDs and sorts by minimum level, preserving ties. Identical targets still
  advance both counter slots during progress; the index contains the quest once.
  Original mob count comes from usp_Read_MobMaxID_R + 1, not a guessed constant.
- `work/mob-quest-links/ps_game.exe`: original Init/GetQuestLink/AddEnd evidence.
  `tools/mob_quest_oracle.py --sql-mobs work/mob-quest-sql-definitions.json`
  regenerates synthetic and full distribution fixtures. The ignored JSON is a
  read-only MobID/Level/HP snapshot; use original MobMaxID_R + 1 as its count.
- `work/mob-death-rewards/ps_game.exe`: ItemDrop 004a5ea0/004a65a0,
  ClearApplySkill 004a0990 and SendLogBothMob 004a1810, still missing providers.
- `work/mob-damage-ownership` / `work/damage-list-release`: constructor/init,
  destructor and real deletion loop evidence.

Existing boundaries: --serve supports login/roster/faction/creation, but selection
still does not enter the world. Live AI, combat dispatch, full death rewards,
obelisk ownership and production scheduling remain incomplete. Guild restored
schema Rank incompatibility remains documented; only disposable catalogs patched.

## Commands and local state

    python3 tools/dev.py build
    python3 tools/dev.py test
    python3 tools/dev.py integration

Use Eternity's isolated Docker SQL Server only. Never reset existing databases.
Integration uses disposable fixtures/rollback; let cleanup finish before new runs.
Credentials/data remain ignored in .local/work. Do not print runtime.env.

Ghidra 12.1.4 and JDK21 live under /home/franco/.local/share/castaway-tools.
Use tools/reverse.py --process, one Ghidra process at a time. Original-execution
fixtures use .local/oracle-env/bin/python. Generate fixtures under work first;
never replace tracked fixtures while running tests may read them.
Verify originals with sha256sum -c SHA256SUMS.

## Nine-terminal launcher — prepared 2026-11-03

Desktop entry points:

- /home/franco/Desktop/Iniciar-Eternity-9-terminales.sh
- /home/franco/Desktop/Eternity-9-terminales.desktop

Published launcher commit: 092ed85. Nine worktrees/branches are prepared under
/home/franco/Projects/Eternity-workers, with the exact prepared base recorded in .local/parallel/manifest.json. Preparation alone does not start agents. The user started the launcher; an initial
desktop PATH failure was fixed in fe410e8. The coordinator relaunched and verified
all nine workers running with distinct session IDs. Running the desktop script
again skips those live workers. Model is
gpt-6.1-sol, reasoning low, fast tier, danger-full-access and approval never.
Existing Codex authentication/config is inherited. No new API key is required.

Read PARALLEL_WORK.md for assignments. Only this coordinator integrates/pushes.
Worker 01..09 own damage, drops, skill ownership, obelisks, party state, quest
starts, quest request checks, mob targeting and mob FSM events respectively.
Core QuestEnd/Select, rewards/removal and mob logs remain coordinator work.

`python3 tools/parallel.py status` reports all nine workers. Prompts, reports,
thread IDs and logs are ignored under .local/parallel. Re-running launch skips
live and completed workers. File locks prevent duplicate worker sessions;
tools/dev.py limits builds/tests to two simultaneous commands and serializes
SQL integration, while tools/reverse.py serializes Ghidra across worktrees.
Credentials/assets and build/output directories are private to each worktree.
The original Ghidra project is shared only through the locked wrapper.

Launcher validation: four meaningful offline tests pass (nine-worktree
isolation/idempotence, actual CLI argv/settings/clean session environment with a
fake Codex process, desktop PATH resolution, and shared-resource lock exclusion/release). The desktop
shell script passes bash -n; all nine real worktrees were prepared successfully.
No model calls were used for launcher testing.

### Integrated coordinator contributions

The three temporary internal agents completed quest_rewards, quest_runtime and
mob_log implementations, original-execution fixtures and native tests. They are
registered in CMake and integrated. All 122 native tests pass (25.72 s) in
.local/parallel-final-tests.log; each new suite also passed standalone ASAN/UBSAN.
The reviewed build and three focused suites pass in
.local/parallel-reviewed-build.log and .local/parallel-reviewed-tests.log.

The first full SQL run (.local/parallel-contributions-integration.log) failed
with a login packet read timeout in creation-network-integration. Its disposable
catalog cleanup completed (zero leftover creation catalogs). The entire clean
rerun, .local/parallel-contributions-integration-retry.log, returned exit 0,
including network creation, all persistence checks and disposable guild cleanup.
No validation remains pending. Do not describe the first attempt as passed.

Quest rewards preserve live callback ordering and MoneyReplica delivery;
QuestRuntime preserves retained payload, cursor and pool reuse; mob logs expose
required map/name context and a death-log bridge. Actual QuestAddItem, core
QuestEnd/Select and production actor/world integration remain coordinator work.
See research/protocol.md for exact evidence and limitations.

### Live workers after desktop PATH fix

All nine external workers were started and have distinct thread IDs in each
.local/parallel/workers/<task>/status.json. Check those files and events.jsonl
before restarting anything. Worker branches started from 092ed85; main now also
contains 43835d2 (rewards/removal/logging) and fe410e8 (desktop Codex resolution).
Do not rebase/reset/change the live workers' checkouts. The coordinator owns
integration of their committed handoffs. The desktop launcher additionally
prepends /home/franco/.local/bin; the Python runner has absolute-path fallbacks.
Four offline launcher tests and the restricted-PATH real CLI --help probe pass.

## Continuous worker supervision — user authorized

The user explicitly requested that all nine workers continue across milestones,
adapting their work toward the complete goal, not stop after one fixed task.
The continuous supervisor is ACTIVE as a background Python process, with PID
and health recorded in .local/parallel/supervisor-process.json and
.local/parallel/supervisor-status.json. It does not add another model context.

- Existing thread IDs are reused through codex exec resume, preserving model
  gpt-6.1-sol, reasoning low, fast, full access and never approvals.
- Milestones are snapshotted before continuation under each worker's rounds/.
- New persistent terminal services reuse their window between blocks. Existing
  initial sessions upgrade after their current block; only a proven finished
  single-session owned window can be retired. No live work is interrupted.
- Workers select substantive missing behavior within their scope, report/commit,
  then continue. WAITING_FOR_ASSIGNMENT is only a concrete dependency marker;
  the coordinator must supply a fresh assignment, not leave it permanently idle.
- Root/coordinator notifications are queued to this same thread UUID, stored in
  manifest.coordinator_thread. The queue command attempts resume; an active
  writer may defer delivery. Inspect the recorded delivery result honestly.
- Use `python3 tools/parallel.py assign WORKER 'Next concrete task' --prefix PREFIX`
  for adaptive scope. Do not merge/reset a live worker checkout. Integrate the
  EXACT milestone commit from its snapshot, not an unchecked moving branch tip.
- Global control is .local/parallel/control.json. active=true and
  objective_complete=false. Successful round count is unbounded. Only verified
  original-client playability can justify marking the global objective complete.
- Persistent errors back off/notify instead of consuming model calls endlessly.
  `stop` stops scheduling after active blocks; the desktop launcher explicitly
  restarts scheduling when requested. Do not undo an explicit user stop.

Live evidence: worker 03-mob-skills completed a milestone and resumed the SAME
thread 01a1a406-7c12-7e20-a675-0eb18344f492 with codex exec resume and low reasoning;
its first milestone snapshot is available for root review/integration. Worker 09
has a queued next scope `mob_ai_states` after its initial event block: reconstruct
real compiled mob FSM ownership/transitions, prioritizing the death state.

Validation: seven launcher tests and ten supervisor tests pass; installed CLI
accepts the exact resume argument shape. Tests cover same-thread arguments,
persistent two-block services, duplicate-writer prevention, durable snapshots,
assignment waits/acknowledgments, pause/complete controls, bounded failures and
coordinator notification spooling. No goal completion has been claimed.

## Milestone 03 cf64bc8b — central validation pending

Reviewed snapshot .local/parallel/workers/03-mob-skills/rounds/cf64bc8b0f789050f24fcc3f.json.
Exact worker commits 0b9643a/e86a21b were cherry-picked locally as 2b705a6/7cd9619.
Do not reset them or merge the moving worker branch. The worker is already
reconstructing CMob::ChkEndTimeSkill 004a12f0 in its continuing context.

Central build/full native tests: running session 96398; logs
.local/mob-applied-integration-build.log and .local/mob-applied-integration-test.log.
Focused ASAN/UBSAN: running session 21968; log
.local/mob-applied-integration-sanitized.log. Both are waiting for the shared
build slots at this record. Do not call them passed or publish this milestone
before inspecting final results. No validation process was interrupted.

Integration still needs actual MobRuntime/MobPool ownership and death/reset
provider wiring, explicitly described in reports/parallel/03-mob-skills.md.
Do not put an unconditional clear into reset_mob_attack: Init retains skills.

## Milestone 01 1c527274 — reviewed, validation pending

Reviewed exact snapshot .local/parallel/workers/01-mob-damage/rounds/1c5272742c4b56474d5e75b9.json.
Applied commits 23a9179/9f13657 locally. Preserve their code and handoff report.
Central review captures the corpse delay BEFORE the clock callback, matching
00490d6a/00490f5c; worker's next callback fixture is also investigating this.
Do not publish until central checks complete. Existing shared build jobs for
milestone 03 remain authoritative; rebuild again after these combat additions
because prior queued build may observe changed CMake input. ASAN 03 log now
shows its focused test passed; inspect session 21968 final status.

The worker continues in the same context; do not modify its live checkout.
Damage/party credit projection still needs canonical MobRuntime/FSM/retarget/log
providers; tests are not production combat or a playable server.

## Milestone 04 a1a80441 — reviewed and applied locally

Exact commits 5696038/6dc509f applied as 7d46c9b/ac20a9d. Resolved CMake
registration conflict by preserving both combat and obelisk targets. No worker
checkout touched. New obelisk runtime has 432 original comparisons; actual map,
scheduler, transports and KC providers remain integration dependencies.
Central skills/combat build passed. Its 124-test suite (session 96398) excludes
this new obelisk target; inspect final exit and then run the 125-target build/test.
No additional canonical build was started concurrently. Pending validations must
not be described as passed, and accumulated local commits await central checks.

## Milestone 08 efe16e98 — targeting reviewed and applied locally

Applied exact 8a1d046/756aad8 from recorded snapshot. CMake conflict resolved by
preserving combat, obelisk and targeting registrations. Source implements
SetTarget/Ex with required help/FSM/random/tick providers and 4,096 original
comparisons. Canonical skill bindings and world/FSM integration remain pending.

Central obelisk build/test session 89902 was already active when this target was
added. Do not start a concurrent build in the same canonical directory. Inspect
its logs .local/obelisk-central-build.log and .local/obelisk-central-tests.log;
then run one new canonical configure/build/test for the final target set (126).
Do not assume the active older suite includes targeting or publish before checks.
Worker continues independently; never edit or reset its live checkout.

## Milestone 06 17f6a9a7 — admission/start applied locally

Reviewed and applied exact f10cfb7/0cd4530 as 138133e/5279858. Preserved all
existing CMake targets while resolving append conflict. 1,536 original traces
and 479 starts are worker-verified; real QuestAdd/inventory/start-log/world
providers remain pending. Worker continues; no live checkout modified.

Central obelisk suite completed: 125/125 passed (47.83 s), session 89902 exit 0.
This covers the prior applied skills, combat and obelisk milestone set, not the
new targeting/start targets. Start a single canonical build/test for 127 targets
in .local/quest-start-central-build.log / .local/quest-start-central-tests.log.
Accumulated local commits remain unpublished until this newer validation passes.

## Milestone 02 55e3da7d — loot applied locally

Reviewed exact ad3247b/f133a52 and applied locally, preserving all existing CMake
targets. Includes slot/catalog limits, quest/sequence/money loot, party delivery,
position and positional SQL loader. Required world/item/quest/guild providers
remain unconnected; no production no-op providers are accepted.

Current single canonical validation is session 70613, logs
.local/quest-start-central-build.log and .local/quest-start-central-tests.log.
If it configures after this addition it should include 130 tests; otherwise run
one subsequent configure/build/test after it ends. Do not start concurrent builds
in canonical build/. Then run build/mob-drops-sql-unit inside the existing
isolated container network/runtime env under resource_lock(ROOT,'sql'); it is
read-only and not ordinary CTest. Worker verified 24,723 rows/27,470 slots, but
central revalidation is still pending. Do not publish until central checks pass.

## Milestones 03 f76f8c30 / 09 2b44bdb5 — applied locally

The 03 delivery is shared CreateSkill IDs, NOT skill expiry (the worker changed
its selected next block). Applied 7ac2fe0/528b1eb as a22484b/0b29553: native
manager-wide atomic ID assignment, retained IDs and zero wrap, with 1,088
original operations and native concurrent-owner checks. Expiry still separate.
Applied exact ee5e0fa/50d8643 for eight CMob event handlers, Lua discriminator
and reset rules, 512 original cases. Real Lua/FSM ownership remains required.
CMake conflicts resolved by retaining every target. Worker 09 already has its
next mob_ai_states assignment; neither live checkout was modified.

Single canonical validation session 70613 was still waiting for build capacity
before these additions. The complete registered set is now 132 tests. Inspect
.local/quest-start-central-build.log and .local/quest-start-central-tests.log;
ensure final configure/build covers all 132 before publishing. Read-only mob
loot SQL check also remains pending. Do not start overlapping canonical builds.

## Milestone 08 97a94923 — live targeting bridge applied locally

Reviewed and applied exact dec69cb/0005635: MobRuntime overloads synchronize
mutable projection fields around random/tick/help/dispatch, retain scene-stack
transitions and preceding writes on exceptions. Required skill-binding resolver
and actual help/FSM providers remain pending. 4,096 original cases through the
bridge plus native callback/exception checks are worker-verified.

CMake registrations preserved during conflict resolution. Central validation
session 70613 had already configured/compiled the earlier 132-target set before
this addition. Do not claim it validates mob-targeting-runtime-unit. Wait for its
final exit, then one canonical configure/build/test must cover all 133 tests;
also run the pending read-only mob-drops-sql-unit under the SQL lock. No parallel
builds in canonical build/. Local commits remain unpublished pending checks.

## Milestone 01 a0c57290 — callback regression applied

Exact f009e09/d975e69 reviewed and applied locally. The worker independently
confirmed the same pre-clock corpse-delay fix already made centrally; resolved
that overlap by keeping one capture plus its source-address comment. Added 512
original callback cases, covering event/log mutations, triple reload and credit
ordering. All prior CMake registrations retained. No live worker checkout touched.

Central session 10886 is the single active canonical build/test, logs
.local/targeting-bridge-central-build.log and .local/targeting-bridge-central-tests.log.
It configured 133 tests BEFORE this new callback target, so it cannot validate
all latest code. Wait for final exit, then one new build/test must cover 134.
Read-only mob loot SQL already passed centrally: .local/mob-drops-central-sql.log
(24,723 rows, 2,747 definitions, zero rejected). Publish only after latest checks.
Worker 01 has its queued Aggro extension; no user permission needed to continue.

## CANONICAL VALIDATION FREEZE — current coordinator priority

133/133 central tests passed (41.17 s), session 10886 exit 0. Latest callback
suite makes 134. One final batch build/test/full isolated SQL integration is
starting in .local/frozen-batch-build.log, .local/frozen-batch-tests.log and
.local/frozen-batch-sql.log. Preserve this code snapshot until that pipeline ends.

DO NOT cherry-pick additional worker code or change CMake/canonical sources
while this frozen batch validates. Review new handoffs and append exact hashes /
snapshot paths to .local/parallel/integration-queue.json instead. Workers continue
in their own branches; adaptive assignments and metadata updates remain allowed.
When all frozen checks pass, commit continuity/docs and push the accumulated
verified main commits, then integrate the next reviewed queue batch. If a check
fails, diagnose/fix and revalidate; never publish an assumed pass.

The read-only mob-drop SQL check passed centrally (24,723 rows / 2,747 mobs /
zero rejected). Full isolated SQL rerun in this frozen pipeline is still pending.

## Reviewed integration queue: worker 05 dd410580

Reviewed ec70b40/1aa9715 (PartyRuntime and reward range, 1,508 original cases).
Recorded exact ordered commits and snapshot in .local/parallel/integration-queue.json.
NOT cherry-picked: frozen batch 18658 remains active and canonical source/CMake
must stay unchanged until its build/test/SQL pipeline ends. Worker 05 is already
running its adaptive membership assignment. Integrate this reviewed queue only
in the next validated batch; no live worker checkout changes or global progress
claim from these test counts.

## Reviewed integration queue: worker 07 65d0b8a4

Reviewed 60be7af/4769b36: QuestEndRequest dispatch and inventory requirement/consumption routines. Worker reports 1,792 original comparisons. Exact snapshot and ordered commits recorded in the ignored integration queue. Not integrated during frozen validation. Actual inventory/world/QuestEnd adapters remain required. Worker already has independent PC-kill/pick-item progress assignment.

## Reviewed integration queue: worker 04 3a932b8c

Reviewed afccb88/07262e3: UpdateRespawnObelisk periodic scheduler, shared terrain helper, manager allocation on periodic replenishment and live definition deadline after Init. Worker reports 2,304 retained original ticks and 121 passing tests. Exact commits/snapshot queued; canonical validation pending. No source changes during freeze. Map tick/reset/corpse integration remains open. All nine workers observed running.

## Reviewed integration queue: worker 09 a6474259

Reviewed 1c85307/018f544 compiled mob stack FSM: five states, 23 transitions, callback-before-stack ordering, empty death polling, and explicit update/diagnostic providers. Worker reports 121 passing tests and original dispatch/registration/initial-state comparisons. Exact snapshot/commits queued, not integrated during freeze. Death rewards remain external after Damage, not automatic on state entry. Other state bodies, Lua ownership and production routing remain open.

## Reviewed integration queue: worker 06 041a1ade

Reviewed 6c607bf/eb6b84f quest-start 144-byte 0501 serialization and wrapping local-calendar time packing. Worker reports 256 complete original comparisons and 121 passing tests. Exact commits/snapshot queued during source freeze. Actual calendar acquisition/log transport remain required; active QuestAdd assignment already supersedes this older waiting handoff. Central frozen tests have acquired their slot and were observed running (23/134 completed), SQL not yet started.

## Reviewed integration queue: worker 08 ec0b735e

Reviewed 10b54f0/09912be three targeting-help paths, cached range/live source reads and float/double distance ordering. Reported 1,536 original cases and 122 passing worker tests. Exact snapshot and commits queued during freeze. Multi-candidate removal/reentrancy, locked live world cursor, canonical fields and SetTarget providers remain required. Frozen central suite observed at 108/134 passing; final result and SQL still pending.

## Reviewed integration queue: worker 02 1b5c23c4

Reviewed 3b1e120/f2d8376 catalog population and original limit initialization. Reported full original/SQL match: 16,047 item definitions, 15,505 members; 123 worker tests. Exact snapshot/commits queued during freeze. Sequence configuration and production catalog/loot providers remain open. Central frozen build and all 134 tests passed (48.11 seconds); full isolated SQL integration is running, not yet claimed passed.

## Reviewed integration queue: worker 05 0fba35db

Reviewed f34712a/bd89213 LevelUp party composition evidence, 256 original slice sequences and 121 reported worker tests. Queue after initial PartyRuntime milestone; no production owner claimed. Existing normal membership assignment supersedes this older waiting report. Central SQL progressing through account/game TCP and rollback contracts, still not complete. Frozen source remains unchanged.

## Reviewed integration queue: worker 07 a491c039

Reviewed 6ea9257/e30fbed complete request/item/replica/pool composition evidence: reported 512 original flows, 209 completions, 680 releases and 122 worker tests. Queue after initial requests implementation. Coordinator must expose serialized retained Actor item access, count synchronization and direct release; do not use Actor::remove_item (duplicate transport/log/exchange effects). Multi-item whole-stack release preserves old count at release. Independent PC-kill/item-pick progress assignment already supersedes older waiting report. Central isolated SQL still running; no source changes during freeze.

## Reviewed integration queue: worker 03 d16a557c

Reviewed 8053594/317a9c4 expiry and shared cursor with recursive clear: unlink precedes required effect reversal and recycling follows it. Worker reports 128 scenarios/1,600 operations and 122 passing tests after fixing an oracle translated-code cache issue; initial failing fixture is not counted as success. Exact commits/snapshot queued during freeze. Actual RemApplySkill effects, canonical ownership and scheduler remain open. Central SQL has progressed through TCP creation, player loading and core/inventory/skill saves; final completion pending.

## Reviewed integration queue: worker 01 88ab6f92

Reviewed 0a2c753/e68ce1c user hit composition: explicit secondary party credit amount, membership reload, stop-before-death and user-ID reload. Worker reports 768 original caller slices and 122 passing tests after rebuilding a stale object; initial failed run is not a pass. Exact snapshot/commits queued during freeze. Existing Aggro assignment supersedes this older waiting report. Central SQL has passed complete character save checks but pipeline exit remains pending.

## Frozen batch completed successfully — 2026-11-04 UTC

Session 18658 exited 0: canonical build, 134/134 native tests (48.11 seconds), and complete isolated SQL integration all passed for frozen source HEAD be9797bc61c45a68ae140619e5e6c33ad0befc7a. Logs: .local/frozen-batch-build.log, .local/frozen-batch-tests.log, .local/frozen-batch-sql.log. Final SQL output includes complete character saves and guild procedures in a disposable catalog. Freeze released after successful pipeline completion. Publish this verified batch before integrating queued worker milestones. Queue remains pending; these later reports are not included in the 134-test validation. Production world/gameplay integration remains incomplete and server is not playable.

## Next reviewed batch integrated locally — validation freeze

All twelve queued reviewed milestones integrated in dependency order through d54ad6e238584b75e3550aa4257eb0e31714cefb. Append-only CMake conflicts resolved preserving both module/test blocks; other changes merged normally. Queue entries now integrated-awaiting-validation. Do not modify canonical code or cherry-pick while the new build/test/full isolated SQL pipeline runs. Logs: .local/reviewed-batch-build.log, .local/reviewed-batch-tests.log, .local/reviewed-batch-sql.log. Prior 134-test published result does not validate this new batch. Review incoming handoffs into a new pending queue only. Production providers/gameplay remain incomplete.

## Reviewed integration queue: worker 08 aba4b14a

Reviewed 9b49604/bc60231 cursor composition evidence: reported 450 original mutation traces and 123 worker tests. Test-only send_leave probe exercises real ZoneMobs shared cursor, not a production adapter. Pending canonical visit_cell_mobs must reuse shared cursor, permit nested lookup/removal/append, avoid restoring cursor or dereferencing actor after callback, and retain zone/cell serialization. Exact snapshot/commits queued for next batch. Current frozen pipeline session 25307 uses .local/reviewed-batch-* logs and is waiting for build capacity; do not integrate this handoff yet.

## Reviewed integration queue: worker 04 4861ad57

Reviewed 0c4b246/f7bc639 ObeliskReset: zone guard before single clock sample, reset flag around actual removal, captured ten-minute deadline, live base country, unconditional notification/persistence and final relink. Reported 648 original scenarios/404 nested destroys and 122 passing tests after oracle trampoline/cache correction; initial failed run not counted as pass. Exact snapshot/commits queued while session 25307 waits for build capacity. Real zone unlink/corpse accounting/configuration/world dispatch remain required.

## Reviewed integration queue: worker 02 d35c8847

Reviewed 52ab268/eace862 sequence init/reload partial state and retained inactive groups; grade values and selection now preserve 32 bits. Reported 256 complete original traces, six wide-grade lookups, original INI and read-only SQL comparison, 124 worker tests. Parser only claims supported unique decimal unquoted INI syntax. Publish configuration after every attempt; preserve backing groups across failures/reloads. Exact snapshot/commits queued during current source freeze. Session 25307 acquired build capacity and was observed at action 38/188; tests/SQL pending. Production loot providers remain required.

## Reviewed integration queue: worker 06 c937d1ff

Reviewed 0e2cb1b/693916e QuestAdd allocation/init/timers/append/accounting/DB ordering. Reported 512 original cases, 438 successful adds with real QuestReplica packets, 122 worker tests. Canonical adapter needs add_active_unaccounted: append/claim only, then live end-type counters increment outside insertion boundary. Existing add_active must retain accounting for other callers; do not double count or compensate after insertion. Exact snapshot/commits queued during freeze. Current central build passed and 147-test suite is running (10 completed when observed); SQL pending. Worker continues request-to-real-add composition independently.

## Reviewed integration queue: worker 03 bc3dc6bd

Reviewed 79f0cc8/fd97f8f non-damage RemApplySkill and SetSkillAbility: wrapped counters, immediate recognized callbacks and live later-slot reads; defense/evasion mapping checked. Reported 1,282 original cases and 123 worker tests with native expiration/attack composition. Damage details 0x3d/0x3f remain mandatory external provider, not reconstructed bodies. Base attribute must come from immutable template. Exact snapshot/commits queued during source freeze. Central 147-test suite observed through test 23 passing; SQL pending.

## Reviewed integration queue: worker 08 fac70d3f

Reviewed 667e37a/f28f799 Targeting/FindTarget first eligible user in cell order, live range, no country gate, SetTarget(true) before unlock and selected identity returned even if SetTarget rejects. Reported 2,048 original comparisons and 124 worker tests. Exact commits/snapshot queued during freeze. Requires early-stop shared user cursor visitor, actor lifetime and actual runtime providers. Central tests observed through 142/147 passing; final test result and SQL pending.

## Worker 08 dependency checkpoint 440c627b and reassignment

Reviewed d761dfa documentation-only checkpoint; queued exact snapshot/commit without claiming new tests. Assigned mob_targeting_bindings recovery for country/chase-range/three attack skills and enabled/cooldown projections, original loader/Init evidence and minimal canonical patch contract. Shared headers remain coordinator-owned during freeze. Central batch build and 147/147 tests passed (47.39 seconds); isolated SQL pipeline session 25307 now running, final exit pending. Do not publish or integrate new code until full pipeline passes.

## Reviewed integration queue: worker 04 a1b1044a

Reviewed 71b37a9/12e4def InitObelisk: startup principals and companions use raw allocation plus ID/seed preparation, without spawn log; complete respawn keeps manager principal/log path. Reported 768 startup scenarios/2,304 retained ticks and 123 worker tests. Exact commits/snapshot queued after reset. Correction: account_obelisk_mob_corpse and ZoneMobs forced removal already exist; bind them instead of duplicating. Canonical MobPool raw allocation/preparation split needed to prevent double IDs/seeding. Configuration/world ownership remain open. Central SQL still running through roster/rollback checks; canonical source freeze unchanged.

## Reviewed integration queue: worker 09 bcd8c46f

Reviewed c20b2e6/04c5139 WhileIdle tick/target cooldown, return-home gates, wandering arithmetic and retry ordering. Reported 1,536 original traces with exact float destinations and 123 worker tests. Exact snapshot/commits queued during freeze. Required template additions: normal_time +5c, normal_step +60, movement speed +68, definition obelisk +7c; chase/zone inputs must stay live. World target/random/map/move providers and remaining movement states remain open. SQL central still running, through TCP creation when observed.

## Reviewed integration queue: worker 01 b435ad7d

Reviewed ba820ce/e6ab5b1 Aggro and direct MobRuntime ledger bridge. Reported 4,096 comparisons across projection/runtime and 123 worker tests. Positive signed guard, status/HP checks, zero-damage user credit clearing party, post-allocation live status/retarget ordering retained; no new target/user gate. Exact snapshot/commits queued during freeze. Assigned next StatusAttackStop 00489ed0 under mob_combat_stop; shared Actor/world owners remain coordinator work. Central isolated SQL still running through quest persistence checks, final exit pending.

## Reviewed batch validated — 147 tests and complete SQL

Session 25307 exited 0: build, 147/147 tests (47.39 seconds), complete isolated SQL including character save and disposable guild catalog checks. Validated source HEAD d54ad6e; logs .local/reviewed-batch-{build,tests,sql}.log. Freeze released; publish verified batch before further integration. Queue statuses distinguish validated twelve milestones from later pending deliveries.

Reviewed/queued worker 07 b666163d (3408c78/e9ac195) PC-kill/item-pick progress: 1,024 reported original cases, 123 tests. Canonical retained quest cursor/event dispatch and Actor inventory dependency remain open. Reviewed/queued worker 06 24bc60f0 (874b9db/ed577f4) real-QuestAdd flow evidence: 640 cases/253 starts/16 consumed-item failures, 123 tests after fixture empty-event fix. Neither new delivery is covered by this batch validation. Unaccounted insertion and real inventory/log/transport adapters remain coordinator work. Server playability incomplete.

## Next batch locally integrated through normal party membership

Reviewed ef3c489/5416e91 normal party entry/recursive leave: 512 reported original comparisons, 122 worker tests and focused ASAN/UBSAN. Registry must retain party/user lifetime through nested leave; required callbacks must not throw/invalidate projections. Integrated pending queue in dependency order through 4464cb0d60d268991dd7029ab36a0cdb4d3e579f; CMake append conflicts retained both blocks. These new changes are not covered by published c724905.

NEW CANONICAL SOURCE FREEZE: build/test/full isolated SQL pipeline starting with logs .local/next-batch-build.log, .local/next-batch-tests.log, .local/next-batch-sql.log. Do not change canonical sources/CMake or cherry-pick during this validation. Review new handoffs into pending queue only. Once full pipeline passes publish verified batch, then prioritize canonical owner interfaces (unaccounted QuestRuntime insertion, Actor retained inventory access, shared world visitors, pool allocation/prepare split) to unblock actual production composition. Production gameplay remains incomplete.

## Reviewed integration queue: worker 04 0db8f0cd

Reviewed 9900269/6382ef4 obelisk publication: 2102 uses full local ID (not base/index), 0207 shared serializer and 0206 list chunking at 100 with live future reads after callbacks. Reported 216 original cases/10,290 list records, 124 worker tests. Exact snapshot/commits queued during freeze. Real transports, configuration, portal/KC and pool bindings remain required. Current canonical pipeline session 36178 logs .local/next-batch-*; build observed at 11/196, tests/SQL pending. Do not change source while it runs.

## Reviewed integration queue: worker 03 633f4c01

Reviewed b074bab/22ecbdb final skill damage coordinator and 15-byte 0607 result. Reported 1,024 original traces and 124 worker tests with recursive expiration composition. Captured scalar HP, live tick/previous-tick, fixed naming decision, death attacker reload and live final zone gate retained. Actual TimeDamage/combat/death/view providers remain required; current node recycled only by caller after delivery. Exact snapshot/commits queued during freeze. Central build session 36178 observed at 119/196 actions; tests/SQL pending.

## Reviewed integration queue: worker 02 8dbc0204

Reviewed 058df95/57429e9 pooled drop item/money creation and craft stat selection. Reported 768 craft/512 item/128 money original paths, 125 worker tests and exact read-only SQL craft comparison over 16,047 definitions. Initial allocator-trampoline fixture failure corrected, not counted as pass. Exact snapshot/commits queued during freeze. Same shared RNG/UID owner required; preserve retained money UID/craft tails and callback-driven live coordinates. Real inventory/zone transfer/release/range/log remain open. Current central build passed; 159-test suite running (3 completed when observed), SQL pending.

## Reviewed integration queue: worker 05 84f7932f

Reviewed d69cb64/7c187da GetPartyType and real default membership provider: retained/current party capture, recursive locks and reverse unlock ordering. Reported 1,024 original classifications plus 512 real-classification entry/leave flows, 124 worker tests and focused ASAN/UBSAN. Exact snapshot/commits queued during freeze. Captured parties/selected slots require stable lifetime; registry/guild/transport wiring remains open. Central suite observed through 49/159 passing; final tests and SQL pending.

## Reviewed integration queue: worker 04 d4952e2f

Reviewed e6f7850/39c3987 persisted ownership and portal restore: exclusive portal ID flag, first-match lookup, raw country nibble decoding and capped 24-hour remainder without changing respawn state. Reported 512 original cases and 125 worker tests. Packed calendar helpers remain required. Important ownership correction from MapInit 0041d010: attach original map obelisk records only to primary create-type-zero zone; do not duplicate independent owners per instance. Exact snapshot/commits queued during freeze. Central build and 159/159 tests passed (44.21 seconds); SQL session 36178 running, final exit pending.

## Reviewed integration queue: worker 09 51c7254f

Reviewed 8eccb5d/395750c WhileReturnHome with actual map/A*/trail/damage owners, arrival/reset ordering and movement/cursor deadlines. Reported 1,536 original traces and 124 worker tests. Exact snapshot/commits queued during freeze. Important definition correction: +68 is chase_step (idle movement gate), with chase_time +64 and size +3f. Out-of-map EnableLine leaves original output scratch uninitialized; native explicitly rejects this unsupported invalid-world path. Actual ResetAttack/move/dispatch providers and WhileMove remain required. Central isolated SQL session 36178 still running through TCP creation; no final pass yet.

## Batch validated — 159 tests and complete SQL

Session 36178 exited 0: native build, 159/159 tests (44.21 seconds), full isolated SQL including complete character save and disposable guild checks. Source HEAD 4464cb0; logs .local/next-batch-{build,tests,sql}.log. Freeze released; publish before further source integration. Queue statuses mark validated milestones separately from pending newer deliveries.

Reviewed/queued worker 01 43249dfc (4f6037a/e3f5db3) StatusAttackStop delegating existing Actor: 1,024 reported original comparisons, 124 tests. No extra packets, retained target ID preserved; actual combat/Death dispatch remains required. Reviewed/queued worker 07 a3bc79ca (7669d2c/4eabd0a) UpdateQuest: 512 reported original cycles/609 expirations, 124 tests. Live definition maximum-time guard and shared cursor preserved; actual QuestEnd failure and scheduler remain required. Neither is included in the completed 159-test batch.

Next priority: canonical owner interface changes and production adapters, rather than accumulating additional disconnected projections. Server remains unplayable; objective open.

## Reviewed integration queue: worker 03 c2e257d2

Reviewed e51bed4/19d8371 complete TimeDamage arithmetic/default CRT precision: 12,544 reported original cases and 125 worker tests including real periodic-removal composition. Preserve signed wrapped negative results and only signed upper cap 30000; no unsigned/saturating normalization. Exact snapshot/commits queued. Skill definition fields and scheduler/combat wiring remain required. Prior batch is published cab0570; no validation pipeline active. Next coordinator step remains canonical unaccounted QuestRuntime insertion and production adapters before freezing another batch. Worker 03 proposes ChkKeepSkill next, within existing applied-skill scope.

## Reviewed integration queue: worker 08 3737d30c

Reviewed 4ab7f85/981e195 positional targeting catalog: distinct stable mob-skill storage, country/chase range and three attack bindings, raw enabled and nullable skill distinctions. Reported 256 complete original loads, 2,048 skill slots/1,024 definitions and 125 worker tests. Exact commits/snapshot queued. Restored SQL distribution comparison remains pending; catalog owner must outlive mob templates/runtime and callbacks. Do not substitute user SkillDefinition map or snapshot live cooldown permanently. Original one-past lookup remains unsupported safely. No central pipeline active; prioritize canonical owner interfaces and integrate pending batch with restored SQL comparison before production resolver publication.

## Reviewed integration queue: worker 02 c3ad7a3c

Reviewed b2f12a9/6fc9192 zone/item/money range gates and unordered NaN acceptance correction. Reported 2,048 original spatial comparisons, 1,024 real-range delivery traces, 127 worker tests and read-only SQL craft regression. Exact snapshot/commits queued. MobLootUser.map means stable live zone identity, not numeric map ID; do not truncate pointers. Money gate intentionally lacks credited-state/mob-zone checks; item range differs from zone-only gate. Actual inventory/grants/logging/world routing remain open. Latest central validated publication cab0570 (159 tests/full SQL); no new central pipeline active.

## Reviewed integration queue: worker 06 e35dd12c

Reviewed 5d9e55c/54418ec both QuestAddItem overloads reusing native pool/init/craft/DB/client serializers. Reported 384 original cases/152 grants and 124 worker tests. Exact commits/snapshot queued. Actor requires raw already-allocated retained-item claim and mirror sync before DB callback; receive_item/load_inventory_item would add wrong effects/allocation. Important shared UID discrepancy: current ItemIds uses 64-bit deadline/zero reset/overflow throw; original uses DWORD wrap/global baseline/low-16 counter. Reconcile shared owner rather than create quest-only IDs. Make-time precedes separately observed UID calendar refresh. Worker continues three-start-grant composition. Latest published central validation remains cab0570; no new pipeline active.

## Fourth batch integrated, canonical QuestAdd admission interface added

Reviewed b60fa26/64046e1 WhileMove (1,536 reported original comparisons, 125 worker tests); integrated pending queue through f9e169c. All fixed stack state update bodies recovered; actual world/Lua polling remains unconnected. Added canonical QuestRuntime::add_active_unaccounted in e24398d; existing add_active retains accounting via this primitive. Existing ownership suite now checks append without accounting and duplicate claim rejection. This unblocks real QuestAdd adapters without double increments.

NEW SOURCE FREEZE at e24398d555e6dea27bb1c9a53967b0a29f9724b8: pipeline build/test/full isolated SQL logs .local/fourth-batch-{build,tests,sql}.log. Do not cherry-pick or modify canonical sources during validation; review handoffs into pending queue. Restored targeting binding SQL comparison remains an additional required check before production resolver activation. Latest published validated batch remains cab0570. No playable-server claim.

## Reviewed integration queue: worker 05 83922fcd

Reviewed a06d652/47ac40f CParty::Send framing, size classes, shared atomic references and live recipient traversal. Reported 1,024 send/512 complete membership comparisons, 126 worker tests and focused ASAN/UBSAN. Exact snapshot/commits queued during freeze. Real pools/connections/worker completion must retain one reference per send and free only at zero; socket projection is not a native descriptor default. Current pipeline session 40893 uses .local/fourth-batch-* logs, observed at build action 19/219; tests/SQL pending. No canonical source changes during validation.

## Reviewed integration queue: worker 08 9858eb38

Reviewed 96af8bc/9a0bdd4 read-only SQL/original/native targeting comparison runner, serialized SQL lock and ignored numeric snapshots. Reported exact 355 skills/2,746 nonzero definitions/8,238 attack positions, including 20 present-but-disabled pointers. Central reproduction after integration remains required. First worker suite failed login session closure assertion; clean full rerun passed 125/125. Suspected close-before-session-release race is an inference, not proven; investigate actual net owner ordering, do not mask failed run. Exact snapshot/commits queued during freeze. Current batch build and 174/174 tests passed (45.48 seconds); session 40893 full SQL pending/running, no final exit claimed.

## Fourth batch passed — 174 tests and complete isolated SQL

Session 40893 exited 0: build, 174/174 tests (45.48 seconds), complete isolated SQL including character save and final disposable guild catalog checks. Validated source e24398d includes unaccounted QuestRuntime insertion and ownership assertions. Logs .local/fourth-batch-{build,tests,sql}.log. Source freeze released; publish before further edits. Queue marks validated entries separately from newer pending handoffs. Restored targeting binding comparison tooling 96af8bc remains pending integration and central reproduction.

Reviewed worker 08 documentation checkpoint 8f3faf1 (96829c23); queued exact snapshot without new test claims. Explicitly assigned ReLoadConstMobSkill 004c50c0/ReLoadConstMob 004c20c0 under mob_targeting_reload, stable in-place pointers and partial-failure publication. Next canonical work: actual owner adapters and shared inventory/visitor/UID interfaces. Server objective open, production gameplay incomplete.

## Reviewed integration queue: worker 07 2a7fbb54

Reviewed f40c155/f1a2c98 QuestEnd/Select with canonical reward/removal composition: failure client/remove/log ordering, wrapped minimum gate, pending selection, descending variants and inventory query. Reported 1,536 original paths and 125 tests after undefined log tails normalized; initial failed test not counted as pass. Exact snapshot/commits queued. Central build must link canonical owners directly, not private worktree snapshot fallback. Captured definition and retained/pool-reused quest payload must survive all callbacks. Actor direct inventory ownership/count/release plus real rewards/transport/log adapters remain integration blockers. Last validated central publication 06f2b75 (174 tests/full SQL), no pipeline active.

## Reviewed handoffs: skills 01f0edba and start grants 4b676918

Reviewed/queued 3d8b480/5906478 Keep/Sleep actual list owner: reported 640 original traces, 126 tests. Shared cursor/status, real TimeDamage, view under lock and sleep reentry after unlock preserved; production scheduler/speed remains pending. Reviewed/queued 68a3fca/78e2c4e persistent three-start-grant evidence: 240 sequences/384 items/24 middle failure recoveries, 125 tests. QuestRuntime unaccounted insertion dependency resolved in e24398d; Actor retained-item claim/access/mirror/release and shared UID remain next canonical work. Exact snapshots/ordered commits retained in integration queue. Latest published validated source 06f2b75; no pipeline active.

## Reviewed integration queue: worker 05 5d9301d7

Reviewed df2e00b/8003285 direct user/roster send: initial buffer reference transferred to worker, no broadcast increment/release; disconnect gate before allocation and no later socket recheck. Reported 1,024 original direct/512 complete membership comparisons, 128 tests and focused ASAN/UBSAN. Original unsafe 2047/2048 copy is explicitly rejected safely, not emulated; >2048 error/close ordering recovered. Exact snapshot/commits queued. Assigned party_runtime_registry creation/Init/manager lifetime next. Actual connection/pool/worker services remain required; no central pipeline active, last publication 06f2b75.

## Reviewed handoffs: combat 4ecd6a35 and Lua lifecycle 8fd05a71

Reviewed b0f4a04/d8853b4 normal mob attack polling on real Actor: 1,536 reported comparisons,125 tests. Resume gate, transform/weapon checks, naming context, post-hit status and final deadline/cancel/motion ordering reviewed; real skill/PvP/world providers remain explicit. Worker continues StatusNextAttack within scheduler scope. Reviewed 121cf33/5a22b4d Lua/FSM lifecycle:768 reported comparisons,126 tests with actual MobPool release. VM backend remains required; default MobPool destruction lacks retained VM/stack cleanup, needs canonical exactly-once destructor hook with service lifetime. Avoid calling whole release wrapper inside pool release_stack_ai (duplicate flags/close). Exact snapshots/commits queued. Latest validated publication 06f2b75; no pipeline active.

## Reviewed integration queue: worker 04 90c70e9c

Reviewed 16f1e66/cd5586a local calendar pack/unpack and SubTimeSecond binary64/truncation arithmetic plus ownership clock adapter. Reported 2,005 original comparisons/126 tests; OLE import responses captured from Wine11.20, not Microsoft DLL equivalence. Actual local-calendar/OLE platform services remain mandatory. Preserve second-operand-first conversion and low DWORD of signed truncation; generic rounded civil seconds would differ. Exact snapshot/commits queued. Initial regeneration polluted by lock stdout corrected, not claimed passed. Production configuration/calendar/world dispatch remains open. Last validated publication 06f2b75; no central pipeline active.

## Fifth reviewed batch integrated — validation freeze

Reviewed ec6ea4c/495eced StatusNextAttack: 2,048 reported original cases,126 worker tests; live self-send reentry/phase/backup targets retained. Integrated pending queue through e41707b04b1cfdaa2cc825136bfd117c1e6da891, resolving CMake append conflicts with both complete blocks preserved. Completion test uses canonical owners when present; snapshot fallback remains for old worker bases only.

NEW SOURCE FREEZE: pipeline build/test/full isolated SQL then read-only targeting restored SQL/original comparison. Logs .local/fifth-batch-build.log, .local/fifth-batch-tests.log, .local/fifth-batch-sql.log, .local/fifth-batch-bindings-sql.log. Do not modify canonical sources or cherry-pick while active. Review new handoffs into pending queue. Publish only after all checks pass; latest published validated source remains 06f2b75. Actual inventory/UID/visitor/Lua/world adapters remain incomplete; objective open.

## Reviewed integration queue: worker 02 4a134709

Reviewed 050179d/cc67d6e real pooled drop delivery bridge: canonical definition identity/recipient validation, actual Actor::receive_item and ZoneItems entry, sequence make V and required log/transport services. Reported 512 original paths,128 tests and read-only SQL regression. Initial merged-stack assertion expected wrong 0702; corrected to established 0707 contract, initial run not passed. Exact snapshot/commits queued during source freeze. Death dispatch/shared RNG/UID/live party and world sinks remain required. Pipeline session 28976 .local/fifth-batch-* currently waiting for build capacity; no source mutations while active. Latest validated publication remains 06f2b75.

## Reviewed integration queue: worker 09 9972ace7

Reviewed 295840f/e6a3c41 configured CKFSM Start/Execute/Evaluate: programs before callbacks, live counts/action reload, wrapped full-result sums without short-circuit and captured transition publication after exit/enter. Reported 768 original comparisons/127 worker tests. Exact snapshot/commits queued during freeze. TravelCondition/token mapping, factory/assets/function registry remain required; configured mainFSM must be owned separately from fixed state_stack. Active ranges/state lifetime must survive callbacks. Central build completed224 actions; session28976 tests/full SQL/restored binding SQL still pending. Do not mutate canonical sources while pipeline active.

## Reviewed integration queue: worker 03 938abe74

Reviewed 5c91dc3/ee9f0cd applied-skill admission/refresh/replacement/debuff on retained owner: equal-level refresh preserves definition/effects, replacement detaches/reverses/reuses ID before relink. Reported1,536 original operations/127 worker tests. Exact snapshot/commits queued during freeze. Full stable mob-skill catalog (name/keep/abilities/time damage) remains required, distinct from user skills; damage-producing replacement reentrancy not covered by this fixture. SendSpeed middle bytes remain unresolved, no guessed ID. Current central build and185/185 tests passed(41.95s); session28976 full SQL and restored binding comparison still pending/running. No source edits during active validation.

## Fifth batch validated — 185 tests, full SQL and restored bindings

Session28976 exited0: build,185/185 tests(41.95s), complete isolated SQL and read-only restored SQL/original/native targeting comparison. Source e41707b. Logs .local/fifth-batch-{build,tests,sql,bindings-sql}.log. Binding result:355 skill rows/356 slots,2747 mob rows/slots,2746 nonzero definitions,1871 links including20 disabled; all matched original execution. Freeze released; publish verified batch before new edits.

Reviewed/queued 87abafd/a99fd79 live reload:432 reported original traces,126 tests and focused ASAN/UBSAN. Typed projections preserve existing pointers; actual owner needs mutable allocated-slot APIs/full normalized bytes under quiescence, real R2 SQL/pool/log providers. Do not const_cast const owner or replace arrays; do not clear existing skill links merely because a skill reload changes effect. Exact snapshot retained. New reload and other pending queue entries are not covered by this completed batch. Production owner/connection/inventory/UID/Lua/world adapters remain incomplete; server objective open.

## Worker08 checkpoint852331dd and stable full mob-skill assignment

Reviewed/queued f2f9c68 documentation-only checkpoint; no new test claims. Earlier fifth freeze has completed and publication23a66ca validated185 tests/full SQL/restored bindings. Explicit expanded assignment now authorizes worker08 to extend its existing mob_targeting_bindings owner in its worktree: full name/keep/ability/time-damage fields, stable per-slot compatible views, owner-controlled mutable allocated slots and complete normalized records where evidenced. No second user-skill map; shared mobs.hpp/skills.hpp remain coordinator-owned. Preserve lifetime/reload pointer identity, compare full original loader and restored SQL under shared lock. This resolves a concrete applied-skills catalog dependency rather than repeating completed reload tests. Canonical Actor retained inventory/shared UID/visitors still pending.

## Reviewed integration queue: worker01 8008757b

Reviewed47c290e/a56b483 skill/casting dispatch on existing Actor: initial target eligibility then live selection reload, context lifetime, mode3 continuation gate and casting-only deadline ordering. Reported2048 original comparisons/127 tests. Exact snapshot/commits queued. Bind actual retained active user CSkill/definition views; mob-AI or passive skill maps do not substitute. Real skill action bodies/world providers remain required; disappeared live selection fails explicitly. Worker continues normal PvP within scheduler scope. Latest central validated publication23a66ca(185tests/fullSQL/restored bindings); no pipeline active.

## Reviewed integration queue: worker04 3b51c632

Reviewed df1a39c/96e6ddb forced obelisk removal via actual ZoneMobs::remove_by_id/MobPool and MobRemovalOwners. Reported256 retained original sequences,768 recycling callbacks,512 reuses and127 tests. Exact snapshot/commits queued. Bind stable primary-map records/source pointers and linked owners; world callback delegates without extra decrement/recycle. Combat Death retains separate country/reset semantics. One-past original index rejected explicitly. Raw allocation/prepare split remains separate startup dependency; production configuration/world/transport remains open. Last validated publication23a66ca; no central pipeline active.

## Reviewed integration queue: worker09 c675b3ac

Reviewed55a91cd/029bf89 typed retained local variables and configured ClearVariable/ResetFSM/onResetUser composition. Reported832 original comparisons/128 tests. Exact snapshot/commits queued. Constructor preserves type/text, clear follows current type and duplicate IDs; missing original map type/text are uninitialized, so factory-populated locals required and missing entries rejected explicitly. Retain variables with configured FSM separate from fixed stack. Factory/catalog/interpreter/world scheduling remain open. Latest validated publication23a66ca; no pipeline active.

## Reviewed integration queue: worker05 d5b54a00

Reviewedf472fc0/0c9e43f party registry creation/lookup/removal/shutdown and recursive membership composition. Reported64 retained lifetimes/1248 original calls,129 tests on clean retry,ASAN/UBSAN. Prior world-network close timeout failed128/129, cause unproven; not counted as success. Exact snapshot/commits queued. Critical: unregister only erases current ID key; does not delete/reset/recycle, even for duplicate-ID mapping. Borrowed party must survive recursive callbacks. Shutdown frees only available chain; producer returning parties remains unrecovered, no guessed recycling. Actual lock/allocation/connection/guild/world owners remain required. Latest central publication23a66ca; no pipeline active.

## Reviewed integration queue: worker01 b284299f

Reviewed942357c/f9a49bf normal PvP scheduler on actual Actor/same_team: directional team check, captured lookup target, range-before-attack and live post-action state/deadline tail. Reported2048 original comparisons/128 tests. Exact snapshot/commits queued. Retain source/target across nested callbacks; real lookup bound to zone/instance, range/attack and connection providers required. Worker continues GetAttackSpeed0048a460. Last validated publication23a66ca; pending queue not yet centrally validated.

## Sixth reviewed batch locally integrated — source freeze

Integrated pending milestones through b19f469ae285bda44b0e0526a90f1b7c2409601a, including normal PvP scheduler, stable reload, applied-skill admission, real drop/obelisk owner bridges, configured FSM variables and party registry. CMake append conflicts preserved both module/test blocks. Latest published validated source remains23a66ca.

NEW SOURCE FREEZE: native build/test/full isolated SQL and restored binding comparison running in .local/sixth-batch-{build,tests,sql,bindings-sql}.log. Do not change canonical sources/CMake or cherry-pick while active. Review new handoffs into pending queue. Publish only after all checks pass. After validation prioritize Actor retained inventory/shared UID and real data/world owner adapters; disconnected reconstruction alone does not achieve original-client playability.

## Reviewed integration queue: worker02 301c06d5

Reviewed940a1f9/c4ef600 selective MobItemsR2 reload/in-place grade/rate counter updates and live slot access. Reported512 original cases,129 worker tests,5 read-only SQL requests/45rows. Exact snapshot/commits queued during freeze. Important limit: buffered SqlServer query errors before row publication; original mid-fetch partial publication needs canonical typed streaming Row/End/Error cursor. No streaming equivalence claimed. Retained definitions must not resize; UID/death/world wiring remains pending. Current central build passed and194-test suite observed through192 passing; session34814 SQL/bindings checks pending. No source edits while active.

## Worker02 checkpoint73fe2c08 — exclusive shared UID owner task

Reviewed/queued5652e56 documentation-only dependency checkpoint, no new execution claims. Explicitly assigned existing ItemIds correction to original shared GenID under mob_drops_uid. Worker02 exclusively owns item_ids.hpp/cpp and their tests in its worktree until delivery; coordinator/others must not concurrently edit those paths. No second quest/drop generator. Recover original baseline/counter/DWORD deadline/local-calendar/server semantics and migrate existing callers with complete original/interleaved tests. Main source freeze remains active; no main edits authorized. Central194/194 tests passed(54.82s); session34814 fullSQL/restored bindings running. Actor retained inventory/world visitors remain coordinator priorities after validation.

## Reviewed worker09 f9e416e7; sixth SQL retry

Reviewed5ff40b2/1f6b02d SetFSM local admission phase only:512 reported original traces/129 tests after fixing conditional type4 text copy. First duplicate/existing value retained; full SetFSM/interpreter remains pending. Exact snapshot/commits queued.

Sixth pipeline34814 FAILED isolated creation-network integration: Timed out reading login packets, exit1. Build and194/194 tests passed54.82s; SQL not passed and bindings not reached. Original failure .local/sixth-batch-sql.log preserved. Verified failed disposable catalog Eternity_Test_Creation_21a3d0f57460 absent (sys.databases count0); cleanup completed. Complete SQL retry plus binding check running session57061, logs .local/sixth-batch-sql-retry.log and .local/sixth-batch-bindings-sql.log. Source freeze remains active; no publication until successful final results. Do not repeat build/tests without new changes/failure evidence.

## Sixth batch validated after SQL retry —194 tests

Build and194/194 tests passed54.82s for sourceb19f469. Initial session34814 SQL failed login-packet timeout; failed disposable catalog verified absent. Complete retry session57061 exited0: full isolated SQL including final guild disposable checks AND restored targeting binding comparison. Logs .local/sixth-batch-sql-retry.log/.local/sixth-batch-bindings-sql.log; original failure retained .local/sixth-batch-sql.log. Bindings355 skills/2746 definitions/1871 links(20disabled) matched original. Freeze released; publish this verified batch, retain failure history.

Reviewed/queued7ecf7f3/821f7f1 evidence-only damage/reentrant replacement:256 reported original flows/128 tests after fixing initial previous-name test setup. Production code unchanged; detached selected node survives damage/death/view before outer admission. Exact snapshot/commits queued and NOT covered by completed batch. Worker03 continues selective vision-fix cleanup in existing scope. Canonical inventory/visitor/Lua adapters remain open; worker02 exclusively owns ItemIds correction on its branch. Objective not complete.
