# Worker 03: mob applied skill ownership

The native list/pool owner, manager IDs, expiration, effect mutations, final-damage
coordination, complete TimeDamage calculation and live keep/sleep traversal are recovered.
Production world/skill-effect integration is still required; the server is not
playable. Canonical headers and other owners' files remain untouched.

## Source evidence

Immutable `originals/ps_game/ps_game.exe`, SHA256
`0fb7c9f13b5bcf186118f8bfe73ab214226b93e8aa502b8e6fded4a8312bc547`.
Ghidra focused output: ignored `work/exports/03-mob-skills/ps_game.exe`;
complete read-only exports: `/home/franco/Projects/Eternity/work/reverse/ps_game.exe`.

| Original symbol/address | Recovered behavior |
| --- | --- |
| `CMob::ClearApplySkill` **004a0990** | Head-to-tail log-before-unlink; return records to shared LIFO skill heap. No effect reversal, final periodic damage or client removal packet. |
| `CMob::ChkKeepSkill` **004a10d0** | Live list traversal, real periodic calculation and previous-tick writes; post-record status checks and post-unlock sleep removal. |
| `CMob::RemApplySkillSleep` **004a16c0** | Recycle every sleep record without per-skill reversal, force sleep zero and send speed once while locked. |
| `CMob::ChkEndTimeSkill` **004a12f0** | Unsigned `end_tick <= now`; log, unlink, `RemApplySkill`, recycle, advance live shared cursor. |
| `CMob::RemApplySkill` **004a0da0** | All branch coordination implemented; damage types 0x3d/0x3f require calculation/combat/death/view providers and can recursively clear the remaining list. |
| `CMob::SetSkillAbility` **004a1420** | Immediate wrapping additions to base/attack fields; individual attack rebuild or speed callback. Disable counters have no callback. |
| `SSyncHeap<CSkill>::Alloc` **0040b180** | Pop free head first; allocate only if empty; count attempts even on failure. Fresh instance ID is zeroed; other payload retained. |
| `CSkill::CSkill` **0040a0c0** | Initialize instance ID (+8) only. |
| `SSyncList<CSkill>::AddBack` **00486200** | Ordered admission and count increment. |
| `CMob::~CMob` **0048ff10**, `SSyncList<CSkill>::Release` **0044bc00** | Delete active records in list order, rather than recycling. Original execution confirms the loop hidden by Ghidra's noreturn inference. |
| `CObjectMgr::CreateSkill` **0040a780** | Shared heap at manager +0x114; atomic increment through IAT **005210f8**, manager counter +0x10, only for zero IDs. Failure consumes no ID; unsigned increment can wrap to zero. |
| `CMob::ResetAttack` **004905c0** | Clear after restoring resources/attributes and zeroing stun/stop/sleep; then HP/speed delivery and attack rebuild. |
| `CMob::Init` **00490040** | Retains the existing applied-skill list. |

Both clear and expiration log action 6 when `m_bLuaAI` (+0x284) is nonzero OR
`m_pRespawn` (+0xc84) exists and `stMobRespawn::bBothMob` (+0x1c) is true. Arguments
include definition name (+3), level (CSkill +0x10) and previous attacker (+0xcf4).
Payload/definition identity survive recycling. IDs remain unchanged unless zero;
a wrapped zero is assigned again on its next creation, with no zero-skipping rule.

## Native behavior and integration

`MobAppliedSkillPool` serializes the shared heap and owns allocations.
`MobAppliedSkills` owns each ordered active list. `append_from_pool` is raw
heap/list admission; `append_created(shared_atomic_counter)` additionally supplies
manager ID assignment using sequentially consistent atomic increment. Neither is
`AddApplySkill` **004a0aa0** effect/refresh/replacement logic.

