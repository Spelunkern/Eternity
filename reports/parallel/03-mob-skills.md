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

## Current validation

- All 12 original EXE/PDB hashes pass `sha256sum -c SHA256SUMS` (exit 0).
  Log: `work/mob-applied-skills-application-hashes.log`.
- Locked Ghidra process export passed: AddApplySkill, AddApplySkillDebuff,
  RemovedSkill and list Remove; required output/JDK21/Ghidra12.1.4.
  Log: `work/mob-applied-skills-application-export.log`.
- Original oracle passed 64 sustained scenarios / 1,536 operations. It executes
  real admission, non-damage application/removal, ability mutation, manager,
  list/pool and GetStatus. Required clock/log/rebuild/speed/allocation/lock ports
  are observed; InterlockedIncrement uses locked xadd. Clock callbacks change
  live keep seconds and effect callbacks change Lua log gates.
- Coverage includes ignored effects, failed/new/reused allocation, equal-level
  refresh retaining changed metadata, first-match weak rejection, levels 0/4/255,
  same-node replacement, wrapped IDs/deadlines/stats, all four status stack modes,
  vision target zero/nonzero, pre-link logging, retained nonzero allocator payload
  and shared-pool reuse. Snapshots compare all stat/control fields, definition
  identity, entire payload, list/free order, counter/attempts and ordered events.
- Repeat fixture generation is byte-identical; generated under work/ and copied
  before native readers. Oracle py_compile and staged whitespace checks pass.
- `python3 tools/dev.py build`: exit 0, no compiler warnings.
- `python3 tools/dev.py test`: exit 0, **127/127 passed**, 31.19 seconds. All eight
  applied-skill suites pass, including prior manager/expiry/keep/effect/damage/math
  regressions after sharing the raw creation primitive.
  Logs: `work/mob-applied-skills-application-{build,test}.log`.

Damage-producing replacement/reentrant world callback composition is not included
in this admission fixture; its required provider bodies and earlier coordination
fixtures remain separate. No SQL access or sanitizer run for this block. Shared
isolated SQL integration and canonical owner changes belong to the coordinator.
Historical expiration oracle cache failure was corrected before its successful
122/122 rerun; that initial failed fixture is not counted as a pass.

Next independent proposal: original/native replacement flows whose old damage
effect causes Death/Clear, followed by same-node reapplication, with the actual
final-damage/effect/list owners and retained lifetime ordering.
