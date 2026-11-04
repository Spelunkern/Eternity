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

## Milestone 11: canonical Actor and real grid/CRT/clock/FSM adapters

Implementation commit: `9d9111f` (workers/08-mob-targeting). New files use the
assigned mob_targeting_actor prefix. Actor, ZoneUsers, worker09 FSM sources,
worker03, main and previous targeting milestones were not edited.

`MobTargetingActorUser` now borrows the retained canonical Actor and projects:

- target ID from Actor::id(), original CUser+dc;
- country from appearance().identity.country (CUser+12d, populated by the
  existing ActorLoadPipeline user_shape_identity call);
- attackable/visible from ActorAppearance, original admin bytes +5775/+5774;
- selection immunity from status().no_mob_targeting, original CUser+1366.
  The older help projection field is named hp_ability, but this byte is the
  effect-89 selection immunity, NOT SkillStatus::hp_ability_applied or its ID.
  Existing AddApplySkillBuff 00484cf0 / RemApplySkillBuff 00485130 and the
  helper original traces establish that distinction.

Every call reads the SAME live Actor; there is no second user record or cached
flags. Canonical Actor currently has NO authoritative CUser+142c life state or
accessor. The adapter therefore requires MobTargetingActorLife::life_status(
const Actor&) const, preserving the signed value and exact dead==1 test. It has
no default implementation. Zero current HP is deliberately not converted to
life==1. Supplying an invented alive constant is not production integration.

`MobTargetingActorWorld` borrows the existing paired ZoneMobs/ZoneUsers, shared
per-executor MicrosoftRandom and actual MobAiStateServices. user_count delegates
to ZoneUsers::size(), rejects a foreign mob grid, and checks signed count bounds.
No new ZoneUsers accessor is needed. random advances the borrowed original CRT
stream; tick_ms uses the SAME FSM services clock. dispatch delegates directly to
existing dispatch_mob_ai_event with the complete event/argument bit patterns.
It does not implement a second FSM table or substitute a successful callback.

Evidence: ps_game.exe SetTarget 0049fad0 / SetTargetEx 004b65e0,
StackStateMachine<CMob,-1>::DispatchEvent 004925f0 / 004929b0,
STATE::OnDispatch 004938c0, InitLuaFSM 004b4c20 and CMob::OnAttacked 004b6120.
New original fixture executes the real SetTarget-to-dispatch-to-event chain:
160 combinations of five fixed states, move modes, signed life values -1/0/1/2,
vision fixation and Lua enabled/disabled, using high-bit target IDs. It compares
raw status, target, timeout, move deadline, attack times, event discriminator,
final stack and the Lua callback's tick/argument/PRE-transition stack. Only the
existing worker09 container/Lua/clock/diagnostic harness boundaries are ports;
SetTarget and DispatchEvent are not replaced with recording stubs.

`tools/mob_targeting_actor_oracle.py` reuses that canonical instruction harness
rather than copying its owner. On this older branch, regeneration command is:
`.local/oracle-env/bin/python tools/mob_targeting_actor_oracle.py --fsm-oracle
/home/franco/Projects/Eternity/tools/mob_ai_states_oracle.py`. After canonical
integration the default same-directory harness path works. Harness SHA256:
42d79fc4612780ae4e3a27797df7b87ad1aea2547b9a18409d5dd880bf23611c.

Native assembly also executes help -> actual SetTarget -> actual fixed FSM ->
required Lua-service callback -> existing configured FSM execute/evaluate.
Its configured callback changes the retained user's life/visibility before the
next candidate. The second acquisition sees live life==1 and is rejected; fixed
stack publication occurs after the callback and the configured owner transitions
normally. These test actions are explicit synthetic world/Lua actions, not a
claim to execute original Lua scripts. Additional checks mutate real country,
admin flags, and effect-89 immunity via the existing effect apply/remove paths;
verify HP-ability trigger independence; detach the actual ZoneUsers member; and
verify a shared CRT stream/authoritative clock feeding a live catalog cooldown.
There are no production success-only providers.

This branch predates canonical worker09 FSM headers/targets. Local validation
used an ignored, byte-for-byte read-only snapshot of mob_events, mob_ai_states,
mob_ai_states_configured and the parameter header from main snapshot
9e35b9c3dfd7f65ec319707fae33c14df3c1326e. CMake conditionally uses that snapshot
ONLY when the canonical FSM header is absent; otherwise it uses the normal
canonical owner/targets. No snapshot source/header is tracked or installed.
Snapshot hashes are in work/mob-targeting-actor-deps.sha256 and were compared
with the source files after validation setup. Central integration must retain
the existing worker09 sources, not import another copy from work/.

Exact production assembly / minimal owner contract:

- Retain the actual loaded Actor and its authoritative life owner throughout
  user projection, complete help calls and reentrant dispatch. The actor/user
  executor and both grids must share the existing serialized world context.
  These adapters do not acquire locks or create actor retention machinery.
- Needed canonical Actor interface: `std::int32_t life_status() const noexcept`
  over the authoritative signed CUser+142c state. Its owner must initialize and
  update that SAME state on actual death/rebirth transitions. If mutation is
  exposed, request `std::int32_t& life_status() noexcept` from that owner too;
  an accessor backed by a new unconnected default value does not close the gap.
  Once available, implement MobTargetingActorLife by direct delegation. Until
  then the explicitly required life-owner port remains; no Actor header patch
  or parallel state map was introduced here.
- Pair world(existing_mob_grid, existing_user_grid, shared_executor_crt,
  actual_fsm_services), then reuse MobTargetingZone(existing_mob_grid,
  retained_full_catalog, world). Wrap the retained Actor with
  MobTargetingActorUser(actor, authoritative_life_owner). Preserve milestone 10
  source/cell/catalog lifetime, deferred SetTarget subject recycling and reload
  quiescence. Caller supplies the actual corresponding user grid; current APIs
  do not expose a global zone identity to derive that pairing automatically.
- MobAiStateServices must be the real retained services: tick, Lua execution,
  error handling and update handlers. DispatchEvent's fixed stack is now wired
  concretely. Configured AI remains its existing separate owner reached by
  registered programs/compiled/Lua handlers; there is no evidence for mapping
  event 1 directly to an arbitrary configured state, so none is invented.
  Actual root Lua/compiled-handler composition and TravelCondition services
  remain required wherever the configured program uses them.
