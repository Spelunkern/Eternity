# Worker 03: mob applied skills

Native list/pool ownership, manager IDs, effect mutation, timed application/removal,
TimeDamage, final-damage coordination and AddApplySkill are recovered. Production
catalog/world integration remains incomplete; the server is not playable.
Only scoped worker files/private CMake were changed. No canonical headers changed.

## Source evidence

`originals/ps_game/ps_game.exe`, SHA256
`0fb7c9f13b5bcf186118f8bfe73ab214226b93e8aa502b8e6fded4a8312bc547`.
Focused Ghidra output: ignored `work/exports/03-mob-skills/ps_game.exe`; complete
read-only evidence: `/home/franco/Projects/Eternity/work/reverse/ps_game.exe`.

| Original symbol/address | Native contract |
| --- | --- |
| `ClearApplySkill` **004a0990** | Log, unlink, shared LIFO recycle; no effect reversal. |
| `AddApplySkill` **004a0aa0** | First-match refresh/reject/replacement or new admission; detailed ordering below. |
| `AddApplySkillDebuff` **004a0ca0** | Negative abilities, wrapping status increments, attribute zero and conditional vision fix. |
| `RemApplySkill` **004a0da0** | Positive abilities/status reversal; damage branches use required combat/world providers. |
| `ChkKeepSkill` **004a10d0** | Live periodic traversal; previous tick committed before combat; post-unlock sleep removal. |
| `ChkEndTimeSkill` **004a12f0** | Unsigned end<=now, log/unlink, effect reversal, then recycle. |
| `SetSkillAbility` **004a1420** | Wrapping stat changes and immediate recognized rebuild/speed callbacks. |
| `RemApplySkillVisionFix` **004a1580** | Zero-ID early return; clear vision before locking, log/unlink/recycle every detail-0x48 node without effect reversal. |
| `RemApplySkillSleep` **004a16c0** | All sleep nodes recycled without individual reversal, counter forced zero, one speed send while locked. |
| `RemovedSkill` **004a1930** | Action-6 log with current skill name/level, before replacement unlink. |
| `TimeDamage` **004a3be0** | Complete native arithmetic, including binary32 resource rounding and low-DWORD conversions. |
| `SSyncList<CSkill>::Remove` **0044bad0** / `AddBack` **00486200** | Shared predecessor cursor on unlink; ordered tail admission. |
| `SSyncList<CSkill>::Release` **0044bc00** / `CMob::~CMob` **0048ff10** | Delete active nodes rather than recycling them. |
| `SSyncHeap<CSkill>::Alloc` **0040b180** / `CSkill::CSkill` **0040a0c0** | Shared reuse; failed construction attempts count; only fresh instance ID reset. |
| `CObjectMgr::CreateSkill` **0040a780** | Assign shared atomic counter (+0x10) only to zero IDs; wrap includes zero. |
| `CMob::GetStatus` **0041d760** | Inactive scalar vs active-empty zero vs active stack top. |
| `CMob::Init` **00490040** / `ResetAttack` **004905c0** | Init retains skills; actual ResetAttack clears after restoring base resources/status. |

## Admission and effect behavior

`MobAppliedSkills::apply` uses the existing list/pool and live MobRuntime. Effect
must equal 3; rejection occurs before locking, allocation or clock sampling.
Search preserves list order. Equal skill ID/level refreshes attacker and ticks
without changing definition/name, order or effects. Incoming detail 0x48 may
still update vision fix even though the old definition is retained.

For a differing incoming level <=3, the first equal-ID node decides: reject when
incoming level <= the existing signed level, otherwise log action 6, unlink and
reverse old effects. Reuse that same object/instance ID; do not Free/Create it.
Differing incoming levels >=4 continue searching and can coexist. No generalized
highest-level selection or initial HP/death gate is inserted.

Fresh construction failure consumes neither ID nor clock/effects. Otherwise set
definition, ID/level and attacker before the clock; read keep seconds afterwards
and compute wrapping `now + seconds*1000`. Apply effects, evaluate the live log
gate, log action 5, then append to the tail. Refresh has one clock but no added
log. Remaining payload words survive fresh allocator memory and reuse.

Application types 0x3e/0x43 negate the three unsigned ability values; removal adds
them back. Preserve slot order and each immediate callback, including recognized
zero-valued abilities. Status counters increment/decrement with DWORD wrap, not
flooring; attribute application is zero and removal restores the base byte.
Vision fix requires nonzero captured attacker ID, checks effective status and
sets scalar status=1/next_move=0 only when not dead; target kind/ID still update
for dead mobs. It is direct original field mutation, not SetTarget dispatch.

## Damage-producing replacement composition

The replacement fixture now runs the complete native apply -> old-effect removal
-> real TimeDamage/final-damage coordinator -> declared combat/death callbacks ->
real clear/optional nested apply -> final result -> outer reapplication flow.
No additional runtime projection or production-code change was needed.

The selected node is unlinked but never free or active during old-effect damage,
death and result delivery. Death-driven clear returns only the other nodes.
A declared reentrant admission can reuse those nodes (and reassign their zero
instance IDs), but cannot consume the selected object. Outer apply still installs
its incoming definition, attacker and deadline and appends the same object/ID,
even when combat published death state or changed old metadata. Preserve name
lifetime through nested logging and the old result before the new application.

These are explicit observed provider scripts: combat publishes its returned death
state; the Death port calls original Clear and optionally original AddApplySkill.
They do not reconstruct the complete Death reward body or claim that it
spontaneously admits skills. They stress the existing coordination's live-callback
contract with actual owner operations and stable retained storage.

## Existing ownership/timing contracts

- One shared pool/counter outlives all lists. Destruction deletes active records;
  clear recycles them without reversing effects. Logging gate is Lua!=0 OR
  respawn present with bBothMob true. Name/definition storage survives callbacks
  and retained pooled payload. Missing providers never default to no-ops.
- Keep uses the real shared cursor and checks status after each record, without
  an initial status/deadline guard. It does not invoke expiration. Death-driven
  clear can recycle its current node before packet construction; preserve storage
  and live field reloads. Expiration unlinks first and recycles after effects/view.
- TimeDamage types 0/3/4/5 copy, multiply current resources, percent maximum, or
  percent current. Preserve intermediate float stores, signed low-DWORD results,
  elapsed/3000 scaling and only signed upper cap 30000. Negative wrapped outputs
  survive. CRT default precision comes from `_setdefaultprecision` **0051136e**,
  `_controlfp` **0051a397**, `_fpreset` **0051a1c0**, `_ftol2` **0051c424**;
  custom nondefault precision/rounding environments are not claimed.
- Final damage captures scalar HP or calculates periodic resources; attacker is
  reloaded before Death. Keep names through combat/death for Lua/both mobs, then
  clear only first bytes. Live final fields form the 15-byte 0607 packet; current
  zone/position, radius 60, options 0/0 and priority 6 gate view delivery.
- SetSkillAbility attack-add offsets +0/+4/+8/+12 map native accuracy/attack/
  evasion/defense. Preserve that mapping, especially defense/evasion groups.

## Required canonical integration

1. Add per-MobRuntime applied-skill ownership, constructed once by MobPool against
   the shared skill heap/counter and matching owner-bound logging/services. Bind
   Death's required clear after damage reset. Init/recycling must not clear.
2. Bind `apply`, `keep`, `expire` and `remove_sleep` to actual combat/scheduler
   paths. Actual ResetAttack needs a separate coordinator: the existing
   `reset_mob_attack` stat helper is also called by Init and must not gain clear.
3. The incoming `SkillDefinition`/name is an explicit stable mob-definition view,
   NOT the user SkillDefinitions loader/map. Canonical `MobTargetingBindingSkill`
   currently has id/detail/cooldown/effect/level only. Extend/unify a stable full
   mob-skill view (or retain one stable adapter per slot), preserving reload
   identity and raw levels, including 0 and >=4. Never use temporary conversions.
   Required original fields: name +0x03 (32 bytes), keep seconds +0x5c (DWORD),
   abilities +0x7c (three byte-type/WORD-value slots), time-damage type +0x6e and
   words +0x70/+0x72/+0x74. They are not TimeHeal. SQL mapping remains untouched.
4. Supply real clocks, attack rebuild, speed/log/view delivery, combat and Death.
   Bind TimeDamage to the recovered pure calculator/resource projection. Base
   attribute comes from the original immutable template byte +0x40, not runtime
   attribute. Catalog/name/template lifetimes must span every callback and pool.
5. SendSpeed **004906d0** writes opcode/two speed bytes but not packet +2..+5;
   resolve caller scratch/protocol evidence before claiming its complete wire
   body or inserting a guessed actor ID. It remains a required provider.

## Milestones

