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

## Milestone 2 — compiled mob stack FSM (`mob_ai_states`)

New Ghidra export via the serialized `tools/reverse.py --process` wrapper:
`work/exports/09-mob-events/ps_game.exe/{decompiled.c,disassembly.txt}`.
Original hashes verified again (all twelve EXE/PDB files match).

Recovered `CMob::InitLuaFSM` 004b4c20 registers five states: idle 0
(`WhileIdle` 004b52c0), combat 1 (`WhileCombat` 004b55c0), death 2
(`WhileDeath` 004b5780), return home 3 (`WhileReturnHome` 004b5c40), and
move 4 (`WhileMove` 004b5790). All enter/leave callbacks are null. The exact
23 transition registrations are compared against original execution, including
push, pop and replace, with the actual event callbacks from milestone 1.

Implemented stack ownership using the existing `MobRuntime::state_stack`
projection, original initial-state selection (`SetInitialState` 00493150),
event dispatch (`DispatchEvent` 004929b0 / `STATE::OnDispatch` 004938c0),
and polling (`Poll` 004a8350). Initial selection clears the stack without leave
callbacks. Event callbacks run before the stack operation; callback changes to
the live stack remain visible. Empty dispatch/pop and depth greater than five
report the recovered diagnostic and reset to idle. Missing transitions report
an error without changing state. Invalid initial -1 retains the stack; an
unregistered state clears/releases it. Host invalid projections fail explicitly.

Death is a replace transition from states 0, 1, 3, 4 on event 3; lower stack
frames remain. Death has no outgoing transitions. Original `WhileDeath` is a
single RET, so polling the corpse succeeds without invoking world services.
There is no implicit reward call on entering/polling this state.

The actual reward connection is external: `CMob::Damage` 00490cf0 sets status
2 (00490d46) and dispatches event 3 (00490d59). `CUser::AttackMob` 0044ce10
calls `CMob::Death` 004910f0 only after Damage returns a kill. Native integration
composes the real existing death reward coordinator after the stack transition,
verifies reward delivery while the current state is 2, real ledger reset, and
that repeated death-state polls do not repeat rewards. Worker 01 owns combat
Damage; no duplicate combat implementation was added here.

### Validation

- `python3 tools/dev.py build`: exit 0, no compiler warnings;
  `work/mob_ai_states-build.log`.
- `python3 tools/dev.py test`: exit 0; all 121 tests pass (39.97 s), including
  `mob-ai-states-unit` (0.02 s) and the prior event suite;
  `work/mob_ai_states-tests.log`.
- Oracle regenerated twice under ignored work and compared byte-for-byte equal
  before validation. All original EXE/PDB hashes match. `git diff --check` clean.
- SQL integration was not run in this worker: this block has no SQL contracts,
  and the coordinator owns final isolated SQL integration/lifecycle.

The original-execution oracle
runs complete InitLuaFSM registration, dispatch, STATE::OnDispatch, initial
selection, polling and event handler instructions. It adapts only container
storage, diagnostics, Lua allocation/call and tick boundaries. State update
routines other than death are observed ports, not claimed as reconstructed.
Cases: 480 dispatch/poll comparisons, five state/23 transition registrations,
seven initial-state paths and two standalone empty/death polls. The Lua boundary
also mutates the stack to exercise post-callback operation ordering.

### Remaining work and integration

- Supply real WhileIdle/WhileCombat/WhileReturnHome/WhileMove implementations
  through required `MobAiStateServices::update`; these are the next independent
  state behavior block. Diagnostics are also required, never default no-ops.
- Production InitLuaFSM still needs Lua VM creation, libraries, registration,
  initialization of its retained fields, and release ownership. This milestone
  recovers the compiled stack table/operations, not Lua VM execution.
- `CAIFactory::SetFSM` 004a9130 and `CMob::ConnectToFSM` 00491db0 build a separate
  configurable CKFSM, not the stack table implemented here. Its text definitions,
  expression transitions and lifecycle remain missing.