- Bind all targeting/random consumers on this executor to the SAME CRT owner;
  retain the existing full catalog and MobRuntimeTemplate owner. No new list,
  actor registry, catalog, random seed or platform clock is allocated here.

Validation results:

- `python3 tools/dev.py build`: exit 0; 380 actions. Nineteen existing warnings
  in unrelated guild_{bank,bank_save,catalog,npc_save,npcs,sql} sources; no new
  adapter warnings. Log: work/mob-targeting-actor-build.log.
- `python3 tools/dev.py test`: exit 0, 129/129 passed, 43.67 seconds;
  work/mob-targeting-actor-tests.log. New actor/FSM suite passed.
- New 160-case original fixture regenerated twice into ignored work/; byte cmp
  against the tracked fixture passed. Old direct-visitor fixtures were not
  regenerated or expanded for this assignment.
- Focused ASAN/UBSAN compile/run under the shared build lock: exit 0, no
  diagnostics; all 160 original paths and Actor/help/fixed/configured/grid/CRT/
  clock checks passed. Instrumented new adapters, target/runtime/help and actual
  fixed/configured FSM/event sources; work/mob-targeting-actor-sanitized.log.
- All twelve original EXE/PDB SHA256SUMS passed; git diff --check clean. No SQL
  data path changed; prior full positional/original/restored SQL validation is
  retained rather than repeating unchanged SQL checks. SQL lifecycle remains
  coordinator-owned.

Remaining dependencies are the authoritative Actor life accessor/owner, actual
root construction/retention/serialization and FSM Lua/clock/error/update/world
services. Country, immunity, admin flags, count, shared CRT, clock delegation
and actual fixed dispatch mappings are implemented. No root network handler or
original-client playability is claimed by these tests.

## Actor adapter assignment completion / life-owner dependency

Reviewed current main continuity/coordination notes and 9d9111f/598067c.
The repeated mob_targeting_actor assignment is complete in milestone 11:
canonical Actor country/admin/immunity projections, real ZoneUsers count,
shared CRT/FSM clock, actual fixed dispatch and configured callback composition,
160 new original execution paths, native tests and sanitizer validation.
No additional independent missing mapping was identified inside this scope.

Read-only inspection still finds no authoritative life_status accessor on
canonical Actor. Main is now in the ninth-batch source freeze; the actor adapter
is not registered there yet. This does not authorize edits to Actor, ZoneUsers,
the shared root, or worker03's newly exclusive mobs/mob_pool files. Preserve the
existing required life-owner port; do not substitute HP, a default alive value,
a parallel actor state table or a second FSM/catalog owner.

Concrete next task: coordinator integrates 9d9111f/598067c after validation and
assigns the Actor owner to expose authoritative signed life_status() over the
actual CUser+142c state with initialization/death/rebirth writes. Then explicitly
assign this worker the direct MobTargetingActorLife delegation and retained root
assembly against the actual user/zone/FSM services. The exact accessor and
remaining service/lifetime contracts are already specified in milestone 11.

Documentation-only checkpoint: git diff --check clean; unchanged build/tests,
original fixtures, sanitizer and SQL checks were not repeated. Last validation
remains 129/129 tests, 160 original SetTarget-to-real-FSM paths and focused
ASAN/UBSAN passed. No main edits, SQL writes, merges, resets or pushes. Global
server playability remains incomplete and is not inferred from these results.

## Milestone 12: reconcile +142c with the existing Actor shared state

Implementation commit: `5b9602c` (workers/08-mob-targeting).

CORRECTION: milestone 11's assertion that Actor lacked the authoritative life
state was incorrect. Its proposed new life_status accessor/field and the later
life-owner dependency checkpoints are superseded. Existing
Actor::attack_state().mode already represents original CUser+142c. No new
Actor field/accessor, life counter, parallel state map or owner is needed.
The host name "mode" does not imply a separate original life word.

Read-only evidence audit of ps_game.exe:

- Exported CUser PDB layout includes m_nStatusLife at decimal 5164 (0x142c).
  Other exported layouts have shifted offsets; assembly is authoritative.
  The separately named PDB m_nAttackMode must not be confused with this host
  ActorAttackState::mode projection.
- StatusAttackStop 00489ed0 loads +142c at 00489ed0, preserves raw 0 and 1,
  and stores zero there at 00489ee8 for other states. Canonical Actor::stop_attack
  performs those same operations on attack_.mode.
- UpdateAttack 0048ca50 reads source +142c (0048ca74), tests source state 3 at
  0048cbb6, and tests the NORMAL PVP target's SAME +142c for 1 at 0048cd05,
  0048cd78 and 0048cf38. Current canonical scheduler and NPC/hidden-user queries
  likewise read target.attack_state().mode==1.
- SetTarget 0049fad0 tests user +142c at 0049fbbd; FindHelpNormal 0049ff70
  tests it at 0049ffa7. These are the same shared life/action state, not HP.
- Death 004586a0 tests +142c==1 at 004586cd and writes 1 at 0045871e, with
  EDX loaded to 1 at 004586c8. Rebirth 00459ec0 admits only +142c==1 at
  00459ed8; XOR ECX,ECX at 0045a13b supplies its zero write at 0045a156.

`MobTargetingActorModeLife` concretely returns the existing signed mode without
normalization. `MobTargetingActorUser(actor)` now delegates through that stateless
projection; the previous explicit-provider constructor remains compatible.
Both target() and help() read the same live Actor state on every call, including
callback-driven changes. No extra mutable value, lock or ownership is introduced.
The existing retained Actor/world serialization contracts remain unchanged.

All actor assembly tests now use the concrete production constructor and mutate
the actual Actor mode, rather than a synthetic separate life owner. Expanded
original SetTarget-to-real-FSM comparison covers 400 paths: every original state
0..5 and unknown/extreme signed values (-2147483648, -1, 99, 2147483647), five
mob scenes, move modes, fixation and Lua gates. Identity projection preserves
all int32 bit patterns; the test sample is not an exhaustive enumeration of
2^32 values. Original target behavior still uses exact ==1, including existing
mob-side exceptions; this adapter introduces no additional rejection gate.