| Implementation commit | Completed block | Native/original validation |
| --- | --- | --- |
| `0b9643aeb251eb2c2c23bb02da438bb6b2f748fa` | List ownership, clear and destruction | 64 scenarios / 956 original operations; 120/120 tests, 33.37 s. |
| `7ac2fe02a3f33f1d4a9949b20d2b25cbdc6f57fb` | Shared manager skill instance IDs | 64 scenarios / 1,088 original operations; 512 concurrent creations plus 512 reused; 121/121 tests, 30.59 s. |
| `8053594202a19aece9476e0926898319704e719d` | Expiration coordination and live cursor | 128 original scenarios / 1,600 operations; 122/122 tests, 41.03 s. |
| `79f0cc8e36ff6261bb28c78d06fbfb0f44174e8e` | Non-damage effect removal and ability dispatch | 1,282 original cases plus native expiration/attack composition; 123/123 tests, 29.79 s. |
| `b074babd303fba3abb92b97a68d13be1068b608e` | Final-damage removal coordination and packet | 1,024 original traces plus recursive expiration composition; 124/124 tests, 27.95 s. |
| `e51bed4e907d5fffd2e5ce72fcbd19ca1acf7260` | Complete TimeDamage calculation | 12,544 original cases plus native periodic-removal composition; 125/125 tests, 38.83 s. |
| `3d8b480c25acefe9aa66a15bc3bc6619d2fa74b1` | Live Keep/Sleep owner methods | 640 complete original traces; 126/126 tests, 32.87 s. |
| `5c91dc31a5049e461848dee7e8fe78b953815651` | Admission, refresh, replacement and debuff application | 64 original scenarios / 1,536 operations; 127/127 tests, 31.19 s. |
| `7ecf7f3a3ced7ec5c19b032c81d21fd1fd7367d1` | Damage/reentrant replacement composition evidence | 256 complete original flows; 128/128 tests, 32.24 s. |
| `2037bb01f581144fad24bd8a4d178181e2645b01` | Selective vision cleanup on live owner and shared pool | 512 original scenarios / 3,072 operations; 129/129 tests, 42.70 s. |

| `bbaac402578986a9d1cdeb4907cf733f4c0e1aa4` | Actual stable catalog / retained applied-owner integration | 12,544 original damage rows, 256 ability cycles, 256 actual R2 cycles; 130/130 tests, 44.43 s. |

| `0e4ce8b39d47ca3f2a00085f7b1d84532b19a3fd` | Catalog recursive Death/Clear/nested admission evidence | 160 native compositions; original replacement fixture regenerated identically; 131/131 tests, 35.70 s. |

| `ce03a18ddc7ae1b07469e048a43b17205b7738e6` | Actual retained mob-pool binding, Init/reset/death/destructor lifetime | 64 original resets + 64 retained pool compositions; 132/132 tests, 39.89 s. |

| `4d8e1edde2bf1e30716b0bdb41716d2f5f02e0aa` | Raw heap/manager split and real obelisk composition | 128 original scenarios / 2,816 operations; 64 obelisk compositions; 134/134 tests, 27.84 s. |

| `a48542ab8ed71b1a935050dd2d2645dd7628bba3` | Concrete obelisk allocation/Init/entry adapter | 48 real-owner compositions; original startup/timers fixtures regenerated; 135/135 tests, 38.85 s. |

| `8e4f19d450e3e9bea75aa5ef13cf5da2118f19fa` | Retained ServerMaps/primary-zone composition | 16 real two-map lifecycles; 64 original map cases regenerated; 136/136 tests, 35.71 s. |

## Vision cleanup milestone

`MobAppliedSkills::remove_vision_fix(MobRuntime&)` now operates directly on the
retained list and shared pool. A zero vision ID leaves even matching nodes alone.
A nonzero ID is cleared before locking; all detail-0x48 nodes are visited in live
list order regardless of attacker, effect, deadline, HP or status. Each matching
node is logged under the current Lua/respawn gate before unlink and LIFO recycle.
There is no effect reversal, speed delivery or target/status/resource mutation.
The original does not clear the vision ID again after a callback restores it.

The oracle executes original Alloc/AddBack/Clear and the complete selective
cleanup body with two owners sharing one retained pool. Required allocation,
lock and logging boundaries are observed, not reconstructed world providers.
Explicit first-log mutation restores vision, toggles Lua and changes a later
live definition to type 0x48; it is a test callback contract, not a claim about
original SendLogBothMob behavior. Subsequent operations include another cleanup,
forced vision restoration and raw reuse into the other owner. Full ten-word
payloads, definitions, core fields, allocation attempts and all active/free
orders are compared. No canonical field changes are needed for this method. The targeting/world adapter
must call this owner method when releasing a fixed vision target; production call
site wiring remains outside this worker scope.

## Previous vision validation

- Original EXE/PDB SHA256 verification passed (12 files), exit 0:
  `work/mob-applied-skills-vision-hashes.log`.
- Serialized Ghidra process export for RemApplySkillVisionFix passed using the
  required JDK/Ghidra and worker output directory:
  `work/mob-applied-skills-vision-export.log`.
- 512 original scenarios / 3,072 operations generated twice identically; fixture
  copied from work/ before native tests. Initial oracle assertion wrongly checked
  the vision-clear precondition during ordinary AddBack locking; limited it to
  the selective-cleanup call, then regenerated both complete successful outputs.
- `python3 tools/dev.py build`: exit 0, no compiler warnings.
  `python3 tools/dev.py test`: exit 0, **129/129 passed**, 42.70 seconds; the new suite compares
  all 512 scenarios / 3,072 operations through actual native owners.
  Logs: `work/mob-applied-skills-vision-build.log` and
  `work/mob-applied-skills-vision-test.log`. Oracle py_compile and git whitespace
  checks passed.

No SQL access in this block. Complete isolated SQL and production owner/catalog/
transport assembly remain coordinator dependencies; these tests do not establish
playability. Worker08 owns the full catalog extension. Existing required log and
pool services must be bound to live production owners; there is no no-op default.
Historical replacement test initialization and expiration oracle cache failures
were corrected before their previously documented successful full reruns.

## Stable catalog integration milestone

Assignment prefix: `mob_applied_skills_catalog`. Implementation commit: `bbaac402578986a9d1cdeb4907cf733f4c0e1aa4`. Reviewed worker08 implementation
`36eab1da3fbfadbf83931c83fcd2aff9e85225c9` and report `849a0fc` using read-only
`git show`. Source contracts are `ps_game.exe` CGameData::LoadConstMobSkill **004c0be0**,
GetMobSkillInfo **004027e0**, ReLoadConstMobSkill **004c50c0** (field publication
**004c5c26** onward), plus the already recovered CMob admission **004a0aa0**,
keep **004a10d0**, expiry **004a12f0**, removal **004a0da0** and TimeDamage
**004a3be0**. This block adds native integration rather than new x86 recovery.
New `MobAppliedSkillsCatalog` retains the actual `MobAppliedSkills`
owner and borrows the existing `MobTargetingBindingCatalog`; no second catalog,
lookup map, replacement definition array or temporary SkillDefinition is created.
Admission obtains `skill_definition(allocated_slot)`; logging, names and periodic
calculation resolve the original identity with `from_definition`. Metadata ID is
not treated as the allocated slot. Missing/foreign retained definitions fail
closed with termination at noexcept provider boundaries; absent admission slots
return false without creating nodes. Existing effect filtering remains in apply.

Admission copies only the name string for the synchronous existing application
API, which immediately owns its own node string. Before scheduling/removal the
adapter synchronizes those copies from the catalog's current name view; detached
final-damage nodes also refresh from their live definition. Log ports receive the
live string_view directly. No borrowed temporary name reference is retained.
Name octets are not reinterpreted as a promised Unicode encoding.

The real owner's admission/refresh/replacement, keep and expiry methods run with
the recovered pure TimeDamage calculator. Catalog periodic views supply type and
three words; the subject's actual runtime template supplies maximum resources and
live base attribute. Published keep seconds affect new admission/refresh; existing
absolute deadlines remain unchanged. Published abilities and damage metadata are
read through old active pointers, including old-node replacement reversal.

### Lifetime and publication contract

The catalog must outlive definitions, every active/recycled skill, bridges and
callbacks. Never resize/replace/move-assign a published catalog. Shared pool and
shared atomic manager counter are retained world owners, not per-mob substitutes.
Each bridge's subject, runtime template and required ports outlive the bridge.
Serialize subject operations and raw ordered R2 reload + publish under world
quiescence; no catalog reload during callbacks. This bridge does not provide a
new global reload lock. Complete successful R2 raw publication must be followed
by catalog.publish_skill_record on the actual affected slot before resuming
readers. Existing node definition addresses, raw name addresses and slot identity
remain fixed. Low-level mutable records do not constitute concurrent transactions.

All clocks, combat, death, logging, speed/rebuild and view transport are mandatory
ports with no defaults. Death can synchronously clear the same owner; callers may
use owner() for that recovered coordination, but must not admit foreign definition
pointers. Real production world assembly remains coordinator-owned. No shared
canonical headers or worker08 files were changed.

### Old-branch validation arrangement

This branch predates worker08. Private CMake conditionally compiles exact committed
worker08 source from ignored read-only `work/mob_applied_skills_catalog_snapshot`
only when the normal catalog header is absent. Eight files are extracted using
`git show 36eab1d:<path>`: mob_targeting.hpp, mob_targeting_runtime.hpp,
mob_targeting_bindings.hpp, mob_targeting_bindings_records.hpp,
mob_targeting_reload.hpp and the three corresponding bindings/records/reload cpp
files. Every extracted file is mode 0444 and byte-compared against that commit.
Integrated trees use normal include paths and existing normal catalog/reload
sources; the fallback is not another implementation and must not be provisioned
in a central checkout that already has the real owner.