`expire(now, projection, removal)` in `mob_applied_skills_expiration.*` preserves
the live shared cursor, including removal-driven recursive clear and subsequent
admission. Logging precedes unlink; the required `remove_effects` port observes
an already unlinked record that is not yet pooled. Recycling follows the port,
even if that callback clears all remaining records. Other payload is retained;
provider writes remain visible. Clear continues to bypass effect-removal ports.

Required coordinator changes:

1. Add `std::unique_ptr<MobAppliedSkills> applied_skills` to `MobRuntime`, bound
   once at mob construction alongside its damage ledger. Supply one shared skill
   pool and manager-wide atomic counter, including other original CSkill owners.
   Pool services, pool, owner-bound logger and borrowed definition storage must
   outlive consumers. Payload access requires world-owner serialization.
2. Bind `MobDeathWorld::clear_applied_skills` to
   `owner.clear({mob.lua_ai != 0, mob.respawn != nullptr,
   mob.respawn && mob.respawn->both})`, preserving damage-reset before skill-clear.
   Bind the required action-6 logger to actual mob logging/context delivery.
3. Call `expire` from the actual serialized world skill scheduler with the same
   flags and a real `RemApplySkill` provider. Missing providers must fail rather
   than silently bypassing effects. Log delivery must not mutate list ownership.
4. Add a real ResetAttack coordinator using the source ordering above. The
   canonical `reset_mob_attack` is also used by Init and is only a stat helper;
   adding unconditional clear there would incorrectly change Init behavior.
5. Actual admission must initialize definition, ID/level, attacker, ticks and
   effect state. `AddApplySkill` and production combat/world wiring remain
   missing. The existing list owner now exposes `keep(now, mob, damage, effects)`
   and `remove_sleep(mob, effects)` using the actual MobRuntime, not copied state.
   Use the same bound subject/list, shared pool, real TimeDamage resolver and
   combat/death/view/speed providers. Schedule expiry separately: Keep does not
   invoke ChkEndTimeSkill.
6. Bind removal to `remove_mob_applied_skill_effects(mob, skill, definition,
   original_base_attribute, services)`. It mutates existing MobRuntime fields;
   no new canonical field is needed. `base_attribute` explicitly projects the
   immutable mob template's byAttrib (+0x40), not its modified runtime attribute.
   `SkillDefinition::detail` and abilities project stSkillInfo +0x32/+0x7c.
   Supply actual attack rebuild and speed delivery. Implement the final-damage
   callback with `finish_mob_applied_skill_damage` and its required providers.
7. Bind periodic TimeDamage (+0x6e type, +0x70/+0x72/+0x74 damage words) separately
   from `SkillDefinition::time_heal`; the canonical definition currently lacks
   these original `byTimeDamageType` / `wTimeDamage[3]` fields. Project them to
   `MobAppliedSkillTimeDamageDefinition` and call
   `calculate_mob_applied_skill_time_damage` with the live mob/template resource
   projection. Needed canonical fields: uint8 time-damage type and three uint16
   damage words; their SQL mapping must preserve the recovered positional data.
   No SQL loader/fixture or canonical definition was changed by this worker.

## Non-damage effect behavior

`mob_applied_skills_effects.*` implements the complete SetSkillAbility dispatch:
wisdom/dexterity/luck, movement/attack speed, all three attack-add categories and
attack-disable counters. Values use x86-compatible 32-bit wrapping addition.
Native `AttackAdd` fields map original offsets +0/+4/+8/+12 to
accuracy/attack/evasion/defense; types 0x1a..0x1c target defense (+12), and
0x1d..0x1f target evasion (+8). Unknown types are original no-ops.

RemApplySkill types 0x3e/0x43 apply all three unsigned ability values positively,
in order, with an immediate callback after each recognized mutation. Original
AddApplySkillDebuff **004a0ca0** negates them at application; removal does not.
Zero values still trigger recognized-type callbacks. Definition slots are read
live, so a callback's later-slot change affects the next application. Types
0x40/0x41/0x42 decrement stun/stop/sleep without flooring and send speed; 0x44
restores the base attribute; 0x48 clears vision-fix ID only. Unknown details and
0x45..0x47 retain their original no-op behavior. No effect handler recycles the
record itself; the expiration/list owner does that after the effect call.

