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


## Milestone 3: ally help selection

Recovered the three direct SetTarget/Ex help callees: `CMob::FindHelpNormal`
0049ff70, `FindHelpNormal2` 004a0200 and `FindHelpAttack` 004a0470 in ps_game.exe.
`mob_targeting_help.hpp/cpp` implements their user gates, bounded 3x3 cell
selection in Z-major/X-minor order, effective-status gate, variant filters,
original X/Z distance calculation and inclusive range comparison. Normal needs
the same uint16 mob type; Attack needs candidate AI 1 or 3; Normal2 needs neither.
No variant excludes the source mob by identity. The source zone user-list count
(+130) must be positive; user country must differ from source definition nCountry
(+78); life status must differ from 1, HP ability byte must be zero, and admin
attackable/visible bytes must be nonzero.

The chase-range byte (+69) is captured once before scanning. Source position and
mob type are live reads for each candidate. Squared distance uses float deltas,
double products/sum/sqrt, then a float distance; NaN never qualifies. A required
world provider traverses each selected cell synchronously under its lock and
advances the live intrusive cursor after callbacks. The selection coordinator
invokes required SetTarget(current candidate, original user, false), preserving
source mutations. Production list ownership/locking and native SetTarget wiring
remain required; there is no fallback empty traversal or no-op acquisition.

New original-execution oracle runs the actual three helper bodies, inline
GetStatus/deque traversal, intrusive cell cursor loops and CRT sqrt. Only locks,
CRT thread-local storage and SetTarget(false) are ports. The fixture contains
1,536 cases, including all three variants, active empty/nonempty and inactive
FSM, all input user gates, multiple map edges/dimensions, equal/outside/zero
radii, NaN/infinity, and SetTarget-driven source position/type/range mutations.
It records 1,570 balanced lock pairs and 289 acquisitions: Normal 79, Attack 55,
Normal2 155. The port asserts original user identity and initialize=false.
The fixture models one candidate per cell; concurrent/removing multi-candidate
world lists still need provider integration coverage.

Needed canonical integration additions: definition nCountry (signed int at +78)
and byChaseRange (uint8 at +69), plus the existing mob type/position/cell and
current zone user count. User projection needs country (+12d), life (+142c), HP
ability (+1366), visible (+5774) and attackable (+5775). The world owner must
provide synchronous lock-held live-cursor mob traversal per cell; a copied
`in_cell` vector cannot claim the original removal/reentrancy semantics. Bind
SetTarget(false) to the current live candidate and original user through the
runtime bridge already implemented. No shared canonical files were changed.

Implementation commit: `10b54f0` (workers/08-mob-targeting).

Validation:

- `python3 tools/dev.py build`: exit 0, no warnings;
  `work/mob-targeting-help-build.log`.
- `python3 tools/dev.py test`: exit 0, all 122 native tests passed (33.65 s).
  `mob-targeting-help-unit` passed in 0.01 s;
  `work/mob-targeting-help-test.log`.
- Regenerated fixture under `work/mob-targeting-help-final.tsv`, byte-identical
  to tracked fixture. Original twelve EXE/PDB hashes passed SHA256SUMS again.
- `git diff --check`: clean. No SQL changes/calls; isolated SQL integration
  remains coordinator-owned.

Remaining work: connect the required world list and live SetTarget providers,
recover/validate multi-candidate removal/reentrant cursor semantics, and supply
the canonical definition/user bindings above. Original-client playability is
still not achieved.


## Milestone 4: multi-candidate callback cursor evidence

Recovered focused Ghidra evidence through the locked --process wrapper into
ignored `work/exports/08-mob-targeting/ps_game.exe/` (no import/project edits).
Additional ps_game.exe symbols: `SSyncList<CMob>::Front` 0041f190, `Next`
0041f1c0, `Remove` 00424ac0, `AddBack` 00424a80, and `CCell::FindMob` 00423680.
Remove changes the shared cursor to the removed node's predecessor only when
removing the current cursor, then unlinks/zeros the node and decrements count.
AddBack appends to the sentinel's tail. The helper's next step follows the
CURRENT shared cursor's next link; it never retains a private next iterator.
FindMob starts at Front and leaves that same cursor at the match or sentinel.

