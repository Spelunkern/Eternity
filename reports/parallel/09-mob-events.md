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

## Milestone 8 — retained configurable CKFSM execution

Evidence: ps_game.exe CKFSM<CMob>::Start 004954c0, ExecuteStateMachine 004a8520,
EvaluateCurrentStateTransitions 004a83e0; KFSMState OnEnter 00495da0, OnExit
00495e00 and OnUpdate 004a8560. Focused Ghidra process export under
`work/exports/09-mob-events/ps_game.exe`; all original hashes verified.

Added an actual retained configurable state owner, separate from the fixed stack
FSM: signed-code ordered state map, retained programs/transitions/conditions,
optional compiled callback slots and explicit current/start codes. Missing state
lookups insert a default empty state, matching operator[]. No canonical headers
were changed. Default slots/disabled flags/vectors were checked against the
state constructor 004963b0; its unused disabled-program declared counts are
uninitialized in the image, so native zero counts for those unused slots are
only internal normalization, not claimed recovered count values.
Programs run before their compiled callbacks; enabled is sampled
once, declared signed count/storage reload on each condition, and return values
are ignored for phase programs. Callback selection reloads after the program.
Invalid declared counts beyond retained storage fail explicitly.

Transitions retain the initial current state's range even if a condition changes
current. Every condition executes without short-circuit; signed DWORD results
sum with wrap and compare with the live expected_sum. Empty-condition transitions
to a different code ignore expected_sum; matching self transitions keep scanning.
On a match, old current and destination are captured, exit and enter run while
current still reflects preceding live writes, then the captured destination is
published after both callbacks. Start publishes start before entering; Execute
updates only and does not implicitly evaluate transitions.

### Validation

- `python3 tools/dev.py build`: exit 0, no compiler warnings; final log
  `work/mob_ai_states_configured-final-build.log`.
- `python3 tools/dev.py test`: exit 0, all 127 tests pass (34.39 s), including
  `mob-ai-states-configured-unit` (0.03 s) and prior worker suites;
  `work/mob_ai_states_configured-tests.log`.
- Regenerated fixture compares byte-identically; original hashes match;
  `git diff --check` clean. No SQL access/contracts added; coordinator retains
  final isolated SQL integration.

768 original executions cover Start, Execute,
Evaluate and Execute+Evaluate; real state phase wrappers execute around observed
TravelCondition and compiled callbacks. Original STL storage is adapted; native
uses actual retained std::map/vectors/callables. Inputs include signed-wide codes,
null callbacks, default insertion, unconditional/self transitions, full integer
condition results, live sum/target/current mutations, program grow/shrink/disable,
callback changes and wrapping runtime-field writes. Fixture regeneration matches
byte-for-byte. State snapshots include ordering, callbacks, enabled/count fields,
transition targets/thresholds and mutable condition results.

### Dependencies / next integration

TravelCondition 00498cf0 remains a required interpreter provider; each opaque
condition preserves a seven-DWORD boundary (0x1c), including opaque token-vector
metadata. Original pointer DWORDs are not native pointers: the future interpreter
must supply retained native token storage/mappings. The oracle substitutes
expression identities/results at this boundary; it does not execute tokens.
No empty interpreter or
required compiled-action default is supplied. Null callback slots are the actual
recovered optional state-function contract, not substituted world services.
Factory parsing, named function bindings and expression evaluation must populate
this owner before production use. Active transition ranges/state/subject lifetime
must survive callbacks; phase program storage may reload between evaluations.

Coordinator should own one configured FSM alongside each MobRuntime's existing
use_pais/status, binding the original CMob mainFSM at +ac; do not reuse state_stack
or move normal status codes into this separate machine. CAIFactory::SetFSM
004a9130 / ConnectToFSM 00491db0, expression/token evaluation, original text assets
and compiled function registry are the next independent factory scope. World/
Lua/target/combat services and canonical destruction dependency from milestone 7
remain open. No SQL access/contracts changed; coordinator retains final isolated
SQL integration/lifecycle. The server is not playable.

Implementation commit: `295840f87eaafdc6380e118955574f0bb8e0957e`.

## Milestone 9 — typed local owner and configured reset composition

Evidence: ps_game.exe AI_Param constructor 0048fa30, CAIFactory::ClearVariable
004a9840, CMob::ResetFSM 00492310 and onResetUser 004b05b0; map insertion helper
0049b7e0 confirms partial initialization. Focused serialized Ghidra exports are
under `work/exports/09-mob-events/ps_game.exe`. Originals remain unchanged.

Added retained typed AI_Param values directly to the existing configurable FSM
owner (no new canonical MobRuntime projection). The native parameter preserves
independent type/int/float/DWORD/VariableID/text fields, including all 128 text
bytes. Constructor behavior zeros only numeric payload and VariableID, retaining
type/text. ClearVariable selects definition locals by AI index, preserves missing
catalog/null slot/nonpositive count behavior, and visits duplicate IDs in order.
Only the ID comes from the definition: clearing follows the CURRENT value's type,
never restores definition defaults, and preserves other payloads/string tails.
Void/function/unknown types remain untouched; extra unlisted locals remain.

ResetFSM enables all four attack types, clears battle/attacked and target kind /
ID, sets status 100, clears variables and enters the live start code through the
actual configurable owner. Entry callback writes survive; there is no post-entry
status/target restoration. onResetUser clears only battle/attacked and target
when vision_fixed_user is zero, retaining status, attack flags and moment flag.

### Validation

- `python3 tools/dev.py build`: exit 0, no compiler warnings;
  `work/mob_ai_states_parameters-build.log`.
- `python3 tools/dev.py test`: exit 0, all 128 tests pass (31.04 s), including
  `mob-ai-states-parameters-unit` (0.14 s) and all previous worker suites;
  `work/mob_ai_states_parameters-tests.log`.
- 768 original clear/reset/user-reset paths plus 64 original retained constructor
  payloads; complete byte comparisons, native map ownership and configured state
  entry composition. Inputs include every supported/unsupported type, duplicate /
  wide IDs, missing catalogs, signed nonpositive counts, retained float bits and
  text tails, vision gate and callback mutations. Oracle regenerated identically.
- Original hash and diff checks completed; no SQL access/contracts changed.
  Final shared isolated SQL integration remains coordinator work.

### Factory boundary / required integration

Original missing-map insertion only zeroes int/float/DWORD/VariableID; its type
and text come from uninitialized stack storage (0049b7e0 and 0048fa30). Native
parameters therefore require an explicit type, and ClearVariable rejects missing
local entries rather than invent those bytes. Negative AI indices and declared
counts exceeding retained definition storage are also explicit host errors.
The factory must populate every referenced typed local before reset. This limit
is tested and must not be described as original missing-entry behavior.

Coordinator must retain this variables map with each configured FSM (+ac), and
bind catalog AI-index slots / local count +21c / local vector +224. Each 148-byte
local contributes its VariableID at +10. Existing template.ai supplies the byte
index read by ResetFSM. No shared canonical headers/loaders were edited here.
Named binding/factory loading and TravelCondition token interpretation remain
next independent work; native values are now available to that interpreter.
World/Lua callbacks, destruction and production scheduling remain unconnected.
The server is still not playable.