Another 138 new original execution sequences run the COMPLETE original
StatusAttackStop followed by SetTarget on the SAME user word, with before/after
target comparisons. Inputs include the raw boundary/known states and 128
reproducible random signed words. Native comparison calls actual
Actor::stop_attack, then observes both target/help projections and actual FSM
acquisition. Dead state 1 is retained by Stop; active/unknown modes normalize
to 0 exactly as the original. Zero HP is independently tested and not converted
into state 1. Existing configured/help callback tests now mutate the actual mode
and verify the next acquisition sees that state.

Native lifecycle audit: Actor::damage passes this same mode into resource damage;
shape delivery treats it as dead==1; stop_attack and stop_casting write it.
The inspected canonical Actor sources currently expose no complete user
Death/Rebirth implementation: clear_effects_on_death only cleans effects, and
resource damage returns an outcome. Therefore no synthetic native Death/Rebirth
transition is claimed. Their eventual world owners must update THIS existing
mode to original 1/0 at the recovered sites; a second life field would diverge.
This lifecycle assembly remains a separate root responsibility, not an accessor
blocker on targeting.

Validation:

- `python3 tools/dev.py build`: exit 0, 140 actions, no warnings/errors;
  work/mob-targeting-actor-mode-build.log.
- `python3 tools/dev.py test`: exit 0, 129/129 passed, 41.65 seconds;
  work/mob-targeting-actor-mode-tests.log. New comparisons include 400 target
  paths plus 138 complete original Stop/target sequences.
- Expanded original fixture regenerated into ignored work/ and repeated byte
  comparison passed; work/mob-targeting-actor-mode-{oracle,repeat}.tsv.
- Focused ASAN/UBSAN under the shared build lock: exit 0, no diagnostics;
  400 original target paths, 138 Stop/gate sequences and live Actor/help/
  fixed/configured/grid/CRT/clock composition passed. Log:
  work/mob-targeting-actor-mode-sanitized.log.
- All twelve original EXE/PDB SHA256SUMS passed; git diff --check clean.
  No Actor/zone-user/shared FSM headers, SQL data path, main or other workers
  changed. SQL lifecycle was untouched; unchanged SQL checks were not repeated.

Production assembly now uses MobTargetingActorUser(retained_actor) directly.
The former authoritative-life accessor/provider dependency is CLOSED. Existing
explicit life providers must delegate to the SAME mode if retained for legacy
call sites. Remaining work is actual root user/zone/catalog lifetime and
serialization, real FSM Lua/update/error/clock services and eventual full player
death/rebirth coordination. No new canonical patch is requested. Original-client
playability is not established by this owner correction or these test counts.

## Shared-state assignment completion / integration checkpoint

Reviewed current main continuity/coordination notes and 5b9602c/3645232. The
repeated +142c reconciliation assignment is complete: original PDB/disassembly,
Stop/UpdateAttack/PvP/target/Death/Rebirth offset evidence, concrete existing-mode
delegation, production constructor and original raw-state/Stop comparisons.
The earlier request for a new Actor life field/accessor remains WITHDRAWN.

Read-only main inspection still finds the previous explicit-life-provider-only
adapter, without MobTargetingActorModeLife or the single-Actor constructor.
Main is in tenth-batch validation freeze. Actor ownership is now explicitly
worker02; ZoneUsers remains worker01. No edits to those owners, main, or shared
runtime/pool files were made. There is no further independent missing life
mapping to implement within this completed assignment.

Concrete next task: coordinator integrates 5b9602c/3645232 after validation and
uses MobTargetingActorUser(retained_actor) at actual root targeting call sites.
Explicitly assign retained root user/zone/FSM service composition if this worker
should own the next block. Future player Death/Rebirth coordination must write
existing attack_state().mode to original 1/0, not add another life value; its
complete implementation is outside the present targeting adapter scope.

Documentation-only checkpoint: git diff --check clean. Unchanged native tests,
original fixtures, ASAN/UBSAN and SQL checks were not rerun. Latest validation
remains 129/129 tests, 400 original target paths and 138 original Stop/target
sequences, plus focused sanitizer success. No main changes, SQL lifecycle,
merges, resets or pushes. Original-client playability remains unverified.

## Milestone 13: complete Rebirth coordination and lifecycle owner audit

Implementation commit: `e80704a` (workers/08-mob-targeting).
This block closes COMPLETE CUser::Rebirth 00459ec0..0045a172 in ps_game.exe.
Death 004586a0 (2348 bytes) and DeathByNPC 00458fe0 were inspected, but their
reward/party/PvP coordination is NOT implemented or claimed complete here.
The next independent lifecycle block remains that Death coordination.

Recovered Rebirth behavior:

- Only initial shared +142c==1 admits the call. Optional restoration copies
  all three DWORD maximum resources +17c/+180/+184 directly to current
  +1238/+123c/+1240 BEFORE corpse lookup; no heal, clamp, party event or reset.
- FindDeadWorld 004176c0 -> LeaveDead 00417820. Ignore LeaveDead's return;
  never reuse/dereference that corpse after callback. Compare the requested
  signed map with the LIVE current WORD +164 after LeaveDead returns.
- Same map: MoveUser 00416260 receives current instance WORD +166 and requested
  XYZ. Only its true return produces cell-view 0551, 0304 game log, and
  SendRecoverChange 00482230, in that order. No zone skips the view only.
- Cross map: original signed map-count and computed pointer guard; then self
  0551, recover-change, ZoneLeaveUserMove 0040d0c0, self 020b with LIVE
  post-transfer map/position, then 0304 log. Transfer is void; no fabricated
  success result. The first 0551 contains pre-transfer position/experience.
- On ANY normally returning admitted path, including no corpse, missing map
  and MoveUser false: clear presentation byte +1434; clamp signed recovery
  +1274 to at least 1; publish shared +142c=0 LAST. A failed move is not a
  successful move, even though original life state still becomes zero.
- 0551 is 23 bytes: DWORD user, BYTE kind, DWORD experience (+13c), XYZ floats.
  020b is 20 bytes: DWORD user, WORD map, XYZ. Kind truncates to one byte.
  No additional target/attack/casting/effect cleanup occurs in this routine.

Owned core uses an explicit live input projection, not another user owner.
Action callbacks may mutate it/reenter; no busy flag, rollback or stale private
life copy suppresses original reentry. Actor bridge publishes to existing
attack_state.mode/recovery and reads actual resources/equipment maxima; it
reprojects after every action. Game log preparation uses actual existing
write_game_log_main 0045b0d0 and the required authoritative subject/calendar.

