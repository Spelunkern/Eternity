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

## Reviewed integration queue: worker09 21e11433

Reviewed11dd712/c254a09 retained compiled function registry/name binding: signed-byte wrapped hash, exact case-sensitive last match, unknown clears, retained subject and global recursive binding lock. Reported384 original sequences/130 tests. Exact snapshot/commits queued. Preserve programs/transitions/current/start; subject/services must outlive retained callbacks and execution must remain serialized separately. FullSetFSM/interpreter/asset/system-functions and six real compiled services remain open. Latest validated publication3e9e1a4(194tests/fullSQLretry/bindings); no pipeline active.

## Reviewed integration queue: worker01 20dc784b

Reviewedbf51740/fc89ef8 attack polling now directly uses canonical effect attack_speed, removing external query. Reported2048 original getter pairs/1024 frames,129 worker tests. Blocked getter includes four signed nonzero blockers; deadline continues raw clamped speed and later blockers do not rerun initial gate. Existing fixtures retained except obsolete external-query observations. Exact snapshot/commits queued. Worker continues actual Actor cancel_shape_on_attack/cancel_move composition within scheduler scope. Same-zone visitor/activeCSkill/world/range/transport remain required. Last central publication3e9e1a4; no pipeline active.

## Reviewed integration queue: worker02 dc1ccaa1

Reviewed5f94027/c2879e3 existing shared ItemIds correction: constructor local stamp, DWORD absolute deadline/baseline/counter wrap, raw_next/next same recursive owner and server added once. Snapshot API retained but sampling outside lock cannot reproduce original contention cadence; live injected clock preferred. Reported73213 original IDs,130 tests,64 read-only SQL bigint conversions. Exact snapshot/commits queued. Existing warning in mob_drops_creation header describes pre-fix state; update after integration. All producers must retain same generator, not a server-zero duplicate. Linux BOOTTIME/localtime is platform choice, not exact Windows timing claim. Latest central publication3e9e1a4; no pipeline active.

## Seventh reviewed batch integrated — shared UID validation freeze

Integrated pending queue throughb916c7f and updated drop UID provider comment to require retained shared ItemIds::raw_next with authoritative clock. Frozen source 5e8d1cb00f9e2fefc4d4903237d09d058291b195. No canonical source/CMake changes or cherry-picks while build/test/fullSQL/restored bindings pipeline runs; logs .local/seventh-batch-{build,tests,sql,bindings-sql}.log. Review incoming handoffs into pending queue. Publish only after checks pass. UID algorithm correction does not mean all item producers are wired to one live owner; that production assembly remains required. Latest published validated source3e9e1a4. Actor retained inventory/world visitors/Lua adapters remain open.

## Reviewed integration queue: worker04 31265b6d

Reviewed9fd122f/438aed3 combat-death ownership bridge: validate owning zone, raw country lookup, Destroy resetfalse and companion removal. Reported256 original sequences/2048operations,128 tests after correcting original fixture global-node payload; initial127/128 failure not counted as pass. Exact snapshot/commits queued during source freeze. Canonical Death alone owns sentinel deadline/count decrement; principal stays registered until corpse cleanup. Forced removal resettrue remains separate. Actual production death-world reward/quest/skill/KC/transport assembly and configuration/pool/calendar dependencies remain open. Central session56420 .local/seventh-batch-* build running254actions; no source changes during pipeline.

## Worker02 checkpointf043f845 — CharacterCommands owner migration

Reviewed/queued783f9ba documentation-only checkpoint, no new tests. Its main snapshot predates local integration: UID correction is already in frozen5e8d1cb. Explicit expanded exclusive ownership assigned to worker02 for character_commands.hpp/cpp and corresponding tests, migrating live shared ItemIds next() while preserving separately observed make-time/UID order. No src/main.cpp/Actor/inventory ownership; no competing generator or invented quest/drop world. Coordinator/other workers must avoid these files until delivery. Existing ItemIds ownership remains worker02. Current session56420 build observed42/254; source freeze active, tests/SQL/bindings pending.

## Reviewed worker05 cfafee42 and PacketParty assignment

Revieweded8ec71/f3aa2f2 BreakParty with live slots/count/ID and existing scene/log emitters:256 reported cycles,130 tests,ASAN/UBSAN. Exact snapshot/commits queued during freeze. Reset then unregister under party lock; no leave/guild/heap recycle. Bounded audit found no direct heap-return producer, not proof of no aliases. Assigned PacketParty004676c0 normal request coordination under party_runtime_requests, reusing retained registry/membership; shared Actor/connection remain coordinator-owned. Current build completed254actions; session56420 tests/fullSQL/bindings pending. No canonical source edits while active.

## Reviewed integration queue: worker09 6b28c2bf

Reviewed60b7108/f6fb947 onBattle/PAI_Attack with real configured reset/local/state-entry composition. Reported1536 original paths/131 tests. Exact snapshot/commits queued during freeze. Retained current_time/deadline, live target country/life/immunity/admin gates, distinct vision removal paths and no immediate replacement attack retained. Country binds stable canonical catalog; world lookup/AttackAI/target/vision providers remain required. Central200-test suite observed177 passing; session56420 fullSQL/bindings pending. Source freeze remains active.

## Reviewed worker08 8e2d703c — full stable skill owner; worker03 adapter assigned

Reviewed36eab1d/849a0fc full normalized catalog records, embedded stable SkillDefinition/name/periodic views, reverse identity and mutable slot/publish APIs. Reported127 tests,ASAN/UBSAN and full793156-byte restoredSQL/original match(all356skill/2747mob slots). Exact snapshot/commits queued during freeze. No const_cast/replacement arrays; serialize raw reload and publish views under quiescence. Assigned worker03 mob_applied_skills_catalog composition against this actual owner, no second catalog/temporary conversions; old branch may test ignored read-only snapshot, central build uses normal owner. Central build/200tests passed62.37s; session56420 SQL/bindings running, final pass not claimed.

## Worker08 checkpoint89e93191 — exclusive canonical mob visitor

Reviewed/queued6731ac2 documentation-only full catalog checkpoint; worker03 catalog composition already assigned. Expanded exclusive scope assigned08: zone_mobs.hpp/cpp and corresponding tests on worker branch, actual synchronous shared Cell cursor visitor and targeting help adapter under mob_targeting_zone. Coordinator/others avoid these paths until delivery. Preserve original nested find/remove/append shared cursor and no post-callback actor dereference; no second list/snapshot/packet probe in production. Existing zone serialization/lifetime contract must stay explicit. No Actor/user-zone/worker03 edits. Main source freeze session56420 remains active,200tests passed,SQL/bindings running.

## Reviewed integration queue: worker03 20b9fe23

Reviewed2037bb0/b2e1273 RemApplySkillVisionFix actual list/pool cleanup: zero ID no-op, clear nonzero ID once before lock, live detail48/log gate and ordered recycle, no target/status/speed/effect reversal. Reported512 scenarios/3072operations,129tests. Exact snapshot/commits queued during freeze. This older milestone proposal does not supersede newly assigned mob_applied_skills_catalog composition against actual full owner; keep that assignment priority. Production loss-of-target call sites remain required. Central200tests passed; session56420 SQL/bindings still running through recovery checks. No canonical code changes.

## Seventh batch validated —200 tests/fullSQL/bindings

Session56420 exited0: native build,200/200tests62.37s, complete isolatedSQL(final guild disposable catalog included), restored targeting binding match355skills/2746definitions/1871links(20disabled). Source5e8d1cb includes corrected shared ItemIds algorithm. Logs .local/seventh-batch-{build,tests,sql,bindings-sql}.log. Freeze released; publish verified batch. Queue distinguishes validated entries from newer pending deliveries. Actual callers still need retained live generator wiring; worker02 owns CharacterCommands migration on branch.

Reviewed/queueda8ee553/90e3b8b portal ownership/publication and two-pass neighbor closure:1024 reported original cases/129tests after correcting native side*4 reference indexing; initial128/129 failure preserved. At most12links, exact DB/map/world packet ordering and live callback reads. Production stable config/transports still required; pending next Death-to-portal composition remains worker scope. This delivery is not covered by completed200-test batch. Objective remains open, real world/Lua/inventory/visitor assembly incomplete.

## Reviewed integration queue: worker09 042b982c

Revieweda16ee67/6479a12 compiled MoveAtSelectedPosition via named registry/actual map/trail/status. Reported1536 original cases/132tests after test-only optional coordinate comparison correction. Exact snapshot/commits queued. Home0.5/20 gates, selectedY clearing/full-position rejection and map line verified; world MoveMob remains required. Raw status3 write must not dispatch/replace fixed state_stack. Actual map/subject/services retained together. Factory/TravelCondition/other handlers remain open. Latest validated publication695ae88(200tests/fullSQL/bindings); no pipeline active.

## Reviewed integration queue: worker04 7de6b2aa

Reviewed31d85d4/3fbc5fb evidence-only actual Death->Destroy->portal publication->corpse/pool composition:256 reported sequences/2048operations,130tests. Exact snapshot/commits queued. Portal map publication routes its actual target map, not necessarily dying actor map; stable global country/obelisk bindings survive callbacks. No additional decrement/recycle. Production configuration/world providers remain required. Last validated publication695ae88; no pipeline active.

## Reviewed worker01 26c281f2 — actual Actor operations and zone visitor task

Reviewedd9bb675/8fce79c scheduler adapter delegates actual Actor cancel_shape_on_attack/cancel_move/send_self;768 reported original comparisons/130tests. Exact snapshot/commits queued. Important backend gap: original immediate0902 effect removal is observed, canonical owner erases replica for later persistence; no wire equivalence claimed. Expanded exclusive ZoneUsers visitor assignment to01: zone_users.hpp/cpp and tests, shared live whole-zone cursor/same tick/retain actor through callbacks, actual scheduler composition. Coordinator/others avoid these paths until delivery; zone_mobs remains08. ActiveCSkill/equipment and real world providers remain open. Last validated publication695ae88; no pipeline active.

## Reviewed worker02 fb9ab9f4 — live UID CharacterCommands

Reviewed9d6d134/1b78fc8 CharacterCommands live retained ItemIds::next then independent make-time, removes obsolete overflow mapping. Reported380 original starter records/146 persistence parameters,131tests plus final3focused; full creationSQL/TCP remains required centrally. Exact snapshot/commits queued. Timestamp injection no longer controls UID; inject retained ItemIdClock with correct lifetime. Quest/drop must receive same owner. Latest validated695ae88; integrating pending reviewed batch next.

## Eighth batch integrated — full catalog/live character UID validation freeze

Integrated reviewed queue through 27df2587ed96c1ce14d7208f3827e7186a8a359e, including full stable mob-skill catalog, actual actor scheduler operations, obelisk death/portals, selective vision cleanup and live CharacterCommands UID. CMake append conflicts preserved both modules/test blocks; no live worker reset/rebase.

NEW SOURCE FREEZE: build/test/full isolatedSQL(includes complete creationTCP)/full restored bindingSQL comparison. Logs .local/eighth-batch-{build,tests,sql,bindings-sql}.log. No canonical source/CMake changes/cherry-picks during pipeline; review new deliveries into pending queue. Publish only after all checks pass. Latest validated publication695ae88. Full normalized catalog comparison should now cover all356skill/2747mob records. Exclusive branch paths still reserved:02 item_ids/character_commands,08 zone_mobs,01 zone_users. Actor retained inventory and real root world/data assembly remain open; objective incomplete.