- No canonical header changes are needed for this block. The coordinator should
  bind the existing optional state_stack to the lifetime of the stack FSM owner,
  route worker 01's dispatch-death event through `dispatch_mob_ai_event(mob,3,0,services)`,
  and retain the existing external `mob_death` call and corpse scheduler.
- No SQL access/contracts changed. SQL lifecycle and final shared integration
  remain coordinator work. The server remains unplayable.

Implementation commit: `1c85307e090d95c8064605fa78f0a70f97151752`.

## Milestone 3 — real WhileCombat update (`mob_ai_states_combat`)

Evidence: original `ps_game.exe` / `CMob::WhileCombat` 004b55c0, complete
read-only Ghidra decompilation/disassembly under the coordinator's
`work/reverse/ps_game.exe`. Structure offsets checked against exported types.
Original EXE/PDB SHA256 checks all pass again.

Implemented the actual combat-state coordinator, usable from the previous
`MobAiStateServices::update` combat case. It reads the tick before testing sleep
and stun, preserves the original ineffective attack-speed gate (004b55e9..004b5603,
including speed 0xff), and uses unsigned direct tick/deadline comparison.
Only-Lua mode or an attack deadline in the future calls Lua `WhileCombat` through
004b1210 when Lua is enabled, with tick, unsigned HP percentage and attacked
count. Percentage uses original double precision, truncation and low-DWORD
semantics, with no clamp to 100; zero maximum HP reproduces invalid FISTP's low
DWORD zero. The function leaves current_time and function_type untouched, except
for live provider writes. It does not reset function_type after the Lua callback.

When the attack is due, it resolves the live target user. Missing users or life
status 1 remove vision fixation before SetReTarget. Other life status values
(including negative) enter AttackAIExp; a false result retargets, a true result
returns. No Lua callback is added after these world operations. Snapshot tick and
live callback-driven field changes retain original ordering.

### Validation

- `python3 tools/dev.py build`: exit 0, no compiler warnings;
  `work/mob_ai_states_combat-build.log`.
- `python3 tools/dev.py test`: exit 0; all 122 tests pass (32.68 s), including
  `mob-ai-states-combat-unit` (0.01 s) and both previous worker suites;
  `work/mob_ai_states_combat-tests.log`.
- `git diff --check`: clean. SQL integration remains the coordinator's check;
  no SQL contracts/access are introduced here.

The oracle executes original WhileCombat
instructions with observed required ports (tick, FindUser 00415f30, AttackAIExp
004b66f0, RemApplySkillVisionFix 004a1580, SetReTarget 004b62c0 and LuaCall
004b1210). Its 1,536 cases include unsigned HP extremes, zero maximum HP,
32-bit tick boundaries, speed 255, missing/dead targets, attack success/failure,
and field mutations from each provider. Windows x87 precision is explicitly 53
bits. Fixtures regenerated twice byte-for-byte equal under ignored work.
The native comparator runs every case through the real stack poll bridge.

### Remaining integration and next work

Bind `update_mob_ai_combat` in the combat-state update case, and supply actual
serialized user lookup, AttackAIExp, vision-skill removal, SetReTarget and Lua
execution using the required `MobAiCombatServices` boundary. No provider defaults
to a no-op. MobAiCombatTarget is an explicit live user projection; no canonical
headers were changed. State transitions remain owned by the called services;
this function does not guess retarget results or substitute attack bodies.
The next independent state work is WhileIdle 004b52c0, then WhileMove 004b5790
and WhileReturnHome 004b5c40. Lua VM lifecycle and separate CAIFactory CKFSM are
still outstanding. No SQL contract changed or SQL lifecycle operation was run;
final shared SQL integration belongs to the coordinator. Server playability is
still incomplete.

Implementation commit: `30434d6c67dc3b088ada40978bc0fe4b50ff7cfd`.

## Milestone 4 — real WhileIdle (`mob_ai_states_idle`)

