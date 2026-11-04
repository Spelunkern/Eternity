# Worker 03: mob applied skill ownership

The native list/pool owner, manager IDs, expiration and non-damage effect
removal are recovered.
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
| `CMob::RemApplySkill` **004a0da0** | Non-damage branches implemented. Damage types 0x3d/0x3f still require an explicit provider; they can invoke Death and recursively clear the remaining list. |
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
   effect state. `AddApplySkill`, periodic application (`ChkKeepSkill`), damage
   removal branches and production combat/world wiring remain missing.
6. Bind removal to `remove_mob_applied_skill_effects(mob, skill, definition,
   original_base_attribute, services)`. It mutates existing MobRuntime fields;
   no new canonical field is needed. `base_attribute` explicitly projects the
   immutable mob template's byAttrib (+0x40), not its modified runtime attribute.
   `SkillDefinition::detail` and abilities project stSkillInfo +0x32/+0x7c.
   Supply actual attack rebuild, speed delivery and final-damage providers.

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

Damage types 0x3d/0x3f dispatch to the mandatory `finish_damage_skill` service.
Their periodic final damage, death/name handling and packet bodies remain
unimplemented; dispatch tests do not claim those bodies execute natively.

## Milestones

| Implementation commit | Completed block | Native/original validation |
| --- | --- | --- |
| `0b9643aeb251eb2c2c23bb02da438bb6b2f748fa` | List ownership, clear and destruction | 64 scenarios / 956 original operations; 120/120 tests, 33.37 s. |
| `7ac2fe02a3f33f1d4a9949b20d2b25cbdc6f57fb` | Shared manager skill instance IDs | 64 scenarios / 1,088 original operations; 512 concurrent creations plus 512 reused; 121/121 tests, 30.59 s. |
| `8053594202a19aece9476e0926898319704e719d` | Expiration coordination and live cursor | 128 original scenarios / 1,600 operations; 122/122 tests, 41.03 s. |
| `79f0cc8e36ff6261bb28c78d06fbfb0f44174e8e` | Non-damage effect removal and ability dispatch | 1,282 original cases plus native expiration/attack composition; 123/123 tests, 29.79 s. |

## Current block validation

- `sha256sum -c SHA256SUMS`: exit 0; all 12 original EXE/PDB files match.
  Log: `work/mob-applied-skills-effects-hashes.log`.
- Locked Ghidra process export: exit 0, JDK 21 / Ghidra 12.1.4 and the required
  `work/exports/03-mob-skills` output; RemApplySkill, SetSkillAbility and
  AddApplySkillDebuff. Log: `work/mob-applied-skills-effects-export.log`.
- New original oracle: exit 0, 770 direct ability cases and 512 non-damage removal
  cases. Repeat output matches `tests/data/mob-applied-skills-effects-oracle.tsv`
  byte-for-byte. Generated under `work/` before copying, with no fixture reader.
- The oracle executes original mutation/dispatch instructions. SetAttack and
  SendSpeed are observed required ports, not simulated original bodies. Snapshot
  comparisons include all projected fields, HP/status/target canaries, definition
  slots and exact callback order/state. Mutating ports change wisdom and a later
  ability slot, testing live reads and lack of callback batching. Damage-removal
  branches are excluded from this original fixture.
- `python3 tools/dev.py build`: exit 0, no compiler warnings.
  Log: `work/mob-applied-skills-effects-build.log`.
- `python3 tools/dev.py test`: exit 0, **123/123 passed**, 29.79 seconds.
  All four applied-skill suites pass. New native composition expires three real
  owned records, applies state removal and calls the actual native attack rebuild,
  checking the complete log/removal/speed/rebuild trace and final attack value.
  Two additional dispatch checks require the outstanding damage provider to run.
  Log: `work/mob-applied-skills-effects-test.log`.
- New oracle `py_compile` and whitespace checks: exit 0.

Historical validation note: the expiration milestone's first fixture failed from
Unicorn's translated-code cache; that oracle was corrected with invalidation and
actual AddBack assertions. Its corrected complete run passed 122/122 (41.03 s),
recorded in `work/mob-applied-skills-expiration-test-fixed.log`.

SQL contracts are unaffected; no SQL access or sanitizer run was performed for
this block. The coordinator owns complete isolated SQL integration.

Next independent block proposal: reconstruct RemApplySkill's remaining damage
coordination (0x3d/0x3f), preserving final periodic calculation, previous-attacker
name lifetime, death and the exact result packet through required combat/world
providers. Do not duplicate the combat or logging owners' underlying bodies.