## Worker02 checkpoint674bfef1 — exclusive SQL streaming owner task

Reviewed/queuedd6c62ae documentation-only checkpoint; main snapshot stale,CharacterCommands migration integrated27df258 and validating session8405. Explicit exclusive sql_server.hpp/cpp and mob_drops_stream tests assignment to02: real retained incremental typed cursor preserving buffered API/transactions/parameters, bind MobItemsR2 completed-row publication with partial failure semantics. Coordinator/others avoid storage paths until delivery. Read-only isolatedSQL only under shared lock; no lifecycle writes/new credential owner. Prior item_ids/character_commands scopes completed, preserve commits. Current central pipeline waiting build capacity; canonical source freeze active. Quest/drop sameUID/root world assembly remains required, not claimed solved by streaming.

## Reviewed integration queue: worker08 38cc79e8

Reviewed6debf69/0017bb5 direct ZoneMobs shared Cell cursor, existing send traversal reuse and real help/catalog/SetTarget adapter. Reported450 original mutation traces/128tests/ASAN. Exact snapshot/commits queued during freeze. Generic visitor never touches actor after callback; retained cell/zone lifetime and complete owner serialization required. Actual SetTarget imposes stronger deferred subject destruction/recycle until return; retain source/user/catalog independently. Actual usercount/random/clock/FSM providers still required. Exclusive zone_mobs paths remain08 until next ownership update; zone_users01. Current session8405 build observed139/270, tests/SQL/fullbinding comparison pending; no canonical source edits while active.

## Reviewed triple handoff — catalog/runaway/visitor checkpoint

Reviewedbbaac40/0bc6799 actual full catalog/applied-skill adapter,reported12544original math rows/256cycles/256R2compositions,130tests. Stable definition identity/no temporary conversions; central build uses canonical catalog,not ignored snapshot fallback. Reviewede42621d/bdbfaf8 MoveRunAway actual map/trail/status,1536original paths/133tests; selected_position is displacement here,absolute in selected-move handler. Reviewed27acdb6 doc-only checkpoint; assigned08 mob_targeting_actor actual retained Actor/world/FSM projection, shared Actor/zone_users read-only (01owns latter). Exact snapshots/commits queued during freeze. Current session8405 build complete270actions,tests/fullSQL/fullcatalog comparison pending. Production root assembly remains open.

## Reviewed integration queue: worker04 0de04e94

Reviewedb6c81d2/d652afe composition-only reset/forced removal with real portal owners:256reported scenarios/131tests. Exact snapshot/commits queued during freeze. Key contract: reset_send suppresses recovered obelisk gate only,not nested portal DB/map/world traffic;47target-map publications occur while true. Retain live global portal/source storage through nested callbacks and route actual target maps. Production configuration/pool/calendar/KC/transport remains open. Session8405 buildpassed and210test suite observed177passing;SQL/fullcatalog compare pending. No source edits during active freeze.

## Eighth batch validated —210 tests/fullSQL/fullcatalog

Session8405 exited0: build,210/210tests58.30s, full isolatedSQL including updated creationTCP and complete character/guild checks, full normalized restored binding comparison. Source27df258. Logs .local/eighth-batch-{build,tests,sql,bindings-sql}.log. Full catalog records/names comparison enabled by36eab1d;355skill rows/2747mob rows/1871links matched original. Freeze released; publish verified batch. New pending handoffs not covered.

Reviewed/queued0e4ce8b/218bf38 evidence-only160 native catalog/death/reentry compositions(131tests,original fixture unchanged) and2858c23 doc-only completion. EXCLUSIVE expanded ownership assigned03: mobs.hpp/cpp and mob_pool.hpp/cpp plus tests, actual applied skill catalog/sharedpool/counter lifetime binding, preserve Init/no unconditional reset-helper clear, exactly-once destruction with service lifetime. Coordinator/other workers avoid those files until delivery. Existing exclusive scopes:01zone_users,08zone_mobs,02sql_server. No other source owners assigned.

User progress estimate10–13percent is rough,not test-derived; server still unplayable. Actual rootworld/Actorinventory/Lua/network composition remains open.

## Reviewed integration queue: worker04 42c735fd

Reviewed46416f8/fe4733f stable parsed obelisk materialization/respawn/member source/KC accounting:64reported original cases,132tests. Exact snapshot/commits queued. Filecountry/template order normalization, consumed IDs before position validation, shared marking before guard, zero-group respawn and radius int-float-double truncation retained. Parser not reconstructed here. mark_obelisk must update original shared definition+7c, not noop/copied template; mobs.hpp remains03exclusive, consider full retained catalog raw record setter via existing API instead of collision. Retain configuration/source backing through all runtime callbacks. Last validated publication9e35b9c(210tests/fullSQL/fullcatalog); no pipeline active.

## Reviewed integration queue: worker09 1cde717c

Reviewedf436d67/276a0d1 MoveRandom/Run actual map/math/status via named registry,2048reported original paths(512actualCRT),134tests. Exact snapshot/commits queued. Preserve shared MicrosoftRandom executor stream,not per-mobServerRandom; normal_step+60 live source,walk/run zone-gate difference and no extra scheduling. mobs.hpp remains03exclusive, no canonical collision. Actual RNG/worldmovement/debug/SetFSM/interpreter remain open. Last validated publication9e35b9c; integrating next reviewed batch.

## Ninth reviewed batch locally integrated —source freeze

Integrated pending milestones through a33b64a11dd3b4f5d6492fb5bae8cdfb75956c17, including direct ZoneMobs visitor/target adapter, actual catalog skill composition, random/runaway handlers and stable obelisk config. All CMake conflicts retained both blocks.

NEW SOURCE FREEZE: build/test/fullisolatedSQL/fullnormalizedbindings compare in .local/ninth-batch-{build,tests,sql,bindings-sql}.log. No canonical sources/CMake edits/cherry-picks while active; review handoffs into pending queue. Publish only after checks pass. Last validated source9e35b9c. Exclusive branch reservations unchanged03mobs/mob_pool,01zone_users,08zone_mobs,02sql_server. Rootinventory/Lua/world construction remains incomplete; objective open.

## Reviewed worker08 b33b343e — Actor/FSM targeting adapter

Reviewed9d9111f/598067c actual Actor country/no_mob_targeting/admin projection, actual usercount/sharedCRT/clock/fixed dispatch. Reported160original paths/129tests/ASAN. Exact snapshot/commits queued during freeze. Remaining lifeport needs reconciliation: worker01 mapped original+142c to existing Actor.attack_state.mode in stop/PvP; assigned08 evidence audit/concrete delegation if confirmed before inventing another field. Existing world execution/retention/Lua/update/error services remain required. Current session45156 build completed270actions,tests/SQL/fullbindings pending. No canonical source edits during pipeline; Actor/zone_users remain read-only for08.

## Reviewed worker08 1d899198 — prior checkpoint superseded

Reviewed/queuedba9d25b doc-only prior Actor adapter checkpoint, no new execution claims. Its assignment33f41e3f predates the current +142c/attack_state.mode evidence audit. Retain newer task, do not add disconnected life value/accessor or repeat old adapter tests. Existing field mapping must be resolved against original writes/target gates. Central217/217tests passed57.50s; session45156 fullSQL/fullcatalog running/pending,source freeze remains active. Latest published9e35b9c.

## Ninth batch validated —217tests/fullSQL/fullcatalog

Session45156 exited0:build,217/217tests57.50s,complete isolatedSQL incl character/guild disposable checks,full restored binding comparison. Sourcea33b64a; logs .local/ninth-batch-{build,tests,sql,bindings-sql}.log. Freeze released; publish verified batch. Pending new handoffs not covered.

Reviewed/queued3c576f5/08c4f17 ordinary obelisk parser:45reported original parser cases+64binder compositions,134tests. Partial writes,case/numeric/textmode/EOF duplicate-last-zone semantics retained; fixedcapacity overflow rejected. Portal/linked parsing and production config/definition flag remain open. Exact snapshot retained. mobs/mob_pool03,zone_users01,zone_mobs08,sql_server02 exclusive branches unchanged. Objective remains open; no playableclaim.

## Reviewed worker03 6b21a3d6 — canonical runtime/pool ownership

Reviewedce03a18/670f70d actual MobRuntime retained skill owner/boundMobPool constructor, AI/VM teardown and destructor damage-before-skill deletion. Reported64originalResetAttack/64nativepoolcycles,132tests. Exact snapshot/commits queued. Bound runtime cannot move; services/catalog/pools/counter and context outlive pool including available objects. FreeMob retains skills/damage,Init helper does not clear,Death callback clears after damage reset. Close extant VM once,stack-only callback not whole wrapper. Root must select bound constructor and provide real ports. Latest validatedf0bafb5; integrating reviewed pendingbatch next.

## Tenth reviewed batch locally integrated — source freeze

Integrated pending queue through 03e120f4f7ad626c7c1f322a5734ed1a82eb1294 including canonical MobRuntime/MobPool skills/lifetime,Actor targeting adapters,obsolete checkpoint docs and obelisk parser. NEW SOURCE FREEZE during build/test/fullisolatedSQL/fullcatalog compare in .local/tenth-batch-{build,tests,sql,bindings-sql}.log. No source/CMake edits/cherry-picks while active; review handoffs into pending queue. Publish only on all checks passing; last validatedf0bafb5. Bound rootworld/services and Actorinventory remain incomplete; exclusive branch ownership unchanged.

## Reviewed triple delivery — SQLstream/ZoneUsers/compiledruntime

Reviewedbc0b0b8/6cb4b94 actual retainedSQLcursor and MobItemsR2 completed-row publication:132tests+3focused,real4conversion failures/5selectiveSQLreloads. BufferedAPI retained; full central storage compatibility check required. Reviewed71febf3/cf52e64 ZoneUsers shared live cursor/recursive membership locking and actual scheduler:256original traces/131tests. Real aliasingActor leases required,detach before deferred destruction; no second list. Assigned01 activeCSkill/equipment concrete bindings under mob_combat_scheduler_bindings,sharedActor/skills/inventory read-only pending accessor evidence. Reviewedf7f2c8a/2f1161a unified compiled runtime/Debug/final startup:768neworiginal sequences,135tests and priorcallbackfixtures; fullfactory/assets/interpreter still open. Exact snapshots/commits queued during freeze. Session66021 tenthbatch currently waiting buildcapacity;no source changes during pipeline.

## Worker02/03 completion checkpoints — inventory/raw pool expanded scopes

Reviewed/queued8ed5c3d/6cd40fa doc-only scope checkpoints,no new tests. Assigned02 EXCLUSIVE actor.hpp/inventory.cpp plus tests: retained-item borrow/countsync/directrelease/rawclaim primitives for quest06/07, no duplicate inventory/no receive_item fallback/no transport sideeffects. Coordinator/others must avoid these files. Previous02SQLcursor scope complete, preserve commits. Assigned03 existing exclusive mobs/mob_pool rawallocate/prepare split preserving manager allocate via composition, real skill/damage/lifetime owner and obelisk startup ordering. No fakeLua/root assembly. Existing01zone_users/08zone_mobs remain exclusive. Current session66021 build345actions observed40;sourcefreeze active, no main source edits.

## Reviewed integration queue: worker04 d21a1d25