New original-execution fixture `mob-targeting-cursor-oracle.tsv` covers 450
traces: all three helpers, 1..5 original candidates, inactive/active empty/active
idle scene stacks, and ten callback modes. These include removing current/next/
previous, appending, nested lookup of tail/current/missing ID, removing then
reappending current, and removing next then lookup miss. The callback driver
executes actual original Remove/AddBack/FindMob instructions with their original
register conventions. Helper selection/next-step logic, CRT sqrt and scene
reads also execute original instructions. Only OS locks/TLS and the SetTarget
callback itself are ports; this does not claim a recovered SetTarget can remove
its own actor or that the fixtures execute actual AI/world handler bodies.

Important recovered outcomes (original idle candidates 1,2,3,4): nested lookup of
4 while acquiring 1 ends the outer traversal; nested lookup miss while acquiring
3 restarts at 1, yielding acquisitions 1,2,3,1,2,3,4. Removing/reappending current
3 yields 1,2,3,4,3 and final membership 1,2,4,3. Appending 5 while acquiring 2
makes 5 visible later in the SAME traversal. Restoring the outer cursor after a
nested lookup, precomputing next, or traversing a membership snapshot would
change original behavior.

`mob-targeting-cursor-unit` compares these traces and final list order against
the actual existing ZoneMobs cell cursor, composing native help selection with
ZoneMobs enter/leave/find_cell. A test-only MobViewer consumes send_leave packets
locally as cursor visits; no packet is transported and no production adapter is
built on this packet probe. Nested lock traces are represented by the test
provider; this validates ordering/cursor semantics, not multithreaded OS locking.
This evidence closes the previous one-candidate fixture limitation for those
explicit mutations, while actual world-handler reentrancy remains integration
work. No canonical ownership or headers were modified.

Exact canonical interface proposal: add a synchronous
`visit_cell_mobs(CellCoordinates, const std::function<void(MobRuntime&)>&)` to
ZoneMobs using its existing Cell::cursor, not a new cursor. Start at its first
member, invoke the visitor, then advance from the possibly changed shared cursor
(zero/sentinel advances to current first member). Allow visitor enter/leave and
find_cell calls; do not save/restore cursor around them. Do not dereference the
visited actor after the visitor returns. The zone owner must serialize/hold the
cell lock across the entire operation and forbid zone destruction during it.
The existing send traversal already demonstrates the required cursor algorithm;
extract/reuse it without creating an independent help membership owner.

Evidence/test implementation commit: `9b49604` (workers/08-mob-targeting).

Validation:

- Locked Ghidra --process export: exit 0; `work/mob-targeting-cursor-ghidra.log`.
- `python3 tools/dev.py build`: exit 0, no warnings;
  `work/mob-targeting-cursor-build.log`.
- `python3 tools/dev.py test`: exit 0; all 123 native tests passed (41.41 s).
  New `mob-targeting-cursor-unit`: pass in 0.01 s;
  `work/mob-targeting-cursor-test.log`.
- Original fixture generated twice identically; repeat remains ignored in
  `work/mob-targeting-cursor-repeated.tsv`. All twelve EXE/PDB SHA256SUMS pass.
- `git diff --check`: clean. No SQL calls/changes; integration remains with
  the coordinator.

Remaining integration: the proposed canonical visitor, skill/country/range/user
bindings, and production SetTarget/help/FSM world wiring. The cursor evidence
allows a shared implementation instead of a duplicate membership projection.
No playable-server completion is claimed.


## Milestone 5: first-user acquisition search

Added `mob_targeting_find.hpp/cpp` for the immediate acquisition callers,
`CMob::Targeting` 0049fcf0 and `CMob::FindTarget` 0049fd20 in ps_game.exe.
Evidence is the existing full Ghidra decompilation/disassembly, especially
0049fe10..0049feb2 (user gates/distance), 0049ff3a..0049ff66 (SetTarget(true)
inside the cell lock, then unlock and return the original selected actor).

Targeting only calls FindTarget for AI 1 or 3. Direct FindTarget has no AI gate.
FindTarget requires positive zone user count, scans a clamped 3x3 neighborhood
Z-major then X-minor in each cell's current user-list order, and stops at the
FIRST eligible user, without sorting by distance. It excludes life status 1,
nonzero shape byte (+1354), nonzero HP ability byte (+1366), and zero attackable/
visible flags (+5775/+5774). Range is inclusive and uses the same original
float-delta / double squared-sum / sqrt / float-distance rounding. Unlike help,
chase range is read for each candidate. NaN/infinity/out-of-range are rejected.
There is NO country, fixed-vision or mob-status selection gate. After choosing,
it calls SetTarget(source, actor, true) under the user-cell lock, then returns the
chosen identity even when SetTarget rejected its status/fixed-vision gate.
Targeting therefore can return true without changing the target fields.