Ghidra process export (serialized wrapper):
`work/exports/09-mob-events/ps_game.exe/{decompiled.c,disassembly.txt}`.
Sources: `CMob::WhileIdle` 004b52c0, GetMoveSpeed 004908c0/00490900,
SVector::Distance2 0041b1f0 and RotateY 0049f750, original ps_game.exe.
All twelve EXE/PDB hashes checked successfully again.

Recovered complete idle coordination: tick/current-time update, new-target
cooldown clearing, FindTargetEx and event 0 with live target ID; obelisk and
movement deadline gates; event 4 when home distance is strictly greater than
0.5 with a chase path, or greater than 10 regardless of path. Stop/sleep/stun,
AI 4, zero definition movement speed/normal step and hold position prevent
wandering, but still schedule a retry (10000 + rand%30000 + current time).

Wandering preserves three CRT draws and the original arithmetic: angle
rand%10000 times float 0x3c8efa35, integral (rand%500)/100, clamped runtime
movement-speed index into {-4,-2,0,2,4} at 00542538 plus definition normal step,
float rotation and destination addition. Zone presence, destination distance
strictly less than 10 and EnableLine gate MoveMob(mode=0). Coordinates passed
to line checking use original signed FISTP truncation, including INT_MIN on
invalid conversion. The wandering deadline also includes definition normal_time,
even when the candidate is rejected. Live provider changes to definition,
movement speed, current time and zone availability preserve original ordering.

### Validation

- `python3 tools/dev.py build`: exit 0, no compiler warnings; final log
  `work/mob_ai_states_idle-final-build.log`.
- `python3 tools/dev.py test`: exit 0, all 123 tests pass (29.07 s), including
  `mob-ai-states-idle-unit` (0.01 s) and all previous worker suites;
  `work/mob_ai_states_idle-tests.log`.
- `git diff --check`: clean. SQL integration remains a coordinator check;
  this block introduces no SQL access/contracts.

The oracle executes WhileIdle, GetMoveSpeed,
Distance2, RotateY and actual original CRT sqrt/sin/cos; only Windows CRT errno
TLS storage is supplied. Target/dispatch/random/line/move/tick are observed
required boundaries. 1,536 original traces include 348 target-found events,
248 return-home events, 441 scheduled retries, 52 line checks and 33 moves,
plus unsigned timer boundaries, range equalities, negative/large coordinates,
and mutations from callbacks. Regeneration is byte-identical. Native comparison
uses real stack poll and dispatch, verifies resulting combat/return-home state,
full provider order, exact float destination bits and final live fields.

### Required integration / next scope

`MobAiIdleProjection` explicitly supplies missing native template fields:
AI byte +26 (already represented by template.ai), normal_time DWORD +5c,
normal_step byte +60, movement-speed byte +68 and obelisk bool +7c. The latter
is the definition flag, not merely the respawn owner's obelisk flag. Coordinator
must add/load these missing canonical definition fields from recovered contracts.
Project chase-path presence from CMob+0xa4 / the live ChaseTrail owner, and zone
availability from CMob+0x60 / mob.zone; refresh the live projection when providers
change those owners. No canonical header or loader was edited here.

Bind update_mob_ai_idle in the idle update case and supply actual FindTargetEx,
serialized CRT random, stack dispatch, map EnableLine and zone MoveMob providers.
They are required pure virtual methods, never production no-ops. Map line output
scratch coordinates and MoveMob's boolean result are not consumed by WhileIdle.
WhileMove 004b5790 and WhileReturnHome 004b5c40 remain the next substantive state
work. Lua lifecycle and the separate CAIFactory CKFSM remain missing. No SQL
contract/access changed; coordinator retains final SQL integration/lifecycle.
The server is still not playable.

Implementation commit: `c20b2e639f4efd5a5cf6f009a65e1287f00f4924`.

## Milestone 5 — WhileReturnHome with real native path owners