Reviewedea242b5/6541a72 stable portal/config target-map binding:64reported original cases/135tests. Exact snapshot/commits queued during freeze. Retain ordinary records and actual country fields, same globalnextID,firsttarget/ordinalreferences/sidecompaction;target_map routes actual destination,zone_present stays live through map lifecycle. Parser/assetcomparison/maproot services remain open. mobs/mob_pool03exclusive unchanged. Current session66021 buildcompleted345actions;tests/fullSQL/fullcatalog pending. No source changes while active.

## Reviewed worker08 e17d9d91 — life dependency closed

Reviewed5b9602c/3645232 concrete targeting life delegates existing Actor.attack_state.mode(+142c),no extra state/accessor. Reported400original targetpaths/138Stop sequences,129tests/ASAN. Exact snapshot/commits queued during freeze. Full userDeath/Rebirth currently missing; assigned08 mob_targeting_lifecycle recovery/composition with actual Actor mode/effect/resource owners,sharedActor/inventory02 andzone_users01 read-only. No backend/world completion claimed. Current221test suite observed173passing;session66021 SQL/fullcatalog pending. Sourcefreeze active.

## Reviewed worker08 3e3b3318 — older life checkpoint

Reviewed/queued836cc99 documentation-only +142c completion, no new tests. New playerDeath/Rebirth assignment already supersedes olda0b6a6b5scope;retain newer task,do not repeat life mapping. Central221/221tests passed61.79s;session66021 fullSQL/fullcatalog running,sourcefreeze active. Last publicationf0bafb5.

## Tenth batch validated —221tests/fullSQL/fullcatalog

Session66021 exited0:build,221/221tests61.79s,fullisolatedSQLinclcreation/character/guild,fullrestoredcatalogcompare. Source03e120f; logs .local/tenth-batch-{build,tests,sql,bindings-sql}.log. Freeze released;publish verifiedbatch. Newer queuedSQLcursor/ZoneUsers/userlife/etc not covered.

Reviewed/queueda2c2a37/d3f91e3/322feff evidence-only112original requests/48nativepolls,132tests. Actual userCSkill owner absent,not private-only; assigned01 canonical user_skill_runtime256stable slots/signedcount/liveDWORDselection/sharedmanagerpool contract and actual scheduler handle. Actor/inventory02exclusive remains read-only; report minimal singleowner attachment. Retained weaponbag0slot5 needs actual item accessor,02task addresses it. No fake runtime/projection equivalence. Existing exclusive branches03mobs/mob_pool,08zone_mobs,01zone_users,userSkillnewfiles,02Actorinventory remain. Objective open.

## Reviewed worker04 526ddbe6 — portalparser

Reviewed2040476/fc2018f portalparser with retained partial headers/references and original fury+7offset:50reportedoriginalparser/64bindercompositions,137tests after native lowercasepatternfix;initial135/137 failure preserved. Exact snapshot/commits queued. Ordinary and portal parser consume same bytes,ordinarybind first thenportal/sharedID/stablesources. Fullasset comparison and root providers remain open. Latestvalidated554401c;integrating queuedbatchnext.

## Eleventh batch integrated — typed cursor/retained zone validation freeze

Integrated pendingthrough 4f62ff6868ebabde028ee77158f52ddf1ea4199e,includingactual typedSQLcursor/MobItemsR2,retainedZoneUsers/compiledruntime,concretelife mapping/skillbindingevidence andportalparser. NEW SOURCEFREEZE build/test/fullisolatedSQL/fullcatalog plus realread-only streamSQLtest in .local/eleventh-batch-{build,tests,sql,bindings-sql,stream-sql}.log. No source/CMake edits/cherry-picks while active; queue new handoffs. FullSQL compatibility mandatory after storage-owner change. Lastpublishedvalidated554401c. Rootworld/realproviders and activeuserSkill/Actorinventory integrations remain open; exclusive branch ownership unchanged.

## Eleventh build configure correction and retry

Initial session19114 failed before build: missing openingBUILD_TESTINGguard at portalparser test block during append conflict integration. Central correction restored guard; originalfailure .local/eleventh-batch-build.log retained. Restart completepipeline build/test/fullSQL/bindings/streamSQL with retrylogs. Freeze remainsactive at correctedHEAD; no worker source cause claimed. Reviewed/queued048ead2/4e7084e fullprivateObeliskasset evidence1388words/138reportedtests;privateasset staysignored,nextbatchonly.

## Reviewed triplehandoff — normalparty/stateadmission/rawpool

Reviewed02d44c2/2d01351 normalPacketParty actual retainedregistry/membership,1116originalexecutions/131tests/ASAN;assigned05 actualExpelUser00447120 next,raid/rootpartyActorbinding pending. Reviewed0f18264/1841ca2 configuredAddState/copy/borrowedconditionowner,1536calls/39251frees/136tests;nonemptytokens/factory/interpreter remainopen. Reviewed4d8e1ed/d00a223 existingMobPool rawallocate/prepare,2816originalops/64obeliskcompositions/134tests;preparebeforeRNG/zoneentry and preserveboundlifetime. Exactsnapshots/commitsqueued duringfreeze. Centralretry71788 build593actions observed20,no sourcemutations;fulltestsSQL/fullcatalog/streamSQLpending.

## Worker03 checkpoint3446c6ec — actual obeliskpool/init/zone adapter

Reviewed/queuedf4a5451 doc-only rawpool completion,no newtests. Assigned03 mob_applied_skills_obelisk newfiles: concrete sameboundpool raw/manager/prepare,canonicalinit/ZoneMobs entry under existing ObeliskRuntimeWorld;worker04sourcesreadonly. Actual remainingworldmethods mandatory,no competingowner/no fakeinit/enter. Keep retainedskills/damage/zone lifetime andoriginalcallbackorder. Current71788retrybuildobserved102/593;sourcefreeze active,testsSQL/fullcatalog/streamSQLpending.

## Reviewed worker02 bd21d632 — actual Actor inventory primitives

Reviewedc02b69c/302dcb2 existing Actor rawclaim/borrow/countsync/mirrorsync/directrelease,135reportedtests/680release/152grantoriginalfixture compositions/read-onlySQLscalar check. Exactsnapshot/commitsqueued duringfreeze. No ItemRemove exchange/log/transport or secondallocation;claimbeforedefaultcraft,syncfullmirrorbeforeDB;multi-wholestack oldcountretaineduntilrelease. Assigned06 quest_start_items_actor(sharedliveUID/pool/rawclaim and canonicalQuestAddbinding) and07 quest_requests_actor(actualconsumption/runtime/completion),Actor/inventory02readonly,oldbases maytestignoreddocumentedsnapshots,centralactualowners. Current71788retrybuildobserved494/593;fulltestsSQLcatalogstreamSQLpending.

## Worker02 checkpoint089d1928 — immediate effectDB integration

Reviewed/queuedb57fe3f doc-onlyinventorycompletion,no newtests.06/07actualinventoryadapters alreadyassigned. Expanded02exclusiveactor.cpp alongsideactor.hpp/inventory.cpp: investigate/implement actual originalimmediateDB0902effectremoval gap from01adapter,requiredDBtransport and exactreplica/client/poolorder,distinguish restoration/shutdownsites,notblindglobalerase. Otherworkersread-only. No fakeDBsink/newcache. Current71788retrybuildcompleted593actions;tests/fullSQL/fullcatalog/streamSQLpending,sourcefreeze active.

## Reviewed worker09 62e0861a — nonempty token ownership

Reviewed0cd5bed/9886060 retained288byteCKToken payload/nonemptyvectors andconstructor partialinit. Reported768originaladmissions/28901frees/64constructors,137tests. Exactsnapshot/commitsqueued duringfreeze. Declaredconditioncount/cursor separate fromtokenssize;metadata/functionIDs remain data,no originalpointerexecution. TravelCondition/fullfactory/assets/world stillopen. Centralretry71788 buildpassed,228/228tests71.34s;fullSQL/bindings/streamSQL running/pending,no finalpassyet. Lastpublished554401c.

## Eleventh batch validated —228tests/fullSQL/catalog/stream

Corrected pipeline71788 exited0:buildretry,228/228tests71.34s,fullisolatedSQL,fullrestoredcatalogcomparison,actualread-onlystreamSQL with fourcontrolledGetDatafailures preservingprefixes,types/NULL/extraresults/bufferedparameters/transaction/retainedconnection. Source48f2c32. Logs .local/eleventh-batch-build-retry.log,.local/eleventh-batch-{tests,sql,bindings-sql,stream-sql}.log. InitialCMakeguardfailure preserved,not passed. Freeze released;publishverifiedbatch.

Reviewed/queued9feae10/e66ceb4 directsharedbObelisk+7c mutation via actualmutablecatalogaccessor,138reportedtests;ID/level/HPguards and267otherbytespreserved. Interfaceblockerclosed,no canonicalfield/bitmapneeded. Actualmapmutableportals/lifecycle/KC/calendar/transport/rootassemblyremain. Newpendingentriesnotcoveredby228-testbatch. Exclusivebranchownershipunchanged. Objectiveopen.

## Reviewed worker05 289307db — normalExpelUser

Reviewed709df36/1de29fe actualExpelUser00447120 inrequests/recursiveLeave,2048reportedscenarios/131tests/ASAN. Exactsnapshot/commitsqueued. Expel differs voluntaryLeave:no directleader/itemcursoradjustment/no0b07;remaininguser pin retains outercall lifetime,unregisterneverrecycles. Actualguildcleanup/Actorpartybindings stillrequired. Assigned05 regularraidEnter/Leave00445a30/00446510 underparty_runtime_raid,existing30slotowner,no normalsubstitution. Lastvalidateda2c4e8c228tests/fullSQL/catalog/stream,no pipelineactive.

## Reviewed worker01 391d05c9 — actual learneduserSkill owner

Reviewed317980f/fc1a9eb retainedUserSkillRuntime256stable slots/signedcount/DWORDselection and actualhandles throughscheduler,64originalsequences/12nativecompositions,133tests. Exactsnapshot/commitsqueued. Rootretainsowner+Actor,notActorownedstrongcycle;realmanager mustshareCSkillpool/IDdomain withmob,notunsafe casts/independentgenerator. Stableuserdefinition/names andactualactionproviders required. Resetcount/index beforeloop preservesstrandedslots;managerstrand/delete vsrecycle explicit. WorkercontinuesLearnSkill004842e0 within scope. Lastvalidateda2c4e8c;integratingpendingnext.

## Twelfth reviewed batch integrated — sourcefreeze

Integratedpendingthrough f545baed016242e83711d1c4521502689f47cdac,includingActorinventoryprimitives,rawMobPool,normalpartyrequests/expulsion,tokens/stateadmission,fullasset/flagsetter andlearneduserSkillowner. CMakecontrolnestingcheckedbeforepipeline. NEW SOURCEFREEZE duringbuild/test/fullisolatedSQL/fullcatalog/realstreamSQL in.local/twelfth-batch-{build,tests,sql,bindings-sql,stream-sql}.log. No canonicalsource/CMakeedits/cherry-pickswhileactive;queuenewhandoffs. Publishonlyallcheckspass. Lastvalidateda2c4e8c. CommonCSkillmanager/userdefinitions/rootworld stillmissing;actualownercomponents do notclaimplayability. Exclusivebranchscopeunchanged.

## Reviewed worker04 9fd22064 — initialportalDBlist

Reviewed528926f/d31569c initialportal0206list:constantcountry3/time0,100rowchunks/livefuturebaseIDs,no target/zoneguard. Reported96originalcases/168packets/139tests. Exactsnapshot/commitsqueued duringfreeze. CWorldSendDBObeliskList portalfirstthenordinarymaps;do not reuse livechange serializer. ActualDB/rootmaplifecycle remainsopen. Current37938twelfthbatchwaitingbuildcapacity;no sourceeditswhileactive. Lastvalidateda2c4e8c.