New ownership discrepancy: original Rebirth clears +1434 (0045a148), and
EnterZone/shape code currently treats ActorAttackState::motion as presentation;
worker01 scheduler separately maps that SAME host member to +1448 attack motion.
These are DISTINCT original bytes. This delivery does not collapse them:
required presentation_motion getter/setter ports describe +1434; the Actor test
keeps attack motion sentinel 0xa5 unchanged. Its +1434 value is explicitly a
fixture projection until the canonical owner separates/exposes it, not a second
production motion field. Actor/ZoneUsers headers remain read-only and untouched.

Precise remaining canonical owner APIs:

- Silent exact current-resource replacement, e.g.
  ActorResources::replace_raw(PlayerResources) noexcept writing current_, and
  an Actor forwarding accessor/method owned by its maintainer. No clamp,
  recovery packets, skill triggers, reset_damage or duplicate resource cache.
  Public heal/recover/finish_loading_resources cannot implement original raw
  restore. Required replace_resources port must publish exact actual values or
  fail explicitly; a no-op/clamping implementation is detected and rejected.
- Resolve presentation +1434 versus attack +1448 at Actor owner: expose the
  actual presentation byte, potentially reusing retained ZoneLifecycleState
  storage, and update Actor::enter_zone_lifecycle / send_shape_self presentation writers/
  readers consistently. Scheduler motion must remain the separate +1448 byte.
  Do NOT point this lifecycle setter at scheduler attack motion. Getter and
  silent setter must be authoritative; bridge publishes presentation before
  recovery/life, matching the original tail. No new +142c field/accessor needed.

Production root contract: retain Actor/PlayerCore, resource, corpse, actual zone
and admission owners through complete calls and synchronous reentry on their
existing serialized executor. Acquire a genuine session/Actor owner lease
through callbacks; never manufacture a non-owning shared_ptr lease. Subject
map/position/instance queries must remain available after source-grid detachment
(from retained PlayerCore/admission state), and are pure;
raw setters are silent, exact and non-reentrant. Other action ports may reenter.
Use real FindDeadWorld/LeaveDead and actual same-map MoveUser/cross-map world
admission transfer providers; this module creates no corpse list, user registry,
map catalog, world owner, clock or transport. +1434 and raw resource access remain
required ports rather than fake success. Missing setter test throws before
FindDead, retaining already-published state and without running the tail.

Required recover-change port is pure SendRecoverChange (0505 plus party energy),
NOT Actor::recover (which mutates/clamps resources and sends different events).
Self delivery delegates actual Actor::send_self; cell view/log sends require
actual root transports; log uses existing common
header writer. Rebirth has no direct SQL call: admission providers retain their
real DB/manager sequencing; no speculative DB write was introduced. map_present
must preserve the original signed count/computed-pointer guard. Original negative
map input can reach transfer: explicit projection cases retain that behavior;
actual providers must handle/reject unsupported retained-map addressing explicitly
rather than index native storage unsafely or silently invent admission success.

Original-execution fixture executes all 693 bytes of Rebirth, with only corpse,
move/transfer, transports, recover-change, SetGameLogMain and cookie ports. 1552
complete cases cover raw states, absent corpse, full/raw restoration, same/cross/
invalid/negative map requests, move failure, zone absence and mutations at all
action boundaries. Sixteen execute an ACTUAL nested original Rebirth from the
FindDead port; nested no-corpse path publishes life=0, yet the admitted outer call
continues its own corpse/movement/log path. Compare complete ordered packets,
pre-callback state snapshots and final resources/location/shared state.

Actual native composition uses existing Actor resource Heal from a world callback,
existing ZoneUsers movement, existing common game-log formatter and retained
EffectCycle nodes. Target/casting/attack deadlines/effects stay retained as the
original Rebirth requires. Missing exact restoration provider fails explicitly.
Native injected provider exceptions preserve partial publication and skip the
tail; this C++ contract is not claimed as verified Windows SEH equivalence.

Death audit for next block: dead==1 early guard; +145c penalty derived from raw
+58bc/+58b0; life=1, DWORD +1430=0 and ALL current resources zero before exchange/
shop/vehicle cleanup and CDamage::GetMax. Distinct max-user/max-mob and fallback
attacker paths lead to PvP/party/raid reward/drop/log handling. Common tail calls
PvPChkLeaveByDeath 0047b530, actual ClearApplySkillByDeath 00488000, CDamage::Reset
004b8aa0, EnterDead 004584d0, clears +581c, sets +1454=tick+200000 and +1458=0,
then SendDeath 00458470 and live party/raid notification under original lock.
No opaque successful death/reward callback substitutes for these missing branches.
Existing Actor::clear_effects_on_death and DamageLedger were inspected read-only;
actual death entry must compose them after reward resolution at the recovered
positions, not clear effects in Rebirth or add another life owner. Actor currently
exposes damage_ledger() const only: future Death needs a mutable/reset accessor
to that SAME ledger, without accidentally adding Scene reset/transport effects.
Existing WorldAdmission exposes entry/wait operations, not ZoneLeaveUserMove;
its actual leave/transfer coordination remains required behind the world port.

Validation:

- Native build: tools/dev.py build exit 0; verified source build 141 actions,
  then corrected test setup build 2 actions; no warnings/errors. Logs:
  work/mob-targeting-lifecycle-{verified,corrected}-build.log.
- Initial full run: 129/130 passed, 38.20 seconds; new suite reached the Actor
  composition after all 1552 original comparisons, then failed "Actor already
  belongs to a zone". The fixture incorrectly set in_zone before actual grid
  attachment. Removed that test-only write and let ZoneUsers own admission.
  Initial ASAN run reached the same fixture error, not a sanitizer diagnostic.
  Preserved work/mob-targeting-lifecycle-{tests,sanitized}.log; neither is counted
  as a passing run.
- Corrected tools/dev.py test: exit 0, 130/130 passed, 36.94 seconds;
  work/mob-targeting-lifecycle-corrected-tests.log. New lifecycle suite: 0.03 s.
- Focused ASAN/UBSAN compile/run under the shared build lock: exit 0, no
  diagnostics; all 1552 original cases, actual Actor/resource/grid/log/effect
  retention, missing-resource and native failure checks passed. Instrumented
  new lifecycle core/bridge/test and existing common game-log writer;
  work/mob-targeting-lifecycle-corrected-sanitized.log.
