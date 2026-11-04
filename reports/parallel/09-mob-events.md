# Worker 09 — mob FSM events

## Evidence and implementation

Read-only Ghidra exports: `work/reverse/ps_game.exe/{decompiled.c,disassembly.txt}`
in the coordinator checkout. Original `ps_game.exe` SHA256:
`0fb7c9f13b5bcf186118f8bfe73ab214226b93e8aa502b8e6fded4a8312bc547`.

Recovered symbols:

- `CMob::OnMove` 004b60d0: empty event handler.
- `CMob::OnMoveEnd(EventObject*)` 004b60e0: clear move mode (+2a0).
- `CMob::OnMoveEnd()` 004b60f0: conditional Lua `OnMoveEnd(tick)`;
  preserves function discriminator. Assembly passes the mob in ESI.
- `CMob::OnAttacked` 004b6120 / `OnAttackable` 004b6170: set discriminator
  1/2, call named Lua function with tick and EventObject argument (+8), clear it.
- `CMob::OnNormalReset` 004b61c0: clear only-Lua-attack (+290), combat timeout
  (+280), current attack (+130), regardless of Lua availability; discriminator 3.
- `CMob::OnDeath` 004b6220 / `OnReturnHome` 004b6270: discriminator 4/5,
  pass attacked count (+28c), not the event argument; clear discriminator afterward.
- LuaCall overload boundaries 004b1090 / 004b1140 take tick / tick plus argument.
  Lua state (+27c) is loaded after GetTickCount (IAT 005210e4).

These handlers do not transition the state stack, perform death rewards, select
new targets or move actors. The implementation preserves that scope. Callback
writes survive except for the explicit discriminator clear. Uses existing
MobRuntime fields directly; no canonical header changes are needed.

## Validation

- `sha256sum -c SHA256SUMS`: all 12 original EXE/PDB files match.
- `.local/oracle-env/bin/python tools/mob_events_oracle.py`: 512 original
  executions; fixtures generated under ignored work before copying. A second
  generation compares byte-for-byte equal. The oracle asserts return address,
  stack cleanup, Lua state, function name, arguments and callback-visible fields.
- `python3 tools/dev.py build`: exit 0, no compiler warnings;
  log `work/mob-events-build.log`.
- `python3 tools/dev.py test`: exit 0; all 120 tests pass (50.65 s),
  including `mob-events-unit` (512 original comparisons, 0.01 s);
  log `work/mob-events-tests.log`.
- `git diff --check`: clean.

The cases cover eight handlers, zero/nonzero Lua flag (including negative bit
patterns), uint32 tick wraparound, distinct event/count inputs, reset behavior
with Lua disabled, and provider mutations that survive the call. Native tests
also verify the state stack/status remain unchanged. Tick/Lua are explicit
observed ports in the oracle; this does not claim original Lua script execution.

## Remaining integration

The combat/FSM owner must invoke `dispatch_mob_event` at its recovered event
sites, distinguish the two move-end overloads, and project EventObject argument
as uint32. Supply a real `MobEventServices` implementation: monotonic original
32-bit tick semantics and Lua execution using `MobRuntime::lua_state`, including
original error handling. The pure virtual boundary has no default no-op.
No SQL queries or stored-procedure contracts are introduced by these handlers;
shared SQL integration belongs to the coordinator. No SQL lifecycle operations
were performed. Live Lua/FSM integration and playable gameplay remain incomplete.

Implementation commit: `ee5e0fa9a5f2b24f386ded21be1b50c034aee4cf`.
