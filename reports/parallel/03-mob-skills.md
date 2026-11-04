# Worker 03: mob applied skill ownership

The native list/pool owner, manager IDs and expiration coordinator are recovered.
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
| `CMob::RemApplySkill` **004a0da0** | Required effect-removal body; can invoke `Death`, recursively clearing the remaining list. Not implemented by this coordinator. |
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
   effect state. `AddApplySkill`, periodic application (`ChkKeepSkill`), real
   `RemApplySkill` effects and production combat/world wiring remain missing.

## Milestones

| Implementation commit | Completed block | Native/original validation |
| --- | --- | --- |
| `0b9643aeb251eb2c2c23bb02da438bb6b2f748fa` | List ownership, clear and destruction | 64 scenarios / 956 original operations; 120/120 tests, 33.37 s. |
| `7ac2fe02a3f33f1d4a9949b20d2b25cbdc6f57fb` | Shared manager skill instance IDs | 64 scenarios / 1,088 original operations; 512 concurrent creations plus 512 reused; 121/121 tests, 30.59 s. |
| `8053594202a19aece9476e0926898319704e719d` | Expiration coordination and live cursor | 128 original scenarios / 1,600 operations; 122/122 tests, 41.03 s. |

## Current block validation

- `sha256sum -c SHA256SUMS`: exit 0; all 12 original EXE/PDB files match.
  Log: `work/mob-applied-skills-expiration-hashes.log`.
- Locked Ghidra `--process --output work/exports/03-mob-skills`, with JDK 21 and
  Ghidra 12.1.4: exit 0. Focused expiration/removal/clear export.
  Log: `work/mob-applied-skills-expiration-export.log`.
- New oracle generation with `.local/oracle-env/bin/python`: exit 0; 128 scenarios,
  832 admissions, 512 expiration passes and 256 clears. A second generation
  matches the fixture byte-for-byte. Generated under `work/` and copied before
  any test reads the target fixture.
- The oracle executes real expiration, list, pool and recursive clear operations.
  `RemApplySkill` is explicitly an observed required port, not recovered effects:
  modes preserve/modify payload, change a later record's deadline, clear the list,
  and clear/reappend a recycled record. It checks logging arguments and linked
  state before unlink, plus effect-call state after unlink and before recycle.
  Snapshots compare both owners, heap order/count/attempts, complete ten-word
  payloads and ordered log/removal events. Ticks include equality, zero,
  signed-boundary values, maximum and wrapped next deadlines; comparison is
  absolute unsigned, not elapsed-time subtraction.
- `python3 tools/dev.py build`: exit 0, no compiler warnings.
  Log: `work/mob-applied-skills-expiration-build.log`.
- First full native test run failed at the new fixture (121/122 passed): Unicorn
  had reused a translated callback block after external code replacement. Fixed
  the oracle to invalidate that cache and assert actual callback `AddBack`
  execution. Regenerated deterministically; native implementation was unchanged.
  Initial failure log: `work/mob-applied-skills-expiration-test.log`.
- Corrected fixture `python3 tools/dev.py test`: exit 0, **122/122 passed**,
  41.03 seconds; expiration, creation and original ownership suites all pass.
  Log: `work/mob-applied-skills-expiration-test-fixed.log`.
- New oracle `py_compile` and staged whitespace check: exit 0.

SQL contracts are unaffected; no SQL access or sanitizer run was performed for
this block. The coordinator owns complete isolated SQL integration.

Next independent block proposal: reconstruct actual `RemApplySkill` effect
reversal in this prefix using an explicit mob-state projection, starting with
state counters/speed, attribute restoration and vision-fix removal, then periodic
final damage and client delivery through required providers.