- Original fixture regenerated into ignored work/, repeated byte cmp exit 0;
  work/mob-targeting-lifecycle-{oracle,repeat}.tsv. No original binary changed.
- All twelve original EXE/PDB SHA256SUMS passed; git diff --check clean. No
  Actor/zone-user/other owner sources changed. No SQL lifecycle/data writes;
  unchanged SQL checks were not repeated, full central integration remains
  coordinator-owned.

No original-client playability or full player lifecycle completion is claimed.

## Milestone 14: complete DeathByNPC and excluded death notification

Implementation commit: `c2811ca` (workers/08-mob-targeting).
This block implements COMPLETE ps_game.exe CUser::DeathByNPC 00458fe0..004591ba
(477 bytes) and SendDeath 00458470..004584be (81 bytes). General CUser::Death
004586a0, its damage-winner/fallback and solo/party/raid/PvP reward distribution,
remain for a subsequent independent lifecycle block. No generic successful
"resolve death/rewards" callback hides that missing coordinator.

Recovered behavior:

- Dead==1 early return on existing +142c, before any world/metadata query.
  Penalty +145c starts 5, becomes 0 when raw +58bc is nonzero or +58b0==3,
  else becomes 1 for +58b0==1/2. Existing SetSkillAbility/skills oracle mappings
  identify those canonical fields as benefits.no_exp_loss and benefits.amulet;
  shifted PDB member names must not substitute another benefit.
- Publish penalty, life=1, DWORD soul +1430=0 before logging. This is distinct
  from Rebirth's byte +1434 and scheduler byte +1448. DeathByNPC does NOT zero
  HP/MP/SP and does NOT call exchange, shop or vehicle cancellation, unlike
  general Death. It does not insert StatusAttackStop/StatusCastingStop.
- Prepare common log first, opcode 0301, WORD NPC ID at 74, zero WORD at 76,
  NPC name at 78. Original GetNpc 0041d640 is a lookup boundary; real original
  CRT strncpy 00504c60 executes in the oracle, pads after NUL and limits copy to
  32, then byte109 is forced zero. Missing NPC writes ONLY byte78=0.
  Full source ID remains DWORD for SendDeath; log ID truncates to WORD.
- PvPChkLeaveByDeath 0047b530 -> ClearApplySkillByDeath 00488000 -> CDamage::Reset
  004b8aa0 -> EnterDead 004584d0. Clear pending move +581c BEFORE GetTickCount,
  then +1454=tick+200000 with DWORD wrap, +1458=0. Later callbacks' mutations
  remain live; life is not blindly republished as 1 at function exit.
- SendDeath executes original 0504/11-byte construction: subject DWORD, source
  type BYTE, source ID DWORD. Current zone absent suppresses this view only.
  CZone::SendViewM1 00421120 receives excluded subject ID, flag0/channel5;
  there is NO additional self send.
- Capture current party P, lock P, reread current membership. If null, unlock P
  immediately and clear captured ownership. Otherwise send to the LIVE current
  party Q, even if Q differs from captured P, and finally unlock P. Do not
  restore membership or acquire Q instead. An early unlock may itself restore
  membership, so the subsequent send condition rereads it.
- NPC variant ALWAYS uses 0c06/6 bytes, including raid membership. General
  Death has a distinct raid opcode branch; its future implementation must not
  blindly reuse this NPC-specific tail.
- The overload is CParty::Send 00445680, which EXCLUDES the original subject by
  CUser pointer identity. Required root send_party therefore receives Actor&
  excluded_subject, not merely an ID or ordinary broadcast. The existing normal
  sender 004455d0 is not an equivalent substitute. Real captured party/user
  owners must stay alive through recursive locks and callbacks.

The core owns only a live explicit field projection. Actor bridge borrows the
existing Actor/NpcDefinitions and required root services; no user, party, NPC,
ledger, resource, registry or world owner is copied/created. Concrete mappings
use actual Actor mode, skill benefits, pending move, resources, clear_effects_on_death,
Actor tick clock and the existing common game-log writer. NPC lookup uses the
actual immutable NpcDefinitions positional find and raw name prefix.

Log scratch provenance is explicit: original stack tails and common-header string
padding can remain unwritten. Core accepts a 110-byte caller seed and preserves
untouched bytes, including missing-name tails. Actual native Actor wrapper starts
scratch at zero, a deterministic portable policy for otherwise unspecified stack
contents; it does not claim to reproduce arbitrary leaked original stack bytes.
Original comparisons seed memory nonzero to verify partial writes rather than
mistaking every untouched byte for an original zero write.

Original fixture covers 2064 COMPLETE DeathByNPC/SendDeath calls: signed/raw life,
amulet and no-exp flags, NPC present/absent and full/short names, both opaque
party identities/no party, zone suppression, DWORD clock wrap, action-boundary
mutations, null/changed memberships during lock/unlock and 16 actual nested
original calls. Eight nested calls stop at dead==1; eight explicitly change that
same word to 0 and execute another complete death before the outer continuation.
No synthetic busy flag suppresses this original reentry. Compared full ordered
ports, log/view/party packets, exclusion arguments, callback-entry snapshots and
final shared/resources/metadata state. Lock traces describe recursive owner
boundaries, not a native multithreaded locking proof.

Native Actor composition seeds real resource Damage and its retained DamageLedger,
initializes real active effects and preserve-on-death nodes, then calls this
bridge. Actual ClearApplySkillByDeath removes the ordinary HP buff, clamps actual
HP through the existing resource owner, clears casting, erases the replica and
releases that effect ID. Preserved node and current target remain. It stops
explicitly at the missing authoritative ledger-reset accessor: mode/penalty/soul
and effect/resource changes remain published, pending move/deadline/corpse stages
have not run. No empty reset provider pretends this production dependency exists.
Current local Actor replica removal is not a claim to close its known immediate
DB0902 transport gap; that Actor/effect work remains worker02-owned.

Additional native exception check supplies explicit fixture-only empty-ledger/
corpse boundaries to isolate Actor-bridge cleanup. A party callback mutates the
ACTUAL Actor and metadata, then throws. Captured lock is released once without
flushing stale projected values over those partial mutations. Unlock deliberately
reads actual state after release and does not write the pre-send snapshot.
This checks native provider exception semantics, not Windows SEH equivalence or
a completed real corpse/party transport implementation.