## Reviewed worker05 0528233d — reentrantnormalexpulsioncorrection

Reviewedc656778/79d68aa normalExpelUser nestedleave correction,not raidimplementation.2560reportedscenarios/282nestedleaves,131tests/ASAN. Preserve originalcount-1/retainedmaximum after callbackdisbands before outerdecrement;subsequent signednonpositive search returnsfalse,no clamp/recycle. Exactsnapshot/commitsqueued duringfreeze. Newerregularraidassignment remainsactive andsupersedes olderexpelscope. Current37938twelfthbatchpendingbuildcapacity;no sourcechanges.

## Reviewed worker08 bb218e8d — Rebirth

Reviewede80704a/ca358f8 completeRebirth/Actorbridge:1552reportedoriginaltraces(16actualreentrant),130tests/ASANaftertestin_zoneattachmentfix,initial129/130notpassed. Exactsnapshot/commitsqueued duringfreeze. Sharedmode1->0 onlytail,exactresourcepublishbeforecallbacks,corpse/move/transferfailuresretainoriginalpartialstate/effects/deadlines. Requiredsilentsigned/rawresources and presentationmotion+1434 distinct fromaction+1448 remaincanonicalActor02owner work;do not conflate bytes/clamprawrestoration. FullDeath/PvPrewards and realrootports missing,08continueslifecyclescope. Current37938buildcompleted355actions;testsSQLcatalogstreamSQLpending,no sourcechanges.

## Twelfth batch validated —238tests/fullSQL/catalog/stream

Session37938 exited0:build,238/238tests68.65s,completeisolatedSQL,fullrestoredcatalogandrealstreamSQLfourcontrolledconversionfailures/types/parameters/retainedconnection. Sourcef545bae;logs.local/twelfth-batch-{build,tests,sql,bindings-sql,stream-sql}.log. Freeze released;publishverifiedbatch. Newpendinghandoffsnotcovered.

Reviewed/queueddd8daae/99dce13 user2101mixedordinary/portalshared100rowchunks,192originalcases/334packets,140tests. Nozoneguard,actualmapfilterbeforetargetread,livefuturecallbackreads;ordinarythenportalwithoutphaseflush. Realuserentry/connection/retainedmaproutingstillrequired. Objectiveopen,exclusivebranchownershipunchanged.

## Reviewed worker09 57693a10 — limitedliteralinterpreter

Reviewedc2de343/34ac828 numeric/stringTravelCondition andretained64valuestack,3048reportedoriginalexecutions/138tests. Exactsnapshot/commitsqueued. Literalcompletionreturns0,not toptruth;capacityoverflowincrementstopwithoutwrite;retaintext/numericscratch semantics. Unsupportedvariable/function/operatorandundefinedfirstnumeric paths explicitlyerror;do notinstallasfullassetinterpreter. Actualfactory/rootservices remainopen. Latestvalidatedc8d07ce238tests/fullSQL/catalog/stream,no pipelineactive.

## Reviewed worker01 e8d0d63b — LearnSkill

Reviewedc813863/df90d11 actual learnedowner LearnSkill new/upgrade/guards/points/DBclientlogorder,192reportedoriginalcases/134tests. Exactsnapshot/commitsqueued. Firstcapturedcostdeducted aftersecondlivedefvalidation,no refundallocationfailure,realpassivecallback/livecount/slotrereads preserved. Stableuserdefinition/commonmanager/rootportsstillrequired. WorkercontinuesSendCharacterSkillcooldownwithin userskill scope. Lastvalidatedc8d07ce;integratingreviewedpendingbatchnext.

## Thirteenthbatch integrated —sourcefreeze

Integratedpendingthrough f59ccd59241c8a19a42afae080e6aace2e23d59e,includingLearnSkill/Rebirth,expelreentrancy,literalVManduserportalpublication.CMakenestingchecked. NEW SOURCEFREEZE duringbuild/test/fullSQL/fullcatalog/streamSQL in.local/thirteenth-batch-{build,tests,sql,bindings-sql,stream-sql}.log. No canonicalsource/CMakeedits/cherry-pickswhileactive;queuehandoffs. Publishonlyallcheckspass,lastvalidatedc8d07ce. Actualcommonmanager/userdefs/lifecycleworld/rootservicesremainopen;exclusivebranchownersunchanged.

## Reviewed worker04 3df27d42 — worldinitialDBsequencing

Reviewed1112ff8/2afd61c realportal/ordinarypublishers onsameDBport,portalfirstthenorderedmaplistswithindependentchunks.128originalcases/452packets/141testsreported. Exactsnapshot/commitsqueued duringfreeze. Latermaprowsreadliveafterearliersends,nozoneguards/stalesnapshots. RootDBconnectionstartup/reconnectandworldassemblyremainopen. Current60360thirteenthbuildobserved12/287;testsSQLcatalogstreamSQLpending,no sourceedits.

## Worker04 checkpoint9f2e57c5 — exclusive actualmapowneradapter

Reviewed/queuedbbe809c doc-onlycheckpoint. Explicitexpanded04exclusive map_config.hpp/cpp plus tests:actualServerMaps mutableportalcountry pointer accessor,mappedretainedconfig/source/position/identity adapter underobelisk_runtime_maps. Coordinator/othersavoidpaths;no secondcountryarray/const_cast/replacementmapowner. Reusefullcatalog/parsedbinders/sharedID,bObelisksetter;primaryzonepresence/routingactualrequiredports. Pool/init/zone03scope unchanged. Current60360buildcompleted287actions;testsSQLcatalogstreamSQLpending,sourcefreezeactive.

## Reviewed realowner adapters03/07

Revieweda48542a/00fdd9a actualsameboundMobPool/rawprepare/initialize_mob/ZoneMobs obeliskadapter,48reportedcompositions/135tests;initialfixtureports/name/resetpublicationexpectationscorrected,nativeadapterunchanged. Reviewedcb7ebf5/c674ca7 actualActorinventory/QuestRuntime/rewardadapter,896originalpaths/1095releases/126testsreported. Exact snapshots/commitsqueued duringfreeze. Inventorysnapshotidentity/payload/unlockedbagmutationsandnestedconsumptionexplicitlyrejected ratherthanunverifiedreentryequivalence. Actualuserexecutor/realreward/transport/logservices andworldAIbackendremainrequired. Current60360buildpassed,243/243tests63.72s;fullSQL/catalog/streamSQLrunning,pendingfinalexit.

## Thirteenth batch validated

Session60360 exited0. Sourcef59ccd59241c8a19a42afae080e6aace2e23d59e passed build,243/243 native tests (63.72s),complete isolated SQL,full restored binding catalog and real stream SQL. Logs: .local/thirteenth-batch-{build,tests,sql,bindings-sql,stream-sql}.log. Sourcefreeze released. New world sequencing and Actor/obelisk adapters remain reviewed in queue and are not covered by this validation. Server reconstruction objective remains open.

## Fourteenth batch source freeze

Integrated reviewed world DB sequencing, concrete pool/zone obelisk adapter, actual Actor quest adapter and documentation checkpoint2244a0a. CMake nesting audited. Source ff9f04445567d7f78cbb054c712614a99ed8a087 frozen during build, native tests, isolated SQL, full binding catalog and stream SQL. Logs .local/fourteenth-batch-{build,tests,sql,bindings-sql,stream-sql}.log. Latest published validated d5310bf; new batch not yet validated.

## Reviewed worker09 efb92f22 — retained local VM reads

Reviewed da753b2/9c9afc8: explicit local-value provider shares actual configured variable map and retained stack, TokenStr signed-char hash, original scalar/string-tail assignments. Reported1920 original executions,6144 tree frees,139/139 worker tests; no central verification yet. Missing typed locals explicitly error without invented insertion; functions/operators/full factory remain open. Exact commits queued during fourteenth source freeze. Worker continues active interpreter scope.

## Reviewed worker01/02 snapshots 72db4b97 /79df4841

Queued979e1a2/d33f9a9 learned snapshot: original sparse physical slots/retained scratch and signed cooldown,192 comparisons/135 worker tests. Reviewed c964d62/5546d0b immediate0902 Actor binding and actual existing SkillReplica delivery,512packets/2304traces. Not suite-passed:136/137,zone-users fixture lacks required transport. Held queue status reviewed-awaiting-fixture-fix. Explicit narrow ownership exception assigned02 to apply prepared zone fixture recording provider and rerun full suite; preserve failed evidence. No production no-op binding. Fourteenth source freeze remains active; no canonical source changes.

## Reviewed worker05 90e6cc64 — regular raid membership

Reviewed ba4f407/1c14552 actual EnterRaidParty/LeaveRaidParty on SAME30slot registry, original wire indices/holes/inactive tails/recursive leave, raid-aware send/type/request adapters. Reported256original sequences,84nested calls,512send comparisons,133/133tests and ASAN/UBSAN. Exact commits queued while fourteenth freeze active. Actual retained Actor/registry root, guild cleanup, transport/log/scene and raid creation/conversion/remaining opcodes still open. Fourteenth build passed290actions; native246test suite running.

## Reviewed worker06 d258c12c — actual quest grant/admission adapters

Reviewed05b9c62/c10c68a: SAME retained Actor/item pool/catalog/live ItemIds, rawclaim before craft and mirror sync before DB, QuestRuntime add_active_unaccounted with once-only live counters. Per-call frames and retained leases; real clock/calendar/DB/client required. Reported384items/253starts/306additional grants,127/127tests after initial fixture observation failure corrected. Queued during fourteenth freeze. NPC/requirement consumption/log/root bridge incomplete; worker continues concrete QuestStartWorld binding. Central246/246tests passed69.65s;SQL/catalog/stream pending.

## Worker02 fixture dependency resolved

Reviewed4a1ebd6/1293d5e exact authorized recording transport migration only in tests/zone_users_unit.cpp. Worker build and137/137tests42.82s pass; initial136/137failed log preserved. Immediate0902 queue entry released from fixture hold; implementation+fixture pending central integration after active fourteenth SQL validation. Real root DB/cache/lifetime still required.

Reviewed6200faa doc-only previousfixture checkpoint; no new implementation/tests. Newer Actor Rebirth assignment remains active (worker02 running); do not overwrite it with obsolete WAITING status. Fourteenth246tests/fullSQL/catalog passed so far, streamSQL/final pipeline exit pending.

## Fourteenth batch validated; raid transport evidence reviewed

Session12060 exited0: build,246/246tests69.65s,complete isolated SQL,full restored binding catalog and real stream SQL all passed. Sourceff9f04445567d7f78cbb054c712614a99ed8a087; .local/fourteenth-batch-{build,tests,sql,bindings-sql,stream-sql}.log. Freeze released; newer queued deliveries not covered. Reviewed5fc2198/d38c171 raid original pooled-send composition evidence:64sequences/2650deliveries/41poolfailures,134tests+ASAN/UBSAN reported, no production membership changes. Exact commits queued. Assigned05 actual raid creation/conversion/remaining opcodes rather than repeated membership fixtures. Objective remains open.

## Fifteenth batch integrated and frozen

