# Eternity parallel work

The user authorized nine independent worker terminals plus the existing main
coordinator on 2026-11-03. Each uses `gpt-6.1-sol`, reasoning **low**, the current
`fast` tier, full access and approval policy `never`. Authentication and other
Codex settings come from the existing user configuration; no new API key is used.

Launch the desktop `Iniciar-Eternity-9-terminales.sh`, or run:

    python3 tools/parallel.py launch
    python3 tools/parallel.py status

`plan` previews settings/tasks; `prepare` creates worktrees without opening agents.
The launcher does not duplicate live workers or rerun finished assignments.
Worker reports and session IDs are in ignored `.local/parallel/workers/`.

## Ownership

The main checkout `/home/franco/Projects/Eternity` remains the coordinator's only
integration branch. Workers use independent `workers/<task>` branches and folders
under `/home/franco/Projects/Eternity-workers`. They commit only to their branch;
only the coordinator merges and pushes. A finished worker writes its handoff under
`reports/parallel/<task>.md`; it does not take another worker's domain.

| Worker | Exclusive subsystem / new file prefix |
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