Implementation commit: `55a91cd7b3d70fc6d0902106949c8207f7cc5a23`.

## Milestone 10 — factory local admission on the retained owner

Evidence: ps_game.exe CAIFactory::SetFSM 004a9130, local-copy phase
004a919a..004a91e0; CKFSM<CMob>::AddLocalVariable 004ade70; parameter copy
0048fa90. Serialized Ghidra export is in `work/exports/09-mob-events/ps_game.exe`.

Added local admission directly into the existing MobAiVariables owner, supplying
the typed entries required by milestone 9. SetFSM's local phase selects the AI
catalog slot, returns without copying for an absent/out-of-range slot, and visits
its signed declared local count. Each definition value is admitted under its
VariableID. AddLocalVariable uses map insertion, not assignment: the
first value of a duplicate ID wins, preexisting runtime values survive, extra keys
remain, and insertion reports success even when the key already exists. All five
scalar fields are copied verbatim; text is copied only for type 4,
through its first terminator. Inactive text and string tails in the temporary
original pair are uninitialized, so new native storage explicitly normalizes
those undefined bytes to zero. Existing values retain every byte. Unterminated
128-byte strings are rejected rather than emulating the original overflow. Values
are retained independently of the definition's storage. Nonpositive counts do
nothing on a present definition.

`install_mob_ai_locals` names only this recovered phase; it is NOT a replacement
for the complete SetFSM. The original also binds the subject at mob+f4 before
copying, then installs functions/states and start code. Those phases remain
pending; this API does not pretend to publish a complete runnable AI factory.
Negative catalog indices and insufficient retained local storage are explicit
host errors, matching the previous admission policy rather than emulating unsafe
reads. No canonical owner/header was changed.

### Validation

- Original oracle: 512 SetFSM local-phase paths, executing actual original
  AddLocalVariable, constructors and parameter-copy instructions. Observed ports:
  STL insertion (first-key retention) and security cookie. Stop addresses are
  004a91e0 (next factory phase) / 004a918d (invalid-slot branch), not an asserted
  successful whole-SetFSM return. AL=1 checked after each original insertion.
- Native comparison checks all defined inserted bytes and every preexisting map
  byte and replays every observed
  insertion through the public native API, then composes local admission with
  ClearVariable on that same owner. Duplicate/wide IDs, existing keys, all
  supported/unknown types, string lengths 0/1/63/127 and signed nonpositive counts
  are included. Inactive text/string tails are masked only on inserted copies.
  Invalid retained local storage and unterminated text are explicit-error tests.
- Fixture regenerated byte-identically under ignored work/ before comparison.
  All original SHA256SUMS pass; diff check clean; no SQL access/contracts changed.
- Initial full test runs failed the new test: it exposed the conditional string
  copy omitted from the initial implementation. Corrected from original
  0048fa50/0048fa90 instructions; the failed run is not claimed as passed.
- `python3 tools/dev.py build`: exit 0, no compiler warnings;
  `work/mob_ai_states_locals-build-final.log`.
- `python3 tools/dev.py test`: exit 0, 129/129 tests pass (30.11 s),
  new local-owner test 0.10 s; `work/mob_ai_states_locals-tests-final.log`.

### Remaining integration

Coordinator can call this local phase with the configured FSM's existing
variables and the same AI catalog used by ResetFSM. It must retain typed local
records from the asset parser; this block does not guess a parser or create a
second runtime variable map. Complete SetFSM function/state publication, compiled
function registry, name binding and TravelCondition interpreter remain missing.
World/Lua providers, actual subject/production scheduling and lifecycle wiring
remain required. Coordinator owns final isolated SQL integration. This local
factory phase does not make the server playable.

Implementation commit: `5ff40b2f568cca09b194fc9316eff651ab5e88a1`.

## Milestone 11 — retained compiled registry and named state callbacks

Evidence: ps_game.exe CMob::ConnectToFSM 00491db0, CKFSM<CMob>::AddStateFunctions
004954e0 and KFSMState<CMob>::SetStateFunctions 0049b040. Focused serialized
Ghidra export: `work/exports/09-mob-events/ps_game.exe`.

The existing configured owner now retains the per-mob compiled-function vector
(original mob+114, vector first/last +118/+11c). Every ConnectToFSM call appends
seven entries, including when there are no state-name records; it does not clear
or deduplicate the vector. Names / original callbacks:

- MoveRandom / CMob::onMoveRandom 004afcf0.
- MoveRandomRun / CMob::onMoveRandomRun 004aff90.
- MoveAtSelectedPosition / CMob::onMoveAtSelectedPosition 004b01c0.
- MoveRunAway / CMob::onMoveRunAway 004b0360.
- Battle / CMob::onBattle 004b0520.
- Debug / CMob::onDebug 004afce0.
- ResetUser / CMob::onResetUser 004b05b0 (uses milestone 9's actual native body).

Binding consumes each 512-byte name record (state, enter, update, exit; four
128-byte C strings). Each phase starts empty, scans the full retained registry,
and retains the last exact name match. Matching is case-sensitive; unknown names
clear previous callbacks. State names hash from 5381 using signed char additions
and wrapped DWORD multiply-by-33. Hash collisions bind the same map state, so the
later name record replaces its three callbacks. Missing states use the existing
native map's default insertion. Program flags/counts/expressions, transitions and
current/start codes are preserved. Published callbacks retain the bound mob,
matching original SetStateFunctions' subject at state+30, even if the executor
passes a different mob reference later.

Original binding is enclosed in one global recursive critical section. Native
uses a global recursive mutex over append/lookup/publication. This does not
replace the owner's required scheduling exclusion from concurrent execution.
Service/subject lifetimes must exceed all retained callbacks. Six callback bodies
remain mandatory MobAiCompiledServices operations: no movement, combat or debug
provider defaults to a no-op. Binding those operations is distinct from recovering
their actual world bodies. Strings lacking a terminator within their retained
128-byte field are explicit host errors rather than unsafe reads.

### Validation

- 384 original retained sequences, one to three ConnectToFSM calls each; actual
  name comparisons, inline signed hash and all AddStateFunctions / SetStateFunctions
  instructions execute. Ports: vector growth, native state-map lookup and already
  initialized critical-section enter/leave. First-use Win32 initialization and raw
  member-pointer ABI translation are not claimed as native implementations.
- Fixtures observe callback code addresses, bound subject, unchanged noncallback
  state bytes, registry growth and lock ordering. Inputs cover exact/case/unknown/
  empty names, preexisting registry values, repeated connection, colliding state
  names Aa/B@ and high-bit string bytes including wrapped hashes.
- Native comparison invokes bound callbacks through real configured Start/Execute
  plus exit calls; verifies retained subject, ResetUser's actual mutation, required
  service dispatch, preserved programs/transitions and unchanged current/start
  during binding. No original executable pointer is used as a native callback.
- Oracle regenerated byte-identically; original SHA256SUMS pass; diff check clean.
- `python3 tools/dev.py build`: exit 0, no compiler warnings;
  `work/mob_ai_states_bindings-build-final.log`.
- `python3 tools/dev.py test`: exit 0, 130/130 tests pass (36.71 s),
  new binding test 0.36 s; `work/mob_ai_states_bindings-tests.log`.

### Remaining integration

Coordinator must retain this registry for the mob lifetime and pass parsed state
name records from the AI definition vector at +248/+24c. The registry is separate
from the fixed state_stack and is now carried with the already retained configured
owner, not a new parallel MobRuntime. Stable subject/service ownership is required.
Complete SetFSM state/transition/program assembly, system-function publication,
asset parsing and TravelCondition remain pending. Production adapters for six
compiled handlers/world/Lua, scheduling and destruction remain required. No SQL
access/contracts changed; final isolated SQL integration belongs to coordinator.
The server is not playable.

Implementation commit: `11dd712d0244bfe5b9db8d02f56741717f484f07`.

## Milestone 12 — actual compiled Battle/PAI body and configured reset

Evidence: ps_game.exe CMob::onBattle 004b0520 and CMob::PAI_Attack 004af9c0;
ResetFSM 00492310, ClearVariable 004a9840 and state OnEnter 00495da0 execute in
reset paths. Focused serialized Ghidra export:
`work/exports/09-mob-events/ps_game.exe`.

Added the real compiled Battle body to the existing named-registry/configured
owner via MobAiBattleCompiledServices. Other compiled handlers remain mandatory
external operations, not fallback no-ops. This reduces the previously required
Battle provider to its actual world dependencies: live user lookup, vision-fix
removal, FindTarget and AttackAI. Reset is composed directly with the retained
configured owner, typed variables, catalog and state entry from milestones 8–9.

onBattle returns for no zone, unsigned next_attack > current_time, nonzero sleep
or stun. It reads the mob's retained current_time, not a fresh clock. The original
0xff attack-speed check is unreachable for that value (the comparison only runs
for 0..6); no signed attack-speed value is suppressed. Stop and only_lua_attack
do not gate this compiled handler. Lookup uses the current target ID. Missing or
life-status-1 targets remove vision fix, then FindTarget; failure resets FSM.
Finding a replacement returns without attacking it. A live target enters
PAI_Attack with current_time reloaded after lookup.

PAI_Attack compares template country DWORD with target country byte, rejects
life-status 1, nonzero immunity, zero admin attackable or zero admin visible.
Eligible targets invoke AttackAI once with the passed time and borrowed target;
there is no success-result check or post-attack retarget. Ineligible targets run
FindTarget without vision-fix cleanup; failure resets FSM, success returns without
attacking the replacement. Neither path rechecks gates after callbacks. Reset's
entry callback writes survive, including current state, target/status and locals.

### Validation

- 1,536 original execution paths across onBattle and standalone PAI_Attack. Ports:
  world user lookup, vision-fix cleanup, FindTarget, AttackAI, STL lookup and state
  entry callback. Actual original reset/clear/OnEnter coordination executes.
- Inputs include no zone, equal/unsigned wrapping deadlines, sleep/stun, speed
  0/6/7/255/negative, nonzero stop/only-Lua, absent/dead/other-life targets, every
  eligibility gate with nonboolean byte values, and retarget success/failure.
  Live callback mutations cover lookup time/target/country/life changes, cleanup
  changes, new sleep/stun/deadline in retarget, entry writes and post-attack writes.
- Native test uses an actual ZoneMobs owner, named Battle binding and configured
  Execute for onBattle, actual typed-variable reset and actual configured Start /
  state entry. Every trace and retained field is compared; no fake reset provider.
- Oracle regenerated byte-identically; original SHA256SUMS pass; diff check clean.
- `python3 tools/dev.py build`: exit 0, no compiler warnings;
  `work/mob_ai_states_battle-build.log`.
- `python3 tools/dev.py test`: exit 0, 131/131 tests pass (39.76 s),
  new Battle test 0.03 s; `work/mob_ai_states_battle-tests.log`.
- Observed fixture coverage: 744 AttackAI paths, 600 FindTarget paths and 298
  actual configured reset/state-entry paths.

### Required canonical integration / remaining work

MobRuntimeTemplate currently lacks country. Original evidence is mob+0xa8 ->
DWORD +0x78; this block uses an explicit live MobAiBattleDefinition reference,
not a shared-header edit. Coordinator should bind it to the retained canonical
mob-definition country field. User target views must retain actual owner lifetime
and expose country +0x12d, life +0x142c, immunity +0x1366, admin attackable +0x5775
and visible +0x5774. A lookup mutation must be reflected before eligibility reads.
No second user catalog or targeting algorithm was introduced. Bind FindTarget to
the recovered target owner, vision-fix removal to applied-skill ownership, and
AttackAI to the actual combat scheduler/action provider. Return values from a
retarget provider cannot stand in for its original target/status side effects.

Remaining compiled move/debug bodies, full SetFSM assembly, TravelCondition and
asset parsing remain independent work. Stable subject/service lifetimes and
production scheduling/destruction still require integration. No SQL contracts or
access changed; final isolated SQL integration belongs to coordinator. The server
is not playable.

Implementation commit: `60b71080f66ec9b02e1c16774d656a92befbf7f2`.

## Milestone 13 — compiled selected-position movement on actual map owners

Evidence: ps_game.exe CMob::onMoveAtSelectedPosition 004b01c0, SVector::Distance2
0041b1f0, CMob::GetStatus 0041d760 and CMap::EnableLine 0042ab80. Serialized
Ghidra export: `work/exports/09-mob-events/ps_game.exe`.

Added the actual selected-position compiled body and MobAiSelectedCompiledServices
adapter for the existing named registry/configured executor. It consumes real
MobRuntime, MapDefinition::enable_line, ChaseTrail and canonical mob_status;
there is no second path, status or trail owner. Actual movement/transport remains
a mandatory service. Other compiled functions delegate to the required remaining
provider, allowing composition with the already recovered Battle adapter.

Without a zone it returns unchanged. Outside battle, it computes horizontal
respawn/current distance. A trail with distance >0.5, or any distance >20,
requests return: state 2 (death) preserves everything; otherwise only raw status
becomes 3. The active stack is not replaced or dispatched. In battle this gate
is bypassed. For the destination phase, selected.y becomes +0 before the range
check. Selected-to-respawn distance must be <20 (not selected-to-current).
Equality or unordered x87 comparison takes the rejection path. Rejection copies
all three current position components to selected position, including current Y.

For an accepted destination, X/Z endpoints truncate toward zero with original
FISTP signed-DWORD invalid-conversion semantics. The actual map line decides
whether to move. A failed line copies current position to selected; a successful
line calls MoveMob with the live selected XYZ and mode 1. The original temporary
line outputs are ignored, so this path needs no guessed clipped endpoint and
out-of-map rejection does not read the uninitialized output scratch seen in the
separate return-home block. Movement failure is not checked by the handler.
Movement callback changes to position, respawn, selected position and status
survive without post-call restoration.

### Validation

- 1,536 original paths on four actual bitmaps (open, wall with gap, solid wall,
  isolated blocks). Original distance/CRT math, complete GetStatus including active
  deque/empty stack/death and complete map line code execute. Only MoveMob and
  Windows lock/CRT TLS platform boundaries are adapted. No distance or line oracle
  provider replaces original execution.
- Coverage: 748 original line checks, 264 original GetStatus checks and 483 moves.
  Inputs distinguish 0.5/20 equality and adjacent float values, battle and zone
  gates, negative/fractional endpoints, raw/active/empty/death statuses, selected Y
  clearing and full-position rejection, movement success/failure and reentry writes.
- Native comparison invokes the named MoveAtSelectedPosition binding through the
  actual configured executor and uses actual ZoneMobs, MapDefinition and ChaseTrail
  owners. Raw float bits, all retained position/status fields, movement arguments
  and unchanged stack/trail are compared.
- Initial build failed because the test used optional<MapTile> equality without
  a canonical MapTile equality operator; corrected the test to compare coordinates.
  No shared header was changed; that failed build is not counted as passed.
- Original fixture regenerated identically; SHA256SUMS pass; diff check clean.
- `python3 tools/dev.py build`: final exit 0, no compiler warnings;
  `work/mob_ai_states_selected-build-final.log`.
- `python3 tools/dev.py test`: exit 0, 132/132 tests pass (31.84 s),
  new selected-movement test 0.02 s; `work/mob_ai_states_selected-tests.log`.

### Remaining integration

Coordinator must bind the same retained map as the mob's current zone, and retain
map/subject/service lifetimes for compiled callbacks. The real MoveMob provider
must perform original world movement/broadcast and preserve its callbacks; it
cannot be a success-only no-op. This block needs no new canonical fields:
selected_position, respawn_position, in_battle, zone, trail and stack/status
already exist. Complete factory assembly and TravelCondition are still missing;
compiled random/run-away/debug bodies remain available independent work. World,
Lua, scheduling/destruction and full original-client playability remain open.
No SQL access/contracts changed; final isolated SQL integration is coordinator work.

Implementation commit: `a16ee679704efb4339a8216ab2be584fd891e475`.

## Milestone 14 — compiled run-away displacement on retained owners

Evidence: ps_game.exe CMob::onMoveRunAway 004b0360; SVector::Distance2 0041b1f0,
CMob::GetStatus 0041d760, CMap::EnableLine 0042ab80 and MoveMob 004169d0.
Focused serialized Ghidra export: `work/exports/09-mob-events/ps_game.exe`.

Added the actual compiled run-away body and named-registry adapter. It shares the
mandatory movement service from the selected-position block and composes with
other required compiled providers. Actual MobRuntime, MapDefinition, ChaseTrail,
status/stack and configured binding/execution owners are used; no new canonical
fields or parallel movement state were added.

RunAway interprets selected_position as an XYZ displacement, computes each current
position + displacement component as a stored float, and keeps that destination
in local storage. No zone returns unchanged. Outside battle, trail/home distance
>0.5 or unconditional home distance >20 requests raw status 3 unless effective
status is death (2). Active stack/status ownership is preserved exactly as in the
original; battle bypasses this home gate. Destination-to-respawn horizontal
distance must be <20; equality/unordered rejects without changing selected
position. Unlike MoveAtSelectedPosition, it neither zeroes selected Y nor replaces
the selected vector on a rejected range or failed line.

For accepted range, coordinates truncate to signed DWORD toward zero and the
actual map line must succeed. MoveMob receives the locally captured XYZ sum and
mode 1, including summed Y; line scratch outputs do not replace the destination.
The move result is ignored, and callback writes survive. This is fixed-displacement
movement, not an invented target-dependent fleeing algorithm. There are no clock,
sleep/stun/stop or new target-selection gates in this handler.

### Validation

- 1,536 complete original handler calls; actual distance/CRT math, GetStatus with
  raw/empty/active/death deque storage and complete map line execute. Ports are
  MoveMob and Windows CRT TLS/lock boundaries, not geometry or distance decisions.
- Four original/native bitmaps: open, wall with gap, solid wall and isolated
  blocks. Coverage includes 600 line checks, 356 GetStatus calls and 486 moves.
  Positions crossing the gap distinguish it from the solid wall. Adjacent floats
  around 0.5/20, battle/zone gates, signed/fractional coordinates, positive/negative
  Y displacement, move success/failure and nested callback writes are compared.
- Native test invokes the actual MoveRunAway name binding through configured
  Execute with real ZoneMobs, map and trail owners. It compares all position/status
  float bits, exact movement arguments and retained stack/trail. Original oracle
  also asserts its deque bytes and trail head remain unchanged.
- Oracle regenerated byte-identically; all original SHA256SUMS pass and diff
  check is clean. No failed comparison/build was counted as passed.
- `python3 tools/dev.py build`: exit 0, no compiler warnings;
  `work/mob_ai_states_runaway-build.log`.
- `python3 tools/dev.py test`: exit 0, 133/133 tests pass (36.56 s),
  new run-away test 0.02 s; `work/mob_ai_states_runaway-tests.log`.

### Remaining integration

Bind the same stable map as the mob's zone and the real world MoveMob transport /
movement owner. The service is mandatory and must preserve original synchronous
callbacks; successful unit movement does not substitute world delivery. Existing
selected_position must preserve its context-dependent meaning: absolute position
for MoveAtSelectedPosition, displacement for MoveRunAway. No shared headers were
edited. Subject/map/service lifetimes must exceed retained callback lifetimes.

Compiled random/random-run/debug bodies, complete factory assembly, token
interpretation and live asset/provider publication remain open. World/Lua,
production scheduling/destruction and original-client playability are still
incomplete. No SQL access/contracts changed; coordinator owns final isolated SQL
integration and full integration checks.

Implementation commit: `e42621dff776eb274b3089da98dd050a6af274fc`.

## Milestone 15 — compiled random/random-run bodies and existing CRT RNG

Evidence: ps_game.exe CMob::onMoveRandom 004afcf0, onMoveRandomRun 004aff90,
SVector::RotateY 0049f750, original CRT rand 00504697; distance/status/map line
0041b1f0 / 0041d760 / 0042ab80. Focused serialized Ghidra export:
`work/exports/09-mob-events/ps_game.exe`.

Added both actual compiled bodies and named-registry adapter on the existing
configured owner. MapDefinition, ChaseTrail, canonical mob_status and required
world movement are retained. Randomness is mandatory from the original executor's
retained MicrosoftRandom/RandomSource owner, not a new per-mob generator.

MoveRandom always consumes two rand results, even without a zone and before its
home/status gate. MoveRandomRun returns immediately without a zone, otherwise
consumes one rand before its home gate. Angle is (rand % 10000) times float bits
3c8efa35, with float angle/sine/cosine staging and original RotateY orientation.
Speed clamps to 0..4 and selects {-4,-2,0,2,4} at original 00542538; template
normal_step byte is added without a nonnegative-length clamp. Walking adds the
integer jitter (second_rand % 500) / 100; running adds no jitter. Step/speed and
position reads occur after RNG calls, retaining callback mutations. Rotated local
vector is added to current XYZ, including Y, before the return/range gates.

Outside battle, trail/home distance >0.5 or home distance >10 (walk) / >20 (run)
requests raw status 3 unless effective status is death 2. Battle skips that gate.
Walking checks zone only after the home gate, so absent-zone walking can still
change status. Accepted destination must be strictly inside the corresponding
respawn distance limit, pass actual map line with truncated signed-DWORD X/Z,
and calls MoveMob in mode 0/1. No destination clipping, post-move restoration,
clock reads, next_move scheduling or selected-vector writes are invented. Movement
failure is ignored. World callback writes survive.

### Validation

- 2,048 original paths: 1,536 controlled rand-result fixtures for edge values and
  512 fixtures executing actual CRT rand with retained TLS seed. All original
  RotateY/sin/cos/distance/CRT math, raw/empty/active/death GetStatus and map line
  code executes. World movement and Windows lock/TLS identity are adapted;
  controlled cases port rand results, seeded cases do not replace its algorithm.
- Native seeded cases use the already existing MicrosoftRandom and compare final
  RNG state as well as consumed result/order. Both handlers compose through their
  real name bindings and configured Execute on actual zone/map/trail owners.
- Inputs include seed wrap, rand modulo/jitter edges, normal step 0/1/5/10/255,
  negative/out-of-range speeds, 0.5/10/20 float boundaries, walls/gap, zone/battle/
  effective-status gates and RNG/world callback mutations. Raw position/status,
  step/speed/battle, movement arguments, unchanged next_move and stack/trail are
  compared. Original coverage: 783 line checks, 456 status checks, 641 movements.
- Initial seeded-fixture generation hit unmapped TLS seed scratch before cases;
  expanded only ignored emulator memory and regenerated complete fixtures before
  validation. No failed generation is counted as a successful comparison.
- Fixture regenerated byte-identically; original SHA256SUMS pass; diff check clean.
- `python3 tools/dev.py build`: exit 0, no compiler warnings;
  `work/mob_ai_states_random-build-final.log`.
- `python3 tools/dev.py test`: exit 0, 134/134 tests pass (44.42 s),
  new random movement test 0.02 s; `work/mob_ai_states_random-tests.log`.

### Canonical integration / remaining work

Template normal_step is the byte at mob+0xa8 -> +0x60. Canonical MobRuntimeTemplate
still lacks it; use the explicit live MobAiRandomDefinition projection until the
coordinator binds the actual retained definition field. This is the same byte
already requested by idle reconstruction. Do not seed/create a separate RNG per
mob or replace the shared original CRT stream with ServerRandom: preserve the
existing MicrosoftRandom executor/thread ownership and interleaving. No canonical
headers were edited. Map, subject, definition and services must outlive callbacks;
world movement/transport remains mandatory.

Debug's logging body, full SetFSM state/function/program assembly, asset parsing
and TravelCondition remain pending. World/Lua/production scheduling/destruction
and original-client playability are still incomplete. No SQL access/contracts
changed; coordinator owns final isolated SQL and complete integration checks.

Implementation commit: `f436d67e53eae3732974774ee2ac6beebb1160b4`.

## Milestone 16 — complete compiled dispatch composition and factory startup

Evidence: ps_game.exe CAIFactory::SetFSM 004a9130, final startup publication
004a9734..004a97ab; CMob::onDebug 004afce0 / literal 00538d2c; actual
ConnectToFSM 00491db0, OnEnter 00495da0 and ResetUser 004b05b0. Focused serialized
Ghidra export: `work/exports/09-mob-events/ps_game.exe`.

Added MobAiCompiledRuntime: one provider dispatches all six compiled enum entries
to the recovered native random/random-run, selected, run-away, Battle and Debug
bodies. ResetUser remains directly bound to its recovered body by ConnectToFSM.
There is no required external compiled-handler fallback anymore. Actual movement,
executor RNG, target lookup/retarget, vision cleanup, AttackAI, condition
interpreter and debug output remain mandatory service operations; this composition
does not invent their production providers or silently skip them.

Debug calls a mandatory output sink with the exact bytes `PAI Debug on\n`,
retaining the original printf newline and ignoring its result. The sink may
reenter/change runtime state. No original code pointer or Win32 PMF representation
is used as a native callable.

Added only SetFSM's final startup phase: hash the retained C start-name with the
already recovered signed-char/DWORD algorithm, publish start, connect named
compiled callbacks (appending the registry), then use actual configured Start to
publish current and execute the live entry program/callback. It does not reset
status/targets/flags/locals or clear existing states. Entry changes to start,
current, targets and programs survive. Unmatched/empty function names can remove
callbacks as recovered in milestone 11. Names lacking a retained terminator in
128 bytes are explicit host errors.

This finalizer is NOT complete SetFSM: the caller must have already admitted
locals, system functions, states, transitions and condition programs. State /
function/program assembly from asset records remains missing and is not replaced
by a no-op provider. No duplicate FSM or runtime owner was introduced.

### Validation

- 768 original retained sequences executing complete SetFSM with its three
  assembly counts zero and an already admitted state; one to three startup calls
  each. Actual ConnectToFSM, OnEnter, Debug and ResetUser bodies execute. Ports:
  vector growth/map lookup, TravelCondition, printf output, generic entry callback,
  already initialized locks and security cookie. This covers startup, not nonempty
  factory assembly or the token interpreter.
- Inputs cover empty/high-bit/colliding start names, Debug/ResetUser/unknown/empty
  callback bindings, preserved callbacks without name records, disabled/negative/
  zero/multiple condition counts, live entry action/count/start changes, debug and
  generic entry reentry, vision-fixed-user gates, and repeated publication.
  Trace/field comparisons check registry completion before entry, variable and
  flag retention, current/start changes and exact debug bytes.
- Existing random, selected, run-away and Battle native comparisons now use
  MobAiCompiledRuntime directly: 5,888 existing original callback paths exercise
  the composed movement/combat routes; 768 additional standalone PAI_Attack paths
  retain their direct helper checks. Original fixtures remain unchanged.
- Initial oracle omitted the prepopulated state's pKFSM at +f4; bound that owner
  pointer before regeneration. The failed fixture attempt is not counted as a
  passed execution. No native workaround or canonical field was added for it.
- New fixture regenerated byte-identically; all original SHA256SUMS pass; diff
  check clean. No SQL access/contracts changed.
- `python3 tools/dev.py build`: exit 0, no compiler warnings;
  `work/mob_ai_states_runtime-build-final.log`.
- `python3 tools/dev.py test`: exit 0, 135/135 tests pass (38.32 s),
  new runtime/startup test 0.02 s; `work/mob_ai_states_runtime-tests.log`.

### Canonical integration / remaining work

Use this composed provider for the mob's retained compiled registry, with stable
map, template normal_step/country projections, catalog and live world services.
Preserve existing executor MicrosoftRandom ownership and same-zone map binding.
Subject, provider and backing definition lifetimes must exceed all callbacks.
Final startup corresponds to template start-name bytes at +108 and must follow
real state/function admission. Bind entry programs to the same retained FSM
(original state+f4); the native configured owner passes itself explicitly.

No shared MobRuntime/header edits were made (mobs/mob_pool remain worker03-owned).
Complete factory state/function/program assembly, asset parsing, TravelCondition,
world/Lua/production scheduling/destruction and original-client playability remain
open. Coordinator owns final isolated SQL and complete integration validation.

Implementation commit: `f7f2c8a8c55d680298fdcd286839d62cf5b707f6`.

## Milestone 17 — real state admission, tree ownership and borrowed program owner

Evidence: ps_game.exe CKFSM<CMob>::AddState 004adef0, KFSMState copy 004965e0,
KFSMTransition copy 0049f630, actual map constructor/insertion/destructor
00495c30 / 00496e40 / 00495a20. PDB state layout: name +44, strings +60/+7c/+98,
StateCode +b4, program vectors +b8/+c8/+d8, counts +e8/+ec/+f0, pKFSM +f4,
enabled bytes +f8..fa. Transition layout: id +0, calc count +4, expected sum +8,
target +c, strings +10/+2c/+48 and condition vector +64. Focused serialized
Ghidra export: `work/exports/09-mob-events/ps_game.exe`.

Added admission directly into the existing configured state's signed-key map.
AddState hashes the state's C name, then inserts a retained value copy. Hash
collisions/duplicates preserve the first admitted value; they do not replace or
merge callbacks/programs/transitions. The copied StateCode is separate metadata
and is not rewritten to the key. Current/start remain unchanged. Owned state name,
program text and transition metadata/text are now retained alongside the existing
vectors and callbacks. Native std::string/vector/map value ownership preserves
copies after the source is cleared/destroyed, including embedded NUL bytes and
heap-sized strings. Only the hash stops at an embedded NUL.

Original state pKFSM is borrowed and copied, not rebound by AddState. Added the
explicit condition_owner binding to the same native state, and phase execution
passes that owner to TravelCondition. The binding is reloaded for each condition;
it does not change the containing FSM's current/start or callback subject. The
legacy direct-input projection's null binding still means its containing owner;
that normalization is not a claim about executing an original null pKFSM. A real
decoded named state must carry its original constructed owner explicitly. Borrowed
owner and callback subject must survive execution and retained state lifetime.

### Validation

- 512 original sequences / 1,536 complete AddState calls; actual STL tree
  construction, signed comparison/insertion, state/transition/string/vector copy
  and all temporary/final map destruction execute. Only allocation/free and
  security-cookie ports are adapted; no container insertion/copy/destructor port.
- Freed allocations are poisoned with 0xDD. Retained tree nodes are inspected
  after temporary and by-value parameter destruction, then the real map is
  destroyed. All owned allocations are freed: only the three external fixture
  shells remain. 39,251 original container frees are observed across fixtures.
- Cases include Aa/B@ collision, signed/high-bit keys, empty/long/embedded-NUL
  names and text, different StateCode vs hash, callback metadata, independent
  declared counts and storage sizes, every phase, nested transition-condition
  vectors, and source/foreign borrowed pKFSM bindings. Allocator padding and raw
  ABI pointer representation are not compared as native data.
- Native comparison clears source values immediately after admission, compares
  all retained metadata/text/condition payloads/callback identities and signed map
  order, and verifies actual configured entry uses a retained foreign condition
  owner. Callback IDs are fixture projections, never original executable pointers
  invoked as native code.
- Nested CKToken vectors are deliberately empty in these fixtures. The original
  owns/copies outer condition vectors; full nonempty token ownership/decoding and
  interpretation remain open. Native seven-word condition inputs retain zero
  pointer/padding slots for this projection, not original addresses.
- Initial tree fixture used +14 as node state offset; corrected from actual copy
  results to +18 (node key remains +10). Initial native suite failed 135/136
  because the fixture owner-mode column changed during its reader compilation;
  rebuilt the finished reader before retry. These attempts are not passed checks.
- Oracle regenerated byte-identically; original SHA256SUMS pass; diff check clean.
- `python3 tools/dev.py build`: final exit 0, no compiler warnings;
  `work/mob_ai_states_admission-build-final2.log` (prior production rebuild
  `work/mob_ai_states_admission-build-final.log` also has no warnings).
- `python3 tools/dev.py test`: final exit 0, 136/136 tests pass (26.76 s),
  admission test 0.07 s; `work/mob_ai_states_admission-tests-final.log`.
  Earlier failed suite remains in `work/mob_ai_states_admission-tests.log`.

### Required integration / remaining work

Coordinator should admit already decoded states through this API before the
startup finalizer, with the same retained condition/token provider and explicit
condition_owner as the original named-state constructor. No second state map was
created. The condition provider must resolve its borrowed FSM's bound subject;
passing a foreign owner does not by itself bind another mob's world services.
Callback std::function captures retain their existing borrowed subject/service
contracts. No shared canonical headers were edited.

Full SetFSM construction from state records, system-function publication, asset
parsing, nonempty token ownership/TravelCondition and root world/Lua/scheduling /
destruction remain incomplete. This is not a claim that complete factory loading
or the server is playable. No SQL access/contracts changed; coordinator owns final
isolated SQL and complete integration validation.

Implementation commit: `0f18264cc59bb061f45ceeb86cdf991840d501fc`.

## Milestone 18 — nonempty CKToken ownership in retained condition programs

Evidence: ps_game.exe CKToken constructor 004a89d0 / destructor 0048fa20,
KFSMCondition copy 00493aa0, state copy 004965e0, transition copy 0049f630 and
AddState/map lifecycle 004adef0 / 00495c30 / 00496e40 / 00495a20. PDB CKToken
layout: function ID +0, DWORD +4, parameter count +8, opcode +c, integer +10,
float +14, type +18, number type +1c, value[128] +20, token text[128] +a0;
size 0x120 (288). KFSMCondition type/count/cursor +0/+4/+8 and token vector +c.
Serialized Ghidra export: `work/exports/09-mob-events/ps_game.exe`.

Added the actual 288-byte owned native token value and a token vector directly
to the existing MobAiConfiguredCondition. Program/transition/state admission now
retains nonempty nested token vectors through normal native value ownership;
there is no parallel condition catalog. Original token copies retain ALL 288
bytes, including both full text arrays, embedded NULs and tails, independently of
token/value type. Unlike AI_Param's conditional string copying, token copying is
not a C-string-only operation. Numeric fields and arbitrary float bits survive.

The original constructor only clears integer, float and DWORD and sets number
type to 1; it leaves function ID, parameter count, opcode, type and both arrays
untouched. initialize_mob_ai_token reproduces that retained-input operation.
Fresh native unknown-field zero defaults are internal normalization, not original
uninitialized stack bytes. Native number_type defaults to the recovered defined
value 1. No destructor callback is needed for the token payload's fixed arrays;
its vector storage remains owned and is destroyed with its condition/state.

### Validation

- 256 original sequences / 768 complete AddState calls with nonempty token
  vectors in all three program phases and nested transition conditions. Actual
  original token/condition/vector/state/transition copies, STL tree insertion and
  destruction execute. Only allocation/free and security cookie are adapted;
  there is no token/copy/container oracle port.
- Original freed allocations are poisoned with 0xDD. Retained tree nodes are read
  after all source/temporary destruction, then the real map is destroyed. Every
  owned allocation is freed; only the three external fixture shells remain.
  28,901 original container frees are observed.
- All 288 payload bytes are compared for each token, including arbitrary metadata,
  enums, float bits and full text tails. Declared condition counts/cursors remain
  independent of vector sizes. Raw vector pointers/allocator padding are excluded
  from native data; native conditions retain the first three metadata words and
  decoded token values, with zero legacy pointer slots in this fixture projection.
- 64 retained constructor payloads execute original 004a89d0 and compare all bytes
  after the native initializer. The native 288-byte size and trivial-copy contract
  are compile-time checked. Source native values are destroyed immediately after
  admission before retained-owner comparison.
- Fixture regenerated byte-identically; original SHA256SUMS pass; diff check clean.
- `python3 tools/dev.py build`: exit 0, no compiler warnings;
  `work/mob_ai_states_tokens-build-final.log`.
- `python3 tools/dev.py test`: exit 0, 137/137 tests pass (30.89 s),
  new token test 0.35 s; `work/mob_ai_states_tokens-tests.log`.

### Required integration / remaining work

Factory decoding should populate condition.tokens, not insert original vector
addresses into native words. Existing metadata words 0/1/2 are condition type,
declared token count and mutable cursor; storage size must remain distinct.
Native values preserve raw function IDs/opcodes/DWORDs as data and never call an
original address. The current mandatory TravelCondition provider still needs the
actual interpreter, stack operations, local/system variable/function binding and
all opcode/type semantics. Nonempty storage alone is not executable AI behavior.

No canonical MobRuntime/header edits or new runtime catalog were made. Borrowed
condition_owner and subject/service lifetime requirements remain unchanged. Full
factory state/system-function assembly, asset parsing and world/Lua/root
scheduling/destruction still require work. No SQL access/contracts changed;
coordinator owns final isolated SQL and complete integration validation. The
server is not playable.

Implementation commit: `0cd5bed831e85d05d3c1d6a105cd88d180f40699`.

## Milestone 19 — literal VM execution and actual retained value stack

Evidence: ps_game.exe CKFSM<CMob>::TravelCondition 00498cf0, numeric dispatch
00498d9d, string dispatch 00498e99, dispatch table 0049af14 and return 0049aee6;
AI_Param assignment 0048fa90; CKFSM constructor 004951b0 passes 64 at 00495226
to KStack<AI_Param> constructor 0049af80. Focused serialized Ghidra export:
`work/exports/09-mob-events/ps_game.exe`.

Added the actual retained value stack to the existing configured FSM owner:
capacity/top/slots correspond to +3c/+40/+44. Default capacity is the recovered
64, top -1. Native unused type/text zeros are internal normalization; original
AI_Param[] construction leaves them uninitialized while zeroing numeric payloads
and VariableID. No parallel VM/stack owner was introduced.

interpret_mob_ai_literals executes only the recovered literal/ignored-kind
TravelCondition subset, through an explicit opt-in condition service. It resets
top to -1 and cursor to 0 on each call without clearing slots. Every fetch
increments cursor before comparing signed declared token count, so ordinary end
leaves cursor one past the count; null first storage also stops at cursor 1.
Numeric kind 2 selects type 1/int, 2/float or 3/DWORD and changes only that payload
in a retained temporary. Previous payload fields survive subsequent literals,
even across separate string literals. An unknown number type after a known one
pushes the previous temporary unchanged. Numeric VariableID is defined zero.

String kind 7 pushes type 4 and zero scalar/VariableID fields, copying through
its first NUL while retaining the destination slot's string tail. Numeric pushes
retain the destination's entire text. All pushes increment the logical top; when
capacity is exceeded the write is skipped without clamping top or clearing the
old slot. Ignored kinds 5/6 and outside the original 2..9 dispatch range do not
push. Literal-only completion returns zero, not the top value. There is no
invented constant-expression truth conversion.

Kinds 3/4/8/9 (variable/function/operator branches) are explicitly rejected by
this LIMITED provider after any already executed prefix. The original first
numeric scratch TYPE is undefined until a recognized number type: that unsafe
untyped-first path is rejected rather than guessed. Invalid capacity/storage,
index wrap, declared count beyond retained storage and unterminated strings are
host errors; these errors are not claimed as original VM results. The full
TravelCondition provider remains mandatory for general assets.

### Validation

- 1,536 retained sequences / 3,048 complete original TravelCondition executions,
  numeric/string/default dispatch and actual AI_Param assignment. Literal paths
  adapt only the security cookie; no interpreter/stack/result oracle port.
- Actual KStack constructor executes with the 64-capacity argument and array
  allocation port: all 64 AI_Param numeric payloads/VariableIDs become zero,
  while poisoned type/text bytes remain untouched. Native defaults compare the
  defined constructor fields only.
- Cases include all supported literal types, ignored kinds, later unknown number
  types, independent signed declared counts, null storage, cursor reset, capacity
  0..6 with two extra canary slots, overflow, repeated calls, all 148 retained slot
  bytes, float bits and string lengths 0/1/63/127. There is no undefined-byte mask
  on valid literal stack results.
- Native checks use the same configured owner's retained stack; a configured entry
  also dispatches through a borrowed condition_owner and changes that owner's
  actual stack. Unsupported branches and undefined first-number-type admission
  are tested as explicit errors, never successful/no-op VM providers.
- Fixture regenerated byte-identically; original SHA256SUMS pass; diff check clean.
- `python3 tools/dev.py build`: exit 0, no compiler warnings;
  `work/mob_ai_states_literals-build-final.log`.
- `python3 tools/dev.py test`: exit 0, 138/138 tests pass (38.17 s),
  literal VM test 0.38 s; `work/mob_ai_states_literals-tests.log`.

### Required integration / remaining work

Keep this stack with the retained configured FSM, initialized once at 64 slots;
TravelCondition must reset only logical top/cursor, not reallocate/clear values.
Actual parsed condition words 1/2 are signed declared token count / mutable cursor;
owned token storage remains separate. Do not install the literal-only service as
an implementation of unsupported expression assets. No canonical shared headers
were edited. Source/condition/stack lifetimes and scheduling exclusion remain
required.

Next interpreter work: local/system variable/function binding, parameter piping,
stack pops and arithmetic/comparison/control opcodes. Complete factory assembly,
asset parsing and root world/Lua/scheduling/destruction remain incomplete. No SQL
access/contracts changed; coordinator owns final isolated SQL and full integration
validation. The server is not playable.

Implementation commit: `c2de343e18245996df2ffb07319a12370670e23a`.

## Milestone 20 — actual local-variable VM reads on retained owners

Evidence: ps_game.exe TravelCondition local branch 00498e24..00498e94,
GetHashCode 0048fb10, real local map constructor/operator[]/destructor
00495a80 / 0049b7e0 / 004959c0 and AI_Param assignment 0048fa90. Original
TravelCondition 00498cf0 executes the complete admitted streams. Focused serialized
Ghidra export: `work/exports/09-mob-events/ps_game.exe`.

Added an explicit local-value condition provider on the existing value/literal
interpreter core. The strict literal-only API retains its previous contract and
still rejects local reads; callers opt into the expanded subset deliberately.
No second variable catalog, value stack or lookup provider was introduced.

Token kind 3 hashes TokenStr at +a0, NOT the value array at +20, function ID or
DWORD payload. Hash uses original signed-char multiply-by-33 / DWORD wrapping
through the first NUL. It reads the already admitted typed value from the actual
configured owner's unsigned-key variables map. Scalar fields, including the
value's embedded VariableID, are copied verbatim; the lookup hash does not
replace VariableID. String values copy their prefix through NUL and retain the
destination slot tail; nonstrings retain the destination's complete text. The
original temporary-copy then stack-copy is represented by the equivalent final
assignment for admitted typed values. It does not mutate the variable map or
numeric-literal temporary.

Lookup/copy occur before push, so even a full stack resolves the local and
validates its string. Push still increments logical top while skipping an
out-of-capacity write. Existing cursor/count, retained stack and literal behavior
are shared with milestone 19. Local/literal-only completion still returns zero.
System functions, arithmetic/assignment/control branches (kinds 4/8/9) remain
explicit errors, not fabricated results. Missing entries and unterminated local
names/values are host errors. Original missing-map insertion has indeterminate
type/text; no native default value or insertion is invented for that path.

### Validation

- 1,024 retained mixed sequences / 1,920 complete original TravelCondition calls.
  Actual TokenStr hash, unsigned STL map lookup, local-value copies and stack
  assignments execute. Real map construction, AddLocalVariable admission and
  destruction also execute. Only allocation/free/security cookie are adapted;
  there is no variable-map, hash or interpreter oracle port.
- Fixtures include empty/127-byte/high-bit names, Aa/B@ collision, competing token
  value/function/DWORD data, typed local values 0..5, float bits, independent
  embedded VariableIDs, string lengths/tails, literal/local interleaving, signed
  declared counts, capacity 0..7/canaries, overflow and repeated calls.
- Original map bytes remain unchanged through execution. All map allocations
  are freed and poisoned on teardown: 6,144 original tree frees observed.
  Native comparison checks complete retained map/stack bytes, top/cursor and
  original zero return without pointer/undefined-byte masks on admitted values.
- Configured native entry with condition_owner bound to another FSM reads that
  owner's actual local map and writes its retained stack, despite a competing
  same-name local in the containing owner. Missing typed locals are tested as
  explicit non-inserting errors. Existing literal-only unsupported-kind checks
  remain intact.
- Fixture regenerated byte-identically; original SHA256SUMS pass; diff check clean.
- `python3 tools/dev.py build`: exit 0, no compiler warnings;
  `work/mob_ai_states_locals_vm-build.log`.
- `python3 tools/dev.py test`: exit 0, 139/139 tests pass (30.90 s),
  local VM test 0.43 s; `work/mob_ai_states_locals_vm-tests.log`.

### Required integration / remaining work

Bind decoded local identifiers from the same name-hash scheme used by the factory;
retain the admitted variables with their configured FSM. Do not rewrite embedded
VariableID from the map key or use a separate world-local cache. Native inputs
for initial map bytes are explicit retained projections, not a claim that the
original allocator initializes unused type/text fields. For foreign condition
owners, use their actual maps/stacks and retain their lifetimes. No shared
canonical headers were edited.

The expanded provider is still not complete TravelCondition. System function
and parameter piping, operator pops/arithmetic/comparisons/assignment, factory
assembly, asset parsing and real root world/Lua/scheduling/destruction remain
open. No SQL access/contracts changed; coordinator owns final isolated SQL and
complete integration validation. The server is not playable.

Implementation commit: `da753b226d0c2784fa677f382fff8ea1ef73c212`.

## Milestone 21 — integer comparison VM and actual conditional transitions

Evidence: ps_game.exe TravelCondition comparison branch 0049a83a, pair pops
0049a885..0049a8f6, type categories 0049a8fa..0049a91b, comparison dispatch
0049a920 / table 0049af60, accumulator increments at e.g. 0049a9fe and return
0049aee6. Complete original TravelCondition is 00498cf0. Focused serialized
Ghidra export: `work/exports/09-mob-events/ps_game.exe`.

Added an explicit int/DWORD comparison provider on the existing local/literal
VM core. Existing strict literal/local services preserve their prior limits.
Kind 9 consumes the top value first, then the next; both logical pops leave the
stored slots intact. For signed int/int operands, opcodes 0..5 test first<second,
second<first, first<=second, second<=first, equality and inequality. For first int /
second DWORD, the integer's DWORD bit pattern participates in unsigned comparison.
A first-popped DWORD contributes no truth result for these admitted types in the
original, including DWORD/DWORD: that behavior is retained, not corrected.

Each true comparison increments a wrapped DWORD accumulator. Comparisons do not
push a boolean and do not short-circuit subsequent tokens. Completion returns the
accumulated count as signed DWORD; unknown comparison opcodes still consume both
values and contribute nothing. Numeric-literal temporaries and local maps remain
independent. Valid pops do not clear or normalize retained stack payloads.

The new provider is deliberately restricted to int/DWORD pairs. Noninteger
comparison operands, stack underflow or logical overflow with undefined operand
storage raise explicit host errors, not fabricated original results. Kind 8
arithmetic/assignment and kind 4 system functions remain unrecovered errors.

### Validation

- 2,048 original retained sequences / 3,840 complete TravelCondition calls, with
  real local map construction/admission/hash/lookup/destruction and original
  numeric literals, comparison pops/dispatch and return accumulator. Only
  allocation/free/security cookie are adapted; no VM/comparison/map oracle port.
- All six comparison opcodes, four int/DWORD operand pairings, signed extrema /
  unsigned high bits and exact inequality/equality cases are covered. Inputs mix
  locals/literals, one to three comparisons, unknown opcodes, signed/truncated
  declared counts, repeated calls and retained/canary slot payloads. Original
  return counts include 0, 1, 2 and 3. Complete stack/map bytes and cursor/top /
  return count are compared. Original map bytes are unchanged and all 6,144 tree
  allocations are freed on teardown.
- Native configured Evaluate uses a real comparison program that returns two
  matches to meet a transition threshold of 2. Changing the retained local makes
  the first poll stay and the next enter the target through existing exit/enter /
  current publication behavior; a boolean-only provider would fail this test.
  Borrowed-owner entry and prior local/literal error-boundary tests remain.
- Underflow and unsupported float operands are explicit-error tests, not asserted
  equivalent original defined behavior. Floating/string matrices remain open.
- Oracle regenerated byte-identically; original SHA256SUMS pass; diff check clean.
- `python3 tools/dev.py build`: exit 0, no compiler warnings;
  `work/mob_ai_states_compare-build.log`.
- `python3 tools/dev.py test`: exit 0, 140/140 tests pass (31.52 s),
  comparison VM test 0.79 s; `work/mob_ai_states_compare-tests.log`.

### Required integration / remaining work

Use the same retained value stack/local map and signed declared cursor/count
contract. Consume comparison return counts without boolean conversion; configured
transitions already sum all condition results with wrapped DWORD arithmetic.
Do not reverse pop order or impose conventional source-expression operand order.
Do not silently fix the original first-DWORD no-result category. No shared
canonical headers or duplicate runtime owners were introduced.

This is not complete TravelCondition: float/noninteger comparison matrices,
arithmetic/assignment/control operators, system functions/parameter piping,
factory assembly/assets and real root world/Lua/scheduling/destruction remain
incomplete. No SQL access/contracts changed; coordinator owns final isolated SQL
and full integration checks. The server is not playable.

Implementation commit: `69e561f750eef780865eec9dcf689ed964caf260`.