### Current validation

- Original 12,544-row TimeDamage fixture regenerated by original execution and
  matched byte-for-byte: `work/mob-applied-skills-catalog-time-damage-repeat.tsv`.
- New native composition checks all 12,544 original expected damage triples
  through retained catalog admission, in-place periodic/name/keep publication,
  keep, refresh and old-node replacement, then actual final-damage expiry.
  Additional 256 cycles verify live ability reversal/replacement and pooled node
  reuse; 256 cycles execute the existing recovered R2 coordinator with typed SQL
  input ports, actual stable raw target, cleanup-before-target checks and explicit
  view publication. R2 preserves the original name despite a different row name.
  Direct name publication tests are explicit owner mutations, not R2 renaming.
- All eight ignored snapshot files match worker08 commit bytes and are mode 0444.
- Initial build failed at link after the interface and R2 CMake inputs changed
  while compilation was active; completed configuration is being rebuilt.
  Initial log retained: `work/mob-applied-skills-catalog-build.log`. Its unrelated
  pre-existing guild warnings are not treated as new adapter diagnostics.
- Final `python3 tools/dev.py build`: exit 0, no warnings in final rebuild,
  `work/mob-applied-skills-catalog-build-final.log`.
- `python3 tools/dev.py test`: exit 0, **130/130 passed**, 44.43 seconds; new
  composition suite passed in 0.16 seconds. Full log:
  `work/mob-applied-skills-catalog-test.log`. No native assertion failures.
- `git diff --check` passed. Existing fixtures and prior implementation remain
  unchanged. No sanitizer run in this block.

Original twelve EXE/PDB SHA256 verification passed:
`work/mob-applied-skills-catalog-hashes.log`. Existing original execution fixtures
are reused unchanged; this is new native composition evidence, not a new claim
that the entire world pipeline ran in the original process. No SQL lifecycle or
access is performed by this worker; isolated full SQL and actual world owner
construction remain coordinator checks. Playability is not established.

Next integration: construct this bridge with the normal retained catalog, live
subject/template, global shared pool/counter and actual combat/log/transport ports;
route scheduler/admission into it. Preserve the explicit world quiescence contract
when wiring the existing SQL R2 publisher. Coordinator owns this production root
assembly and complete isolated SQL validation. No additional canonical header
changes are required by this adapter.

## Catalog recursive lifecycle evidence milestone

The completed catalog adapter is now exercised with 160 additional native
compositions: actual keep (periodic only), expiry and replacement, periodic and
scalar-HP final damage, both combat outcomes, optional synchronous admission from
Death, all Lua/respawn-name and zone-send gates. Two actual bridge owners share
one real pool, catalog and global atomic ID counter. The other owner's node must
remain active and unavailable throughout all callbacks. Construction after the
three seeded nodes is rejected, so recursion must use retained pool objects.

Callbacks assert detach/free state at combat, Death and view delivery. Expiry and
replacement retain the selected node outside the pool through every callback;
Death clears the remaining real list, and recursive admission reuses its filler
node. Replacement subsequently admits its original selected node/instance ID.
Keep's selected node remains linked at combat; recursive Clear can recycle it,
and the nested admission can reuse that exact object. The test therefore checks
the packet's live post-callback skill identity instead of assuming a detached
node in all three paths. This is native ownership composition evidence, not a
new full original-process comparison of that particular nested keep sequence.

The scalar port checks captured pre-combat HP even after the callback changes
current HP. Periodic damage checks the pure calculator result and mutates the
returned triple; exact packet words must reflect that mutation. All packet
headers, old node level, radius/priority, optional delivery, previous-name
cleanup, callback ordering, final active/free identity, original instance IDs,
no extra constructions and shared counter are checked. Published definition and
keep-time views come from the actual existing catalog. No adapter/production
logic change was needed; required world ports remain mandatory.

Source basis remains ps_game.exe AddApplySkill **004a0aa0**, RemApplySkill
**004a0da0**, ChkKeepSkill **004a10d0**, ChkEndTimeSkill **004a12f0**,
ClearApplySkill **004a0990** and TimeDamage **004a3be0**. Original full replacement
execution (256 flows, including existing reentrant callbacks) was regenerated
and matches the retained fixture byte-for-byte:
`work/mob-applied-skills-catalog-lifecycle-original.tsv`. This does not substitute
for actual combat/world/SQL providers or original execution of every new native
composition. The prior original and native fixtures remain unchanged.

Implementation/evidence commit: `0e4ce8b39d47ca3f2a00085f7b1d84532b19a3fd`.

- `python3 tools/dev.py build`: exit 0; final build log has no warnings:
  `work/mob-applied-skills-catalog-lifecycle-build-final.log`.
- `python3 tools/dev.py test`: exit 0, **131/131 passed**, 35.70 seconds.
  New 160-case composition suite passed in 0.01 seconds. No failed native runs.
  Log: `work/mob-applied-skills-catalog-lifecycle-test.log`.
- Original 256-flow regeneration/byte comparison and git diff --check passed.

All twelve original EXE/PDB hashes
passed (`work/mob-applied-skills-catalog-lifecycle-hashes.log`). The ignored
read-only worker08 snapshot and normal-catalog central build arrangement from
the previous milestone are unchanged. No SQL access or sanitizer run in this
block. Main's eighth-batch validation freeze was respected.

Remaining production dependencies are unchanged: world root construction, shared
catalog/pool/counter identity, synchronous subject lifetime through Death/view,
external reload quiescence and real combat/log/transport providers. Complete SQL
validation belongs to the coordinator. No canonical interface edits are needed.

## Scope completion checkpoint

The repeated catalog assignment is complete in bbaac40/0bc6799, followed by the
recursive lifecycle evidence in 0e4ce8b/218bf38. Current main continuity explicitly
reviews/queues the catalog adapter while the eighth-batch source freeze remains
active. No additional independent implementation is identified within the
catalog-only prefix without duplicating completed work or inventing world
providers. Production root assembly and SQL validation remain coordinator-owned;
this checkpoint does not imply server completion.

Concrete dependency: wire the retained catalog, global pool/counter, actual mob
subject/template and mandatory combat/death/log/transport/clock ports into the
production scheduler/admission root under its serialization and reload-quiescence
contract. This needs authorization for the relevant production owner files, not
another catalog or temporary definitions. The existing adapter requires no new
canonical fields.

Proposed next independent assignment: expand ownership back to selective applied
skill cancellation, starting with evidence recovery for ps_game.exe
CMob::RemStateSkill 004a5a30 / CancleApplySkillDebuff 004a5d90, then expose it
through the retained owner/catalog bridge. These are investigation targets, not
newly recovered behavior. Do not implement them under the current catalog-only
assignment without coordinator scope confirmation.

Checkpoint validation: clean worker tree before this report update; prior
131/131 native result and original fixture/hash comparisons remain the latest
implementation validation. No source/test/fixture changes and no build, SQL or
original-execution rerun in this documentation-only checkpoint. git diff --check
passed. Awaiting coordinator assignment; no main or other-worker files changed.

## Retained MobRuntime/MobPool binding milestone

Implementation commit: `ce03a18ddc7ae1b07469e048a43b17205b7738e6`.

This assignment supersedes the catalog-only waiting checkpoint. Coordinator
explicitly granted the four canonical mobs/mob_pool files, inspected against
current main read-only (their prior APIs match this branch). No main, worker09,
worker08, zone_users, storage or Actor files were edited.

`MobAppliedSkillsRuntime` borrows the existing stable catalog, shared skill heap,
shared atomic skill counter and required per-subject port resolver. The new
three-argument MobPool constructor binds one MobAppliedSkillsCatalog to each fresh
MobRuntime beside its actual damage ledger. Binding happens before CreateMob
assigns the ID; port resolvers must retain the subject ADDRESS, not cache that
initial ID. Reuse changes the mob ID but retains the same owner/ports and active
skill pointers. No list, catalog or damage owner is duplicated.

The historical two-argument MobPool constructor remains for existing damage-only
projection fixtures. It does NOT provide skill or destructor-AI services;
applied_skills() rejects unbound access instead of supplying no-ops. Production
root construction must use the bound constructor. Migrating unrelated fixtures
and root callers is not silently treated as completed integration.

Bound mobs reject move construction/assignment before any field is moved, because
the catalog bridge borrows their exact address. Unbound legacy projections retain
their old move support. The initial build caught this legacy move requirement in
zone-mobs-unit after adding an out-of-line destructor; a first-member move guard
restores compatibility without permitting bound owner relocation. Initial failed
build retained: `work/mob-applied-skills-runtime-build.log`.

### Recovered lifecycle and providers

ps_game.exe CMob::Init **00490040** / InitAttack **00490290** retain skills.
initialize_mob and reset_mob_attack remain unchanged. The separate full
ResetAttack **004905c0** adapter restores attack buffers/resources/status counters,
then clears skills, sends current 10-byte 0305 HP when a zone exists, sends speed,
and rebuilds attack. It preserves target/status/vision fields. HP world delivery
must use the live zone/position, radius 60, zero options and priority 6; this is a
required port, not a default packet sink. No per-skill effect reversal runs here.

