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

## Milestone 6: authorized mob_targeting_bindings expansion

Coordinator explicitly expanded ownership to the new mob_targeting_bindings
prefix. Reviewed current canonical MobRuntimeTemplate and SkillDefinition
read-only. Added a positional decoder/owned catalog; no shared headers or world
owners changed. Earlier milestones and fixtures are preserved, not regenerated.

### Recovered loader and Init behavior

ps_game.exe evidence: `CGameData::LoadConstMobSkill` 004c0be0,
`LoadConstMob` 004c1720, `GetMobSkillInfo` 004027e0, `CMob::Init` 00490040 and
`SetAttack` 004902f0. Focused Ghidra exports were generated with the locked
--process wrapper at `work/exports/08-mob-targeting/ps_game.exe/`.

The assembly at 004c1d7a..004c1de8 is decisive: it saves/restores the requested
SKILL ID and original enabled BYTE. Misleading decompiler aliases call these the
mob ID and attack index. Those aliases must not be copied into native code.

Each attack starts with the requested uint16 type/skill ID, uint8 byOK, and null
skill pointer. If type is zero, preserve byOK and clamp attack range to 60 only
when byOK != 0. If type is nonzero, lookup the separate mob-skill table; accept
only effect 1, effect 3, or effect 2 with detail 0x29/0x2a. An accepted skill
retains the original requested ID and byOK, including byOK=0. Rejected/missing
skills clear type/byOK/pointer but do NOT run the normal-attack range clamp.
First/second attack attributes >4 become zero; third attribute is retained.
Third special >15 becomes zero (existing respawn selector convention).

Country at stMOBInfo +0x78 is DERIVED from the third attack attribute, not loaded
from an independent Country column: raw 1 -> country 0, raw 2 -> country 1,
everything else -> country 2. Chase range +0x69 is an un-clamped byte. Mob-skill
wResetTime +0x4a is a uint16 count of SECONDS, including 0 and 65535; multiplying
by 1000 belongs to targeting, not loading. Skill level is forced to 100 by the
mob-skill loader. Init retains the existing definition pointer, does not rewrite
definition/skill bindings, and resets the three runtime attack timers to zero.
No per-mob skill allocation/copy or reference ownership transfer occurs.

### Positional SQL contract (one-based procedure columns)

Load `usp_Read_MobSkillMaxID_R`, allocate/zero MaxID+1 mob-skill slots, then read
`usp_Read_MobSkills_R` (82 columns). Only then load `usp_Read_MobMaxID_R`,
allocate/zero MaxID+1 mob-definition slots and read `usp_Read_Mobs_R` (69 columns).
These are distinct from user skill definitions/levels. Preserve fetch order;
duplicate IDs overwrite the same stable slot (last row wins). Null selected
fields are zero because each original row buffer starts zeroed. Column labels
are not the binding contract; the native test deliberately supplies duplicate
misleading labels. Do not use SELECT * from a guessed table or reorder aliases.

| Procedure | Column | Projection / original offset | ODBC binding |
| --- | ---: | --- | --- |
| MobSkills_R | 1 | uint16 skill ID, +0 | length 2, C type 5 |
| MobSkills_R | 17 | uint8 effect, +0x31 | length 1, C type -6 |
| MobSkills_R | 18 | uint16 detail, +0x32 | length 2, C type 5 |
| MobSkills_R | 38 | uint16 cooldown seconds, +0x4a | length 2, C type 5 |
| Mobs_R | 1 | uint16 mob ID, +0 | length 2, C type 5 |
| Mobs_R | 5 | uint8 AI, +0x26 | length 1, C type -6 |
| Mobs_R | 45 | uint8 chase range, +0x69 | length 1, C type -6 |
| Mobs_R | 46 + 8*i | requested uint16 skill/type | length 2, C type 5 |
| Mobs_R | 47 + 8*i | uint32 attack time | length 4, C type 4 |
| Mobs_R | 48 + 8*i | uint8 attack range | length 1, C type -6 |
| Mobs_R | 49/50 + 8*i | uint16 damage/damage add | length 2, C type 5 |
| Mobs_R | 51/52 + 8*i | uint8 attribute/special | length 1, C type -6 |
| Mobs_R | 53 + 8*i | uint8 enabled | length 1, C type -6 |

Here i=0,1,2 and attack base is +0x80+20*i (type +0, enabled +2, time +4,
range +8, damage +10, add +12, attribute +14, special +15, skill pointer +16).
Country comes from column 67 before the above country mapping. The fixture footer
records EVERY original GetData column's length/C type, including unused fields.
The decoder validates required row widths and selected numeric ranges, preserves
16/32-bit signed-bit-pattern inputs and zero nulls, and rejects missing/out-of-
allocation rows. Caller must validate MaxIDs fit the supported uint16 ID domain
before passing them; no implicit narrowing from a database scalar is acceptable.
It is a targeting projection, not a replacement for all 82 skill fields, all mob
normalizations, ODBC diagnostics or the original loader's failure/publication FSM.

### Concrete owner/lifetime and minimal canonical patch contract