Damage types 0x3d/0x3f dispatch to `finish_damage_skill`; implementations can now
call `finish_mob_applied_skill_damage` in `mob_applied_skills_damage.*`. The latter
recovers the original coordination, not the calculation or combat provider bodies.

## Final-damage coordination

For 0x3d, read the clock, then the live previous tick, and calculate unsigned
elapsed time. TimeDamage receives the captured definition; its mutable HP/SP/MP
outputs go through the original three-resource Damage overload. For 0x3f, capture
HP before combat and call the scalar Damage overload even when HP is zero.
IsBothmob **0044c610** runs after calculation/HP capture; its branch stays fixed through
combat/death callbacks. Copy the skill name before combat for Lua/both mobs; after
combat and optional Death, clear only the first byte of previous user/skill names.
Ordinary mobs retain both complete buffers.

Damage receives current attacker type/ID. If it reports death, reload both values
before the Death call. Packet **0x0607**, 15 bytes, is built after callbacks from
current mob ID, low 16 skill ID bits, low 8 level bits and low 16 resource-result
bits. Periodic results preserve combat's mutable outputs. Scalar results retain
captured pre-combat HP and zero SP/MP. Reload the current zone gate after Death;
PSendView **00421220** receives current position, radius 60, options 0/0 and
priority 6. No node is recycled by this helper; the expiration owner does that
only after the complete helper, including result delivery, returns.

Clock, definition resolution for TimeDamage **004a3be0**, Damage
**00490ee0/00490cf0**, Death **004910f0** and view delivery remain mandatory
providers. The actual TimeDamage math is now available as a pure native function. Do not replace missing providers with
no-ops. The helper requires the subject/skill/definition to survive callbacks;
`skill.name` projects the selected definition's NUL-terminated 32-byte name.

## Complete periodic damage calculation

`mob_applied_skills_time_damage.*` recovers TimeDamage **004a3be0** without world
or calculation stubs. Type 0 copies the three unsigned damage words. Type 3
multiplies each nonzero word by its current resource using low-32-bit products,
then floors signed nonpositive intermediates to 1. Type 4 uses percentages of
maximum resources, type 5 percentages of current resources. Other types produce
zero. Percentage resources are rounded to binary32 before the multiply; the
percentage division/product follow the CRT's 53-bit arithmetic precision. Each
nonzero word's converted intermediate also has the original minimum of 1.

All branches then scale by unsigned elapsed milliseconds / 3000, with the
original binary64 factor store and signed int64 truncation retaining only the
low DWORD. Final values are capped only when signed greater than 30000; negative
wrapped results survive and fractional small results can become zero. This is
not saturating or unsigned damage math. Current and maximum HP are uint32;
SP/MP are uint16. `project_mob_applied_skill_time_damage` binds the existing
MobRuntime and MobRuntimeTemplate fields without canonical edits.

`_fpreset` **0051a1c0** calls `_setdefaultprecision` **0051136e**. The oracle runs
the latter and actual `_controlfp` **0051a397**, yielding CW 023f in Unicorn:
53-bit precision / nearest rounding. It then executes actual `_ftol2`
**0051c424** and the routine's FISTP conversions. Native arithmetic follows that
normal CRT environment and does not change the host rounding mode; custom
nondefault precision/rounding modes are not claimed.

## Live keep and sleep ownership