CObjectMgr::FreeMob **0040a630** releases compiled AI then stack/flags/VM and
retains skill/damage lists. CMob::~CMob **0048ff10** releases compiled/Lua state
before damage destruction and active skill-list deletion. Bound runtime
destruction now executes this ordering while all required services are alive;
manual unique_ptr resets prevent later member destruction from doing it twice.
Active skills are deleted, not logged/recycled as ClearApplySkill would do.
Death **004910f0** remains unchanged: actual world callbacks must delegate final
clear to mob.applied_skills().clear AFTER actual damage-ledger reset.

This supplies the bound-pool path for worker09's documented destructor dependency: bind release_stack_ai
to stack-only teardown and close_lua to the real VM owner; do not call the entire
ReleaseLuaFSM wrapper inside stack teardown. Providers can be invoked at FreeMob
and again at final destruction, as in the original; compiled release must accept
an already empty owner. Each extant stack/VM is closed once, and null/reset state
prevents duplicate VM closure. No Lua backend or compiled FSM owner was invented.
Destructor providers must not throw or destroy/recycle the subject during the
callback. World serialization and deferred subject destruction remain required.

Lifetime order: catalog and shared skill/damage pools/counters, per-subject
ports, AI/VM services and their real world owners outlive every bound mob and its
MobPool, including AVAILABLE objects. The runtime binding context also outlives
the pool. Unlink all actual zone membership before pool destruction. Serialize
catalog raw reload/publication against callbacks and retain definitions through
all active/recycled skill nodes. No concurrent reload lock is invented here.

### Validation

New original ResetAttack oracle executes the complete reset and real empty-list
ClearApplySkill; HP world view/SendSpeed/SetAttack are observed ports. All 64
cases compare resources/control, exact HP packet and callback order, with dirty
attack buffers asserted zero before SetAttack. Actual native SetAttack still runs;
its arithmetic has prior original comparisons. Original inputs cover high word/
DWORD values and both zone gates, preserving dirty target/status/vision values.
Two original generations match byte-for-byte; fixture copied before tests.

New native test also runs 64 actual retained-pool compositions: two mobs sharing
catalog/heap/counter, Init retention, publish-in-place ability/keep mutation,
reset logging before unlink with restored resources, actual Death damage-before-
skill clear, pool release/reuse (including active skill retention), bound move
rejection and destruction of both active/available objects. Required ports assert
resource survival during AI teardown and reject duplicate VM closure. Shared
heap counts prove active nodes were deleted at destruction rather than recycled.
VM handles are scripted provider tokens, not an execution claim about real Lua.

- Corrected `python3 tools/dev.py build`: exit 0, no warnings,
  `work/mob-applied-skills-runtime-build-final.log`. A subsequent consistency
  build also passed with no work remaining:
  `work/mob-applied-skills-runtime-build-consistent.log`.
- `python3 tools/dev.py test`: exit 0, **132/132 passed**, 39.89 seconds.
  New 64-original/64-composition runtime suite passed in 0.01 seconds; no native
  test failures. Full log: `work/mob-applied-skills-runtime-test.log`.
- Original fixture repeat, py_compile and git diff --check passed. No sanitizer
  run in this block. The first failed build is retained as described above.

Original twelve EXE/PDB SHA256SUMS passed:
`work/mob-applied-skills-runtime-hashes.log`. Serialized Ghidra process export
for ResetAttack/destructor/FreeMob passed using the required JDK/Ghidra/output:
`work/mob-applied-skills-runtime-export.log`. Oracle py_compile and whitespace
checks passed. No SQL access in this block; complete isolated SQL remains central.

Remaining root work: choose the bound MobPool constructor, supply stable actual
AI/VM/combat/log/world owners, route full ResetAttack to the new adapter and final
Death clear to the retained bridge. No further canonical fields are needed by
this binding; real Lua/backend, scheduler/transport assembly and playability are
not claimed complete.

## Runtime scope completion checkpoint

The repeated retained-runtime assignment is implemented by ce03a18/670f70d.
Read current main continuity/parallel notes and inspected canonical mobs/mob_pool
APIs read-only again; main already exposes the bound constructor and retained
owner accessors. Do not repeat the completed pool/Init/death/reset/destructor
comparisons or add a second service/data owner. No new independent behavior is
identified within the current binding scope.

Concrete dependency: production construction must select the bound constructor
and provide the actual retained compiled FSM/stack/VM release implementations,
clock, combat, log and transport owners. The pool now supplies the destruction
boundary requested by worker09, but that does not create a Lua VM backend or the
root world assembly. Those missing providers cannot be replaced with defaults.

Proposed next assignment: explicitly authorize a production AI lifetime adapter
that delegates release_compiled_ai/release_stack_ai to worker09's actual retained
owners, with designated authoritative owner APIs and a real VM-close provider.
Keep worker09 sources read-only; the coordinator should designate the production
construction site before any root wiring. Alternatively reassign selective
skill cancellation recovery under a new explicit scope. No unauthorized edits
or new tests are needed merely to wait for that ownership decision.

Documentation-only checkpoint: latest implementation validation remains
132/132 native tests, 39.89 seconds, plus 64 original reset comparisons and 64
retained pool compositions. No new build, original execution or SQL run; no
source/test/fixture changes. Clean tree before report update and git diff --check
passed. Global completion is not claimed. Awaiting next assignment.

## Raw heap versus manager preparation milestone

Implementation commit: `4d8e1edde2bf1e30716b0bdb41716d2f5f02e0aa`.

Assignment explicitly expands the existing exclusive MobPool paths. Read current
main coordination and worker04 report, especially the InitObelisk raw-pool
integration dependency; no worker04/09/08 source or main was modified.

Existing MobPool now exposes allocate_raw() and prepare_allocated(MobRuntime&).
allocate() composes exactly those operations. Raw allocation performs the old
fresh construction/damage/skill/lifetime binding or LIFO retained reuse, without
manager preparation; reuse leaves ID/random state and counter untouched. Fresh construction still has
its own original Unix-time sample. Preparation increments the manager counter,
writes that ID, then samples Unix time and seeds the existing ServerRandom with
time XOR ID. It neither constructs nor replaces a skill/damage owner. Reused IDs
and random words remain untouched until preparation; counter wrap includes zero.

Outstanding raw allocations are tracked by this SAME pool. A foreign, released,
already prepared or zone-attached actor is rejected without counter/clock/random
writes. These are explicit host ownership guards, not claimed original error
branches. FreeMob cancels pending preparation while retaining payload/owners.
Clock and teardown providers must obey the existing serialized nonthrowing
runtime contract; a provider exception is not an implicit retry authorization.
No new pool, default no-op preparation or second counter was introduced.

### Original evidence

ps_game.exe SSyncHeap<CMob>::Alloc **0040ab80**, CMob constructor **0048fd50**,
CObjectMgr::CreateMob **0040a5e0** (preparation tail **0040a5f2..0040a61d**),
FreeMob **0040a630**, CMap::InitObelisk **00429a60** and ObeliskRespawn
**0042a6f0**. Startup raw principal preparation is at **00429acf..00429b05**;
complete-respawn raw companion preparation at **0042a96b..0042a99d**. Worker04
already recovered that startup uses raw allocation for both principal and
companions, while complete respawn uses manager preparation for its principal.

The new oracle executes actual constructor/heap/FSM/Lua-release/damage/random
code. R calls raw Alloc; P enters the exact manager tail with its original saved
register stack, without allocating again. Manager A executes complete CreateMob.
128 scenarios / 2,816 operations include allocation failure, fresh raw state,
out-of-order raw preparation, retained ID/random state, LIFO reuse, raw release
without preparation, complete manager allocation and DWORD counter/time wrap.
Snapshots compare payload, events, attempts/counter/free membership and random
state after preparation. Source MT words before first seeding are
uninitialized; both projections mask those words instead of guessing values.
The constructor explicitly writes index 625 at 0048fe96, which is compared.
The constructor's actual seed/time sample remains compared.

### Actual obelisk composition

New native test invokes existing initialize_obelisks and respawn_obelisk with
real bound MobPool, stable catalog/shared skill heap/counter, initialize_mob and
ZoneMobs. The explicit world maps allocate(true) to pool.allocate(), false to
pool.allocate_raw(), and prepare_companion to pool.prepare_allocated(). Each
initialization checks preparation already happened exactly once. It verifies
startup versus respawn raw/manager/time counts, ID wrap, zero-to-three companions,
three countries, absent-zone gate, failed startup principal, actual leave/release/
reuse, stable bound owner identity, real entry and destructor service lifetime.
It also checks rejected duplicate/released/foreign/attached preparation attempts.
World/KC/VM/combat providers outside these paths fail explicitly in the fixture.
This is new native owner composition; it does not claim the combined world ran
in the original process. Original worker04 startup/respawn evidence is preserved.

Old branch lacks worker04 owners: four exact files (obelisk_runtime.hpp,
obelisk_runtime_publication.hpp and corresponding cpp) are read-only mode 0444
under ignored work/mob_applied_skills_runtime_obelisk_snapshot, extracted from
commit 03e120f4f7ad626c7c1f322a5734ed1a82eb1294. Private CMake uses them only if
the canonical obelisk header is absent; central builds use existing normal
owners. No snapshot is copied over the branch or committed.

