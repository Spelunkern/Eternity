# Worker 03: mob applied skill ownership

The native list/pool owner, manager IDs, expiration, effect mutations and final-damage
coordination are recovered.
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
   effect state. `AddApplySkill`, periodic application (`ChkKeepSkill`), periodic
   damage calculation and production combat/world wiring remain missing.
6. Bind removal to `remove_mob_applied_skill_effects(mob, skill, definition,
   original_base_attribute, services)`. It mutates existing MobRuntime fields;
   no new canonical field is needed. `base_attribute` explicitly projects the
   immutable mob template's byAttrib (+0x40), not its modified runtime attribute.
   `SkillDefinition::detail` and abilities project stSkillInfo +0x32/+0x7c.
   Supply actual attack rebuild and speed delivery. Implement the final-damage
   callback with `finish_mob_applied_skill_damage` and its required providers.
7. Bind periodic TimeDamage (+0x6e type, +0x70/+0x72/+0x74 damage words) separately
   from `SkillDefinition::time_heal`; the canonical definition currently lacks
   these original `byTimeDamageType` / `wTimeDamage[3]` fields. A later calculation
   block needs an explicit definition projection until the coordinator adds them.
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

Clock, TimeDamage **004a3be0**, Damage **00490ee0/00490cf0**, Death **004910f0** and
view delivery remain mandatory providers. Do not replace missing providers with
no-ops. The helper requires the subject/skill/definition to survive callbacks;
`skill.name` projects the selected definition's NUL-terminated 32-byte name.

## Milestones

| Implementation commit | Completed block | Native/original validation |
| --- | --- | --- |
| `0b9643aeb251eb2c2c23bb02da438bb6b2f748fa` | List ownership, clear and destruction | 64 scenarios / 956 original operations; 120/120 tests, 33.37 s. |
| `7ac2fe02a3f33f1d4a9949b20d2b25cbdc6f57fb` | Shared manager skill instance IDs | 64 scenarios / 1,088 original operations; 512 concurrent creations plus 512 reused; 121/121 tests, 30.59 s. |
| `8053594202a19aece9476e0926898319704e719d` | Expiration coordination and live cursor | 128 original scenarios / 1,600 operations; 122/122 tests, 41.03 s. |
| `79f0cc8e36ff6261bb28c78d06fbfb0f44174e8e` | Non-damage effect removal and ability dispatch | 1,282 original cases plus native expiration/attack composition; 123/123 tests, 29.79 s. |
| `b074babd303fba3abb92b97a68d13be1068b608e` | Final-damage removal coordination and packet | 1,024 original traces plus recursive expiration composition; 124/124 tests, 27.95 s. |

## Current block validation

- `sha256sum -c SHA256SUMS`: exit 0, all 12 original EXE/PDB files match.
  Log: `work/mob-applied-skills-damage-hashes.log`.
- Locked Ghidra process export: exit 0, JDK 21 / Ghidra 12.1.4, output
  `work/exports/03-mob-skills`; RemApplySkill, IsBothmob and TimeDamage.
  Log: `work/mob-applied-skills-damage-export.log`.
- New original oracle: exit 0, 1,024 traces across both damage branches, all 16
  Lua/respawn/both/zone flag combinations, both death outcomes and 16 callback
  mutation modes. Repeat output matches the fixture byte-for-byte. Generated
  under `work/` and copied before any native test reads the fixture.
- The original RemApplySkill and IsBothmob bodies execute. Calculation, combat,
  death and view ports are explicitly observed, not their underlying bodies.
  Comparisons include full previous-name buffers and retained tails, skill/mob
  payload, every call's live arguments/state, clock/previous-tick ordering,
  mutable three-resource outputs, HP capture, narrowed packet bytes, view options
  and zone changes during callbacks.
- `python3 tools/dev.py build`: exit 0, no compiler warnings.
  Log: `work/mob-applied-skills-damage-build.log`.
- `python3 tools/dev.py test`: exit 0, **124/124 passed**, 27.95 seconds.
  All five applied-skill suites pass. Native composition executes expiration,
  effect dispatch, final-HP coordination, death-driven recursive clear and view
  delivery; it verifies pool order, name clearing and exact callback trace
  `log, removal, damage, death, remaining-log, view` before current-node recycling.
  Log: `work/mob-applied-skills-damage-test.log`.
- New oracle `py_compile` and whitespace checks: exit 0.

Historical validation note: expiration's first fixture failed from Unicorn's
translated-code cache. Invalidation and actual AddBack assertions fixed it; the
corrected complete run passed 122/122, 41.03 s, in
`work/mob-applied-skills-expiration-test-fixed.log`.

SQL contracts are unaffected; no SQL access or sanitizer run was performed for
this block. The coordinator owns complete isolated SQL integration. Its canonical
validation freeze was respected: only this private worker checkout was changed.

Next independent block proposal: reconstruct TimeDamage **004a3be0** calculation
using explicit byTimeDamageType/wTimeDamage projections, real resource limits,
original floating-point/truncation behavior and capped result values. Continue to
leave combat, Death and logging implementations with their established owners.