MobTargetingBindingCatalog allocates both arrays once, fills the skill array
first, then resolves definitions' non-owning pointers into that array. Copying
is disabled; moving transfers buffers and preserves pointers (tested). The data
owner must retain the catalog for longer than every template, pooled MobRuntime
and targeting/AI callback that references it. Do not return bindings pointing
into a local ResultSet, temporary decoded row, or temporary catalog. Do not
resize/replace the skill array while such definitions remain published.

Minimal integration does NOT require changing user SkillDefinition or copying
world membership: add this catalog to the canonical persistent game-data/template
owner, initialize it from the same original procedure results/MaxIDs before
publishing mobs, and expose a required lookup by canonical definition->id.
MobTargetingRuntimeServices::bindings can return catalog.mob(id)->targeting();
help/find source adapters read its country/chase_range. Missing definitions must
fail owner setup rather than return empty bindings. Existing MobRuntime keeps
its canonical definition pointer and timers; Init must not rebuild this catalog.

If the coordinator elects to fold fields into canonical structs instead: add
signed country and byte chase_range to MobRuntimeTemplate, three attack binding
records retaining requested type, RAW enabled byte and nullable stable mob-skill
reference, and uint16 cooldown_seconds to the separate mob-skill definition
projection. Existing SkillDefinition effect/detail fields correspond to +0x31 /
+0x32, but its user-skill map must not substitute mob-skill storage. Preserve
forced mob level 100 and distinct owners. Do not infer a skill from damage or
conflate enabled=false with absent pointer.

Original GetMobSkillInfo compares index <= stored COUNT, which is MaxID+1; this
admits a one-past-allocation read. The fixture deliberately avoids that undefined
slot. Native lookup safely rejects it and out-of-range IDs; holes and ID zero
remain absent. Do not manufacture a native pointer from adjacent allocation
contents. This boundary is documented, not claimed as deterministic original
behavior. Live skill reload must update existing slots in place or hold all old
owners until quiescent; this startup decoder does not implement reload or partial
publication on SQL failure. The original runtime can observe live pointed-to
cooldown changes; taking permanent per-mob cooldown snapshots would lose that.

### Validation scope

New oracle executes COMPLETE original skill and mob loaders, real GetMobSkillInfo,
Init and SetAttack. SQL/allocator/cookie/random/tick/path/compiled-AI/Lua boundaries
are explicit ports. 256 paired full loads yield 2,048 skill-slot comparisons,
1,024 definition comparisons and 1,024 Init checks. All 3,072 attack projections
match; 762 have skills, including 248 present-but-disabled; 178 normal attacks
retain nonzero enabled with NO skill. Cases include missing/rejected effects,
effect-2 details 40/41/42/43, wide enabled bytes, nulls, range clamps, unsigned
cooldown boundaries, duplicate rows, country mapping and stable references.
Native tests also compose existing Init and verify allocation/row guards and
pointer-preserving catalog move. Old targeting fixtures were not repeated by
the new oracle.

Implementation commit: `4ab7f85` (workers/08-mob-targeting).

- Locked Ghidra --process export: exit 0; log
  `work/mob-targeting-bindings-ghidra.log`.
- `python3 tools/dev.py build`: exit 0, no compiler warnings;
  `work/mob-targeting-bindings-build.log`.
- `python3 tools/dev.py test`: exit 0, all 125 native tests passed (35.87 s).
  New `mob-targeting-bindings-unit`: pass, 0.15 s;
  `work/mob-targeting-bindings-test.log`.
- Original full-load fixture regenerated identically under
  `work/mob-targeting-bindings-repeat.tsv`; byte comparison exit 0.
- All twelve original EXE/PDB entries passed SHA256SUMS. `git diff --check`
  clean. No generated Ghidra files, credentials or database data tracked.

Actual restored SQL distribution comparison remains coordinator integration work:
query the four ORIGINAL procedures above under Eternity's isolated SQL lock,
retain their exact positional result order, then decode before enabling the
production resolver. No SQL lifecycle/schema operation or credential exposure
was performed. Required world visitors/FSM/helper services remain unconnected;
this decoder and its tests do not make the server playable.

## Milestone 7: restored SQL binding distribution comparison

This block resolves milestone 6's pending restored-data comparison without
repeating the targeting fixtures or changing the decoder/canonical owners.
Added `mob_targeting_bindings_sql_unit.cpp` and the serialized read-only runner
`tools/mob_targeting_bindings_sql_oracle.py`. It calls only the four recovered
read procedures, validates scalar MaxIDs/result widths, exports an ignored
positional snapshot, runs the COMPLETE original skill/mob loaders and Init over
that snapshot, then compares all populated native targeting projections against
original execution. The runner uses Eternity's existing SQL resource_lock and
isolated container/network/runtime env; no DDL, reset, provisioning or writes.
It is deliberately not registered as an ordinary credential-free CTest.

The existing bindings oracle now accepts --sql-rows and allocates larger mapped
arrays for restored distributions. Its default synthetic output remains
byte-identical to the tracked milestone 6 fixture (regression checked after
refactor). An initial local argument-shadowing error in that refactor was fixed
before the successful regression/SQL runs; it was not a SQL failure. The SQL mode
explicitly refuses any input reference to the original one-past skill slot, so
no allocator-adjacent contents are invented as binding evidence.

Restored isolated SQL results:

- `MobSkillMaxID = 355`: 356 allocated slots, 355 procedure rows.
- `MobMaxID = 2746`: 2,747 allocated slots and 2,747 procedure rows.
- 2,746 nonzero-ID definitions compared; reserved ID-zero is excluded by the
  native lookup contract. These counts do not mean every record is spawnable;
  canonical HP/level/admission checks remain separate.
- All 355 skill projections and all 2,746 definition projections match original
  execution, including 8,238 attack positions and 1,871 resolved skill pointers.
  Twenty of those pointers have disabled byOK and must remain PRESENT.
- Original Init/SetAttack executed for all 2,746 nonzero-ID definitions; pointer
  retention, unchanged definition bytes and zeroed runtime attack timers passed.

Snapshots remain ignored in work/mob-targeting-bindings-sql-{rows,original}.tsv.
They contain all numeric procedure fields in fetch order; name columns are
intentionally replaced by the oracle's fixed SQL_C_CHAR name port. No name or
full non-targeting skill-layout equality is claimed. Relevant country/range,
three full attack records, skill ID/effect/detail/reset-time/forced-level and
binding/null identity are compared; no restored data is committed.

Reproduce after native build:

    python3 tools/mob_targeting_bindings_sql_oracle.py

Implementation commit: `96af8bc` (workers/08-mob-targeting).

- `python3 tools/dev.py build`: exit 0, no warnings;
  `work/mob-targeting-bindings-sql-build.log`.
- `python3 tools/mob_targeting_bindings_sql_oracle.py`: exit 0, complete
  read-only SQL/original/native comparison;
  `work/mob-targeting-bindings-sql-check.log`.
- Default synthetic oracle after refactor: byte-identical to the existing fixture;
  ignored `work/mob-targeting-bindings-refactored.tsv`.
- First `python3 tools/dev.py test`: exit 8, 124/125 passed (42.24 s), with
  login-network-unit reporting "Malformed frame left an authenticated session
  behind". All targeting tests passed. This failed run is NOT counted as a pass.
- Clean full rerun: exit 0, all 125 passed (39.55 s);
  `work/mob-targeting-bindings-sql-native-retry.log`. Initial log remains
  `work/mob-targeting-bindings-sql-native-tests.log`.
- Read-only source inspection identifies a plausible independent timing window:
  src/net/login_server.cpp close() shuts down/closes the socket BEFORE
  sessions->release_login; tests/login_socket.hpp closed() returns immediately
  on EOF/reset, then login_network_unit.cpp line 75 checks session_count.
  This is an inference from ordering, not a proven root-cause trace. Coordinator
  should review session-release/closure ordering or synchronize that assertion.
  No files in that subsystem were edited and no third retry was used.
- All twelve EXE/PDB SHA256SUMS pass; git diff --check clean. No SQL/credential
  snapshots or runtime files tracked.

The minimal canonical owner/lifetime and positional patch contract in milestone
6 is unchanged. Actual startup publication, stable lifetime through callbacks,
in-place or quiescent reload and production resolver/world wiring still belong
to integration. Passing this read-only data comparison is not a running world
or original-client playability.

## Binding assignment completion / integration dependency checkpoint

Reviewed current main CONTINUITY.md/PARALLEL_WORK.md and latest delivery
96af8bc/9a0bdd4. The repeated expanded assignment has already been fulfilled:
owned binding decoder, complete original loader/Init comparisons, positional SQL
and lifetime contract, and restored SQL distribution comparison are committed.
Main has reviewed the initial decoder milestone; the SQL comparison commits
must also be integrated/revalidated before enabling a production resolver.

Read-only inspection still finds no targeting-binding owner/resolver or shared
cell visitor declarations in canonical mobs.hpp, skills.hpp and zone_mobs.hpp.
The coordinator is now prioritizing canonical owner interfaces, and a new central
source validation freeze is active. No additional independent change is needed
for the requested startup binding decoder. Repeating its completed fixtures or
adding another data/world owner would not connect production behavior.

Concrete next task: coordinator integrates 96af8bc/9a0bdd4 after the freeze,
retains MobTargetingBindingCatalog in the persistent canonical game-data owner,
and supplies the required resolver/visitor interfaces. Then assign this worker
production targeting composition against those interfaces. Alternatively assign
explicit live ReLoadConstMobSkill 004c50c0 / ReLoadConstMob 004c20c0 recovery if
stable in-place reload is the next desired binding behavior; it is not claimed
by the startup decoder and needs its own failure/publication evidence.

Documentation-only checkpoint: git diff --check clean; native and SQL checks
were not repeated with unchanged code/data. Previous exact validation and the
initial login-network failure remain recorded in milestone 7. No main changes,
SQL lifecycle actions, merges or pushes. Waiting for a concrete owner-interface
resolution or a new scoped assignment; server playability remains incomplete.

## Milestone 8: authorized live reload recovery

New exclusive prefix: mob_targeting_reload. Recovered COMPLETE original
`CGameData::ReLoadConstMob` 004c20c0 and `ReLoadConstMobSkill` 004c50c0 in
ps_game.exe, including real GetMobSkillInfo 004027e0. Focused Ghidra --process
export succeeded under the shared lock in work/exports/08-mob-targeting.
No shared headers, previous binding implementation or world owners changed.

### Publication and failure behavior