Reviewed fdbff48 old raid scope doc checkpoint; newer creation/conversion assignment already active. Integrated reviewed queue through a8abc5eb5d917bfa6a8d1c0be952f0e46ef105c0 including local VM reads, user skill snapshot, immediate0902 with corrected zone fixture, raid membership/pooled send and actual quest grant adapters. CMake conflicts combined and nesting audited. Frozen for full build/tests/isolatedSQL/catalog/stream pipeline .local/fifteenth-batch-*.log. Latest validated published2ac53b3; new source unvalidated.

## Reviewed worker01 c57485e2 — learned cooldown persistence

Revieweda6d1d1c/4a0a2cc learned0804 phase on actual UserSkillRuntime, retained initialized scratch/physical holes/runtime numbers, unsigned deadline comparison and definition reset_seconds cap; count256 processes rows without send. Reported192whole-original phase comparisons/116actual SkillReplica-load compositions,136/136tests46.85s. No whole native character-save equivalence claimed: real resource/position/equipment phases precede and later effects/quests reuse returned tick. Stabledefinition+4a/rootmanager/saveassembly remain required. Exact commits queued during fifteenth sourcefreeze;255native suite currently running after335actionbuild passed. Worker continues proposed EnableUseSkillUser/Mob recovery within owned prefix.

## Reviewed worker09 e25d484b — counted integer VM comparisons

Reviewed69e561f/72bad7f explicit int/DWORD subset, original pop order/retained slots and first-DWORD no-result, wrapped count of successful comparisons consumed by actual transition threshold. Reported3840original TravelCondition executions/140tests31.52s. Float/noninteger comparisons, arithmetic/assignment/functions/factory remain open and unsupported paths explicitly error. Exact commits queued during fifteenth freeze; no source mutation.

## Fifteenth initial validation failed; fixture migration retry

Session82269 exited1:254/255tests68.21s, mob-combat-scheduler-actor-unit missing required Actor0902 transport. SQL was not started. Preserved original fifteenth logs. Coordinator migrated only test Scene to explicit recording ActorEffectDatabase, same missing-capability correction as zone fixture. Retry full build/tests/SQL/catalog/stream with separate fifteenth-retry logs. Reviewed5b7a35b/cca7796 raid conversion/config/disband1280original requests34nested135tests+sanitizers;queued. Raw auto_join byte dependency remains explicit.

## Reviewed worker04 ccf623ba — actual ServerMaps binding

Reviewed4db2ac3/7b0a529 actual mutable country accessor and retained map/config/position identity, no second country table or const_cast. Live primary ZoneMobs routing and DB/world/map services remain required. Reported64original compositions/68recoveredmapload/142tests. Exact commits queued during fifteenth retry source freeze. Same-pool/init/zone adapter composition assigned03 next,04 ownership map_config unchanged.

Reviewedf2d96ed worker04 doc-only checkpoint, no new runtime or suite execution. Actual main root only validates const local ServerMaps, no retained mutable primary-zone/network owner yet. Worker03 already owns pool/map composition; do not duplicate. Last04validation142tests remains historical. Fifteenth retry255/255tests79.97s passed; fullSQL/catalog/stream final exit still pending, do not publish.

## Reviewed three owner milestones — queued during fifteenth retry

01-mob-damage: reviewed ed28b5b/c11c253. Runtime readiness suffix only: cooldown/prerequisite/block/range actual handles;256original comparisons137tests. Cost/equipment prefix and actual action/root bindings remain open.

02-mob-drops: reviewed a7d46ab/cca5d9e. Actual ActorResources raw replacement and existing zone_lifecycle presentation+1434 distinct attack+1448;512original instruction slices8actualbridgecompositions139tests. Includes minimalresources.hpp API necessary;wholeRebirth/rootports not newly completed.

09-mob-events: reviewed 5f7c42e/0eb029e. Numeric comparison matrix int/float/DWORD with float staging/NaN,7936originalcalls141tests. Arithmetic/assignment/functions/factory still absent.

Worker08/root actual Rebirth bridge can now bind Actor::replace_resources_raw and presentation_motion once integrated; shared life stays attack_state.mode. Original Actor user-shape/zone-entry now use existing zone_lifecycle motion, not attack motion. Minimal resources.hpp raw setter accepted as necessary access to existing owner, no second resources. Preserve all required corpse/move/recovery/log services. Currentretry255tests passed;SQL/catalog/stream final confirmation pending.

## Fifteenth retry validated

Session97601 exited0:build255/255tests79.97s/full isolatedSQL/fullbindingcatalog/realstreamSQL passed at408826d. Original254/255 failure remains .local/fifteenth-batch-tests.log; passing logs .local/fifteenth-retry-*.log. Freeze released, publish validated batch only. New queued snapshots excluded.

## Reviewed worker05 8c1a6380 — raid wire indices and leadership

Reviewed999721b/6c5a403 actual0b12/13/17 on same30slot registry, wire index exchange without moving pointers, original leader/subleader callback ordering/live rereads. Reported1280originalrequests18nested136/136tests40.37s+ASAN/UBSAN. Invalid selected-null projections explicitly rejected; no actual root lifetime safety claimed. Exact commits queued after validated published d59d595. Remaining raw auto_join byte/autojoin/invites/responses/raidexpel/root services open; worker continues current raid request scope.

Reviewed8228718 doc-only Rebirth ownership checkpoint, no new runtime/tests. Explicitly assigned02 pure SendRecoverChange00482230 Actor notification using existing resources/party owner, no heal/clamp/mutation/damage reset. Worker08 lifecycle stays separately owned. Prior139tests remain historical; pending actual APIs require central integration.

## Reviewed worker08 63bd8b3d — NPC death coordinator

Reviewedc2811ca/7105169 whole DeathByNPC/SendDeath, original subject exclusion/capturedpartylock/live rereads/NPC0c06 evenraid. Reported2064originaltraces16reentrant131tests39.47s+sanitizers after generated-thunk cache invalidation corrected fixture; original130/131failure preserved. ActualActor bridge only prefixes to missing same-ledger reset, not completed production NPCdeath. Needs actual penalty/soul/deadline/code owner fields, ledgerreset, corpse/PvP/partyexcludedtransport. GeneralDeath/PvPrewards remainopen. Queue exact snapshots; DB0902 mandatory fixture compatibility must be checked centrally. Actor02 rawresources/presentationAPIa7d46ab now delivered; pureSendRecoverChange assigned02.

## Sixteenth batch integrated/source freeze

Reviewed0d84227/9408243 raid0b1a sameowner expulsion:512originalrequests768direct43nested137tests+sanitizers reported, queued then integrated with all reviewed pending deliveries through f4e875cb6320d4e672b14226cc235e5765d5e6b8. Includes actual Actor Rebirth resource/presentation APIs,NPCdeath coordinator,maps binding,readiness/persistence,numericVM and raid requests. CMake conflicts combined/nesting audited. Freeze canonical sources during .local/sixteenth-batch-{build,tests,sql,bindings-sql,stream-sql}.log pipeline; latestvalidatedpublishedd59d595. No new batch validation claim. Known new NPCdeath fixture must satisfy mandatory0902binding if exercising removal.

## Reviewed worker09 90fae173 — integer arithmetic VM

Reviewed495a8a4/bbd680f int/DWORD add/sub/mul/div on same retained stack/map/resulttemporary, original copied second operand metadata and unsigned mixed division; counted conditions drive actualtransition. Reported3840originalexecutions142/142tests42.50s. Undefined operand storage/div0/INT_MINdiv-1 explicitly error, no CPUfault equivalence claim. Float arithmetic/assignment/control/functions/factory remain open. Exact commits queued during sixteenth freeze,build currently running; no sourcechanges.

## Reviewed worker01 6466d39d — original CRT53 resource availability

Reviewed165eb33/2bd193e exact SP/MP cost prefix float32operands/stageddouble53/truncation/overflow/wrap/signed availability and required nearest rounding.4096cases8192completeoriginalcalls; final138/138tests34.27s. Preserve unrelated prior137/138world-network replacement disconnect registry failure; teardown race hypothesis unproven,nofixclaim. No resource spending or full EnableUseSkill claimed. Equipment middle/action/full stabledefinition/rootservices remain required. Exact commits queued during sixteenth sourcefreeze;385actionbuild passed,tests pending.

## Sixteenth initial failure and pure recovery notification review

Session29069 exited1:265/266tests63.97s; new NPCdeath Actor fixture missing mandatory0902binding. SQL not started. Preserve .local/sixteenth-batch-tests.log as failure. Coordinator fixture-only recording transport migration; full retry .local/sixteenth-retry-*.log. Reviewedba1bd05/a49d54c actual pureActor recoverychange, view snapshot then liveparty energy,768originaltraces8Rebirthcompositions141tests41.73s reported. Queued for next batch,not partofretry.

Reviewed05a347b doc-only RecoverChange checkpoint, no new execution. Assigned02 actual same damage_ ledger silent reset (existing private reset_damage adds scene callback and is unsuitable). Audit missing deathmetadata original owners before adding fields; worker08 lifecycle remains read-only. Sixteenth retry still active/sourcefreeze.

## Reviewed worker09 c45dee34 — integer assignment

Reviewed73dc722/bbb5956 actual same map/stack writeback via embeddedVariableID, resultpush beforewrite and retained metadata/text.1792originalcalls143/143tests42.76s reported. Missingwritebacktarget errors afterpush without invented insertion. Compound/float/functions/factory remainopen;workercontinuescompound. Queued during sixteenth retry freeze. Retry266/266tests54.90s passed;SQL/catalog/stream finalexit pending.

## Reviewed worker06 f42d5e3a — concrete QuestStartWorld

Reviewed73edfa4/2e5413d actual sameActor/runtime/admission/grants/logserialization bridge; family+12e correction distinct faction+12d.224originalflows79starts159actualgrants128/128tests45.64s reported. Root lastitemuse+57d4/+57d0 actualowner missing; references must be authoritative and retained, no zero shadow. RealNPC/07inventoryconsumption/log/DBclient routing required. Exact commits queued during sixteenth retry freeze.

## Sixteenth retry validated; autojoin reviewed

Session90533 exited0: build266/266tests54.90s/fullisolatedSQL/fullbindingcatalog/realstreamSQL all passed at4b4e16d. Original265/266failure retained .local/sixteenth-batch-tests.log; passing .local/sixteenth-retry-*.log. Freeze released. Reviewed3b806fc/1cb1faa actual0b11 active-name retainedlookup/same30slotadmission/errorordering;1024originalrequests46nested138tests+sanitizers reported. Exactcommits queued,newer deliveries not covered by validated batch. Invitations/rawauto_join/rootservices remainopen.

## Reviewed worker09 fbcf5ccc — compound integer writeback

Reviewed55abbe4/c18c19f compoundint/DWORDupdates11..16, originaltwo-pop increment/decrement/embeddedVariableIDwriteback/secondoperandmetadata on sameFSMmapstack.3072originalcalls144/144tests42.08s reported. Missingtargets/CPUfaultdivision expliciterrors,noindeterminateinsertionclaim. Float/functions/factory/rootservices remainopen. Exactcommits queued, latestvalidatedpublished27798a7.

## Reviewed worker02 8c156299 — silent same-ledger reset

Revieweda12ce74/cd52511 Actor::reset_damage_ledger delegates existing damage_.reset only,noScene/resource/life/effect changes.65originalReset scenarios142/142tests51.19s and actualNPCdeath success/corpsefailure compositions reported. Root can bind reset_damage(Actor&) to this method. Four authoritative deathmetadata owners/initialization still missing; explicit02followup assigned. Realcorpse/PvP/partyexclusion/transport remainrequired. Exactcommits queued,latestvalidated27798a7.

