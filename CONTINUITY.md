# Eternity continuity — active 2026-11-03

The user resumed complete native server reconstruction. Continue server work.

## Objective and constraints

Reconstruct the complete EP4.5 server natively on Linux in C++20. Completion means
an original EP4 client can connect and play with original server behavior. Client
work is deferred; continue the server independently. Use Ghidra and SQL Server;
SQLite migration is deferred. Exclude ps_gamewar and Phoenix. Preserve originals.
Code/documentation in English, progress in Spanish. Rough global estimate remains
about 7%; the server is NOT playable. Test counts do not measure completion.
Do not spawn subagents without explicit authorization. Read AGENTS.md.

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
