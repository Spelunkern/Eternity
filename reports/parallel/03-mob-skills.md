# Worker 03: mob applied skill ownership

## Recovered behavior

Source: immutable `originals/ps_game/ps_game.exe`, SHA256
`0fb7c9f13b5bcf186118f8bfe73ab214226b93e8aa502b8e6fded4a8312bc547`.
Ghidra export: ignored `work/exports/03-mob-skills/ps_game.exe`.

- `CMob::ClearApplySkill` **004a0990** locks the applied-skill list, visits
  head-to-tail, conditionally logs action 6 **before** unlinking, and returns each
  record to `g_pObjectMgr->m_heapSkill` (+0x114). The free list is LIFO and shared.
  The log gate is `CMob::m_bLuaAI` (+0x284) nonzero OR a nonnull `m_pRespawn`
  (+0xc84) with `stMobRespawn::bBothMob` (+0x1c) true.
- The log arguments are skill definition name (+3), skill level (CSkill +0x10),
  and the mob's previous attacker name (+0xcf4). No `RemApplySkill`, effect
  reversal, final periodic damage, client removal packet or skill destructor is
  called by this clear operation. Payload and definition identity survive reuse.
- `SSyncHeap<CSkill>::Alloc` **0040b180** pops the free head first. Only an empty
  heap calls allocation; attempts increase even on failure. Fresh construction
  resets instance ID (+8) only (`CSkill::CSkill` **0040a0c0**).
- `SSyncList<CSkill>::AddBack` **00486200** appends a node and increments count.
  `CMob::~CMob` **0048ff10** invokes `SSyncList<CSkill>::Release` **0044bc00**,
  which deletes active nodes in list order rather than recycling them. Original
  execution confirms the deletion loop hidden by Ghidra's noreturn inference.
- `CObjectMgr::CreateSkill` **0040a780** additionally assigns a shared instance
  ID when the allocated ID is zero. The new admission API deliberately represents
  list/pool ownership, not this manager's ID assignment or `AddApplySkill`
  **004a0aa0** effect/refresh/replacement rules.

## Implementation and integration contract

`MobAppliedSkillPool` owns objects and serializes heap operations;
`MobAppliedSkills` owns each ordered active list, returns nodes on `clear`, and
permanently deletes active nodes on destruction. Pool allocation and per-mob
logging require explicit providers. No production no-op providers are supplied.
The payload projects all ten CSkill data words, borrowed definition identity and
log name. Definition storage, pool services, shared pool and owner-bound logging
must outlive their consumers. Payload access requires world-owner serialization.
Fresh undefined original fields are initialized by the explicit admission caller;
reused fields are retained. `append_from_pool` is not a combat skill application.

Required coordinator changes (canonical headers intentionally untouched):

1. Add `std::unique_ptr<MobAppliedSkills> applied_skills` to `MobRuntime`, bound
   once when `MobPool` constructs the mob, alongside its existing damage ledger.
   Add required pool/log provider access to `MobPoolServices`, or equivalent
   object-manager ownership. Every mob must share the same skill pool. Neither
   `CMob::Init` **00490040** nor mob recycling should clear this list implicitly.
2. Implement `MobDeathWorld::clear_applied_skills(mob)` as
   `mob.applied_skills->clear({mob.lua_ai != 0, mob.respawn != nullptr,
   mob.respawn && mob.respawn->both})`. Preserve death's existing damage-reset
   before skill-clear ordering. Missing ownership/provider is an error.
3. Supply owner-bound action-6 logging using the mob context, previous attacker
   string, skill name and level. The coordinator owns `SendLogBothMob` production
   delivery; the required callback is not that delivery implementation.
4. `CMob::ResetAttack` **004905c0** also calls this clear after restoring
   HP/SP/MP, base attributes and zeroing stun/stop/sleep; it then sends HP/speed
   and rebuilds attack values. The canonical `reset_mob_attack` is currently only
   an attack-stat helper also called by `initialize_mob`; do not add an
   unconditional clear to that helper (original Init retains the list). The real
   ResetAttack coordinator needs a separate path/provider.
5. Real skill admission must finish initialization (definition, ID/level,
   attacker, ticks, and effect state) and supply manager-wide instance IDs from
   recovered `CreateSkill`, preserving IDs on reuse. `AddApplySkill`, timed skill
   removal/effects and user ownership of the same original CSkill heap remain
   separate reconstruction/integration work.

## Validation

- `sha256sum -c SHA256SUMS`: exit 0, all 12 original EXE/PDB files match.
- Locked Ghidra `tools/reverse.py ps_game --process` export: exit 0, with the
  required output directory, Ghidra 12.1.4 and JDK 21.
- `.local/oracle-env/bin/python tools/mob_applied_skills_oracle.py`: exit 0;
  64 original scenarios / 956 operations (572 admissions, 256 clears,
  128 list destructions). Regenerated under `work/`; repeat generation compares
  byte-for-byte with `tests/data/mob-applied-skills-oracle.tsv`.
- `python3 tools/dev.py build`: exit 0, native C++20 full build. No warnings
  from the new source/test; existing unrelated guild sources emit warnings.
  Log: `work/mob-applied-skills-build.log`.
- `python3 tools/dev.py test`: exit 0, **120/120 tests passed**, 33.37 seconds.
  `mob-applied-skills-unit` passed, comparing every operation's complete lists,
  free order/count, allocation attempts, retained ten-word payload and definition
  identity, logging order/arguments and permanent deletion ownership.
  Log: `work/mob-applied-skills-test.log`.
- `python3 -m py_compile tools/mob_applied_skills_oracle.py` and
  `git diff --cached --check`: exit 0 before implementation commit.

No SQL code/contracts change or SQL test is needed for this ownership component;
shared isolated SQL integration remains assigned to the coordinator. Sanitizer
checks were not run by this worker. The server is not playable.

Implementation commit: `0b9643aeb251eb2c2c23bb02da438bb6b2f748fa` on `workers/03-mob-skills`.