## Seventeenth batch integrated/sourcefreeze

Reviewed29c7fd9 doc-only old ledger scope checkpoint;newer02metadata assignment already running. Integrated all reviewedqueue through 86811964a1fc00020b6fbe1d0d0ede5714c8dc0b including integerarithmetic/assignment/compound,costprefix,RecoverChange,QuestStartWorld,autojoin,silentledgerreset. CMake conflicts combined/nesting audited. Sourcefreeze during .local/seventeenth-batch-{build,tests,sql,bindings-sql,stream-sql}.log pipeline. Lastvalidatedpublished27798a7. Newbatchnotvalidated.

## Reviewed worker09 53a53166 — full numeric assignment subset

Reviewed057dc30/def67fe int/float/DWORD assignment ninepairs,original signedFISTP conversion includingDWORDdestination/invalid80000000 and retainedNaNpayloads.4608originalcalls145/145tests37.41s reported. Existingtypedtargets/nearestmaskedmode required;missingtargetinsertion,FPexceptionstatus equivalence notclaimed. Floatbinarycompound/functions/factory/rootremainopen. Exactcommits queued during seventeenthfreeze;379actionbuildpassed,tests underway.

## Reviewed worker03 2281ff98 — retained per-primary composition

Reviewed8e4f19d/716a0c4 same ServerMaps/config/fullcatalog/boundpool/sharedInitRNG/actualZoneMobs startup/tick/reset/removal/death composition.16native two-mapcycles136/136tests35.71s;64worker04originalmapcasesregeneratedbyteidentical,not fullcombinedworldoriginalclaim. Drainactualzone/members/principals then detach whileoldzonealive before replacement;maps/config/servicesoutlivepoolincludingrecycledmobs,routerdestructionalone doesnotdrain. RequiredAI/Lua/KC/linked/transports/lifecycle/rootdispatchremainopen. Exactcommits queued during seventeenthfreeze.

Revieweddfd08d3 doc-only compositioncheckpoint;no newexecution. Explicit03newscope authoritative retainedprimaryZoneMobs lifecycle/leases under mob_primary_zones, reuse existingauthority ifpresent, no competingregistry;productionroot construction hook proposed,main coordinator-owned. Actual requiredbackends stay explicit. Seventeenth275/275tests89.69s passed;SQL/catalog/streamfinalexitpending,sourcefreeze active.

## Reviewed worker01 edf21542 — resolved skill action dispatch

Reviewedd929764/c0ccb72 actual retained skill/resetdeadline/captureddefinition and originaleffect/spend/disable callbackordering, concrete casting scheduler adapter.768originalcomparisons8compositions139/139tests reported. Actualdamage/effects/spending/eligibility/world services remainrequired,no fullcastclaim. WorkercontinuesChkKeepSkill+1428expiration. Exactcommits queued during seventeenthfreeze;SQLstillrunning.

## Reviewed worker02 cb2a3f26 — authoritative death metadata

Reviewed1b6b0c1/d1e0929 sameActor optionalmetadata owner, initializeonce requires explicitinheritedpenalty/soul/code and clearsdeadlineonly; resetprojectiononlydeadline.216originalconstructor/resetprojections143/143tests43.83s plus actualNPCdeath->Rebirth sameActor reported. Originalinitialization preserves otherDWORDresidues;no inventedzeroseed orSQLcolumns. Rootallocation/admissionmustsupplyactualcontext and resetatcorrectstage;realcorpse/PvP/party/transportrequired. Exactcommits queued during seventeenthfreeze.

## Seventeenth batch validated; metadata checkpoint reviewed

Session44687 exited0:build275/275tests89.69s/fullisolatedSQL/fullbindingcatalog/realstreamSQL allpassed at8681196. Logs.local/seventeenth-batch-*.log. Freeze released; publish validated batch. Reviewedf3368dd doc-only metadata checkpoint no new runtime/tests. ActualActorconstruction/admission/reuse must supply explicit inherited metadata and resetatcorrectstage;no zeroseed/shadowpool/SQLcolumns. New queued snapshots excluded from275validation. Objective remainsopen.

## Reviewed worker09 158d5233 — finite operand float arithmetic

Reviewed50da97b/effd181 sameFSMstack/map numericbinary arithmetic,float32integercoercion then53bitoperation/typedresult/truncatingFISTinvalidbits.6144originalcalls146/146tests36.65s reported. Finiteinputs can generateinfinity/negativeQNaN;nonfiniteinputpropagation explicitlyunsupported. Floatcompound/functions/nonnumericcomparison/factory/rootremainopen. Exactcommits queued aftervalidatedpublishedc071248.

## Reviewed worker09 6651c8d4 — finite floating compound updates

Reviewed242e935/7c138da sameFSMmapstack floatcompound updates with originaltwo-popunary/coercion/metadata/pushbeforeVariableIDwrite.5466originalcalls147/147tests38.95s reported. Admittedfiniteinputs only;generatednonfinitevaluesnotreusedaslaterarithmeticinputs. Nonfinitepropagation/functions/factory/rootremainopen;workerproposesactualTravelConditionfunctiondispatch next. Exactcommits queued,latestvalidatedpublishedc071248.

## Eighteenth batch integrated/sourcefreeze

Reviewed933f471/3e265c8 ChkKeepSkill tailonly,strictdeadline/freshclock/livespeed/deathguardcleanup512originalcomparisons140tests reported. Actual periodic/protect/detect visitor prefix+unlock remainsrequired,never substituteexpire/count. Integratedreviewedqueue through 2515956ace68b3a182fb831654ead80fb5f42bfd including map/poolcomposition,deathmetadata,attackdispatch,numericfloatVM. CMake conflicts combined/nesting audited. Freeze during .local/eighteenth-batch-{build,tests,sql,bindings-sql,stream-sql}.log. Latestvalidatedpublishedc071248,newbatchunvalidated.

## Reviewed worker09 80c00ec9 — zeroargument system dispatch

Reviewedb3c480c/94b0988 real TravelCondition virtualdispatch and sixMobRuntimequeries, including consumingattack_at_moment and clampingattacked_count.1536originalexecutions2282actualDoFunctioncalls148/148tests47.92s reported. Must bindcatalog/dispatcher to actualconditionowner+48/+58,not containingFSMsubject or compiledstateactions;factoryassemblystillmissing. CalculateParam/F_OUT/otherIDs/nonfinite/rootremainopen. Exactcommits queued during eighteenthsourcefreeze;382actionbuildpassed.

## Eighteenth initial failure; invitations reviewed

Initialpipeline281/282tests87.64s failed user-skill-runtime-disable-unit missingrequired0902transport; SQL notstarted. Preserve eighteenth-batch logs. Coordinator migratesfixtureScene recordingtransportonly;retryfullpipeline eighteenth-retry logs. Reviewed31db02a/8a0c3e6 raid14/15/18 samepartycreation/invites/responses2048requests99nested139tests+sanitizers. Queued outsidefrozenretry. Explicit05expandedownership existingregistryauto_join byte/rawserialization and normalmembershipkind!=1validation, no secondowner.

## Reviewed worker01 b1eb0bd7 — skill use preparation

Reviewedfa43aba/f291bfb actual UseSkillUser/Mob ready/instant/rejected state/runtime paths and concrete scheduler adapter,1024originalcomparisons4nativepipelines141/141tests40.64s reported. Eligibilityrequiredactualcost->retainedweapon/shield->readiness; no synthetictrue. Captureddefinitions/targets/Actorservicesmustoutlivecallbacks;originalviewexclusionsrequired. Fullaction/spending/periodic/rootremainopen. Exactcommits queued during eighteenthretryfreeze,tests running.

## Reviewed worker08 babe195b — ordinary Death only

Reviewed97087b8/806e118 ordinary/fallbackDeath1552originaltraces16reentrant132/132tests42.86s+ASAN/UBSAN reported;separatecanonicalactualActorrawresources/ledger0902ordering verified. Playermaximumbranch raises explicitrequired exception after originalprefix/GetMax;mustcontinuefromthatpoint,neverrerunprefix orinstallasfullDeath. Rootexchange/shop/vehicle/PvP/corpse/drop/partyexclusion/transport remainrequired. Fourmetadataowner1b6b0c1 nowdeliveredpendingintegration;bindactualActorrefs with explicitheritedcontext. Exactcommitsqueued duringeighteenthretryfreeze.

## Reviewed worker09 07f3f7a1 — CalculateParam values

Revieweda14f20d/a84f256 CalculateParam value-only subset privatecapacity20stack/sharedconditioncursor/sameownermap, calleroutput preserved onexhaustion/invalidtop.2047originalcalls149/149tests42.78s reported after explicittypedvectorinit fixed initialbuildfailure. Operators/nestedcalls/vectorownership/F_OUT/systempositiveparamintegration remainopen. Exactcommitsqueued during eighteenthretryfreeze.

## Eighteenth retry validated; PvP helper reviewed

Session17870 exited0:build282/282tests90.26s/fullisolatedSQL/fullcatalog/realstreamSQL passed49f51f1. Preserve initial281/282failure in eighteenth-batch logs;passing eighteenth-retry logs. Freeze released. Reviewed945a074/40df026 completePvPGetDeathType8192originalcases133/133tests40.55s+sanitizers reported. Actualidentity/asymmetricsubjectduelbeforeguild/rawIDs;forcedPvPDeath branch differsandstillmissing. ActualPvPowner/root/rewardcontinuationrequired;do notrerunpublishedDeathprefix. Exactcommitsqueued,newdeliveriesexcludedfrom282validation.

## Reviewed worker09 798706a7 — argument vectors and F_OUT

Reviewede97c5bc/c515f94 ownedargumentvector/actualdispatch/F_OUTcopyback viaembeddedVariableID,sameFSMmap;livepostcallbackmetadata reread and outputbefore resultpush.1920originalexecutions5721arguments150/150tests52.41s reported. Providersmustnotretainvectorrefs;perownerSysFuncMapcounts/CKFunctionParammetadata+90/+48subject factorybindingrequired. Missingtargets/undefinedfirstargument expliciterrors. CalculateParamoperators/nestedcalls/worldhandlers/factory/rootremainopen. Exactcommitsqueued,latestvalidatedpublisheda24001b.

## Reviewed worker08 14c740f8 — forced/unforced Death route

Reviewed31c98fa/cfd35c5 boundedplayerDeathroute16384originalcomparisons134/134tests39.87s+ASAN/UBSAN afternativeomittedunforcedspecialpenaltywrite corrected;initial133/134andassertionabort preserved. Importantcorrection: Actor::relations alreadyowns duel_state/opponent/guild/opposingguild,priorreportedPvPfieldgapwaswrong;no newfieldsneeded. ActualActor adapter reads sameowner,requiresnonnullretainedattacker afteroneDeathprefix/GetMax/lookup;forced/unforcedspecialpenalty-1 semantics retained. PvPChkLoseDeath/rewards/fullcontinuation/root remainopen. Exactcommitsqueued,lastvalidateda24001b.

## Nineteenth batch integrated/sourcefreeze