- Native build passed. Full recompilation emitted existing unrelated guild
  warnings; no new adapter diagnostic. Logs:
  work/mob-applied-skills-runtime-raw-build.log and
  work/mob-applied-skills-runtime-raw-build-consistent.log.
- Tightened fixture comparison to include the explicitly initialized index 625,
  retaining only undefined word masking; final build exit 0, no warnings:
  work/mob-applied-skills-runtime-raw-build-verified.log.
- Final python3 tools/dev.py test: exit 0, **134/134 passed**, 27.84 seconds.
  New raw suite: 128 scenarios / 2,816 original operations, 0.10 seconds;
  real obelisk composition suite: 64 cases, 0.01 seconds. Existing bound skill,
  death/reset/destructor and legacy manager allocation regressions all pass.
  Log: work/mob-applied-skills-runtime-raw-test-verified.log.
  Earlier full passes (38.86 s, then 28.41 s after additional constructor/reuse
  assertions) remain in raw-test.log and raw-test-final.log; no failed tests.
- All four ignored worker04 files match commit bytes and mode 0444; oracle
  py_compile, fixture repeat and git diff --check pass.

Raw objects must be prepared before gameplay random draws or zone entry.
Original fixture generated twice identically and
copied from work/ before native readers. All twelve EXE/PDB SHA256SUMS pass:
work/mob-applied-skills-runtime-raw-hashes.log. Locked Ghidra process export passed
with required JDK/Ghidra/output: work/mob-applied-skills-runtime-raw-export.log.
Oracle py_compile and git diff --check passed. No SQL access or sanitizer run.

Worker04 can now bind its two allocation/preparation ports directly to the real
pool methods above. Real configuration/definition flags, world initialization,
zone/transport, Lua/AI backend and root service construction remain required;
no new canonical fields are needed for this split. Full isolated SQL and central
integration remain coordinator-owned. The server is not claimed playable.

## Raw/preparation scope completion checkpoint

The repeated assignment is complete in 4d8e1ed/d00a223. Reviewed current main
continuity and parallel rules: eleventh-batch validation freeze remains active;
main's canonical MobPool header does not yet expose allocate_raw/prepare_allocated.
Do not recreate the split elsewhere or copy main over the worker branch.

No additional independent implementation is identified in this completed scope.
Concrete next dependency/task: coordinator review/integration of 4d8e1ed after
its freeze, then designate the production ObeliskRuntimeWorld construction owner
to bind allocate(false) to allocate_raw, allocate(true) to allocate, and
prepare_companion to prepare_allocated on the SAME bound MobPool. Other required
init/zone/clock/world providers must remain actual retained services. Editing
worker04's production adapter without a new ownership assignment is excluded.

Documentation-only checkpoint; no source/test/fixture changes or repeated runs.
Latest worker validation remains 134/134 tests (27.84 s), 2,816 original operations
and 64 actual obelisk compositions. Clean tree before this report update and
whitespace check passed. Central integration/SQL and global playability remain
unresolved; awaiting coordinator assignment rather than repeating completed work.

## Concrete obelisk allocation/Init/entry adapter milestone

Implementation commit: `a48542ab8ed71b1a935050dd2d2645dd7628bba3`.

New prefix mob_applied_skills_obelisk; worker04 sources remain read-only.
MobAppliedSkillsObelisk is a constructible final ObeliskRuntimeWorld implementation
that performs allocation, preparation, initialization, entry and random sampling
on the EXISTING bound MobPool, canonical initialize_mob/MobInitPorts and ZoneMobs.
It does not create a pool, zone, catalog, list or AI backend. The remaining ten
world boundaries are mandatory named callables; constructor validation rejects
missing transport/KC/log/portal/removal/name services and incomplete Init ports.
There is no second abstract world layer requiring another allocation/Init stub.

Exact routing: allocate(false) -> allocate_raw, allocate(true) -> allocate,
prepare_companion -> prepare_allocated once. initialize validates this pool's
live prepared actor, assigns the borrowed definition and calls initialize_mob
with definition.id and the original respawn reference. enter delegates to the
same ZoneMobs, preserving actual registration/publication and Lua-entry order.
random uses init.random directly, sharing the same executor stream consumed by
Init's movement delay instead of using per-mob ServerRandom or an extra stream.
Two small read-only MobPool queries expose bound-context presence and live
prepared ownership; no other canonical owner behavior changed.

Missing binding, foreign/free/raw-pending Init/entry and rejected coordinates
produce explicit errors. Allocation failure still returns null to the original
obelisk coordinator. Entry rejection does NOT silently recycle, undo consumed
IDs or clear a source record that may already reference the actor. If a backend
throws after canonical registration, that registered membership remains live;
callers must inspect/recover the actual owner state. The adapter propagates the
error rather than reporting success or inventing transactional rollback.

### Original ordering and evidence limits

ps_game.exe CMap::InitObelisk 00429a60, UpdateRespawnObelisk 00429da0,
ObeliskRespawn 0042a6f0, CMob::Init 00490040 and CZone::EnterMob 00416860.
At 00429b09..00429b1e the original assigns the definition and passes its WORD ID
and stable respawn to Init, then writes position/source membership before entry.
Worker04's original startup and timers fixtures were regenerated read-only via
Python -B into work/ and match retained main fixtures byte-for-byte: 768 startup
plus retained-tick cases and 768 timer cases. Their Init/Enter ports are observed
boundaries; this is not original execution of a complete live world backend.
New native composition crosses those ports into the actual existing owners.

Serialized Ghidra export recovered the three obelisk coordinators and Init;
EnterMob decompilation is marked FAILED in both focused and existing complete
exports. Its complete original disassembly remains available, along with prior
ZoneMobs comparisons. No missing EnterMob body was guessed or reimplemented here.
Log: work/mob-applied-skills-obelisk-export.log. Four ignored, mode-0444 worker04
snapshot files used by the old branch match current main exactly; central builds
use normal existing owners. Previous catalog/snapshot contracts are unchanged.

### Native composition and corrections

48 cases execute startup, an actual unfair companion Death, actual corpse
accounting/recycling, periodic manager replenishment, forced principal/companion
removal and complete respawn through the NEW adapter. They share real bound skill
heap/counter/catalog, per-mob damage ledgers and one ZoneMobs. Three distinct
country definitions and one-to-three companions check live country mutation
from entry/name callbacks, membership-before-entry notification, exact RNG/Unix
cadence, ID wrap, pointer/skill retention across FreeMob/Init and deletion at final
pool destruction. No source/configuration snapshot is retained across callbacks.

Failure cases reject missing backend/Init services and legacy unbound pools
without allocation side effects; reject Init before preparation; propagate
coordinate rejection; and propagate an entry transport exception while retaining
actual registered ownership for explicit cleanup. A test cleanup guard keeps
zone and actor lifetimes valid even on failed assertions.

Initial compile failed on mixed signed/unsigned auto declaration in the new test;
corrected to separate declarations (work/mob-applied-skills-obelisk-build.log). Initial native suites
were 134/135, failing the new test's name-provider expectation: forced Destroy
asks for a still-retained but unlinked subject, and reset=true still emits log
and conditional country-change DB/portal publication. Corrected fixture ports
and asserted those real calls, without changing adapter logic. Initial logs:
work/mob-applied-skills-obelisk-test.log and
work/mob-applied-skills-obelisk-test-diagnostic.log.
Focused corrected test passes all 48 cases using tools/dev.py's SAME shared
resource_lock/IMAGE under a focused container invocation:
work/mob-applied-skills-obelisk-focused.log.

- Corrected python3 tools/dev.py build: exit 0, no warnings:
  work/mob-applied-skills-obelisk-build-corrected.log.
- Final python3 tools/dev.py test: exit 0, **135/135 passed**, 38.85 seconds;
  new 48-case adapter suite passed in 0.01 seconds. Existing original raw/manager,
  retained skill/catalog and bound destructor suites all pass.
  Log: work/mob-applied-skills-obelisk-test-final.log.
- Original startup/timer fixture comparisons, twelve EXE/PDB SHA256SUMS and
  git diff --check passed. Hash log: work/mob-applied-skills-obelisk-hashes.log.
  No sanitizer run; no failures are counted as successful validation.

### Production construction and remaining services

Retain catalog, shared skill/damage heaps and counters, source/configuration,
map/zone, AI/VM/transport/KC/log/portal backends and their callables through every
mob and synchronous callback. Use prior ce03a18/4d8e1ed bound/raw pool APIs; supply
one shared MobInitPorts executor RNG/clock and actual compiled/Lua initialization.
Construct MobAppliedSkillsObelisk(pool, zone, init_ports, services), then pass it
to the existing initialize_obelisks/update_obelisk_respawns/respawn_obelisk.
The adapter's zone must be the source map's actual zone. Serialize all world
operations and reload publication; defer subject destruction/recycling until
callbacks and enclosing owner operations return. The adapter owns no backend
lifecycle and introduces no global lock or automatic teardown policy.

Name results must borrow stable actual definition/name storage. Removal callbacks
must use actual zone/removal owners, including nested destroy/publication. Reset
is not authorization for no-op log/DB/portal services. Actual Lua execution,
compiled factory/backend, transport and production root construction remain open;
no playable-server or real DB-delivery claim. No SQL access in this block;
complete isolated SQL and integrated normal-owner builds remain central checks.

