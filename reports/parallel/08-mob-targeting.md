# Worker 08: mob targeting

## Scope and evidence

Recovered from immutable `originals/ps_game/ps_game.exe` (SHA256
`0fb7c9f13b5bcf186118f8bfe73ab214226b93e8aa502b8e6fded4a8312bc547`):
Ghidra full exports in `/home/franco/Projects/Eternity/work/reverse/ps_game.exe/`
(`decompiled.c`, `disassembly.txt`, `types.txt`). No shared project edits.

- `CMob::SetTarget` 0049fad0, `CMob::SetTargetEx` 004b65e0.
- Real `CMob::GetStatus` 0041d760: inactive FSM reads +124; active empty stack
  returns zero; otherwise reads the top scene's +28 status.
- Help boundaries: `FindHelpNormal` 0049ff70 (mode zero), `FindHelpAttack`
  004a0470, `FindHelpNormal2` 004a0200. Dispatch boundary 004925f0.

## Implemented behavior

Explicit native projection preserves target type/ID, raw and scene status,
movement mode/time, combat timeout, fixed vision, AI kind, and three enabled /
skill-present / uint16 cooldown slots. Providers are mandatory abstract methods.

SetTarget status 100 bypasses life/move/FSM gating, changes the target unless
vision-fixed, and initializes all enabled skill timers (including slot zero).
Other statuses require idle or moving status 4 with move mode != 1, and life
status != 1. Inactive FSM changes status to 1 and clears next move, then writes
target; the initialize flag controls help/timers only. Active FSM calls Ex then
dispatches event 1 with the CURRENT target ID, even when vision-fixed blocked Ex.
Ex has no life/movement gate, clears combat timeout after writing the target,
and initializes help/timers only for effective status zero. Normal initialization
zeros slot zero; disabled or missing skills retain their timers. Zero cooldowns
zero timers; other slots call random before tick, using uint32 wraparound.
Help occurs after target/status/timeout writes and before timers, and providers
can mutate the live projection. Random and tick providers are separate required
ports, not silently seeded substitutes.

## Validation

- `python3 tools/dev.py build`: exit 0 (initial full build and final incremental
  build). Final build has no compiler warnings. Initial build reported existing
  guild-source warnings outside this ownership scope.
- `python3 tools/dev.py test`: exit 0; all 120 native tests passed in 37.29 s.
  `mob-targeting-unit` passed all 4,096 original-execution comparisons (0.01 s).
- `git diff --check`: clean.
- Logs remain ignored: `work/mob-targeting-build.log`,
  `work/mob-targeting-final-build.log`, `work/mob-targeting-test.log`.
Original oracle generated twice identically: 4,096 cases using real SetTarget,
SetTargetEx and GetStatus instructions. Covers both entry points, inactive/active
FSM (with independently seeded raw/scene status), statuses -1/0/1/2/4/5/100,
life/move/fixed gates, all AI help kinds,
independent enabled/present masks, cooldowns 0/1/65/65535, wraparound ticks,
retained timers and target mutations by help providers. Help/event bodies are
observed boundaries, not claimed as original world execution. All twelve
EXE/PDB entries passed `sha256sum -c SHA256SUMS`.

SQL integration is deferred to the coordinator per PARALLEL_WORK.md and task
instructions; this subsystem has no SQL calls or schema changes. No database
lifecycle action was performed. The server remains unplayable.

## Integration dependencies / canonical changes needed

No shared canonical files changed. The runtime bridge now maps `MobRuntime` fields:
`state_stack.has_value()` -> fsm_enabled; top stack status (zero when empty) ->
scene_status; status, move_mode, vision_fixed_user, target_kind/target_id,
combat_timeout, next_move, attack_times and definition->ai directly.
The bridge writes changes before every help/random/tick/dispatch callback and
reloads live fields and definition bindings afterward. It never replaces the
scene stack. Call the MobRuntime overloads using an explicit
`MobTargetingRuntimeServices` implementation.

Canonical definition bindings still need three original `stMobAttackInfo`
entries: enabled byte at definition +82 +20*i, optional skill pointer at
+90 +20*i, and referenced skill's uint16 cooldown at +4a. Existing
MobRuntimeTemplate lacks these bindings; SkillDefinition does not expose this
cooldown. Preserve enabled and missing-skill independently. Do not infer the
skill pointer from attack damage or treat absent bindings as disabled attacks.

Required production help implementations and actual scene event dispatch remain
external providers. Worker 09 / coordinator owns FSM handler execution; this
module emits the exact recovered event and argument, without inventing scene
transitions or a default no-op. User projection needs ID (+dc) and life status
(+142c). Native scene lifetime/dispatch integration remains coordinator work.

Implementation commit: `8a1d046` (workers/08-mob-targeting).
This report is committed separately after validation.


## Milestone 2: live MobRuntime integration

Added `mob_targeting_runtime.hpp/cpp`: overloads for the real `MobRuntime` owner,
using existing canonical fields without shared-header changes. Inactive, empty
active, and populated active scene stacks map to the original GetStatus rules.
Help and dispatch providers receive the live object after preceding writes;
random/tick callbacks also observe current timers and may update live bindings.
Reloading after each callback retains changed target IDs, status, movement,
fixed vision, attack timers and provider-owned scene-stack transitions. Original
SetTarget continues to dispatch event 1 with the target ID after help. Exceptions
propagate with preceding live writes retained; the bridge never commits an old
snapshot during unwinding.

`MobTargetingRuntimeServices::bindings` is required and supplies the AI byte and
three live enabled/skill-present/cooldown bindings. It must be a read-only
snapshot resolver, not an inferred damage mapping. Production help, shared
random/tick and scene dispatch are still required implementations. No world
provider has been defaulted to a no-op. Original instruction evidence remains
SetTarget 0049fad0 / SetTargetEx 004b65e0 / GetStatus 0041d760 in ps_game.exe.

New `mob-targeting-runtime-unit` compares all 4,096 original-execution cases
through real MobRuntime objects, independently of the projection-only test.
Additional integration cases cover empty FSM stacks, help replacing the scene
stack, changed cooldown after random, mutations of earlier/later timers during
random/tick, current target delivery to dispatch, dispatch replacing the stack,
and exception propagation after target/timeout writes. Those additional callback
mutation assertions are native integration checks; they do not claim execution
of the missing original world/scene handler bodies.

Milestone implementation commit: `dec69cb` (workers/08-mob-targeting).

Validation:

- `python3 tools/dev.py build`: exit 0, no warnings in
  `work/mob-targeting-runtime-build.log`.
- `python3 tools/dev.py test`: exit 0, all 121 native tests pass (34.71 s).
  New `mob-targeting-runtime-unit`: pass, 0.03 s. Log:
  `work/mob-targeting-runtime-test.log`.
- Regenerated original fixture under `work/mob-targeting-runtime-evidence.tsv`;
  byte-identical to tracked oracle. All twelve original EXE/PDB hashes verified.
- `git diff --check`: clean. SQL integration remains coordinator-owned;
  this block has no SQL changes or calls.

Remaining integration: coordinator can now call the MobRuntime overloads directly;
no separate projection adapter is needed. Required production bindings must still
come from the canonical mob/skill definitions described above, and production
help/FSM providers and combat call sites must be connected. The native bridge
preserves provider scene transitions but does not implement scene handler bodies.
No global gameplay completion is claimed.