Required canonical/root contract:

- Existing Actor +142c mode is authoritative; no new life field/accessor needed.
  Actor owner must expose authoritative +145c penalty, +1430 soul DWORD,
  +1454 rebirth deadline and +1458 raw code (silent reference/setter access).
  These remain explicit required projections until then; do not add a parallel
  Actor metadata map. All references/queries must remain valid and side-effect
  free, with the same serialized world executor and genuine retained Actor lease.
- Actor currently exposes damage_ledger() const only. Add mutable access or a
  silent reset to the SAME ledger equivalent to CDamage::Reset. Do not use
  end_duel/recover or private reset_damage's additional Scene effects to fake it.
- Real PvP leave/corpse publication, log/view transports and current party identity
  bindings remain required. Actor.party_kind() alone cannot provide pointer
  identity. Retain actual PartyRegistryParty/membership owners through captured
  locks, even when callbacks unregister/switch current membership.
- Party sender must implement original 00445680 excluded-user semantics, full
  live count/normal-or-raid owner and real buffer/transport ownership. Do not
  route it through a seven-slot normal broadcast for raid members. The sender
  itself must retain its own original recursive lock/buffer/fanout semantics;
  the coordinator preserves only its separate captured outer lock. Existing
  party owner interfaces currently cover the other broadcast overload; request
  its exclusion-aware composition from that owner rather than copy its list.
- Rebirth's raw resource-write and +1434/+1448 separation contracts are unchanged.
  This NPC path does not require a fake raw zero-resource setter. Full Death
  still needs actual reward/drop/log/PvP/party/raid coordination and its distinct
  action/resource cleanup before reusing any shared primitive.

Validation:

- tools/dev.py build: exit 0; final failure-regression source build 142 actions,
  no warnings/errors; work/mob-targeting-lifecycle-death-failure-build.log.
- Initial full run: 130/131 passed, 35.67 s; first complete nested case 2049
  exposed a driver error: Unicorn reused translated generated-thunk immediates
  from case2048, so nested NPC type/ID were stale. Invalidated ONLY the generated
  thunk range after each host write (ctl_remove_cache), regenerated into ignored
  work/ and copied after readers finished. Native behavior was not changed to
  accommodate that erroneous fixture. Preserved initial tests/sanitized logs;
  neither initial failure is counted as a pass or sanitizer diagnostic.
- Corrected intermediate 131-test runs passed before the final exclusion/
  exception audit; final tools/dev.py test exit 0, 131/131 passed, 39.47 s,
  new unit 0.45 s; work/mob-targeting-lifecycle-death-verified-tests.log.
- Final ASAN/UBSAN compile/run under the shared build lock: exit 0, no
  diagnostics. Instrumented new core/bridge/test plus actual Actor, resources,
  effect cycle/effects/skills, NPC owner and common log writer. All2064 original
  traces, actual effect/resource/ledger-prefix and exception-mutation checks
  passed; work/mob-targeting-lifecycle-death-verified-sanitized.log.
- Original full fixture repeated byte-identically after the correction and
  exclusion-argument assertion; work/mob-targeting-lifecycle-death-final-repeat.tsv.
- All12 original EXE/PDB SHA256SUMS passed; git diff --check clean. No main,
  Actor/party/zone-user/shared-owner changes, merges/resets/pushes or SQL lifecycle
  writes. No SQL data path changed; unchanged SQL checks were not repeated.
  Existing central integration and real DB/effect transports remain required.

No full player lifecycle, production NPC death assembly or playable-server
completion is claimed from this coordinator and these fixture counts.


## Milestone 15 — ordinary Death routes and actual canonical owner composition

Implementation: `97087b8` (preserves all previous milestones). Source:
`ps_game.exe`, `CUser::Death` 004586a0..00458fd3, `SendDeath` 00458480,
`CDamage::Reset` 004b8aa0 and the called owner boundaries below. The new
`mob_targeting_lifecycle_ordinary` API deliberately covers ordinary/fallback
routes only; it is not the complete general Death entry point.

Recovered behavior:

- Already-dead raw state 1 returns immediately. Otherwise the original amulet/
  no-exp-loss inputs determine penalty; life uses the SAME Actor attack mode,
  soul is cleared and HP/MP/SP become zero before exchange cancellation,
  shop termination and CancleActionVehicle2 0044b330. Do not substitute the
  different existing cancellation function 0044b2c0.
- Maximum damage is queried after those callbacks. Non-player maxima use their
  source for the mob log; absent maxima use the caller route. Runtime mob type
  supplies the log word, while the retained full catalog supplies its name.
  The absent-maximum/player-caller branch uses an untouched failed-GetMax output
  source slot for the log DWORD, but the actual caller source for FindUser.
  Scratch provenance is explicit; no invented authoritative attacker value.
  ItemDropByUser is called even with a null lookup result when source differs
  from victim ID. Live kill counters are copied AFTER that callback.
- Common tail preserves PvP leave, actual effect death cleanup, silent ledger
  reset and corpse entry order. Pending movement clears before the fresh clock;
  deadline wraps at tick+200000. The zone packet excludes the subject. Party
  publication uses live membership with the original captured outer lock,
  including callback removal/replacement/reentry. General Death chooses 0c06
  for normal party and 0c11 for raid, unlike the previous NPC path. Corrupt party
  kind leaves the original opcode scratch slot untouched.
- Player maximum kind 1 raises MobTargetingPlayerDeathRequired AFTER the
  recovered prefix. It cannot be retried as a fresh complete Death call:
  initial state/resources/cancellations have already been published. No player
  reward, drop, log or rebirth success is fabricated.

Actual owner integration:

- Existing Actor supplies mode, current resources, damage maximum, effects,
  casting cleanup and pending movement. Existing ZoneMobs supplies borrowed
  runtime mobs; the SAME retained binding catalog supplies full mob records.
  No second Actor, life field, ledger, list, catalog or world owner was added.
- Concrete defaults delegate to canonical Actor::replace_resources_raw and
  Actor::reset_damage_ledger. A checkout lacking either API fails explicitly;
  it does not heal/clamp, invoke Scene reset effects or silently succeed.