Evidence: ps_game.exe `CMob::WhileReturnHome` 004b5c40, Ghidra process export
under `work/exports/09-mob-events/ps_game.exe`. All original hashes verified again.
Implemented tick gating, arrival/reset, collision/path selection, blocked-home
recovery, trail consumption, full/partial steps, deadlines and cursor advancement.
Uses actual MapDefinition collision/line checks, AStarPath and ChaseTrail, plus
real MobDamageLedger reset; those bodies are not default services or stubs.

Arrival (distance strictly less than 10) calls ResetAttack, clears path/found,
clears trail, resets damage, then emits event 2. It happens before stop/sleep/stun
checks. Blocked destinations distinguish no line progress (replace respawn with
current position) from progress (retain respawn); neither branch resets damage
or attack. A* failure also adopts current position as respawn and returns to idle
without clearing damage. A retained trail is popped into AStar::SetPath.
Existing path cursors bypass collision checks for the home destination.

Movement preserves integer-coordinate subtraction, float normalization and
remaining distance minus size+1, definition step plus speed table, and live
chase interval reads after MoveMob. Short steps target the complete path tile,
write x/z on successful movement, advance even on failure and use the original
signed FISTP deadline (including 0x80000000 on invalid conversion). Partial
steps rely on MoveMob's position update and use tick+live interval.

### Validation

- `python3 tools/dev.py build`: exit 0, no compiler warnings;
  `work/mob_ai_states_return-build.log`.
- `python3 tools/dev.py test`: exit 0, all 124 tests pass (38.62 s), including
  `mob-ai-states-return-unit` (0.69 s) and previous worker suites;
  `work/mob_ai_states_return-tests.log`.
- Oracle regenerated twice, byte-identical; all ten instrumented original
  branches executed, including blocked destination without line progress and
  failed A*. Original hashes match; `git diff --check` clean.
- No SQL access/contracts added; isolated SQL checks remain coordinator work.

Original oracle executes full WhileReturnHome,
actual map/line/A*/trail/vector/CRT bodies. Allocation, Windows lock/TLS, clock,
ResetAttack, damage reset, event dispatch and MoveMob are observed boundaries.
1,536 cases cover 610 movement calls, 184 arrivals, blocked goals, failed searches,
retained cursors/trails, stop/sleep/stun, negative/zero movement limits, wrapping
and signed-overflow deadlines, movement failure and callback changes to home,
position, step, size, deadline and chase interval. Native tests use actual pooled
damage release and ResetAttack, compare exact float bits, complete path/cursor/
trail state and provider sequence, and separately compose event 2 with real FSM.

### Dependencies / limits

Required `MobAiReturnServices` providers are actual tick, ResetAttack, stack
dispatch and zone MoveMob. Required definition projection is byte chase_step
+68, byte size +3f and DWORD chase_time +64. Note +68 is byChaseStep, the same definition byte
used as the movement gate in WhileIdle (there named move_speed in its explicit
projection; bind that presence check to chase_step). No canonical header/loader was edited.
MapDefinition, MobRuntime path/trail and damage owners must remain serialized and
valid across callbacks. Bind this update in state 3; event 2 pops back to idle.

An explicit host error rejects line probes with out-of-map endpoints: original
EnableLine leaves previous-tile output scratch unwritten in that case, and
WhileReturnHome subsequently compares that uninitialized stack storage. No
specific stack residue or invented deterministic recovery is claimed here.
Production world-coordinate validity or a recovered caller scratch contract is
required before that invalid-world path can be supported faithfully.

WhileMove 004b5790 is the next independent state body. Lua lifecycle and separate
CAIFactory CKFSM remain missing. No SQL access/contracts changed; shared SQL
validation/lifecycle remains coordinator work. The server remains unplayable.

Implementation commit: `8eccb5d2b915c1eba677512b9bc96d9801e1bbdf`.

## Milestone 6 — WhileMove and mode-sensitive completion

Evidence: ps_game.exe `CMob::WhileMove` 004b5790, focused serialized Ghidra
export under `work/exports/09-mob-events/ps_game.exe`; immutable hashes rechecked.
Actual native MapDefinition, AStarPath, ChaseTrail and MobDamageLedger are reused.
No canonical headers were changed.