## Concrete adapter scope completion checkpoint

The repeated allocation/Init/entry assignment is complete in a48542a/00fdd9a.
Read current main continuity and parallel rules; thirteenth-batch validation is
frozen and the concrete adapter header is not yet present in main. Existing
worker04 publication/root tasks remain outside this worker's ownership. No new
independent implementation is identified without duplicating completed adapter
work or inventing missing backend services.

Concrete next dependency/task: coordinator review/integration of a48542a, then
designate the production per-zone construction owner and authoritative backend
interfaces that supply the ten required world callables plus MobInitPorts' actual
compiled/Lua initialization. Bind one adapter to the SAME live zone, bound pool
and executor RNG; retain all required service/source backing through callbacks
and mob teardown. Do not replace missing services with no-ops or another world,
pool, catalog or list. Root/backend wiring needs a new explicit ownership scope.

Documentation-only checkpoint: no source/test/fixture changes or new execution.
Latest worker validation remains 135/135 tests (38.85 s), 48 actual adapter
compositions and byte-identical original startup/timer regeneration. Tree was
clean before this report update; git diff --check passed. No main/other-worker
edits, SQL access or global completion claim. Awaiting coordinator assignment.

## Retained primary-map composition milestone

Implementation commit: `8e4f19d450e3e9bea75aa5ef13cf5da2118f19fa`.

This assignment supersedes checkpoint 2244a0a. Reviewed exact worker04 code
4db2ac3bf6b6137e7ad71c4156d628b4e8ad4d66 and report 7b0a529 through git show.
New MobAppliedSkillsObeliskMaps borrows the ONE existing ObeliskRuntimeMaps,
ServerMaps, coherent runtime/full catalogs, SAME MapsServices primary-zone
provider, bound pool, Init ports and shared KC array. It lazily constructs only
host bindings for actual primary zones; it owns no map record, country table,
zone, catalog, actor pool or independent random stream.

Each binding reuses the actual MobAppliedSkillsObelisk actor adapter,
ObeliskRuntimeZoneRemoval and ObeliskRuntimeDeathOwnership. Actor calls query the
live authoritative primary pointer again after callbacks; a changed route fails
explicitly before another allocation/Init/entry/random operation. Absence between
operations returns false without clock/RNG/ID consumption. Boolean true means
original coordination was invoked, not that every original spawn succeeded.
No rollback or second source snapshot is invented. Top-level operations refresh
the EXISTING Maps liveness views on success and exception unwind.

Publication routes are concrete: change -> owner.publish_change(map,index), DB
-> owner.database, world -> owner.broadcast, portals -> owner.update_portals.
Removal uses the actual zone/removal owner and bound pool, including nested
Destroy and linked delegation. Names borrow the actual full mob record, with
runtime-template identity/full-row checks; no constant name or temporary
definition conversion is supplied. Required remaining backends are game log,
KC add/subtract/status and the actual linked removal owner. Init AI/Lua, map/DB/
world transport and passive primary-zone lifecycle still use required real ports.
Calendar/restore/user/persistence remain on the retained Maps owner, not copied
into another adapter. No canonical owner fields were added or edited.

### Lifecycle and production construction

The configured maps and their tile/portal storage, both coherent catalogs,
configuration Maps owner and shared state must stay fixed and outlive the bound
MobPool, including AVAILABLE objects with retained respawn/definition pointers.
Shared heap/counters, AI/VM/skill services outlive all pool objects. Router/backend
closures and zone storage remain alive through every synchronous operation;
serialize publication and defer destruction/recycle through callbacks.

Use the SAME MapsServices instance for bind_obelisk_runtime_maps and this router.
The template projection and full catalog must describe the same admitted data;
coherent loading/reload publication is a root responsibility. This class does
not synthesize or replace either projection. Minimal composition:

```cpp
auto records = bind_obelisk_runtime_maps(server_maps, templates, full_catalog,
    ini, state, map_services);
// Retain records/catalogs beyond pool destruction, including recycled actors.
MobPool pool(mob_ids, pool_services, shared_skill_binding);
MobAppliedSkillsObeliskMaps runtime(server_maps, templates, full_catalog,
    *records, map_services, pool, shared_init_ports, state.kill_count_add, backends);
runtime.startup(primary_map);
runtime.tick(primary_map, now);
runtime.reset(primary_map, tick_provider);
runtime.detach_primary(primary_map); // Requires drained actual zone/source refs.
```

Reset is the actual recovered operation, with its real publications; it is not
an implicit destructor policy. Drain all primary-zone actors, detach while that
zone is still the authoritative live object, then destroy/replace it. A different primary pointer
without detach is rejected without using the old zone; detach with live actors
is rejected. Pointer checks do not implement generation/lease ownership and
cannot detect allocator address reuse; explicit pre-destruction detach is required. A route hidden during a callback must retain storage until return;
partial allocated/registered state remains owned and requires explicit recovery.
The router destructor only destroys host bindings, never frees source records or
sends gameplay traffic. Death wrappers preserve the existing caller-side
accounting: the actual mob_death does its exactly-once sentinel/count update.

### Evidence and validation

Source contracts remain ps_game.exe LoadObelisk 00427300, InitObelisk 00429a60,
UpdateRespawnObelisk 00429da0, ObeliskRespawn 0042a6f0, ObeliskReset 0042aa40,
ObeliskDestroy 0042a410, SetKeepObelisk 00413bd0, Death 004910f0 and
MobRemoveByID 0041e910. This block adds host-owner composition rather than
inventing a new original map owner. Worker04's exact 64-case original map fixture
was independently executed and matched byte-for-byte:
work/mob-applied-skills-obelisk-maps-original.tsv / -expected.tsv.
Those original cases have absent reset principals; the new native cases below
supply actual mobs. Full combined-world original execution is not claimed.

The native test runs 16 two-map compositions with actual original-format map
files loaded by ServerMaps, actual template/full catalog loaders, shared bound
pool/skill/damage owners and real ZoneMobs. It verifies absent primary guards,
shared executor RNG state, constructor/preparation Unix cadence, global ID wrap,
actual source pointer/country/terrain identity, shared bObelisk writes and no
second configuration-ID consumption. It executes native Death/ownership/corpse
cleanup/respawn, reset with nested removal/publications, live later-map mutation,
DB callback route loss, exception-view refresh, legal detach/rebind, rejected
foreign context/live detach/unannounced replacement and final pool destruction
while source configuration/services are still alive. Active skill deletion is
checked after router destruction, while teardown services observe the actual
configuration owner still alive.

Old-branch compatibility uses 29 exact mode-0444 worker04 headers/sources under
ignored work/mob_applied_skills_obelisk_maps_snapshot. Private CMake substitutes
those target sources, including the new mutable-country map_config implementation,
only when the canonical Maps header is absent; it never edits/replaces the live
branch files. Consistent public snapshot declarations avoid mixing two
ServerMaps definitions. Normal integrated builds use canonical modules and no
snapshot. All 29 files verified against exact 4db2ac3 bytes; source ownership
map_config/worker04 is unchanged.

- python3 tools/dev.py build: passed. Initial full recompilation emitted existing
  unrelated guild warnings; no new-module warnings. Final build exit 0, no warnings:
  work/mob-applied-skills-obelisk-maps-build-final.log.
- Focused native composition: 16/16 passed under tools/dev.py's shared lock:
  work/mob-applied-skills-obelisk-maps-focused.log.
- python3 tools/dev.py test: **136/136 passed**, 35.71 seconds; new map suite
  passed in 0.37 seconds. No failed builds/tests in this block:
  work/mob-applied-skills-obelisk-maps-test.log.
- Compatibility source-filter consistency build passed without binary work:
  work/mob-applied-skills-obelisk-maps-build-consistent.log.
- Original fixture comparison, all 12 original EXE/PDB SHA256SUMS and
  git diff --check passed. Hash log: work/mob-applied-skills-obelisk-maps-hashes.log.
  No SQL access or sanitizer run. Prior Ghidra/source evidence is retained;
  worker04's exact recovery is reused, not reported as a fresh decompilation.

Remaining root gaps: actual passive primary-zone lifecycle/leases, coherent
catalog loading/reload, real AI/Lua/backend factory, shared clocks/RNG, KC/linked
world services, DB/world/map/user connections and startup/tick/reconnect dispatch.
The per-primary composition itself is implemented; real backend assembly and full
isolated SQL remain coordinator work. No global completion/playability claim.

## Primary-map composition scope completion checkpoint

The repeated assignment is complete in 8e4f19d/716a0c4. Reviewed current main
continuity/parallel rules and last report; seventeenth-batch source freeze is
active. A bounded read-only check of src/main.cpp still finds its const local
ServerMaps validation load and no primary_zone or MobAppliedSkillsObeliskMaps
construction. No authoritative retained root owner was identified there.

No further independent implementation is identified inside this completed
composition scope. Concrete next dependency/task: coordinator designate and
explicitly authorize the actual retained primary-zone lifecycle/lease owner and
its production construction site, then bind that authority into the SAME
ObeliskRuntimeMapsServices and this router's startup/timer/reset dispatch.
Map/catalog coherence, real AI/Lua, KC/linked and DB/world/map/user connections
remain actual required providers; another registry, copied country table or
successful no-op callback would not resolve this dependency. Root/main and
worker04 map_config edits remain outside the current worker assignment.

