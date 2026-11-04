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
