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

No shared canonical files changed. Map projection to `MobRuntime` fields:
`state_stack.has_value()` -> fsm_enabled; top stack status (zero when empty) ->
scene_status; status, move_mode, vision_fixed_user, target_kind/target_id,
combat_timeout, next_move, attack_times and definition->ai directly.
Copy projection changes back before any provider callback and reflect provider
mutations before resuming. Alternatively integrate the coordinator into the
production owner directly with the same tested ordering.

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