Added a native reload coordinator over mutable owner-provided record views.
Profile failure returns without logging/connecting. Connect failure logs only.
Allocation failure after connect logs WITHOUT FreeDB/Disconnect/Release (the
original lifecycle, not an added RAII cleanup). Query or GetData failure logs
BEFORE FreeDB -> Disconnect -> Release. Successful fetching performs that same
cleanup BEFORE checking row count and publishing. Zero/multiple rows log and
fail after cleanup. A complete first row followed by a failed second row does
not publish the first row. Only SQL_SUCCESS=0 is accepted for Query/GetData;
SQL_SUCCESS_WITH_INFO must not be accepted by a SQL_SUCCEEDED-style adapter.
Fetch statuses other than SQL_NO_DATA=100 enter GetData, including errors.

The staging record is zeroed ONCE before fetching, not per row. NULL leaves its
destination unchanged; multiple-row decoding can retain previous staged data
although the final count gate rejects publication. Normalization matches the
startup loader, including raw byOK, skill-effect acceptance, normal range clamp,
country/chase fields and timer/attribute derivations. Exactly one successful row
publishes by RETURNED ROW ID, not the requested R2 ID. Fixtures request ID 7 but
return row ID 1, checking the actual query text and destination separately.

Publication is an ordered field patch in the original 268-byte mob / 160-byte
skill layouts, not memcpy or object replacement. Unchanged fields do not increment
the caller's DWORD change counter; changed fields increment it with wraparound.
ID and name are never copied. Padding is retained; mob loot tail is untouched.
Skill level +2 is unconditionally set to 100, WITHOUT a counter increment even
when it changes. Skill bItemLink +156 is patched from its zeroed staging DWORD.
Mob's repeated byType comparison produces no second write/increment after the
first. Source evidence: mob publication 004c2a1b onward, skill publication
004c5c26 onward; row gates 004c2250 / 004c5bb3. The instrumentation trace reserves
its storage before the first field write, preventing trace allocation from
introducing an allocation failure midway through in-place publication.

Own SQL failures occur before publication: this is NOT the startup loader's
per-row partial catalog publication. Nevertheless there is no rollback of changes
made by required pool/log callbacks to live owners. Fixtures mutate an owner byte
and the change counter during Release; subsequent comparisons/increments read
the new values, and a failed zero-row reload retains those external changes.
No extra callbacks are introduced between original field writes.

### Owner integration and positional contract

The module owns NO world or definition catalog. Required profile/parse/pool/SQL/
logging services expose the original lifecycle, and target(kind,row_id) returns
a stable mutable record from the EXISTING game-data owner after cleanup. Query
uses usp_Read_Mobs_R2 requestedID (69 columns) or usp_Read_MobSkills_R2 requestedID
(82 columns); no MaxID query or array reallocation occurs during reload. Exact
column destination/length/C-type triples are recorded in the fixture footer.
The mob quest scalar retains the original length=1/C_LONG binding convention;
its actual typed destination is four bytes immediately before the row buffer.
Other positions are the milestone 6 startup contract. Procedures and numeric
conversion belong to the actual SQL owner, not an empty provider.

Original pointers are opaque stable tokens in the wire-layout view; they must
not be truncated native addresses. A required resolver maps them to existing
MobTargetingBindingSkill objects. New projection adapters update the EXISTING
mutable MobTargetingBindingSkill / MobTargetingBindingDefinition structs, retaining
ID, object address, raw enabled byte and non-owning skill identity. A native alias
check verifies an already-published definition observes a changed cooldown via
the SAME skill pointer. Skill reload never rebinds/filter-clears existing mob
links merely because the skill's effect changed; only a subsequent mob reload
re-evaluates skill acceptance. No MobRuntime Init/reset is called by reload;
existing attack deadlines remain runtime-owned.

Minimal canonical additions: serialized mutable allocated-slot accessors to the
persistent data owner (distinct from public nonzero-ID/admission find helpers),
plus retained complete normalized record views for full-field change counting.
The startup catalog currently exposes const lookups only: do NOT const_cast a
possibly genuinely const published owner. Supply mutable references through its
actual owner API and invoke the typed adapters under owner quiescence. The raw
views must reflect current canonical values, not zero-filled reconstructed
snapshots. Do not replace arrays/catalogs or relocate skill slots. The owner,
skills, definitions, caller counter and all published references must outlive
reload; retain them through pool cleanup and projection publication. Readers
must be paused/serialized across decode, cleanup and ordered patching: no
thread-safety or atomic whole-record publication is claimed by the original.
Returned row IDs must be within allocated storage; out-of-allocation writes and
the GetMobSkillInfo one-past boundary remain unsupported undefined inputs.

### Original execution / checks

New oracle runs 432 complete original reloads (120 successful, 312 failed),
recording all SQL column reads, cleanup/log order, final complete record bytes,
change counters and 7,492 field writes with exact offset/width/order. Coverage:
profile/connect/allocation/query/fetch/GetData failures, first/middle/last column,
zero/two rows, failure after a complete row, NULLs, unchanged reload, effect/type
validation, raw enabled bytes, derived fields, exact R2 query arguments, cleanup
mutations, counter wrap, padding/name/ID retention and forced skill level.
The native coordinator matches all traces and has no default providers.