The new required provider synchronously traverses live user lists under their
cell locks, stops immediately when the visitor returns false, and unlocks before
returning. It supplies a nonzero opaque actor token (not CharID, which can have
independent values) and resolves it for SetTarget(true). Native world ownership,
user-list locking, actor lifetime and source bindings remain provider work;
no no-op/empty traversal or fake users were added. No shared headers changed.

New oracle executes both original entry points, REAL original SetTarget/GetStatus
and CRT sqrt. Help bodies, OS user-cell locks and CRT TLS remain observed ports;
attack definitions are disabled for this search-focused fixture. 2,048 cases
contain two users per occupied cell, all AI gates, zero/positive zone counts,
rectangular dimensions/edges, user eligibility flags and same-country users,
zero/exact/adjacent-float/large ranges, NaN/infinity/large coordinates, changes to
source position/range during lock acquisition, and status/fixed-vision rejection
after selection. It records 2,357 balanced locks, 432 SetTarget(true) calls and
63 help calls. Native tests compose search with the real native SetTarget,
checking return value, final target/status/movement fields and ordered callbacks.

Needed canonical integration: a synchronous early-stop user-cell visitor using
the shared user-list cursor, current shape/HP/admin/position projections, source
zone count/cell/position and live definition AI/chase range. Wire selected tokens
to existing MobRuntime SetTarget overloads and existing required runtime services.
Do not add hostility filtering to FindTarget because help has such a filter, or
interpret a successful Targeting return as proof the target was changed.

Implementation commit: `667e37a` (workers/08-mob-targeting).

Validation:

- `python3 tools/dev.py build`: exit 0, no warnings;
  `work/mob-targeting-find-build.log`.
- `python3 tools/dev.py test`: exit 0; all 124 native tests passed (45.15 s).
  New `mob-targeting-find-unit`: pass in 0.03 s;
  `work/mob-targeting-find-test.log`.
- Regenerated original fixture under `work/mob-targeting-find-repeat.tsv`;
  byte-identical to tracked oracle. All twelve original EXE/PDB hashes pass.
- `git diff --check`: clean. This search has no SQL calls or schema changes;
  isolated SQL integration remains coordinator-owned.

Remaining work: production user/mob visitor integration, source/skill/user
bindings and actual world/FSM/helper services. The original native acquisition
search is implemented through mandatory providers, not a live world scheduler.
The server remains unplayable.

## Dependency checkpoint after milestone 5

Reviewed current main CONTINUITY.md/PARALLEL_WORK.md and commits
667e37a/f28f799. Main is under the coordinator's canonical validation freeze;
this worker has not changed its sources, CMake, databases or live workers.
The assigned SetTarget/Ex coordination, their help selection, shared-cursor
mutation evidence, immediate Targeting/FindTarget callers and MobRuntime bridge
have all been delivered. The remaining work is production integration, not a
reason to repeat the completed fixtures or add another simulated world owner.

Read-only inspection confirms canonical ZoneMobs still has no synchronous mob
visitor API, and MobRuntimeTemplate still lacks the country/chase-range/three
skill bindings needed by these routines. User cell traversal/actor projections
and actual FSM/help services are also required. Existing in_cell snapshots do
not satisfy the original cursor semantics verified in milestone 4. A packet
probe was useful for tests but must not become a production traversal adapter.

Concrete next-task proposal: coordinator supplies/reviews canonical synchronous
shared-cursor mob and early-stop user visitors plus live definition/user bindings,
then assigns this worker the production acquisition/help composition and its
original-execution integration comparisons. Alternatively explicitly assign the
mob definition/skill binding recovery under mob_targeting_bindings, coordinated
with the canonical data owner. Until then, no independent meaningful change is
left within the current exact function/interface ownership without duplicating
shared ownership or inventing providers.

This checkpoint changes documentation only. Validation: read-only canonical API
inspection and git diff --check; native/SQL suites were not repeated because no
code or fixtures changed. Last substantive validation remains milestone 5:
124/124 native tests passed, 2,048 new original search comparisons. This is not
production integration or original-client playability. Waiting for a concrete
assignment/interface dependency resolution; no merge, push or main edits.