Documentation-only checkpoint: no new build, original execution, SQL or fixture
changes. Latest validation remains 136/136 native tests (35.71 s), 16 two-map
lifecycles and the byte-identical 64-case original map replay. Tree was clean
before this report update; git diff --check passed. Awaiting coordinator scope,
without repeating completed tests or claiming global completion.

## Authoritative retained primary-zone lifecycle

Implementation: d4ea8e5. Scope: new mob_primary_zones files and private CMake
registration; no main, worker04, map_config, zone or other worker changes.

Reused the existing ZoneRegistry/RuntimeZone authority in zones.hpp/zones.cpp.
Its original recovery is ps_game.exe CWorld::InitZones 00414870 and
CWorld::GetZoneByMapID 00413df0. Initialization still runs the actual registry
across all configured maps, including instance-cache preparation. Permanent
(create_type == 0) routes use that SAME registry; no second zone registry or
copied map/country/configuration was introduced. Obelisk behavior uses exact
8e4f19d composition and worker04 4db2ac3 retained maps recovery, including
InitObelisk 00429a60/respawn 0042a6f0. Lease generations, staged close and explicit
shutdown are native host coordination, not newly claimed original fields or a
recovered complete world shutdown implementation.

MobPrimaryZones owns the existing bound MobPool by unique ownership transfer,
actual ZoneRegistry, retained obelisk configuration/router, shared ServerMaps
and required shared ZoneServices assembly. Constructor validates bound skill
runtime, an inactive pool and all three required transport providers. The same
pool address is exposed for construction of the actual obelisk router, which
is then transferred into the root with its configuration. No backend defaults
or fake Init/AI/Lua/transport implementations were added.

Move-only primary leases retain the root graph and execute visits under a
recursive serialization gate shared across maps and the shared CRT executor
RNG. No thread-affine mutex ownership escapes in a lease. Leases protect zone
resources, not individual recycled actors. Startup, obelisk timer and reset
methods call the actual router. Other canonical RuntimeZone work can run in
lease.visit; serialized() protects whole-world operations. Passive primary_zone
is a borrowed compatibility route, NOT a lease; consumers must stay in root
serialized dispatch and must not retain its raw pointer. Source views refresh
after successful or exceptional detach/reopen. Reentrant close stops new
admission immediately, but physical close defers until leases and all dispatch
frames finish. Existing leases may finish their work.

Drain performs actual obelisk reset, releases remaining regular actors through
RuntimeZone/ZoneMobs into the SAME pool, detaches obelisks while the old zone is
alive, then invokes real backend release. Failed backend release remains hidden
and retryable without repeating the reset clock/publications. Reopen invokes
actual backend InitObjects on the SAME permanent RuntimeZone/map/ID; only a
successful populated owner advances the native resource generation. Failed
reopen remains hidden until drain cleans partial work. No new ZoneRegistry IDs
are consumed. Pre-attachment drain handles construction failures without
pretending an obelisk reset occurred.

Important lifetime requirement: FreeMob retains raw respawn pointers. Drain
moves the ORIGINAL MobPopulation/LinkedMobPopulation objects into retained
backing storage, not copied records, before backend release. Router dies first;
the actual pool then destroys every bound actor while retained spawn sources,
registry containers, obelisk configuration, maps, catalogs/heaps/AI services are
alive. Sources/configuration/services die afterward. Repeated reopen therefore
retains old source allocations until pool destruction; reclamation is not
invented without stronger actor-reference evidence. Concrete ZoneServices must
own/alias the complete dependency assembly without owning a strong root cycle.
Its release_objects must support partial-release retry and primary sources
already transferred into retention. Instance-cache release remains the existing
registry/backend contract: that backend must retain any such source objects
borrowed by pooled actors. Concrete RuntimeZone destruction must clean partial
Init while the pool/services are still alive. Root destruction requires every
successfully initialized primary to have completed drain; violating that
explicit shutdown precondition terminates rather than silently skipping world
cleanup or inventing destructor publications.

Validation:

- python3 tools/dev.py build passed; final current build log
  work/mob-primary-zones-build-verified.log has no warnings. Initial new-test
  compilation exposed explicit unique_ptr conversion and wrong constructor
  arguments; these were corrected before successful builds.
- python3 tools/dev.py test: **137/137 passed**, 44.83 seconds, recorded in
  work/mob-primary-zones-test.log.
- Four substantive native compositions use actual original-format ServerMaps,
  one existing ZoneRegistry, real regular/linked population initialization,
  actual ZoneMobs, bound skill/damage pool, full stable catalog and retained
  obelisk maps/router. They cover regular plus obelisk startup, periodic removal
  and replenishment, shared RNG/raw-ID cadence, reset, deferred reentrant close,
  lease self-release during dispatch, cross-map thread serialization, failed
  release/reopen, unchanged zone IDs, absent providers, partial Init cleanup and
  actual final AI/skill/pool destruction. Dropping external assembly/root
  references while a lease exists retains the complete graph; final drain and
  last release destroy it. Focused log: work/mob-primary-zones-focused-owned.log;
  subsequent additional failure/lifetime cases passed in the full suite.
- Focused root and test compiled with AddressSanitizer/UndefinedBehaviorSanitizer
  passed all four compositions: work/mob-primary-zones-sanitized.log. Existing
  linked game/storage archives were normal builds; this is not a claim that all
  dependencies were instrumented.
- Original execution with .local/oracle-env/bin/python tools/zones_oracle.py
  matched tests/data/zones-oracle.tsv byte for byte. Output generated under
  work/mob-primary-zones-original.tsv before comparison; no fixture overwritten.
- All 12 original EXE/PDB SHA256SUMS passed
  (work/mob-primary-zones-hashes.log); git diff --check passed. Exact ignored
  worker04/catalog compatibility snapshots from prior milestones are reused;
  central builds use canonical owners. No SQL access, database lifecycle work
  or new Ghidra import. These checks do not establish a playable server.

### Coordinator construction/dispatch proposal (main remains untouched)

In src/main.cpp's --serve branch, before world->start()/io.run(), retain a real
production dependency assembly and shared mutable ServerMaps. The const local
ServerMaps in --check-original-loaders is validation only, not runtime state.
Move the ONE bound unique MobPool into MobPrimaryZones::create with shared
ZoneServices and actual DB/world/map publication functions. Call initialize;
construct worker04's bind_obelisk_runtime_maps using *root as the SAME maps
services, then construct the existing MobAppliedSkillsObeliskMaps using
root->mob_pool(), real MobInitPorts/KC/world services and shared executor RNG.
Transfer configuration/router with attach_obelisks; startup each permanent map.
All borrowed catalogs/templates/skill heaps/counters/AI ports must be retained
by the shared assembly through final pool destruction. Use weak backreferences
to avoid cycles. Call tick for obelisk timers only; regular AI/user/NPC/combat
frames still require their real canonical dispatch through leased RuntimeZone
visits. Dynamic instance admission is not exposed by this primary facade;
future integration must extend/use the SAME registry, not create another one.

On shutdown first stop producers/admissions and retain root strongly. Release
outstanding leases, dispatch drain_all using the real clock on the serialized
executor and repost if deferred. Handle backend failure explicitly and retry
its retained stage; do not drop the last root while primaries remain live.
Only after completed drain release root/assembly. This ordering belongs in
coordinator-owned serve/signal wiring; no src/main.cpp patch was applied.

Remaining concrete gaps: production ZoneServices/RuntimeZone factory and partial
Init/release contract, coherent catalog reload ownership, actual AI/Lua backend,
KC/linked/calendar/log/DB/world/map/user providers, executor clocks/RNG and main
startup/frame/shutdown assembly. Root zone leases do not replace actor lifetime
leases or implement those backends. Coordinator owns full isolated SQL and final
integration. Primary retained lifetime and dispatch are now concrete and tested;
global gameplay completion is not claimed.

## Retained primary-zone scope completion checkpoint

Repeated assignment reviewed against current main CONTINUITY/PARALLEL_WORK,
AGENTS.md and d4ea8e5/9a5aabe. The requested authoritative primary lifetime,
serialized lease dispatch and actual registry/obelisk integration are already
implemented and validated above. Main is now in nineteenth-batch source freeze;
its serve path still calls world->start at line 96 and io.run at line 99, while
ServerMaps::load remains validation-only at line 120. No primary root or actual
ZoneRegistry construction is present there. Read-only inspection only.

No additional independent behavior is identified within this completed scope.
Concrete next assignment proposal: authorize a production ZoneServices assembly
and concrete RuntimeZone factory, explicitly supplying retained canonical
Init/AI/catalog/heap/clock/world providers and implementing partial Init/release
ownership, then connect it to MobPrimaryZones at the coordinator-owned serve
hook. Missing Lua/transport/KC providers must remain explicit dependencies;
constructing a second registry or successful empty factory would not resolve
this gap. Main wiring and other workers' backend implementations remain outside
this assignment.