OS profile/connection parser/pool/SQL/log/cookie are explicit oracle ports;
GetMobSkillInfo and both reload bodies execute real original instructions. No
original database connection, pool shutdown or actual SQL reload was performed.
The new tests use owned input projections and existing binding structs; they do
not claim production mutable accessors or world scheduling are integrated.
Startup/SQL distribution fixtures were not regenerated for this block.

Implementation commit: `87abafd` (workers/08-mob-targeting).

- Locked focused Ghidra --process export: exit 0;
  `work/mob-targeting-reload-ghidra.log`.
- `python3 tools/dev.py build`: final exit 0, no warnings;
  `work/mob-targeting-reload-final-build.log`.
- `python3 tools/dev.py test`: final exit 0; all 126 tests passed (45.28 s).
  New reload unit: pass, 0.07 s; `work/mob-targeting-reload-final-tests.log`.
- Focused ASAN/UBSAN compile and 432-case comparison under the shared build lock:
  exit 0, no sanitizer diagnostics; `work/mob-targeting-reload-sanitized.log`.
- Original reload fixture regenerated identically under
  `work/mob-targeting-reload-repeat.tsv`; byte comparison exit 0.
- All twelve original EXE/PDB SHA256SUMS pass; git diff --check clean.
  Generated exports, logs and executable remain ignored; no credentials tracked.

Remaining production dependency: canonical mutable stable allocated-slot access,
full normalized record storage/counter synchronization and actual R2 SQL/pool/log
providers. Apply typed projection adapters only to the actually published owner
record after success; do not publish a failed staged row. Earlier world visitor,
actor lifetime and FSM wiring remain required. No actual SQL lifecycle operation
or repeat of the completed restored startup SQL check was performed. The native
reload coordinator and tests do not imply a playable server.

## Reload assignment completion / canonical access dependency

Reviewed current main continuity/coordination notes and latest commits
87abafd/a99fd79. Both explicitly assigned reload routines, failure/publication
ordering, raw enabled/skill-pointer semantics, original trace comparisons and
SQL/quiescence contract are delivered in milestone 8. The repeated assignment
adds no new independent behavior to implement without repeating that work.

Read-only canonical inspection confirms MobTargetingBindingCatalog still exposes
only const mob()/skill() access, with no stable mutable allocated-slot API or
complete raw reload record storage. Main is under the fifth-batch validation
freeze. This worker cannot connect live publication by editing those shared
headers, const-casting published storage or building another catalog owner.

Concrete next task: coordinator integrates milestone 8 and adds owner-controlled
mutable slot access/full normalized record views under quiescence, then assigns
the production R2 reload adapter against that interface. Alternatively explicitly
expand scope to the full stable mob-skill definition projection needed by worker
03 (name/keep/ability/time-damage fields), using the recovered 82-column loader
and the SAME persistent owner rather than another user-skill/world store.

Documentation-only checkpoint: git diff --check clean; no native, sanitizer or
SQL reruns with unchanged implementation. Milestone 8's 126/126 tests, 432
original traces and clean sanitizer run remain the last substantive results.
No main/other-worker edits, SQL lifecycle changes, merges or pushes. Waiting for
a concrete owner interface or new assignment; global gameplay remains incomplete.

## Milestone 9: full stable mob-skill catalog and owner reload access

Explicit coordinator scope extends the EXISTING mob_targeting_bindings owner.
No second skill map, copied world owner, shared mobs.hpp/skills.hpp edit or
worker03 source change was made. Evidence remains ps_game.exe
LoadConstMobSkill 004c0be0 (normalization/copy 004c1606..004c1695), LoadConstMob
004c1720, original stSkillInfo layout, and the recovered R2 field publisher.

Each existing skill slot now owns its complete normalized 160-byte record and
an EMBEDDED SkillDefinition-compatible object. Each mob slot retains its complete
normalized 268-byte post-LoadConstMob record, with pointer fields encoded as
stable allocated-slot tokens. These are representations in the SAME owner,
not another registry. All numeric fields, zeroed padding, names and normalization
are retained, not reconstructed later from the former small projection. Mob
quest scratch retains its last non-NULL value across rows, matching the separate
original GetData destination outside the freshly cleared row buffer. Mob item
links loaded by later services are not claimed as reconstructed by this loader.

| MobSkills_R column | Original offset | Stable data |
| --- | --- | --- |
| 3 | +0x03, char[32] | name octets; GetData length 30, maximum 29 bytes plus NUL; +0x22 forced zero |
| 48 | +0x5c | keep seconds; original two-byte GetData zero-extends into DWORD |
| 65/66 | +0x7c/+0x7e | ability 1 byte type / uint16 value |
| 67/68 | +0x80/+0x82 | ability 2 byte type / uint16 value |
| 69/70 | +0x84/+0x86 | ability 3 byte type / uint16 value |
| 58 | +0x6e | time-damage type |
| 59/60/61 | +0x70/+0x72/+0x74 | three uint16 time-damage words |
| 74/75/76 | +0x8e/+0x90/+0x92 | three TimeHeal words, kept SEPARATE from time damage |

