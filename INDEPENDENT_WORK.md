# Independent work — 2026-11-04

**Historical, finished.** No workers run now and these worktrees receive no new work.
Their results are in `main`; current status is in [PROGRESS.md](PROGRESS.md) and
[CONTINUITY.md](CONTINUITY.md).

Goal: original EP4.5 client connects and plays with original server behavior. No
claim of playable completion. Nine fresh interactive Codex sessions start from
18a3891 in separate worktrees and branches; they do not auto-restart. Previous
worker worktrees and preserved dirty changes remain untouched. The coordinator
reviews and integrates each delivery on main; required shared owners, SQL and
Ghidra use the existing locking discipline. See `.local/independent/*.prompt`
for exact assignments and `.local/independent/*.sh` for terminal launchers.

| ID | Worktree | First delivery |
|---|---|---|
| 01-entry | `Eternity-independent/01-entry` | Character selection and DB load |
| 02-world | `Eternity-independent/02-world` | World startup and lifetime |
| 03-entry-packets | `Eternity-independent/03-entry-packets` | World entry protocol |
| 04-movement | `Eternity-independent/04-movement` | Movement and visibility |
| 05-mobs | `Eternity-independent/05-mobs` | Mob runtime and AI integration |
| 06-combat | `Eternity-independent/06-combat` | Combat integration |
| 07-inventory | `Eternity-independent/07-inventory` | Inventory and loot integration |
| 08-quests | `Eternity-independent/08-quests` | Quest and NPC integration |
| 09-persistence | `Eternity-independent/09-persistence` | Persistence and reconnect |

Each session must report in `reports/independent/<id>.md` in its own checkout:
commit, original evidence, checks, exact integration dependencies and limits.
Each branch owns its distinct `vertical_*` source prefix and private CMake
registration. Main integration must preserve actual shared Actor, zone, pool,
connection, SQL and lifetime owners; reject fabricated success providers.
First cross-system milestone: authentication, selection, entry, movement,
disconnect and reconnect with actual retained services and protocol evidence.