- Canonical positive execution used a stable read-only snapshot of headers and
  built archives from main `2515956ace68b3a182fb831654ead80fb5f42bfd`.
  `tools/mob_targeting_lifecycle_canonical_check.py --canonical
  /home/franco/Projects/Eternity` checks source HEAD and archive digests around
  copying, overlays only our lifecycle headers, and compiles/runs both owned
  test suites against that actual Actor implementation. Manifest and copied
  artifacts stay in ignored work/mob-targeting-lifecycle-canonical-check/.
- The NPC test now cohosts the real ActorEffectDatabase sink when available.
  It verifies mandatory 0902 publication after replica/runtime removal and
  before effect-ID release, and no duplicate publication on shutdown. This is
  real interface composition, not a claim of a running DB transport receiver.

Validation (all final commands exit 0):

- tools/dev.py build: 145 actions, no warnings/errors;
  work/mob-targeting-lifecycle-ordinary-owner-build.log.
- tools/dev.py test: 132/132 passed, 42.86 s; new unit 0.83 s;
  work/mob-targeting-lifecycle-ordinary-owner-tests.log.
- 1552 complete original ordinary/fallback Death traces, including 16 actual
  nested reentries, match native output. Regeneration into work/ repeated
  byte-identically. Raw states, callback mutations, missing lookups, packet/log
  scratch, normal/raid membership and captured-lock changes are covered.
- ASAN/UBSAN under shared build lock: all 1552 comparisons and Actor compositions
  passed without diagnostics; old private checkout reports raw/reset API 0/0.
  work/mob-targeting-lifecycle-ordinary-owner-sanitized.log.
- Canonical snapshot execution: all 1552 comparisons passed with ACTUAL raw/reset
  APIs 1/1; all 2064 previous NPC traces and mandatory 0902 ordering passed.
  work/mob-targeting-lifecycle-canonical-verified.log. This separate execution
  closes the API-availability gap without modifying main or canonical owners.
- All 12 original EXE/PDB SHA256SUMS passed; git diff --check clean. No SQL data
  path changed, so unchanged SQL checks were not repeated. No SQL lifecycle
  writes, main/other-owner edits, merges, resets or pushes.

Remaining integration and next block:

- Recover the player-maximum branch 0045876a..00458cf6, including PvPGetDeathType
  0047b270, PvPChkLoseDeath 0047b2c0, GetExp 004581b0, AddExpFromUser 00458240,
  KillPCFrom 0045b050, KillPCTo 0045aed0, ItemDropByUser 0045a180 and live
  party/raid reward iteration. Continue after one GetMax/prefix, not a second
  invocation. The ordinary API must not be installed as a complete Death owner.
- Canonical raw-resource and silent-ledger interfaces are now available; no new
  life/accessor request remains. Four authoritative metadata references still
  must be bound to the canonical Actor metadata owner (+145c/+1430/+1454/+1458),
  initialized with actual inherited allocation context rather than zero seeds.
- Actual exchange/shop/vehicle2, PvP leave, corpse publication, player lookup/drop,
  logging/view transports and exclusion-aware normal/raid party sender remain
  required ports. Retain Actor, PlayerCore, catalog, mob and captured party owners
  through callbacks, on the real serialized world executor; defer destruction
  until leases/locks release. Party kind alone cannot replace pointer identity.
  Admission/transport/root assembly is not implemented by these projections.

No complete player-death, production lifecycle assembly or playable-server
completion is claimed from these checks.


## Milestone 16 — complete PvP death-type helper

Implementation: `945a074`. Recovered the complete `ps_game.exe`
`CUser::PvPGetDeathType` 0047b270..0047b2bf into
mob_targeting_lifecycle_pvp. This is the prerequisite classification helper for
Death's outstanding player-maximum branch, not that entire reward coordinator.
No unchanged ordinary/NPC/Rebirth oracle was regenerated in this block.

Disassembly resolves the decompiler's incorrect implicit register signature:
EAX is the subject, ESI is the peer. At Death 004588b1..004588b5 these are the
dead victim EDI and looked-up attacker EBP respectively. PDB CUser fields identify
+54cc as m_nPvPStatus, +54d4 as m_dwPvPUser, +1748 as m_dwGuildID and +54e4 as
m_dwPvPGuildID (decimal offsets 21708/21716/5960/21732). These guild values are
IDs, not pointers or inferred guild-membership objects.

Exact recovered semantics:

- Same user identity returns 3, regardless of all relationship fields. Equal
  projected values on distinct users do not imply identical users.
- Subject status exactly 4 plus subject peer ID equal to peer +dc returns 4.
  The peer's own status and peer ID are NOT checked. This takes precedence over
  an otherwise valid guild-war relationship.
- Otherwise both guild IDs must be nonzero and each opposing-guild ID must equal
  the other's actual guild ID to return 5. All remaining cases return 3.
  Raw signed/high-bit states and IDs retain their original 32-bit comparisons.

The input is a synchronous borrowed value projection, with explicit actual-user
identity supplied by the caller; no new Actor/life/guild/user owner or lookup map.
The initial integration review missed Actor::relations(), whose existing
UserRelations already owns duel_state, duel_opponent, guild_id and
opposing_guild_id. Milestone 17 below corrects this dependency and adds direct
Actor adapters; no new PvP fields/accessors are needed. Do not derive a duel
status from life/attack mode. Retain both real users on the existing serialized
executor until classification and subsequent callbacks end. No shared header
was changed.

Validation:

- Executed the COMPLETE original helper without hooks or instruction patches
  for 8192 raw/correlated relationship cases. Native comparison passes all:
  211 duel, 221 guild and 4096 same-user cases. Explicit native assertions also
  preserve asymmetric duel eligibility and duel-over-guild precedence.
- Oracle regeneration into ignored work/ is byte-identical to the tracked
  fixture. Original EXE loading verifies its expected SHA256; additionally all
  12 SHA256SUMS pass (work/mob-targeting-lifecycle-pvp-sha.log).
- tools/dev.py build exit 0, 144 actions, no warnings/errors;
  work/mob-targeting-lifecycle-pvp-build.log.
- tools/dev.py test exit 0, 133/133 passed, 40.55 s; new unit 0.08 s;
  work/mob-targeting-lifecycle-pvp-tests.log.
- ASAN/UBSAN and -Wall/-Wextra/-Werror compile/run under shared build lock:
  exit 0, all 8192 comparisons passed, no diagnostics;
  work/mob-targeting-lifecycle-pvp-sanitized.log. git diff --check clean.