Documentation-only checkpoint: no source/fixture changes or repeated execution.
Latest validation remains 137/137 tests (44.83 seconds), four focused sanitized
compositions, byte-identical original zones replay and all 12 binary hashes.
Tree was clean before this update; git diff --check passed. No SQL access or
global completion claim.

## Concrete canonical zone factory and partial initialization

Implementation: ff62d6c2ad634eadec728c025da4e859b4f162f6. New exclusive
mob_zone_services header/source/test and private CMake only; zones.hpp/cpp,
main and other worker files remain unchanged. Current continuity confirms the
nineteenth integration freeze and the explicit factory assignment.

MobZoneServices now implements ZoneServices with concrete RuntimeZone allocation,
map preparation and object creation over canonical owners. There is no new
registry, map configuration, catalog or pool. Its zones are allocated by the SAME
ZoneRegistry inside MobPrimaryZones. Map preparation creates actual
ZoneEnvironment, ZoneUsers, ZoneNpcs, ZoneCorpses and ZoneItems, plus canonical
per-zone regular and linked spawn copies. NPC initialization uses
ZoneNpcs::initialize with actual NpcDefinitions. Map object initialization calls
MobPopulation::initialize, whose required linked callback calls
LinkedMobPopulation::initialize with the SAME bound MobPool and MobInitPorts.
No clock/RNG samples, IDs or initialization results are fabricated. Primitive
clock/RNG providers must not throw, matching original primitive operations.

Source evidence reused/read directly: ps_game.exe CZone::MapInit 0041c8a0
(copies map mob/linked sources and calls CMap::InitNPC), CMap::InitNPC 00429130,
CZone::MapInitObject 0041c5f0 (regular spawning followed by linked initialization),
CZone::MapInitBothMob 0041b470, CZone::MapReleaseObject 0041c3b0 (unlink/index clear,
AI cleanup/recycle and instance portal clearing), and CWorld::InitZones 00414870.
Existing canonical modules retain their original recovery. This is an owner
composition, not a claim of new complete original world execution equivalence.

The factory requires a retained dependency assembly, bound pool, templates,
NpcDefinitions, MobInitPorts, WorldCorpses, linked configuration projection,
Unix/local clocks, NPC heap release and actual instance wait-queue publication.
A required per-RuntimeZone bind function returns retained mob/NPC/corpse/item
service ports. All four must exist before map resources are created. This
binding matters: cell service calls carry coordinates without a map ID, so a
single unbound cell transport across maps would be incorrect. Ports must bind
the actual zone and avoid strong backreferences into root/registry. No successful
empty AI/Lua/transport/KC provider was added. Required callback presence is
validated; semantic backend availability remains the provider's responsibility
and unsupported operations must report failure.

Each concrete zone retains its factory and per-zone ports. Lifetime-stable
geometry is stored in that zone for corpse/item owners that borrow geometry.
Partial map/object initialization uses the same concrete destructor cleanup as
normal destruction. AI initialization exceptions before EnterMob explicitly
return the unregistered subject to the real pool, then propagate; zone cleanup
releases previously entered actors. Bulk release recycles canonical members and
retains the ORIGINAL regular/linked source objects in the service when not
already retired by MobPrimaryZones. Thus instance-cache preparation and failed
initialization also preserve pooled raw respawn pointers until pool destruction.
Release preserves map/NPC/environment owners, matching the separation between
MapInit and MapReleaseObject. Reopen rebuilds retired spawn copies and runs real
population initialization. Normal root retirement still owns primary sources;
factory retention handles other release/partial-init paths without duplication.

Real users, corpses and ground items must be drained by their world owners before
release. A nonempty view explicitly rejects release; it is not silently detached
or freed without its backend. The root's retained stage permits that owner to
evacuate and retry release without replaying obelisk reset. At final concrete
zone destruction, NPCs leave the actual ZoneNpcs and return through the required
heap provider. Views are destroyed before their retained geometry/ports die.
Live views at destruction violate the explicit shutdown precondition. Primitive
allocation/ID/clock providers and destructor cleanup must obey their nonthrowing
contracts; this does not promise recovery from arbitrary allocation failure
inside canonical pool/list primitives. Raw public mob zone fields after bulk
release remain non-owning residues; backend AI teardown must not dereference a
destroyed partial-init zone through them.

Validation:

- python3 tools/dev.py build passed; final
  work/mob-zone-services-build-verified.log has no warnings. An earlier reuse of
  a test main under a renamed symbol emitted a return-type warning; diagnostic
  suppression is scoped to that unchanged fixture include. No build failure.
- python3 tools/dev.py test: **138/138 passed**, 25.87 seconds, in
  work/mob-zone-services-test-bound.log. Earlier complete passes were 138/138
  (47.03 and 27.00 seconds); preserved rather than hidden.
- After adding the real corpse blocked-release/retry case, final focused CTest
  passed **2/2**, 0.04 seconds: work/mob-zone-services-focused.log. This exercises
  both the existing primary owner suite and the final factory suite.
- Six factory scenarios reuse the existing original-format map/catalog/bound
  damage/skill fixture instead of duplicating fixture data. Success creates a
  regular mob, linked leader and linked companion per map; actual obelisk
  startup adds two actors. They cover actual periodic obelisk dispatch,
  corpse-owner release failure and retry, reopen, complete drain/destruction,
  invalid linked radius after entry, missing linked configuration, compiled AI
  and Lua initialization exceptions before entry, and absent per-zone services.
  Every failure leaves all allocated mobs available in the SAME pool. The
  successful case verifies canonical source pointer identity and real cell
  owners. Unchanged primary tests retain their lease/lifetime/ordering coverage.
- Final root/factory/test ASan+UBSan run passed all six cases:
  work/mob-zone-services-sanitized-final.log. Linked existing game/storage
  archives remain normal builds; not a fully instrumented server claim.
- Original execution via .local/oracle-env/bin/python tools/mob_population_oracle.py
  and tools/linked_mobs_oracle.py matched existing mob-population-oracle.tsv and
  linked-mobs-oracle.tsv byte for byte. Outputs stayed under ignored
  work/mob-zone-services-{regular,linked}-original.tsv; no duplicate/rewritten
  fixture was committed. Native original comparisons passed in the full suite.
- All 12 original binary/PDB SHA256SUMS passed
  (work/mob-zone-services-hashes.log); git diff --check passed. No SQL access or
  database lifecycle changes. Existing exact worker04/catalog snapshots remain
  read-only compatibility inputs; integrated builds use canonical modules.

Production hook refinement (coordinator-owned main remains untouched): retain
an assembly containing the SAME maps, loaded NPC/runtime/full catalogs, actual
LinkedMobConfiguration, heap/counter/AI services, executor RNG and MobInitPorts.
Construct the bound unique MobPool against that assembly; create
MobZoneServices with its pool reference and retained assembly token. Its linked
projection should return the existing LinkedMobConfiguration::for_map view,
not parse/copy another catalog. Bind each actual RuntimeZone to real cell
transport/allocation/Lua ports. Move that SAME unique pool into
MobPrimaryZones::create with the new concrete factory; initialize the existing
registry, then attach the exact retained obelisk owners and dispatch startup.
This occurs before serve's world->start()/io.run(), not in the validation-only
ServerMaps local. Regular/linked frames can use canonical
RuntimeZone::update_mob_population inside root leases with the real
LinkedMobWorld and obelisk callback; root.tick remains obelisk-only.

Stop admission/producers, evacuate real users/corpses/items, release leases and
complete retained root drain before releasing root. Root destroys the actual
pool before registry containers/source storage/configuration/assembly services;
no other owner may destroy or replace that pool. Keep source/definition and
non-mob heap providers alive through their respective final destruction. The
factory does not supply the real AI/Lua factory, map/user transport, KC/linked
world operations, DB/log/calendar/queue implementations, global frame scheduler
or signal/serve assembly. These precise providers remain required inputs; no
second authority or no-op fallback fills them. Full isolated SQL and final main
integration belong to the coordinator; gameplay completion is not claimed.

## Concrete zone factory scope completion checkpoint

Repeated assignment reviewed against current continuity/parallel rules and
ff62d6c/22ebd6c. Nineteenth batch is now validated and its source freeze released;
this does not alter worker ownership. The requested concrete factory, partial
Init cleanup, retained source ownership and actual regular/linked/obelisk
compositions are already delivered above. Read-only main inspection still finds
serve world->start()/io.run() at lines 96/99 and validation-only ServerMaps::load
at line 120, with no MobZoneServices/MobPrimaryZones/ZoneRegistry construction.

No additional independent behavior is identified inside this completed factory
scope. Concrete next-task proposal: assign the actual per-zone service binding
assembly (mob/NPC/corpse/item cell transports and heap ports plus retained AI/Init
providers), using existing canonical delivery owners, then connect it to this
factory and the SAME primary root. Coordinator must identify the production
world/connection authority and required backend owner rather than authorize
another registry or a successful empty transport/Lua implementation. Main
serve/signal integration remains coordinator-owned, with the exact construction
and drain contract already documented above.

Documentation-only checkpoint: no source/fixture changes, repeated tests or SQL
access. Latest validation remains 138/138 native tests, final 2/2 focused checks,
six factory sanitized scenarios, unchanged original population fixture replays
and all 12 original binary hashes. Tree was clean before this report update;
git diff --check passed. No global completion claim.
