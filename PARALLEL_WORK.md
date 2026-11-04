# Eternity parallel work

The user authorized nine independent worker terminals plus the existing main
coordinator on 2026-11-03. Each uses `gpt-6.1-sol`, reasoning **low**, the current
`fast` tier, full access and approval policy `never`. Authentication and other
Codex settings come from the existing user configuration; no new API key is used.

Launch the desktop `Iniciar-Eternity-9-terminales.sh`, or run:

    python3 tools/parallel.py launch
    python3 tools/parallel.py status

`plan` previews settings/tasks; `prepare` creates worktrees without opening agents.
The launcher does not duplicate live workers. A durable supervisor resumes each
existing Codex thread for successive implementation blocks. Worker reports,
milestone snapshots and session IDs are in ignored `.local/parallel/workers/`.
It is a Python scheduling process, not an additional model/session.

## Ownership

The main checkout `/home/franco/Projects/Eternity` remains the coordinator's only
integration branch. Workers use independent `workers/<task>` branches and folders
under `/home/franco/Projects/Eternity-workers`. They commit only to their branch;
only the coordinator merges and pushes. Each milestone writes a handoff under
`reports/parallel/<task>.md`; it does not take another worker's domain.

| Worker | Initial exclusive subsystem / new file prefix |
| --- | --- |
| 01 | Mob damage and credit / `mob_combat` |
| 02 | Mob loot and drops / `mob_drops` |
| 03 | Mob applied skill ownership / `mob_applied_skills` |
| 04 | Obelisk ownership/respawn / `obelisk_runtime` |
| 05 | Party state and reward range / `party_runtime` |
| 06 | Quest admission/start / `quest_start` |
| 07 | Quest end requests/item requirements / `quest_requests` |
| 08 | Mob targeting / `mob_targeting` |
| 09 | Mob FSM events / `mob_events` |

Core QuestEnd/Select, QuestSuccessResult, quest removal/ownership, mob logging and
their integration belong to the main coordinator. Workers must not duplicate
those active assignments. Shared canonical-header changes are requested in the
handoff, not made independently. CMake registration is allowed in a worker's
private checkout and reviewed centrally. Do not edit another worktree/main.

## Continuous coordination

The supervisor snapshots a finished block before restarting that same thread.
Continuation reads the latest main continuity/coordination notes and selects
missing meaningful behavior within the worker's ownership. It does not repeat
completed work. The coordinator may adapt a worker's scope using:

    python3 tools/parallel.py assign 09-mob-events 'Concrete next implementation' --prefix mob_ai_states

Assignments are stored in `.local/parallel/control.json` and apply at the next
milestone. Prefixes already owned by another worker cannot be claimed. A worker
with a concrete dependency ends with `WAITING_FOR_ASSIGNMENT`; it waits for a
new assignment instead of spending model turns polling or duplicating another
front. The coordinator reviews and integrates committed milestones, resolves
dependencies and assigns the next work. Notifications are queued to the same
coordinator thread through `codex queue --resume`; an active writer may defer
delivery, which the supervisor records without claiming a new turn began.

New terminal services keep their window alive between blocks. Existing first-run
terminals upgrade when their current block ends; only a proven single-session
finished worker window can be retired. No active worker is interrupted for this
upgrade. Failure retries have backoff; persistent/account failures wait for the
coordinator instead of looping without progress.

The scheduling loop has no fixed number of successful rounds. Only verified
completion of the original playable-server objective may set `objective_complete`.
Passing tests or finishing one subsystem is not global completion. The user can
stop scheduling after current blocks with `python3 tools/parallel.py stop`, and
explicitly resume it with the desktop launcher. `supervise` attaches the scheduler
to already-running workers without launching duplicate model sessions.

## Resources and evidence

`tools/dev.py` automatically permits two simultaneous builds/tests and serializes
isolated SQL integration. `tools/reverse.py` serializes Ghidra across worktrees.
Locks live in the common Git directory and release when the owning command exits.
Do not bypass these wrappers for shared operations. Do not start, restore, reset,
provision or change database schemas. The coordinator owns SQL lifecycle/cleanup.

Each worktree has private build, test, fixture and log paths, copied runtime
assets and private credential files (mode 0600). No credentials are printed or
tracked. The shared original Ghidra project is accessed only through the locked
wrapper, with `--process --output work/exports/<worker>`; never re-import/reset it.
Existing exports in the main `work/` tree may be read as evidence.

Preserve original binaries, cite symbols/addresses and compare real original
execution with native behavior. Required providers cannot silently become no-ops.
Keep native C++20, SQL Server, English code/docs and Spanish updates. Client work,
SQLite, ps_gamewar and Phoenix remain out of scope. The server is still not playable.

CLI/configuration parameters were checked against installed Codex CLI 0.166.0 and
[official CLI documentation](https://learn.chatgpt.com/docs/developer-commands?surface=cli).