The embedded view also projects all existing common SkillDefinition fields:
id/detail/effect, raw forced level 100, weapons/weapon value/bag, attribute,
defense type/value, apply range, limit HP, aggro, change type/level, abilities,
keep seconds, death-preservation flag and TimeHeal. It deliberately does NOT use
the user SkillDefinitions loader: no 1..3 level admission, detail-93/122 effect
remapping, zero-ability-value promotion, or keep-time hours conversion. The mob
loader leaves zero ability values intact and keeps seconds directly even when
the raw death-preservation flag is 1; offline_countdown is false. Original raw
bytes remain available for fields not represented by the shared header.

### Exact worker03 adapter / lifetime contract

- Acquire `catalog.skill_definition(slot_id)` and retain that const pointer in
  MobAppliedSkill::definition. It points INSIDE the original persistent slot;
  do not copy a SkillDefinition per node or keep a pointer to a local adapter.
  Forced level 100 is intentional, not a user-skill level to clamp/filter.
- Given a retained `const SkillDefinition&`, use
  `catalog.from_definition(&definition)` for identity-checked reverse access.
  This scans existing slots and adds no map. An unrelated user definition with
  the same ID/value is rejected; user-origin skills need their own genuine owner
  adapter, not a mob-ID fallback or empty provider.
- `slot->name()` is a bounded string_view INTO the same raw slot bytes; no
  temporary string backing. `definition.keep_time_seconds` and
  `definition.abilities` are the actual stable common view.
- Project worker03 MobAppliedSkillTimeDamageDefinition from
  `slot->time_damage_type` and `slot->time_damage` at the original read point.
  Those members have stable per-slot addresses and update live. Do NOT use
  definition.time_heal as periodic damage. No worker03 header is copied here.

The catalog/arrays must outlive templates, active/recycled applied skills and
all callbacks. Moves transfer existing storage and preserve raw/view/member
addresses (tested); replacing/destroying a published owner is still forbidden.
There are no per-node snapshots to mask later definition changes. Retained
nodes can observe changed effects/abilities/keep/damage after a live reload,
matching original pointer semantics.

### Owner-controlled mutable record access / existing reload integration

`mutable_skill_record(slot)` / `mutable_mob_record(slot)` expose bounded spans
for ALLOCATED slots, including zero/empty slots that public admission lookups
reject. No resize or insertion occurs. Existing reload services.target returns
one of these actual owner buffers for the RETURNED row ID. After a successful
ordered reload, call `publish_skill_record(returned_slot)` or
`publish_mob_record(returned_slot)` under the SAME quiescence boundary. They
synchronize existing legacy fields and embedded compatible/periodic views in
place; addresses and borrowed skill links do not move. Do not use the older
minimal reload projection alone on this expanded owner: it cannot refresh the
new complete views. Never publish a failed staging row.

Hold owner serialization across decode, pool cleanup, raw field publication and
view synchronization. Raw mutable spans are low-level access, not concurrently
readable mutation transactions. External callbacks that modify these records
must synchronize/publish their own changes before exposing views. Skill reload
does not re-evaluate mob links; a later actual mob reload can clear a now-invalid
binding. Skill ID/name/padding and forced raw level behavior continue to come
from the recovered ordered publisher, not whole-record replacement.

Opaque link tokens are allocated slot indices, not mutable stored skill-ID
metadata or truncated native pointers. A targeted native check changes stored
ID while preserving the slot and confirms decoding still binds the indexed
pointer, consistent with GetMobSkillInfo's nonzero-ID guard. The original
one-past lookup remains safely unsupported. Complete normalized records now
satisfy the old reload counter/storage dependency without const_cast.

### New original and restored-data comparisons

`--full-skills` adds varied names (empty, spaces, 29 bytes, UTF-8 octets and
embedded NUL), all new fields, raw level extremes, zero/large ability values,
wide periodic words, keep/death flags and NULL scratch retention. 256 COMPLETE
original paired loaders compare 3,584 full allocated records, including holes
and slot zero. The original GetMobSkillInfo and Init still execute; name/ODBC
conversion is an explicit SQL_C_CHAR input port, not a Microsoft driver/codepage
claim. Names are raw octet views rather than promised Unicode strings.

The read-only restored SQL runner now hex-encodes actual name octets instead
of replacing names. It compares ALL 356 skill records and ALL 2,747 mob records
(793,156 raw bytes) plus the existing 3,101 selected projections. All match:
355 skill rows, 2,747 mob rows, 2,746 nonzero definitions, 8,238 attack positions,
1,871 links including 20 present-but-disabled. Maximum restored skill name is
21 bytes, below the original name buffer's truncation boundary. Snapshots remain
ignored; no actual game data or credentials are committed.

New owner composition runs the EXISTING native R2 reload over these persistent
records, then synchronizes views. It checks old applied-definition/raw/name
addresses, live keep/ability/periodic updates, ignored replacement name, retained
mob link after unsupported effect change, actual subsequent mob revalidation,
reverse identity guards, allocated empty slots and pointer-preserving move.
Shared worker03 code was inspected read-only; its actual scheduler/effect/world
providers are not claimed as integrated by this data-owner test.

Implementation commit: `36eab1d` (workers/08-mob-targeting).

- `python3 tools/dev.py build`: final exit 0, no warnings;
  `work/mob-targeting-bindings-full-final-build.log`.
