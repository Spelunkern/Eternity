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