Reviewed6ab5cc4/bcdbc69 int/DWORDCalculateParamexpressions private20stack/sharedcursor,vectors/F_OUT;4095direct1920fulloriginalcalls152tests47.48s reported. Assignment/float/nested/worldhandlers stillopen. Integratedreviewedqueue through 4bbe21740b7a850640a670135455b202fa9098c1 including raidinvites,UseSkill,ordinaryDeath/PvProutes,VMsystem/argumentpipeline. NPCdeathfixture conflict manually resolved to incoming mandatoryDBtransport+ordering assertions using canonicalheadercapability;CMakeconflicts combined/nesting audited. Freeze during .local/nineteenth-batch-{build,tests,sql,bindings-sql,stream-sql}.log. Latestvalidateda24001b,newbatchunvalidated.

## Reviewed worker01 310b79ba — complete eligibility coordination

Revieweda0db59e/83333c4 equipment/shield originalrealtypebyteindex and cost->equipment->readiness1344originalcomparisons14nativecompositions142/142tests43.08s reported. SameActorretained_inventory_item bag0slot5/6 requiredcentrally;nonemptyvalue-onlymirror musterror,notabsence. Fulluserdefinition16weaponbytes/statetype/rawpadding source required;no inventedpermissions. Gameplayaction/spending/rootproviders remainopen,workerproposesSendUseSPMP. Exactcommitsqueued during nineteenthfreeze;buildrunning.

## Reviewed worker03 a1e4d715 — retained primary-zone owner

Reviewedd4ea8e5/9a5aabe retainedfacade SAMEZoneRegistry, serializedleases,startup/obelisktimer/reset/drain/reopen.137/137tests44.83s,4focusedASANUBSANcompositions reported;linkedarchivesnormalnotfullyinstrumented. Retiredactualregular/linkedpopulationsretainedthroughpooldestruction;allprimariesmustcompletedrainbeforelastrootrelease,leasesprotectzone not recycledactoridentity. ActualZoneServicesfactory/partialInitrelease/AI/Lua/catalog/clocks/KC/transports andmainstartup/frame/shutdownstillmissing. Exactcommitsqueued during nineteenthfreeze;344actionbuildpassed.

Reviewed4c1a2fc doc-onlyprimaryownercheckpoint,no newexecution. Explicit03newscope concreteZoneServices/RuntimeZonefactory usingactualcanonicalowners,partialInit/release/reopen,requiredrealbackends retainedinputs,no no-opdefaults/secondregistry. New mob_zone_services prefix;main remainscoordinator-owned. Nineteenth292test suite running/sourcefreeze active.

## Reviewed worker01 9e2cc739 — skill spending transition

Reviewedf42b478/c5b4eb8 SP/MPdebitandself050c4096wholeoriginalnopartyexecutions143/143tests35.43s reported. Cost1usescurrentrawsignedresource sentinel,bothcostcalculationsstillrun,clampednonnegativerequests/strictsignedresourcecomparison/WORDdeltas. ActualActorrawresourcesbinding and group-tail callbacks/locks/live membership stillrequired;notfullspendingservice. Workercontinues nonnullgroup originaltraces. Exactcommitsqueued during nineteenthfreeze;292/292tests91.11s passed,SQLpipelinepending.

## Nineteenth batch validated; PvP loss reviewed

Session49796 exited0:build292/292tests91.11s/fullisolatedSQL/fullcatalog/realstreamSQL passed4bbe217. Logs.local/nineteenth-batch-*.log. Freeze released. Reviewede8f1f1b/56125c6 completePvPChkLoseDeath2064originaltraces16reentrant135/135tests+sanitizers/canonicalsameledger0902reported. Realbilling+578c mustnotaccount/entityID,guildPvPUserLeavereason3 notnormaldisconnect,actualwager/locks/transportsrequired. Ordinaryplayerrewards/fullsingleDeathcontinuation remainopen. Exactcommitsqueued,newdeliveriesexcludedfrom292validation.

## Reviewed worker09 e211fd0c — recursive argument calls

Reviewedf76a372/7c03696 recursiveCalculateParam samecursor/perframeprivate20stack/argumentvector and F_OUT beforeoutercontinuation.4145originalDoFunction4635CalculateParamcalls5simultaneouslevels153/153tests58.57s reported. Never invoke newTravelCondition forchild (wouldresetmainstack/cursor). Argumentassignment/compound/float/worldhandlers/nonfinite/factory/rootremainopen. Exactcommitsqueued,lastvalidatedpublisheda6cf428.

## Reviewed worker01 423c26fc — spending group tail

Reviewedf890116/7331a18 capturedrecursivegrouplock/livemembership and energy rereads,SPthenMPordering480wholeoriginaltraces144/144tests27.64s reported. Originalgroupfanout boundaryonly,sourceexcluded00445680 senderstillmissing;mustreuse same30slotparty/sharedsendbuffer,never broadcasttosource. Nativeinvalidkind/nullMP/throwcasesexplicitfailureunlock,noundefinedoriginalequivalence. Rootactualrawdebit/partybindings remainrequired. Exactcommitsqueued,lastvalidateda6cf428.

## Reviewed worker03 b28b16b3 — concrete zone factory

Reviewedff62d6c/22ebd6c actualRuntimeZonefactory/canonicalcellowners/users/npcs/corpses/items/regularlinkedpopulation, retainedperzoneproviders/partialInitcleanup/release/reopen.138/138tests6ASANUBSANscenariosreported. Requiredsameboundpool/sourcecatalog/Init/worldcorpses/backendlifetimes;pooloutliveszonecleanup andretiredpopulations/servicesoutlivepool. Users/corpses/items mustdrainbeforerelease;missingAI/Lua/transport/KC remainexplicit,nosuccessdefaults. Exactcommitsqueued,latestvalidateda6cf428. Mainstartupshutdownassemblypending.

## Twentieth batch integrated/sourcefreeze

Reviewed495196f doc-onlyzonefactorycheckpoint,no newtests. Integratedreviewedqueue through 3620439ae843a940e674627eb8926e551c4eb5b5 including actualprimaryzoneowner/factory,eligibility/debit/grouptail,PvPloss,recursiveargumentcalls. CMakeconflicts combined/nesting audited. Freeze during .local/twentieth-batch-{build,tests,sql,bindings-sql,stream-sql}.log. Latestvalidatedpublisheda6cf428,newbatchunvalidated. Actualperzoneworld/connection/AI/transportprovidersandmainassemblyremainmissing;no successfulplaceholder.

## Reviewed worker09 b18c69f7 — argument integer writes

Reviewed60e3e18/8e3e456 CalculateParamassignment/compoundintDWORDsameFSMmap/private20stack/sharedcursor;directVariableIDzero writes unlikeF_OUTzeroskip.4095direct1536nestedoriginalcalls155/155tests57.72s reported. Existingtypedtargets required,missingtarget errorbeforequery,noindeterminateinsert. Argumentfloatmatrices/worldhandlers/nonfinite/factory/rootremainopen. Exactcommitsqueued during twentiethsourcefreeze.

## Reviewed worker01 7cd00ffb — selected strike coordination

Reviewedbcff828/ba6cd04 selectedskilldetail21..27/multihitlivecount/deathguards/selfsacrificeroute usingactualActorrelations/resources/mode.6400originalcomparisons4nativeattackcompositions145/145tests30.78s finalreported. Intermediate144/145world-networkreplacementregistryfailure preserved,nofixclaim. Actualhit/damage/Death/range/announcementservicesremainrequired;no newActorstate. WorkerproposesSendUseSkill. Exactcommitsqueued during twentiethfreeze;332actionbuildpassed,299testsnearingcompletion.

## End of day requested by user — 2026-11-04

User explicitly requested stop all sessions, update documentation and power off after preservation. Continuous scheduling disabled via tools/parallel.py stop; objective remains OPEN, do not auto-resume tonight. Let current worker blocks finish before closing. Twentieth pipeline11245 exited0:build299/299tests103.07s/fullisolatedSQL/fullcatalog/realstreamSQL passed source3620439. Logs.local/twentieth-batch-*.log;freeze released. Latest handoff08 378d196f HEAD481b36c received and queued pending review,not integrated. Other reviewed pending queue entries remain for next session. Publish validated batch, preserve every worker checkout/HEAD/status and dirty changes before shutdown. Resume manually from queue/snapshots after user request; no playable-server/globalcompletion claim.

## Final shutdown preservation

Validated twentieth batch published5ea007d. All external Codex worker executions stopped gracefully by SIGINT at user request; interrupted blocks may show failed exit130, which is intentional shutdown rather than test failure. Scheduler disabled and Eternity worker terminals/supervisor closed. No automatic continuation tonight. Every worker HEAD/dirty status, binary patch and untracked-file archive saved under .local/parallel/end-of-day-2026-11-04/manifest.json and matching files; worktrees preserved in place, including unfinished code, not marked validated.

- 01-mob-damage: HEAD ba6cd0456595cbba42f72185ef5a5d013707e670; dirty=True; untracked=5.
- 02-mob-drops: HEAD f3368dde4377552065734a3bf31f8205485039e1; dirty=False; untracked=0.
- 03-mob-skills: HEAD 495196fe8849a35f10662c8dd89b08465306125b; dirty=False; untracked=0.
- 04-obelisks: HEAD f2d96ede6b137231e6cf6d29f6fef40ae84b8d41; dirty=False; untracked=0.
- 05-party-state: HEAD 8ce709586dd4e4a446b08657de80b5ef417d7008; dirty=True; untracked=0.
- 06-quest-start: HEAD 2e5413deedf0aff7808f5160bbd3858dd85136e9; dirty=False; untracked=0.
- 07-quest-requests: HEAD c674ca72e5db76c54e69c12d30e4238f16638f63; dirty=False; untracked=0.
- 08-mob-targeting: HEAD 481b36c0c6e7673904f5b3603747ab925e43c439; dirty=False; untracked=0.
- 09-mob-events: HEAD 8e3e456fe16d0c771bc3ed0840f98e086515f260; dirty=True; untracked=7.

Next session: inspect final preservation manifest and latest worker rounds; review pending queue, then resume only on user instruction. Objective remains open; estimated15-20percent with lowconfidence,not playable. Shutdown after final doc publication and filesystem sync.

## Resume — 2026-11-04

User requested continued work toward playable EP4.5. Integrated reviewed 60e3e18/8e3e456 and bcff828/ba6cd04 as f15d705..f52bbbf; combined CMake test blocks. Container build succeeded, 302/302 tests and isolated SQL integration passed; logs .local/resume-{build,tests,sql}.log. Full catalog and stream SQL checks not rerun yet. Worker 08 commit 9e7c0d8 remains pending review; preserved dirty worker work untouched. Actual world entry and runtime providers remain open; server not playable.

## Independent sessions — 2026-11-04

Integrated reviewed 9e7c0d8/481b36c as bfc2502/18a3891. Container build, 303/303 unit tests and isolated SQL integration passed; logs `.local/rewards-final-{build,tests,sql}.log`. Full catalog/stream SQL checks were not repeated. User requested independent terminal work: nine new Terra sessions on separate `independent/*` branches from 18a3891; previous worker worktrees and dirty work preserved. `INDEPENDENT_WORK.md` and `tools/independent_status.py` describe assignments and status. Sessions are separately prompted, manually assigned bounded follow-ups and do not auto-restart; review each report and validate its changes before integrating. Main runtime still has roster-only application and no playable world entry.

## Coordination checkpoint