- `python3 tools/dev.py test`: final exit 0, all 127 tests passed (45.00 s).
  New full owner suite: 0.34 s;
  `work/mob-targeting-bindings-full-final-tests.log`.
- Focused ASAN/UBSAN compile and full original/owner/reload suite under the
  shared build lock: exit 0, no diagnostics;
  `work/mob-targeting-bindings-full-final-sanitized.log`.
- `python3 tools/mob_targeting_bindings_sql_oracle.py`: final exit 0;
  `work/mob-targeting-bindings-full-final-sql.log`. Full normalized records and
  names are included, not just the old targeting subset.
- `--full-skills` original fixture regenerated identically into ignored
  `work/mob-targeting-bindings-full-repeat.tsv`; comparison exit 0.
- All twelve original EXE/PDB SHA256SUMS passed; git diff --check clean.
  No restored SQL snapshots, logs, credentials or generated executable tracked.

Remaining integration is actual root data-owner construction/lifetime and
worker03's identity-based definition/name/periodic adapters, plus real SQL R2/
pool/log and world/effect/scheduler providers. The mutable stable record/view
access dependency itself is now implemented in the existing owner; no shared
header patch or const_cast is needed for that interface. Do not continue using
an out-of-date minimal projection adapter for the full catalog. No production
world services or playable-server completion are claimed.

## Full catalog assignment completion / adapter dependency

Reviewed current main continuity/coordination notes and commits
36eab1d/849a0fc. The repeated expanded assignment is complete: full stable
per-slot compatible views, names/keep/abilities/periodic words, mutable allocated
record access, normalized complete retention, live reload alias composition,
original full-load comparison and restored read-only SQL comparison are all
delivered in milestone 9. No additional decoder or second map is needed.

Read-only canonical inspection still finds the earlier small binding catalog,
without compatible/from_definition/mutable_skill_record/publish_skill_record.
Main is in seventh-batch source validation freeze; this worker has not changed
main or other workers. The exact remaining worker03 adapter contract is already
specified above and cannot be wired by editing worker03's owned sources here.

Concrete next task: coordinator integrates the exact milestone 9 snapshot after
validation, then assigns worker03 to retain catalog.skill_definition(slot) and
resolve its name/periodic data through from_definition, preserving the SAME
catalog lifetime. For this worker, authorize the production root catalog loader/
resolver composition once that owner interface exists, or explicitly assign a
new independent subsystem. Do not use an ID-only user-skill fallback or copy
compatible definitions per applied node while this handoff waits.

Documentation-only checkpoint: git diff --check clean. No native/sanitizer/SQL
reruns with unchanged code; milestone 9 records the last 127-test/full-record/
restored-SQL results. No main edits, SQL lifecycle writes, merges or pushes.
Waiting for integration/interface resolution or a new scoped task; the server
is still not playable.

## Milestone 10: direct shared-cursor visitor and live targeting assembly

Implementation commit: `6debf69` (workers/08-mob-targeting). Explicit expanded
ownership covered zone_mobs.hpp/cpp and corresponding tests on this branch;
main, Actor, zone-user ownership and worker03 files were not changed.

Evidence remains ps_game.exe `FindHelpNormal` 0049ff70, `FindHelpNormal2`
004a0200, `FindHelpAttack` 004a0470, `SSyncList<CMob>::Front` 0041f190,
`Next` 0041f1c0, `Remove` 00424ac0, `AddBack` 00424a80 and `CCell::FindMob`
00423680. Original helpers retain the cell critical section across SetTarget
callbacks and advance the SAME list cursor after callbacks. Reused the complete
milestone 4 original execution fixture, including real original list mutations;
its SetTarget mutation driver remains an explicit port, not original AI execution.

`ZoneMobs::visit_cell(CellCoordinates, const function<void(MobRuntime&)>&)` now
exposes that existing cursor directly. It synchronously visits current members,
then advances using the retained cell only. No actor dereference follows the
callback, no next iterator is saved, and no cursor is restored. Removing current
rewinds to predecessor, append is visible, nested successful find changes the
remaining traversal, and nested miss/complete visit leaves the sentinel so the
outer traversal restarts at head. Empty cells remain owned by ZoneMobs.
Existing appearance/removal sends now use this SAME visitor. Existing all/in_cell
snapshot APIs remain available but targeting does not use them.

The 450-case cursor unit now invokes the direct production visitor; the packet
probe is removed. Events/final membership remain identical to the original.
Represented L/U events still describe owner exclusion boundaries; they do not
claim new native OS mutexes or prove concurrent locking correctness.

New `MobTargetingZone` borrows the actual ZoneMobs and full persistent binding
catalog. Help projects current runtime FSM status/type/position and catalog AI;
source country/radius come from the catalog, bounds from existing MapGeometry.
Each acquisition calls the actual MobRuntime SetTarget bridge with initialize
false, and refreshes live source position/type afterwards. Root SetTarget uses
catalog attack links/enabled bytes/cooldowns and routes AI help recursively back
to this coordinator. There is no duplicate mob/skill map, packet traversal,
temporary SkillDefinition, snapshot cursor or replacement world owner.