The initial move mode selects completion cleanup before FindTargetEx callbacks:
mode 3 resets attack/damage; every other mode notifies Lua OnMoveEnd through the
existing recovered no-argument event handler. Modes 2/3 search for targets before
the movement deadline gate and emit event 0 with the live target ID on success.
Movement samples time once; only the optional Lua completion takes another tick.

Arrival is strictly less than 0.5 with a trail (skipping rand), or strictly less
than rand%5+5. Arrival precedes stop/sleep/stun checks. Arrival and failed A*
emit event 6 first, then copy the live current position to respawn, clear path /
found / trail, and perform the captured mode's cleanup. Event/callback changes
cannot alter that initial cleanup decision. Lua discriminator writes survive
OnMoveEnd, as recovered in milestone 1.

Path selection uses line checking, then live trail pop/SetPath or actual A*.
A retained path uses its cursor directly. The float goal is captured before the
random callback; changing move_target there affects later ticks, not this goal.
Full and partial steps preserve mode=0, integer/float vector arithmetic, size+1,
normal-step speed table, live normal_time reload, signed FISTP deadlines and
cursor advancement even after rejected full-node movement.

### Validation

- `python3 tools/dev.py build`: exit 0, no compiler warnings;
  `work/mob_ai_states_move-build.log`.
- `python3 tools/dev.py test`: exit 0, all 125 tests pass (32.65 s), including
  `mob-ai-states-move-unit` (0.88 s) and all prior worker suites;
  `work/mob_ai_states_move-tests.log`.
- Fixtures regenerated twice byte-identically; ten original branch counters
  asserted nonzero. All original hashes match; `git diff --check` clean.
- No SQL contracts/access changed; SQL integration remains coordinator work.

Original oracle runs WhileMove, the no-argument
OnMoveEnd handler, actual collision/A*/trail/vector/CRT code and observed required
world/Lua ports. All ten instrumented branches execute. 1,536 cases include 625
moves, 158 target-found events, 140 completions, 104 Lua calls and 15 attack/damage
cleanups. Cases cover six modes, callback mode/target/position/definition changes,
timer boundaries, path failures, retained cursors/trails and movement failure.
Native comparison runs through real stack polling, compares exact float bits,
complete path/cursor/trail, damage ownership and provider ordering. Two additional
flows route event 6 through the real FSM and verify that clearing move_mode does
not change the captured mode-3 damage reset versus other-mode Lua notification.

### Integration / remaining work

Bind this implementation in state 4 with real target lookup, serialized CRT RNG,
ResetAttack, event dispatch, Lua and MoveMob providers. Definition projection is
normal_step byte +60, size byte +3f and normal_time DWORD +5c (already requested).
The map, mob path/trail/damage and projected definition must outlive callbacks.
The remaining required services are pure virtual; no production no-op is supplied.

All five fixed stack states now have recovered update bodies (death is the
original empty body), but production polling/event routing remains unconnected.
Independent work remains in InitLuaFSM/ReleaseLuaFSM VM lifecycle and the separate
CAIFactory/configurable CKFSM factories; real Lua/script, world and scheduler
providers must still be integrated. The invalid out-of-map scratch limit noted
for WhileReturnHome remains. No SQL access/contracts changed; final isolated SQL
integration/lifecycle stays with the coordinator. The server is not playable.

Implementation commit: `b60fa26fbe3b6ced6cad7e0c1c81a75d9b00a8fd`.

## Milestone 7 — Lua VM / compiled FSM lifecycle

Evidence: ps_game.exe InitLua 004b14f0, ReleaseLua 004b1530, LuaInitCall 004b1550,
InitLuaFSM 004b4c20, ReleaseLuaFSM 004b5260 and LuaError 004b52b0. Focused Ghidra
process export: `work/exports/09-mob-events/ps_game.exe`. Original hashes match.