- No SQL data path changed or SQL lifecycle writes performed. No main,
  Actor/zone-user/other-owner edits, additional agents, merges/resets or pushes.

Next block remains the player-maximum Death continuation/reward coordination.
Its forced-PvP flag path 0045885c..004588ac is deliberately NOT replaced by this
helper: it writes penalty -1 and has different reciprocal-duel/guild eligibility.
Only the unforced call uses PvPGetDeathType. PvPChkLoseDeath 0047b2c0 and the
actual reward/drop/party/transport boundaries remain to be recovered/composed;
do not re-run Death's published prefix to reach them. Existing canonical death
metadata/resource/ledger APIs and previously documented retention/admission/
transport contracts remain applicable. No production Death or playable-server
completion is claimed.


## Milestone 17 — Death forced/unforced routes and live Actor relations

Implementation: `31c98fa`. Source: `ps_game.exe`, `CUser::Death` decision region
0045885c..004588bd plus special-defeat prefix 00458cab..00458cb6; complete called
PvPGetDeathType 0047b270. New mob_targeting_lifecycle_route implements this bounded
post-lookup decision. Rewards, PvPChkLoseDeath and common Death tail are exits,
not empty success providers. It is not the full player-maximum Death coordinator.

Exact behavior:

- Input requires a NONNULL looked-up attacker AFTER its name copy. Missing
  attacker bypasses this decision in the original; never fabricate a peer or
  write penalty -1 merely because the forced flag was set on an earlier call.
- Any nonzero raw forced byte first publishes penalty -1. Duel requires victim
  state exactly 4 AND reciprocal victim/attacker opponent IDs. Attacker duel
  state is not inspected; same-user identity is not rejected. Otherwise a
  nonzero VICTIM opposing-guild ID selects guild defeat, without checking either
  actual guild ID or reciprocal opposing-guild relationship. If absent, skip
  rewards at 00458cbe, retaining penalty -1. This does not mean defeat succeeded.
- Unforced input calls the recovered helper. Ordinary type 3 enters rewards and
  preserves penalty. Types 4/5 additionally publish penalty -1 at 00458cac before
  the special-defeat call. They do not preserve penalty as an isolated helper
  invocation would. No life/resources/effects/target mutation belongs here.

Owner integration correction:

The previous report missed Actor::relations(). Its existing UserRelations owns
all four fields: duel_state (+54cc), duel_opponent (+54d4), guild_id (+1748) and
opposing_guild_id (+54e4). Existing same_team/Actor::end_duel already consume
this SAME owner. The previous missing-accessor claim is corrected above.
New concrete Actor overloads query those fields and Actor::id(), use actual
address identity, and observe live updates on every invocation. No new field,
PvP map, PlayerCore copy, life counter or canonical accessor is required.

Production assembly: after one Death prefix/GetMax and a retained nonnull
same-zone attacker lookup, call the Actor overload with the actual forced byte
and authoritative victim penalty reference. Canonical Actor::death_metadata().
penalty is now available for that reference (allocation/admission must already
have bound metadata with the explicit inherited context). The overload returns
rewards/duel/guild/skip_rewards only; the owner must then perform the actual
corresponding continuation exactly once. It cannot retry the original Death
prefix, which already published state/resources/cancellations. Existing
ZoneUsers::find_actor returns a borrowed pointer; its documented contract
requires the root to acquire its own owner lease before crossing subsequent
callbacks. Retain both Actors/zone and serialize live state on the existing world
executor. No hidden lock, retention registry or success provider was added.

Validation:

- 16384 original bounded decision executions, without instruction patches or
  helper hooks. The oracle stops at actual reward/defeat/tail branch exits and
  checks the actual pushed attacker for defeat. This is bounded region evidence,
  NOT complete Death execution. Raw flags 0/1/2/128/255, raw states/IDs/penalties,
  self identity, reciprocal/asymmetric matches and guild/duel precedence are
  compared. Results: 232 duel, 11520 guild, 1618 skip, 3014 reward exits; 264
  unforced special routes explicitly exercise the extra penalty write.
- The native suite also constructs actual existing Actors for every row, assigns
  their real UserRelations, compares both concrete overloads, mutates live duel
  relations and checks the next call observes them. Life mode and resource HP
  remain unchanged. Required scene ports fail on unexpected calls; no world
  operation is replaced with an empty provider.
- Initial build passed. Initial full tests: 133/134 passed, 27.96 s; new test
  found mismatch at case 70 because the implementation omitted 00458cac's
  unforced special penalty write. Initial sanitizer likewise failed that
  assertion and emitted a fatal abort stack. Both logs are preserved, neither
  counted as a pass. Corrected native behavior from disassembly; original data
  was not changed to fit the implementation (fixture header was clarified to
  list the additional executed special-defeat prefix).
- Final tools/dev.py build exit 0, 145 actions, no warnings/errors;
  work/mob-targeting-lifecycle-route-verified-build.log.
- Final tools/dev.py test exit 0, 134/134 passed, 39.87 s; new unit 0.03 s;
  work/mob-targeting-lifecycle-route-verified-tests.log.
- Final ASAN/UBSAN under shared build lock exit 0, all 16384 comparisons and
  Actor compositions passed, no diagnostics. Instrumented new route/helper/test
  and actual Actor/resources/effect cycle/effects/skills sources;
  work/mob-targeting-lifecycle-route-verified-sanitized.log.
- Original fixture regenerated into ignored work/ byte-identically after the
  correction; all 12 original SHA256SUMS passed; git diff --check clean.
  No unchanged original startup/SQL fixture regeneration, SQL path change or
  lifecycle write. No main, Actor/zone-user/other-owner edits, agents, merges,
  resets or pushes. SQL/root/transport integration remains centrally owned.

Remaining: actual PvPChkLoseDeath 0047b2c0 (duel item drops/debuffs/ledger/counters/
DB060c/self2405/logs; guild captured-lock/PvPUserLeave), ordinary player rewards/
party/raid iteration, then composition into one complete Death continuation.
Existing root exchange/shop/vehicle2/PvP/corpse/item-drop/party-exclusion/log/view
transport requirements remain explicit. No full player-death, rebirth admission,
production transport or playable-server completion is claimed.