`MobAppliedSkills::keep` in `mob_applied_skills_keep.cpp` executes against the
existing owner and MobRuntime. For each definition type 0x3d, calculate
`now - previous_tick`, then commit previous_tick=now before combat. It shares the
already verified periodic hit/name/packet path with final removal. No new clock
sample or expiration check is inserted. Even an initial dead status does not
skip the first record; status is checked after each record, including non-DOT
records. The canonical `mob_status` implements the original GetStatus
**0041d760** distinction between inactive status, active empty stack (zero), and
active stack top. The list's actual shared cursor survives callbacks.

After any periodic hit, unlock the first traversal, reload status and remove
sleep only if it is not 2. Unlike expiration, Keep has not unlinked its current
record: death-driven Clear can recycle that record before the result packet is
built. Storage must remain alive, and live packet-field reads must not become
cached/deferred-release substitutes. Previous-tick and other payload changes
survive recycling. Owner matching and subject lifetime remain caller contracts.

`remove_sleep` checks only sleep!=0, not positivity or mob status. It logs before
unlink, recycles all type-0x42 nodes without RemApplySkill, then forces sleep=0 and
calls SendSpeed exactly once while locked, even if no matching nodes existed.
Zero initial sleep leaves matching nodes untouched. The native owner/counter and
pool behavior is complete; SendSpeed remains a mandatory transport provider.
Its **004906d0** listing writes opcode and two speed bytes but does not initialize
packet bytes +2..+5; caller scratch/protocol evidence must be resolved before
claiming complete speed-wire reconstruction or inserting a guessed actor ID.

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

## Current block validation

- `sha256sum -c SHA256SUMS`: exit 0; all 12 original EXE/PDB files match.
  Log: `work/mob-applied-skills-keep-hashes.log`.
- Locked Ghidra process export: exit 0, JDK 21 / Ghidra 12.1.4, output
  `work/exports/03-mob-skills`; Keep, Sleep and GetStatus.
  Log: `work/mob-applied-skills-keep-export.log`.
- Original oracle: exit 0, 512 Keep and 128 direct Sleep traces. Actual Keep,
  Sleep, recursive Clear, TimeDamage/CRT, status/deque reads and list/pool
  instructions execute; combat/death/log/view/speed/allocation/locks are observed
  required ports. A fixed trampoline executes real Clear on death.
- Comparisons cover mixed/empty lists, zero and negative sleep counters,
  inactive/dead/active-empty/active-dead stacks, first-record dead behavior,
  zone/name gates, wrapping elapsed time, previous-tick writes, metadata reloads,
  stop after callbacks, full retained node/name payloads, exact 0607 packets and
  free-list order. Stack counts project active frames only; inactive retained
  implementation bytes are not new canonical state. Original lock assertions
  verify view delivery under Keep's lock, post-unlock Sleep reentry and speed
  delivery under Sleep's single lock. No native fields were added or copied.
- Repeat oracle output matches the fixture byte-for-byte; generated under work/
  and copied before any native fixture reader. New oracle py_compile passed.
- `python3 tools/dev.py build`: exit 0, no compiler warnings.
- `python3 tools/dev.py test`: exit 0, **126/126 passed**, 32.87 seconds.
  All seven applied-skill suites pass. The new comparisons use the actual native
  owner, MobRuntime, TimeDamage, shared hit/result coordinator, pool and status
  helper. Prior final-removal fixtures still pass after sharing the hit helper.
  Logs: `work/mob-applied-skills-keep-build.log`,
  `work/mob-applied-skills-keep-test.log`.
- Staged whitespace checks: exit 0.

Historical validation note: expiration's first fixture failed from Unicorn's
translated-code cache. Invalidation and actual AddBack assertions fixed it; its
corrected complete run passed 122/122 (41.03 s).

SQL contracts are unaffected; no SQL access or sanitizer run was performed for
this block. The coordinator owns complete isolated SQL integration. Only the
private worker checkout changed; canonical owner-interface work remains required.

Next independent block proposal: reconstruct AddApplySkill admission/refresh and
replacement directly on this same native owner, sharing manager IDs, effect
mutations and required world services instead of adding disconnected projections.