The new assembly unit verifies real FSM dispatch observing the published target,
current removal and append from dispatch, separate retained subject lifetime,
source type/position and user ID changes, subsequent acquisitions, root help and
actual skill cooldown clocks, live published catalog country/radius/AI, user
visibility/count gates and detached-source rejection. Direct visitor destruction
of a removed actor and nested direct traversal also execute under ASAN/UBSAN.

Production assembly contract:

- Retain ONE existing full MobTargetingBindingCatalog loaded skills-before-mobs
  using milestone 9 positional SQL contracts. Borrow it in each zone adapter;
  canonical MobRuntimeTemplate pointers remain owned by the existing template
  owner and supply the stable definition ID, not a copy of live catalog AI.
- Construct MobTargetingZone(existing_zone, catalog, actual_world). Provide a
  retained MobTargetingZoneUser adapter over the real user owner. Its target()
  supplies live ID/life and help() supplies country/life/HP ability/admin
  attackable/visible. Each acquisition reprojects that SAME retained user.
- Required pure virtual MobTargetingZoneWorld ports are actual zone user_count,
  original shared random provider, authoritative tick_ms, and actual FSM
  DispatchEvent(subject,event,argument). No port defaults to success/no-op.
  Actor/user-zone/AI owners must implement these; they were not copied here.
- The existing zone owner serializes the COMPLETE call and all reentrant
  membership/find/visit operations on its execution context. If implemented
  using locks, they must allow synchronous original-style reentry. ZoneMobs
  adds no mutex and does not become thread safe. Keep the zone/cell storage
  alive; release_to_pool/destruction cannot run during traversal. Source stays
  with this owning zone for the help call; cross-zone source migration is not
  implemented by this adapter.
- A generic visitor can remove/destroy its current actor before returning.
  SetTarget has a stronger retained-subject requirement: runtime dispatch/help
  may unlink the subject, but its owner must defer recycling/destruction until
  that SetTarget call returns because the bridge republishes live state.
  Retain source/user separately through the complete help call. Callbacks and
  visitors are synchronous and must not escape.
- Serialize catalog raw reload/publication against these calls under existing
  owner quiescence. Retain the same allocated slots/embedded views and catalog
  lifetime; do not replace arrays while runtime/applied-skill pointers exist.
  This block changes no positional SQL loader or reload publication contract.

Validation:

- `python3 tools/dev.py build`: exit 0, no warnings/errors;
  `work/mob-targeting-zone-build.log`.
- `python3 tools/dev.py test`: exit 0, 128/128 passed, 46.93 seconds;
  `work/mob-targeting-zone-tests.log`. Existing zone-mobs-unit passed and all
  450 original cursor traces compare through the new direct visitor.
- Full original cursor fixture regenerated with `.local/oracle-env/bin/python
  tools/mob_targeting_cursor_oracle.py` into ignored work/; cmp exit 0.
- Focused ASAN/UBSAN: real zone/target/catalog assembly and all 450 direct
  visitor traces passed, exit 0 with no diagnostics. Changed visitor/adapter
  and targeting/help/runtime/catalog sources were instrumented under the shared
  build lock; `work/mob-targeting-zone-sanitized.log`.
- All twelve original EXE/PDB SHA256SUMS passed; git diff --check clean.
  No binaries, SQL lifecycle/state, credentials, main or other workers changed.
  SQL loaders were unchanged; their full original/restored read-only SQL checks
  are preserved in milestone 9 rather than repeated without a data-path change.

Remaining integration is coordinator construction of these borrowed adapters
and actual user-count/user-lifetime/random/clock/FSM ports under the real zone
execution owner. The required shared canonical visitor change is now delivered;
no additional Actor/user-zone/shared skill header patch is proposed here.
No production network/world handler assembly or playable-server completion is
claimed by the unit tests.

## Direct visitor assignment completion / production-owner dependency

Reviewed the current main continuity/coordination notes and delivery
6debf69/0017bb5. The repeated mob_targeting_zone assignment is complete in
milestone 10: direct shared cursor, actual help/runtime/full catalog composition,
450 original mutation comparisons, lifetime checks and production assembly
contract. Do not duplicate the visitor, restore a packet probe or add another
mob/skill owner.

Read-only main inspection still finds no ZoneMobs::visit_cell or root
MobTargetingZone assembly. The eighth-batch native/SQL validation source freeze
is active; this delivery remains for coordinator integration after that freeze.
No independent missing implementation within this completed assignment was
identified. Actual user retention/projections, zone user_count and live FSM
DispatchEvent adapters require owners outside this task's authorized files.

Concrete next task: integrate 6debf69/0017bb5 after central validation, then
explicitly assign the root targeting-world/user adapter against the retained
Actor/zone-user owner and actual compiled-AI dispatcher. Reuse the existing
MobTargetingZoneWorld/MobTargetingZoneUser interfaces and single full catalog;
require real random/clock/user-count/FSM providers and owner serialization.
Until that scope is assigned, keep the production ports required rather than
implementing empty providers or taking Actor/zone-user/worker03 ownership.

Documentation-only checkpoint: git diff --check clean; no unchanged native,
sanitizer or SQL reruns. Milestone 10 remains the latest validation: 128/128 tests,
450 original direct-visitor traces and focused ASAN/UBSAN passed. No main edits,
SQL lifecycle writes, merges or pushes. Overall playability remains unverified.