Native lifecycle now composes existing MobRuntime fields and fixed stack owner;
no additional state projection or canonical header change was introduced.
InitLua overwrites the VM pointer with the allocator result, reloads that live
pointer separately for base/math/os library opening and returns success for an
initially successful allocation even if callbacks later changed the pointer.
It does not enable Lua, register the mob, release a previous VM or reset the FSM.

InitLuaFSM resets retarget_time=5000, Lua attacked count, only-Lua-attack, hold,
order time, move mode/target and discriminator, activates stack ownership before
VM creation, opens libraries, sets lua_ai=1, and registers the mob/script. Existing
stack entries remain visible during registration. Fixed state/transition setup
and initial state zero follow regardless of allocation/script failure. Allocation
failure retains the old lua_ai flag, rather than normalizing it. Combat timeout,
lock-on tick, current time and status are retained.

ReleaseLuaFSM clears stack ownership before VM close/GC, conditionally clears
lua_ai and only_lua_attack only when lua_ai was nonzero, then closes a non-null
live VM and always clears its pointer after close. Callback writes to flags
survive close; a replacement pointer does not. Plain ReleaseLua only closes and
clears the pointer. LuaError clears both enable flags while retaining VM/FSM.
LuaInitCall checks only lua_ai and invokes Init() with no arguments, even with
a null VM pointer; the required VM provider must handle the real operation.

### Validation

- `python3 tools/dev.py build`: exit 0, no compiler warnings; final log
  `work/mob_ai_states_lifecycle-final-build.log`.
- `python3 tools/dev.py test`: exit 0, all 126 tests pass (31.82 s), including
  `mob-ai-states-lifecycle-unit` (0.01 s) and previous worker suites;
  `work/mob_ai_states_lifecycle-final-tests.log`.
- Initial full suite also passed (37.86 s), before adding real MobPool composition.
  Regenerated oracle fixtures are byte-identical; hashes and diff checks pass.
- SQL contracts/access unchanged; coordinator retains final isolated SQL checks.

768 original-execution fixtures cover all
six functions (128 each), allocation success/failure, null/retained VM handles,
active/inactive FSM, zero/positive/negative Lua flags and mutations from allocator,
each library, registration, close and Init callback. Every provider observation
includes all 18 projected pointer/stack/retained-field values. The original
InitLuaFSM also asserts five states / 23 transitions and their update handlers.
Fixtures regenerated byte-identically. Native tests compose the real stack with
script error, death transition/poll and release, while unexpected providers fail.
They also compare actual MobPool::release against the recovered standalone
release using the same live VM-close callback, including post-clear flag writes,
and assert exactly one compiled/stack/close provider invocation in order.

### Production dependency / next scope

`MobAiLuaServices` requires actual create, library opening, LuaMob registration /
CMob global / Data/Lua/<type>.lua loading, Init call and close. There are no default
no-op providers, and this block does not claim actual Lua script execution. The
caller owns subject/stack lifetime throughout callbacks. As in the original,
initialization must not silently auto-close a retained previous VM; call the
appropriate release lifecycle when the owner requires destruction/replacement.

Bind initialization through MobInitPorts after a real Lua VM/binding backend
is available. MobPool::release already performs this release ordering: bind its
stack-release and close providers without calling the complete release wrapper
inside release_stack_ai (that would duplicate flag/close processing).
Canonical destruction still needs attention: CMob::~CMob 0048ff10 invokes
ReleaseLuaFSM at 0048ff50, after CKFSM teardown and before damage destruction.
The current default MobPool destructor only destroys unique_ptr<MobRuntime>;
its raw lua_state has no destructor hook. Coordinator must add exactly-once VM /
stack release for every retained pool object before destruction, preserving
service lifetime and the existing separately recovered damage destructor. The next
independent factory work is the separate CAIFactory/configurable CKFSM path;
production update/event/target/combat/world services still require coordinator
integration. No SQL contracts/access changed; final isolated SQL checks remain
coordinator work. The server is still not playable.

Implementation commit: `121cf33fdd3b686361ce0fab9503a774d1fa0afa`.
