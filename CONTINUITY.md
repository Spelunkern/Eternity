# Eternity continuity — paused 2026-11-02

The user explicitly paused work for today. Resume only when requested.

## Objective and constraints

Reconstruct the complete EP4.5 server natively on Linux in C++20. Completion means an original EP4 client can connect and play with original server behavior. Client work is deferred; continue the server independently when resumed. Use Ghidra for reverse engineering and SQL Server for now; SQLite migration is deferred. Exclude ps_gamewar and all Phoenix work. Preserve original binaries. Keep documentation/code in English and progress updates in Spanish. Report meaningful changes to the rough global estimate, currently about 7%; the server is NOT playable. Do not equate test counts with completion.

Repository: https://github.com/Spelunkern/Eternity, branch main. Local checkout: /home/franco/Projects/Eternity. Last published implementation: e14c0fa (actor experience, level notifications, maximum resource delivery and corrected user-ID routing). Read AGENTS.md before continuing. Do not spawn subagents without explicit authorization.

## Uncommitted work — preserve it

The working tree contains the next implementation, not yet committed or published:

- MobRuntime owns a unique MobDamageLedger. MobPoolServices now requires damage_pool(); all pool fixtures provide a shared heap through tests/mob_damage_fixture.hpp. Fresh allocation binds the ledger; Init/recycling retain its records and fair-combat flag.
- Damage ledger destruction now deletes active records instead of returning them to the shared heap. Explicit reset still recycles records and restores fair_combat=true.
- Original mob-pool fixture expanded to 128 pools / 2,432 operations, including embedded damage accumulation, recycling, reuse and explicit reset. Native comparison passed.
- New destruction fixture covers eight original Add/reset/destruction/subsequent-allocation scenarios. Native destruction and existing shared-pool tests passed.
- SQL population fixture checks damage ownership, retained credit through bulk release/reinitialization, and explicit reset. Its latest complete integration result is still pending.
- research/protocol.md records the new evidence. git status is authoritative; do not reset or discard these changes.

## Validation at pause

Latest normal build completed and all 117 native tests passed (30.04 seconds): .local/mob-damage-ownership-final-build.log and .local/mob-damage-ownership-test.log.

Two checks were still running when continuity was written; do not call them passed without inspecting their terminal results:

- Full ASAN/UBSAN: .local/mob-damage-ownership-sanitized.log; exec session 23766.
- Full isolated SQL integration: .local/mob-damage-ownership-integration.log; exec session 88271.
- Normal build/test orchestration session 73196 had printed all tests passed but had not yet returned its final exit status at the last poll.

These already-authorized checks may finish while the agent is paused. On resume, poll the existing handles or inspect authoritative process/container state before restarting anything. SQL integration creates disposable fixtures and uses cleanup/rollback; avoid interrupting it in the middle of cleanup. Never reset existing databases. Credentials and generated assets remain ignored under .local/work.

## Evidence and next steps

Ghidra exports (ignored):

- work/mob-damage-ownership/ps_game.exe: CMob constructor 0048fd50, destructor 0048ff10, Init 00490040. Damage owner is embedded at 01f0, initially fair=true. Init and FreeMob 0040a630 retain damage state.
- work/damage-list-release/ps_game.exe: SSyncList<stDamageMob>::Release 004b9480. Ghidra marks operator_delete as noreturn and omits the following loop in its listing. Raw bytes and full original execution confirm ADD ESP,4 / CMP / JNE after the call, deleting every active record.
- CDamageMob destructor 004b8cf0 calls that release; Reset 004b8d10 returns records to shared LIFO heap. Add is 004b8dd0, winner queries 004b8ef0/004b9000.
- work/mob-death-lifecycle/ps_game.exe: complete CMob::Death 004910f0, still to reconstruct. It selects credit, sends 0606, awards solo/party/raid experience, invokes drops/quests, handles linked/obelisk transitions, logs, resets damage, then clears applied skills. Assembly guards prevent duplicate principal transitions; do not follow misleading decompiler duplicates.
- work/mob-death-rewards/ps_game.exe contains damage selection, experience, drops, skill cleanup and logging dependencies.
- work/mob-exp-awards and work/experience-level-up contain experience application and level transitions already reconstructed; work/level-up-resources and work/level-up-party-resources contain maximum/resource announcements.

First resume action: verify pending checks, fix any failures, then commit/push the current damage-ownership implementation only after validation. Next connect the existing damage ledger, experience/actor coordinators and linked/obelisk APIs to complete CMob::Death, preserving party/raid sharing, drops/quests, log and cleanup ordering. Production world integration and original-client play remain far from complete.

Important existing boundaries: --serve supports login/roster/faction/creation, but character selection still does not enter the world. Live AI, combat dispatch, complete death rewards, obelisk ownership and the production world scheduler remain incomplete. Guild restored-schema Rank incompatibility remains documented; only disposable integration catalogs are patched for that fixture.

## Commands

Use the local Docker installation through tools/dev.py:

    python3 tools/dev.py build
    python3 tools/dev.py test
    python3 tools/dev.py integration

Ghidra 12.1.4 / JDK21 paths are under /home/franco/.local/share/castaway-tools. Use tools/reverse.py --process; run only one Ghidra process at a time. Original-execution fixtures use .local/oracle-env/bin/python. Never replace fixture files while running checks may read them. Verify originals with sha256sum -c SHA256SUMS. Do not print runtime.env or credentials.
