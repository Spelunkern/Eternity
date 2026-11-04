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