User resumed the active objective. Nine independent Terra sessions remain open; see `python3 tools/independent_status.py`. Follow-up assignments sent manually to 01-entry, 04-movement, 06-combat, 07-inventory and 08-quests. Reviewed drafts expose two integration hazards: 01-entry and 09-persistence must share exactly one Players::load (JoinDate and gift effects), and 02-world and 05-mobs must not execute population/respawn twice in one frame. Notifications queued to both relevant pairs. Independent branches are not yet integrated or validated centrally; several builds are queued under shared locks. 06-combat has bounded original caller evidence but still requires actual AttackSuccess/AttackDamage providers; 04-movement initially reported missing live owner services. Continue monitoring live sessions and review completed deliveries rather than treating report presence as completion.

`tools/dev.py test` previously ignored all trailing CTest arguments, so apparent
filtered worker commands could run the entire suite. Fixed in f899785 to forward
arguments; `integration` now rejects unexpected arguments instead of silently
running the entire SQL suite. Mocked command construction verifies forwarding
and rejection. Existing worker checkouts predate this fix; do not describe their
runs as filtered unless the actual CTest output establishes it.

## Independent combat caller integrated for validation

Reviewed original caller fixture and integrated independent `abe9b2b`, `598421d`,
`856d536` as `4865abe`, `603e6c4`, `2b49ef0`. These recover AttackMob caller
ordering against intercepted original callees; they do not implement actual hit
calculation, world dispatch or full combat. Main validation pipeline is waiting
for the shared build slot, logs `.local/independent-combat-{build,test,sql}.log`.
Worker 06 received a bounded AttackSuccess follow-up; preserve required actual
providers and review independently before integrating.

Observed 05-mobs cancelled its locked build while Docker container a6096c7fe556
continued compiling after the wrapper exited, leaving work outside the lock.
Stopped that exact orphan; partial worker build remains unvalidated. Main
`tools/dev.py` now gives build/test containers unique names and stops them on
interruption before releasing the lock (0c4fb6e); mock verifies cleanup and CTest
argument preservation. Existing independent checkouts still have the old wrapper;
avoid cancelling their builds and verify no orphan containers on interruption.

Independent 09-persistence native build completed and its 304/304 tests passed
in `.local/vertical-persistence-test.log` in that worktree; adapter remains
unintegrated and has no direct new SQL round-trip validation. Independent 05
revised mob frame ownership (`ea27be2`, report `002463a`) to leave respawn to
world frame. Its report requires idle traversal then death cleanup then single
respawn. The 02-world draft previously ran population before the callback; both
sessions received this ordering issue. Revised 05 build was interrupted and is
unvalidated. Central combat validation session 99411 remains live waiting for a
shared build slot; do not restart it solely because it is waiting.

Independent 09 handoff `c0ebb2b` confirms 304/304 native tests but no direct SQL
round trip for the new adapter. Coordinator clarified that existing integration
may create/drop its own disposable catalogs without modifying source databases,
and assigned a selection/save/reload SQL test plus single-load integration.
Independent 06 has new `5c1e82b` AttackSuccess evidence and code (8192 normal,
4096 skill original traces reported), still awaiting independent review/build;
no actual AttackDamage or live root claim. Central combat validation remains in
live session 99411 waiting on shared build slots.

Coordinator independently regenerated both `5c1e82b` AttackSuccess original
fixtures in the independent 06 checkout and byte-compared them to the checked-in
vectors: 8192 normal and 4096 skill cases match. This verifies fixture
reproducibility, not the native implementation or completeness beyond fixture
coverage. Requested explicit x87/native floating-environment and zero-divisor
review before integration; delivery remains unvalidated on main.

02-world full build completed, but its test source includes another unit with
`main` renamed, producing `control reaches end of non-void function` for the
renamed function. Coordinator notified worker 02 to remove the undefined
behavior and revalidate. Its frame ordering remains under review. Worker 05
received a bounded assignment to recover the actual ZoneMobs shared-cursor
iteration with canonical owner/lifetime semantics; no integration claim yet.
Central validation session 99411 remains live and waiting for a build lock.

Independent 03's in-progress original packet-gate fixture indicates location 3
accepts opcode 0201 for world entry. Current `WorldProtocol::receive` drops the
non-01 opcode family while in character selection, so the connection must move
its protocol/cipher state at the original transition before that packet arrives.
Coordinator requested exact transition and crypto evidence plus a TCP test from
03. The fixture and adapter remain uncommitted/unvalidated; do not infer entry
works from this gate alone. Session 99411 remains live waiting on build locks.

Original ps_game.exe disassembly shows successful PacketUserDBChar writes
CUser+5756=1 at 0046d6f5, immediately before flush 0046d6fc; the same flag is
CUser::LeaveCharSelect 00466c80. Incoming traffic retains AES per existing crypto
research. Sent evidence to independent 03 for exact socket/cipher transition,
ordering relative to previous entry sends, world location 3 and 0201, and TCP
regression. This evidence does not yet establish a working world entry.

Coordinator independently regenerated independent 03's in-progress original
PacketProcessing gate vectors (60 cases) and byte-compared them successfully;
the native gate/actor admission composition and TCP cipher transition remain
unvalidated. These inputs are currently uncommitted in worker 03 and are not a
claim that original client world entry works.

Coordinator regenerated independent 04's 4096 stUserMoveChk::Update original
fixture cases and byte-compared successfully. The fixture executes original
Update/Reset and CRT sqrt with patched deterministic clock boundary; this
verifies reproducibility only. The native movement implementation is still
uncommitted and awaiting compilation/tests. Main validation 99411 remains live.

Coordinator regenerated and byte-compared independent 07's in-progress
PacketMaininterface 0205 pickup-handler fixture. It executes original packet
handler with patched cancellation/shop/pickup boundaries, including packet ID
mutation during callbacks. Reproduction passed; native implementation remains
uncommitted and its build/test is queued, so no pickup or SQL validation claim.

`tools/targeted.py` supplies a locked, isolated container build of one CMake
target followed by its exact CTest name, with no-tests=error and interrupted
container cleanup. This avoids a full 859-target initial build for focused worker
checks without bypassing shared locks. It accepts only Eternity Git worktrees.
Command construction was mock-checked; a real container run remains to verify.
All nine sessions were notified to use it only after their current builds finish.
Full SQL and integrated regression validation remain separate requirements.

Independent 04 committed `885b02d` movement checker and its incremental build
completed, but the new test suite is still queued. An SQL run failed at
creation-network-integration; no pass should be claimed until a successful
retry is observed. Coordinator requested review of nearest rounding, FTZ/DAZ,
nonfinite and sqrt assumptions against original x87 before integration. Worker
06 is adding explicit floating-environment guards to its uncommitted AttackSuccess
follow-up; review that final source and tests before integration.

Coordinator identified a network transition race: `Connection::read` decoded and
filtered frames while a previous application call was still changing selection/
world state on its worker strand. A pipelined AES 0201 could be discarded before
`enter_world` reached the socket executor. Main now queues up to eight undecoded
frames and processes them after the preceding application call and its ordered
channel posts. Added a TCP application regression that sends a world frame while
the selection transition is blocked. Validation pipeline session 69227 runs
focused target, build, relevant network/combat tests and isolated SQL; it is
waiting for the shared build lock. This remains unvalidated until it finishes;
original timing and complete playable entry still require further evidence.

Independent 07 committed pickup request adapter `1a5d2f7` and report `552f881`;
512 original handler cases regenerated successfully, but native build/test and
SQL integration have not run. Its focused runner is waiting on a build slot.
Keep unintegrated until validation and review of actual action/shop/Actor/zone
bindings. Main serialized-transition validation session 69227 also remains live
and waiting; do not restart while its handle is active.

Independent 04 full suite failed `world-network-unit`: peer observed replacement
socket EOF while Registry still contained the login. Coordinator found
`LoginServer::Connection::close` closed the socket before release_login. Main
55c1644 moves guarded release before the socket shutdown; session identity and
location checks remain in Registry. No validation claim yet. Cancelled main
session 69227 while it was still waiting on the build lock (no container was
started) because source changed. Replacement session 53983 tests world-network,
world-application (including pipelined 0201), then builds and tests relevant
network/combat targets plus isolated SQL. Preserve original failing worker log.

Main focused `world-network-unit` passed after login release ordering fix using
`tools/targeted.py`; `.local/world-close-targeted.log`. The runner's first real
container invocation therefore also passed. The pipelined world application
regression remains queued in live session 53983, followed by build/network/
combat/SQL validation. Independent 06 reports 306/306 tests and isolated SQL
passed for `bad2e9f`/`770173d`; review and central integration remain pending.
Original AttackSuccess incoming x87 control can change outcomes; worker explicitly
limits its native implementation to measured 53-bit nearest/masked environment.

Coordinator inspected `bad2e9f` AttackSuccess final implementation: explicit
nearest/masked checks, binary64 staging, original integer wrap and invalid FISTP
boundaries are present, with the documented restriction to 53-bit precision.
Native worker evidence remains 306/306 and isolated SQL; root binding and
AttackDamage are absent. 06 was assigned AttackDamage 0044d540. Main network
application test session 53983 remains live, waiting for the shared build slot.

Coordinator independently executed 06's original FP oracle: AttackSuccess
returns different results for controls 027f vs 037f and for 027f vs 067f, as
reported. The CRT default-precision helper behavior also reproduced, but that
alone does not establish each real combat thread's control word. Requested
worker 06 trace actual combat executor initialization before production binding.

Independent 01 delivered selection adapter and SQL test, but its new SQL path was
not run because the disposable creation catalog did not exist. Coordinator
explicitly authorized using the existing uniquely named disposable creation
harness and assigned integration of the test; existing databases must not be
reset. 01 owns that harness change; 09 supplies its separate fixture requirements.
The original DB-agent 0407 path checks the retained five-slot roster before load;
`vertical_entry_select` appears to call Players::load directly. Requested 01
verify/adapt the live roster gate and test eligibility against original evidence
before integration. Do not assume SQL ownership check replaces the roster gate.

Independent 02 full suite produced 303/304: its new `vertical-world-unit`
aborted with `terminate called without an active exception`; see worker
`.local/vertical-world-tests.log`. Coordinator requested diagnosis, corrected
idle/death/respawn order and rerun. `5c3819a` is not ready to integrate despite
its successful build. Main session 53983 remains live waiting for shared build
slot; current 01/02 worker suites hold both slots.

Main `world-application-unit` passed the new TCP pipelined-transition regression
in `.local/world-serialized-application-test-final.log`; the focused runner built
and executed the exact test under the shared lock. Main session 53983 continues
with the full native build, focused network/combat regression and isolated SQL;
those remaining phases have not passed yet. This TCP test does not establish
original-client world entry or complete correctness of production admission.

Independent 03 delivery `9473a9b`, `0c8ff0d`, `66762c1`, `669e675` now reports
successful native build, 305/305 tests and isolated SQL retry. Coordinator
verified logs and previously reproduced original gate fixture; root integration
remains pending until main validation 53983 completes. Required next evidence:
original location-2 waiting packet queue and when early 0201 is processed after
zone assignment, plus cipher/flush/TCP ordering. Worker 03 was assigned this;
its current adapter does not itself implement a live connection or scheduler.

Coordinator inspected ps_dbAgent.exe PacketUserChar 004059f0..00405a25:
the 0407 branch scans five retained entries at stride 0x5c, rejects when the
entry +0x58 signed DWORD is greater than zero or entry +0x28 is nonzero, then
posts job 7. Native Players::load currently queries `RemainTime=0` and
CharacterSummary collapses remaining-time to bool. This does not prove the SQL
raw value maps unchanged to that retained field; worker 01 was asked to trace
loader conversion and negative values before altering or integrating the gate.
