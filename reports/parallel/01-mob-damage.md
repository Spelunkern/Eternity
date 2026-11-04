# Worker 01: mob damage and party credit

## Evidence and implementation

Immutable `ps_game.exe` SHA256:
`0fb7c9f13b5bcf186118f8bfe73ab214226b93e8aa502b8e6fded4a8312bc547`.
All entries in `SHA256SUMS` verified successfully. Read Ghidra's complete
`work/reverse/ps_game.exe/{decompiled.c,disassembly.txt}` in the main checkout;
no shared project or original binary was modified.

Recovered `CMob::Damage` 00490cf0 / 00490ee0 and `CMob::DamageParty`
00490e40 / 00490e90. The explicit `MobCombatState` projection owns no canonical
runtime fields. Damage rejects status 2 and zero HP, increments the optional hit
counter with 32-bit wrap, subtracts unsigned HP, and credits full incoming damage
(including overkill). Lethal damage rechecks status, writes state 2, optionally
dispatches event 3, clears secondary state and schedules a 5,000/10,000 ms corpse
deadline with tick wrap. Logging precedes resource subtraction and ledger credit;
retarget follows credit only when remaining HP is nonzero and status is 1.

The triple overload preserves the original asymmetry at 00490fcc–00490fdd:
MP compares against damage[1], but subtracts damage[0]. SP compares/subtracts
signed damage[2], with 16-bit wrapping subtraction. HP comparisons are unsigned.
Aggro is base + bonus * 10 with 32-bit wrap. Party overloads credit type 5,
party identity, first damage value, zero aggro and zero membership field, without
changing resources or retargeting. Both overloads are equivalent after projecting
the pointer overload's first value.

`MobCombatServices` requires status, FSM event, clock, logging and retarget
implementations. There are no default no-op world services. Existing
`MobDamageLedger` performs actual native ownership/aggregation and selection.

## Validation

- Original execution oracle: 768 deterministic cases across all four entry points;
  executes the real coordinator instructions, observing GetStatus, FSM, clock,
  log, Add and retarget ports. Input credit is compared against native ledger
  records; Add internals remain covered by existing ownership/pool fixtures.
- Fixture regenerated twice, byte-identical (`cmp`, exit 0).
- `python3 tools/dev.py build`: exit 0, all 379 build steps completed.
  `work/mob-combat-build.log` records 19 existing guild-related warnings; none
  originate in mob combat sources.
- `python3 tools/dev.py test`: exit 0, **120/120 tests passed**, 30.01 seconds;
  `mob-combat-unit` passed its 768 original comparisons and pooled party-credit
  composition check. Log: `work/mob-combat-test.log`.
- `git diff --check`: exit 0.
- No SQL contracts or fixtures changed. SQL lifecycle and complete isolated SQL
  integration remain with the coordinator, per parallel-work instructions.

## Integration dependencies / canonical interface requests

Connect this projection to the production mob owner; no shared canonical headers
were edited. Fields evidenced in CMob: HP +224, MP +228, SP +22a, hit-enable +258,
hit-count +25c, state +124, secondary state +128, corpse deadline +cb0;
stacked FSM selector +44; definition +a8 points to the nonzero long-delay selector
at +6c. Logging is enabled by +284 or a linked owner +c84 with byte +1c nonzero.
GetStatus 0041d760 must query the actual stacked FSM when +44 is nonzero, and
otherwise the ordinary state; it cannot be replaced by an unrelated cached field.

Required providers: event 3 (StackStateMachine dispatch 004925f0),
SendLogBothMob type 2 (004a1810), GetTickCount, ReTargetByAggro (00490ab0) or
ReTargetByAggroEx (004b7490). The combat caller still owns composing attack
calculation, individual and party credit calls, death rewards and corpse removal.
This module does not make the server playable.

Implementation commit: `23a9179` (`Reconstruct mob damage and party credit coordinators`).
This report is committed separately after validation.

## Follow-up block: synchronous callback fidelity

Implementation commit: `f009e09` (`Preserve mob damage callback ordering and live state evidence`).

Added 512 original-execution cases in `mob-combat-callbacks-oracle.tsv` and a
native comparison suite. Both Damage overloads execute real original coordinator
instructions. Required service bodies remain observed ports, with explicit
synthetic mutations to audit reads/writes across callbacks; these mutations are
not claims about the real logging or clock implementations. Snapshots include
HP, MP/SP, state, secondary state, corpse deadline, delay/FSM/log selectors,
triple values, ledger record count and call argument. Coverage includes 58 event
3 dispatches, 119 clock calls, 148 logs, 252 credit additions and 132 retargets.

Recovered integration facts:

- Event 3 runs after HP becomes zero and ordinary state becomes 2, but before
  secondary-state reset, clock/log calls and insertion of this hit's credit.
  Later processing can observe HP restored by the event and still schedules the
  corpse deadline. A returned boolean therefore follows the final live HP.
- Event/log changes to resources, logging flags, status and FSM selection remain
  visible to subsequent operations. The triple damage values are reloaded after
  those boundaries; scalar damage stays captured. Logging sees resources before
  the triple's MP/SP deductions, while Add sees the resulting resources.
- `CMob::Damage` 00490cf0 selects its delay at 00490d6a/00490d6d before the
  GetTickCount call at 00490d6f/00490d82. The triple overload does the same at
  00490f5c/00490f5f before 00490f61/00490f74. Changed native code to capture the
  delay before invoking `tick()`, preserving this ordering independently of C++
  operand evaluation order.

Validation:

- All immutable executable/PDB hashes verified again (`sha256sum -c SHA256SUMS`,
  exit 0); no original/shared project changes.
- New oracle regenerated twice, byte-identical (`cmp`, exit 0).
- Before the fix: `tools/dev.py build` exit 0; `tools/dev.py test` exit 1
  (CTest exit 8), 120/121 passed. Only the new callback suite failed, at case 10:
  native deadline 9984 versus original 4984. Log:
  `work/mob-combat-callbacks-baseline-test.log`. This run was not a pass.
- After the fix: `python3 tools/dev.py build` exit 0, no compiler warnings,
  `work/mob-combat-callbacks-build.log`.
- `python3 tools/dev.py test` exit 0, **121/121 passed**, 46.96 seconds,
  including the previous 768 cases and all 512 callback cases. Log:
  `work/mob-combat-callbacks-test.log`.
- `git diff --check` exit 0. No SQL contracts changed or SQL commands run;
  final isolated SQL integration remains assigned to the coordinator.

Integration requirement: use the existing mob-owned ledger and publish projected
HP/state before dispatching event 3. An adapter that merely copies fields back at
function return would hide intermediate original state from production services.
Synchronize callback changes before subsequent coordinator steps, and preserve
live triple storage throughout the call. Do not pre-credit the fatal hit ahead
of dispatch to compensate for an assumed reward ordering. Actual event 3 and
reward behavior belongs to its owners; this block does not infer those bodies.

Remaining work: production mob/provider integration remains external to this
worker's writable scope. A next evidence candidate within mob combat is tracing
original combat callers to establish the relative order of individual Damage
and aggregate DamageParty calls. The server remains non-playable.

## Follow-up block: user-hit and party-credit caller composition

Implementation commit: `0a2c753` (`Compose recovered user mob hit and party credit call ordering`).

Added `mob_combat_attack.hpp/cpp`: an explicit user projection and required
StatusAttackStop/Death service ports compose party credit, native triple damage,
attack stop, and death notification. This is the hit segment, not complete attack
validation, damage calculation, skill execution, packets or secondary-hit guards.
No canonical headers were changed.

Read Ghidra disassembly for all six direct DamageParty references in ps_game.exe.
The original-execution oracle runs these caller slices, including real
DamageParty 00490e40 and Damage 00490ee0 bodies; Add, status, clock, FSM,
retarget, StatusAttackStop and Death are observed ports.

| Caller | Executed slice (end exclusive) | Party call | Damage call |
| --- | --- | --- | --- |
| CUser::AttackMob 0044ce10 | 0044cf04–0044cf61 | 0044cf19 | 0044cf3f |
| CUser::AttackMob 0044fd90 | 0044ff7c–0044ffe8 | 0044ff95 | 0044ffc4 |
| same, primary vector +18 | 0045009a–00450108 | 004500b3 | 004500e4 |
| same, secondary vector +24 | 004501a2–00450223 | 004501ce | 004501ff |
| same, primary vector +24 | 00450359–004503c1 | 00450374 | 0045039d |
| same, secondary vector +18 | 0045045b–004504d6 | 00450489 | 004504b2 |

The offsets +18/+24 in this table refer to caller ESP, not mob/user fields.
All six credit the party before damaging the mob. They reload the party ID after
DamageParty even when its boolean result is false, then pass user type 1, user
ID (+dc), party membership and aggro inputs to Damage. A present party with ID
zero is distinct from no party and still receives a type-5 record. Aggro is
`CUser(+1230) * 10 + skill(+64)` with 32-bit wrapping arithmetic; the skill field
is zero-extended from 16 bits, and normal AttackMob uses zero for that term.

The two secondary slices deliberately pass the PRIMARY vector to DamageParty,
but their secondary vector (skill +76/+78/+7a) to Damage. The native helper
therefore requires an explicit `party_damage`, separate from applied HP damage.
One original case credits party 77 with 99 while crediting the user's secondary
hit with 137. Do not replace this input with `damage[0]` automatically.

On a true Damage result, each caller invokes CUser::StatusAttackStop 00489ed0,
then reloads the user ID and invokes CMob::Death 004910f0 with type 1. The helper
preserves that order and ID reload. It does not directly implement either service
or infer reward completion. Earlier fatal FSM event 3 still runs before the
individual fatal credit; this caller-level Death call runs after that credit.

Validation:

- `sha256sum -c SHA256SUMS`: exit 0 for all immutable executables/PDBs.
- 768 original caller cases (128 per slice), regenerated twice byte-identically
  with `cmp`, exit 0. Each slice finishes at its declared boundary with balanced
  ESP. Final records, resources, state, timer, user ID and ordered port snapshots
  match the native composition. There are 214 observed conditional Death calls.
- Initial `tools/dev.py test` run: exit 1 (CTest exit 8), 121/122 passed,
  `work/mob-combat-attack-test.log`. Case 0 exposed an old object using the
  previous aggro argument mapping: source changes followed that object's build.
  This run was not a pass. No fixture expectation was changed to conceal it.
- Recompiled current sources with `python3 tools/dev.py build`: exit 0,
  no compiler warnings; `work/mob-combat-attack-final-build.log`.
- Final `python3 tools/dev.py test`: exit 0, **122/122 passed**, 35.56 seconds,
  including all 768 caller cases, 768 earlier damage cases and 512 callback cases.
  Log: `work/mob-combat-attack-final-test.log`.
- `git diff --check`: exit 0.

Integration requirements: supply the live attacker ID, optional party identity
and +1230 aggro bonus; call with zero base aggro for normal attacks and the
zero-extended skill +64 value for these skill slices. Supply the primary HP
amount explicitly for a secondary hit's party credit. Use the same mob-owned
ledger for pre-credit and damage; bridge required stop/death ports to real owners.
A caller must honor its original primary-result guard before a secondary hit.
No SQL contracts changed; isolated SQL integration remains with the coordinator.

Remaining work: production attack validation/calculation, packets, actual
StatusAttackStop and canonical owner/provider integration are outside the current
four-function assignment. This block supplies their recovered hit boundary.
The server remains non-playable.


Next assignment/dependency: the four assigned CMob functions and their recovered
user-hit boundary are now covered. Production integration requires canonical
mob/actor ownership changes and concrete world providers described above; those
files remain outside this worker's write scope. Proposed next scope under the
same prefix: CMob::Aggro 00491080, its positive-aggro/status/HP guards and native
ledger/retarget coordination, leaving targeting bodies with worker 08. An
alternative integration assignment is CUser::StatusAttackStop 00489ed0 plus a
coordinator-supplied canonical combat-owner interface.

## Assigned extension: CMob::Aggro and live damage ownership

Implementation commit: `ba820ce` (`Recover mob aggro accumulation and live runtime ownership bridge`).

New files use the assigned `mob_combat_aggro` prefix. Recovered ps_game.exe
CMob::Aggro **00491080** from the complete Ghidra disassembly; all immutable
executable/PDB hashes verified again with `sha256sum -c SHA256SUMS`, exit 0.
The entry receives signed amount in EAX, mob in ECX and user ID on the stack;
its RET 4 confirms that stack argument count despite the decompiler's signature.

Behavior:

- Signed amount <= 0 returns before GetStatus. Otherwise status 2 or zero HP
  rejects the call (00491083–0049109d). There is no user-ID lookup, target-present
  check, or identity-zero rejection here. Target resolution belongs to retarget.
- Adds type 1, supplied user ID, **zero damage**, supplied aggro and **party 0**
  to this mob's existing CDamageMob at +1f0 (0049109f–004910b5). Existing damage
  survives; existing user membership is cleared to zero. Same-ID type 2/type 5
  records and another mob's records remain independent. No party aggregate is
  added. Aggro accumulation wraps at 32 bits rather than saturating; selection
  still compares signed values (e.g. INT_MAX + 1 becomes INT_MIN).
- After Add, rechecks live status and calls retarget only for status 1, using
  the current +44 selector: ReTargetByAggro **00490ab0** or ReTargetByAggroEx
  **004b7490** (004910ba–004910d9). Allocation failure does not suppress this
  recheck/call. HP is not checked again after allocation.
- Aggro itself leaves MP/SP, hit count, corpse time and secondary state alone.
  These unchanged fields are asserted in both the original oracle and native
  comparisons. No damage/death/clock/log operation is introduced.

The projection entry requires status/retarget providers with no default bodies.
The MobRuntime overload directly uses its existing `damage` owner and native
`state_stack`/`status`/HP fields. It refreshes those fields at each status read,
including after allocator callbacks, and delegates actual retarget through a
required MobCombatAggroWorld. It never substitutes a temporary ledger or silently
accepts missing ownership; absent `damage` throws `invalid_argument`. Subject and
ledger storage must survive the synchronous invocation. No canonical header or
previous combat module was edited.

Original-execution coverage:

`tools/mob_combat_aggro_oracle.py` executes complete Aggro 00491080, actual
GetStatus 0041d760 (ordinary, single-scene stack and empty stack), Add 004b8dd0,
shared heap operations, Reset 004b8d10 and GetMaxAggro 004b9000. Only OS locks,
allocation and world retarget are ports. It observes 2,007 status calls,
435 new-allocation attempts and 267 retargets across 2,048 cases. Two live owners
share one heap; cases cover existing user/non-user/party records, recycled
records, failed allocations, signed boundaries, zero/maximum identities, fair
flags, record identities, and complete selection output including untouched
bytes. Synthetic allocator changes audit live status/FSM/HP reads; a synthetic
retarget provider executes real Reset to audit pool returns. These mutations
are not claims about real allocator/targeting world behavior.

The native suite replays every fixture through both the explicit projection and
the MobRuntime overload: **4,096 comparisons**. The bridge compares final state,
heap/records, selection and external allocation/retarget observations; its status
reads are internal native code, so original GetStatus-entry observations apply
only to the explicit projection run. Missing runtime ownership is also rejected.

Validation:

- Fixture regenerated twice with `.local/oracle-env/bin/python`, byte-identical
  via `cmp`, exit 0. No tracked fixture was replaced while a reader was active.
- Projection build: `python3 tools/dev.py build`, exit 0; projection/full test
  run: `python3 tools/dev.py test`, exit 0, **123/123 passed**, 31.55 seconds.
  Logs: `work/mob-combat-aggro-build.log`, `work/mob-combat-aggro-test.log`.
- After adding the actual runtime bridge: `python3 tools/dev.py build`, exit 0,
  no compiler warnings; `work/mob-combat-aggro-runtime-build.log`.
- Final `python3 tools/dev.py test`, exit 0, **123/123 passed**, 35.96 seconds;
  the Aggro suite compares all 2,048 original cases through both entries (4,096
  comparisons). Log: `work/mob-combat-aggro-runtime-test.log`.
- `git diff --check`, exit 0. All builds/tests used the shared-resource wrapper.
  Prior damage/credit/callback/caller tests remain passing.

Integration: no new canonical fields are needed for Aggro. Bind the existing
MobPool-owned ledger and connect MobCombatAggroWorld to the real normal/stacked
retarget implementations, with actual user lookup/target/FSM world services.
Do not add an early target/user-presence gate or reset the ledger on entry.
Compiled FSM ownership/transitions remain with worker 09 and targeting bodies
with worker 08. Production combat dispatch must invoke this overload; this block
does not claim such dispatch is connected. SQL contracts were not changed and
SQL lifecycle/final isolated integration remain with the coordinator. The server
remains non-playable.


Assignment boundary: Aggro 00491080, its actual ledger/helper execution and native
runtime entry are now implemented and verified. Further production progress
requires the world retarget/compiled-FSM providers and combat dispatch owned
outside this prefix. Proposed next coordinator assignment: recover
CUser::StatusAttackStop 00489ed0 under a newly assigned combat-action prefix,
then bridge the existing user-hit stop/death ports once the canonical actor owner
interface is supplied. No new scope is taken in this milestone.

## Assigned extension: CUser::StatusAttackStop / canonical Actor

Implementation/evidence commit: `4f6037a`
(`Validate original attack stop through Actor and user hit death boundary`).

Recovered the complete ps_game.exe CUser::StatusAttackStop **00489ed0–00489f0c**
from Ghidra disassembly and original execution. This function contains no CALL,
transport operation, allocator, lookup, target-release operation or world
callback. Do not invent a packet or a stop-target notification for this boundary.

For mode +142c equal to 0 or 1, **all fields are retained**, even negative +1274,
nonzero target type and stale ticks. For every other mode (including negative
values), original writes occur in this order:

1. +142c = 0 (attack mode).
2. +1528 = 0 (current target type).
3. +143c = 0 (previous attack tick).
4. +1440 = 0 (next attack tick).
5. +1274 = 1 only when the original signed value was < 1.

The signed comparison at 00489ee1 precedes these stores; its flags are consumed
at 00489f00. There is no callback between comparison and writes. The target ID
at **+152c is retained**, as are other action/casting/target-history fields.
CUser::GetTargetMob 0044c0f0 corroborates the pair: it checks +1528 at 0044c0fd
and reads +152c for lookup at 0044c10f. Clearing classification is not ownership
release, and retained ID alone must not be treated as an active target.

Read canonical Actor and actor_actions headers/source during the coordinator
freeze. `Actor::stop_attack(EffectStatus&)` already implements these exact rules.
New `mob_combat_stop(Actor&)` delegates to that existing owner rather than
introducing another attack-state implementation. No canonical source/header or
previous combat module was edited. No new Actor fields are needed:

| Original field | Existing canonical field |
| --- | --- |
| +142c | `actor.attack_state().mode` |
| +1274 | `actor.attack_state().recovery` |
| +143c/+1440 | `previous_tick` / `next_tick` |
| +1528 | `actor.status().current_target_type` |
| +152c, retained | `actor.status().current_target` |

`tools/mob_combat_stop_oracle.py` executes 512 complete standalone calls without
any function-body ports. It observes every memory write and rejects changes
outside the five recovered fields in an otherwise canary-filled 8-KiB user.
An instruction observer rejects any escape into external code. Another 512
original CUser::AttackMob slices (0044cf04–0044cf61) execute actual DamageParty,
Damage and **actual StatusAttackStop**, with required Add/status/clock/retarget/
Death ports observed. There are 174 conditional Death calls; each sees the
completed stop state. The native comparison composes the previous user-hit
coordinator with the real Actor stop and compares final state and ordered credit/
retarget/Death events. Standalone and composed cases cover 0/1 modes, negative
and signed-boundary modes/recovery, retained IDs, and wide tick values. Native
Actor scene operations are counted and must remain zero. Existing target-history,
motion and resume fields are also checked for preservation.

Validation:

- All original executable/PDB SHA256SUMS verified, exit 0.
- 1,024 fixtures generated under work/, then copied while no reader was active.
  Independent regeneration is byte-identical (`cmp`, exit 0).
- `python3 tools/dev.py build`: exit 0, no compiler warnings;
  `work/mob-combat-stop-build.log`.
- `python3 tools/dev.py test`: exit 0, **124/124 passed**, 33.73 seconds,
  including all 1,024 new Actor/stop/caller comparisons and previous milestones;
  `work/mob-combat-stop-test.log`.
- `git diff --check`: exit 0. SQL contracts unchanged; no SQL commands run.
  Final isolated SQL integration remains assigned to the coordinator.

Minimal production adapter: the existing MobCombatAttackServices::stop_attack
implementation binds the same serialized live Actor as the attacker and calls
`mob_combat_stop(actor)`. Do not replace it with `scene.stop_target_mob`, clear the
retained ID, cancel casting/movement, or emit a packet. Actor, its required scene
and the hit's user/mob/ledger projections must remain valid through the caller's
subsequent Death invocation; do not destroy/recycle the actor on this local stop.
Actor execution and callbacks remain serialized by its existing owner. Actual
Death and other combat world operations remain mandatory external providers,
not no-op implementations supplied by this module.

Assignment boundary: StatusAttackStop and its real Actor/caller composition are
verified. Production connection/actor lookup and world Death/targeting dispatch
remain outside this prefix. Proposed next assignment: recover the user attack
scheduler's guarded invocation of AttackMob/StatusAttackStop, or supply the
canonical combat-owner API so the stop/death boundary can be connected. The
server remains non-playable.

## Assigned extension: actor attack polling / normal-mob dispatch

Implementation commit: `b0f4a04` (`Recover normal mob attack polling on canonical Actor state`).

Resolved PDB-backed names through the locked Ghidra `--process` export:

- CZone::UpdateUserAttack **00421930**, 96 bytes.
- CUser::UpdateAttack **0048ca50**, 1,356 bytes.
- CUser::StatusNextAttack **0048c970**, 223 bytes.
- CUser::GetAttackSpeed **0048a460** / GetAttackSpeedNow **0048a4b0**.

Focused export is ignored under `work/exports/01-mob-damage/ps_game.exe`;
`work/mob-combat-scheduler-ghidra.log` records successful completion (exit 0).
No import or manual shared-project changes. Original executable/PDB hashes
verified again (`sha256sum -c SHA256SUMS`, exit 0).

Read canonical Actor/actor_actions source. Actor already owns attack state,
target classification/identity, stop and movement cancellation. No existing
UpdateAttack polling implementation was found there or elsewhere in canonical
game source; the new module coordinates those owners directly, without another
actor-state copy, zone list or polling process. No shared header/source edited.

This block implements **normal-mob UpdateAttack**, skip paths and failures;
it does not claim the complete skill/PvP scheduler. Required `remainder` dispatch
is explicit, with no default success/no-op: casting mode 3 after the tick guard;
normal mode's skill phase after speed/transform processing; and non-self normal
user targets after the weapon/self guards. These stage-specific providers must
supply actual missing coordination and must not re-enter UpdateAttack's prefix.
StatusNextAttack and those branches are the next independent work in this prefix.

Recovered normal-mob ordering:

1. Compare `ActorAttackState::resume_tick` (+1450) to the supplied tick as an
   unsigned absolute deadline. Future deadline returns false without callbacks;
   equality proceeds. Other inactive modes return true. No clock is sampled.
2. For mode 2, GetAttackSpeed == 255 stops and sends reason 10. Otherwise call
   RemApplySkillToMOB 00489560 before reloading phase (+1440, canonical field
   `next_tick`). Phase 1 selects normal attack; non-1/non-2 phases return true.
3. Missing weapon (+1d8) stops and sends reason 7. Self user target stops without
   a packet. Mob lookup/eligibility failure 0044cd80 also stops without a packet.
4. Range failure 0044c030 writes deadline **before** stop and reason-5 delivery.
   Failure responses are exactly 17 bytes: 0502, reason, user ID, ten zero bytes.
5. Eligible/in-range mob: test IsBothmob 0044c610, optionally copy the user's
   null-terminated name into previous_attack_user, invoke normal AttackMob
   0044ce10, then clear only byte zero of the previous user/skill strings for
   that naming branch. Retained tails survive. Query the mob's live FSM state;
   state 2 invokes the existing Actor stop, even if AttackMob already stopped it.
6. Compute the deadline from the LIVE speed after callbacks using clamped
   GetAttackSpeedNow and image table 005424e0:
   `4000,3000,2750,2500,2250,2000,1750,1500,1250,1000`. Add to the supplied tick
   with 32-bit wrapping. Cancel movement, then write motion byte (+1448) zero.
   Changes to the deadline during cancellation remain; its motion changes are
   overwritten by the later zero. A stopped actor can still receive this deadline.

Complete original UpdateAttack executions cover the implemented paths, not
interior slices. Required lookup/range/effect/attack/send providers are observed
ports; actual StatusAttackStop, GetAttackSpeedNow, CancleMove (pending move zero)
and the security-cookie return execute. There are 1,536 cases, including 519
AttackMob calls and 465 failure deliveries. Ordered snapshots at every provider
include live Actor state, speed, weapon presence, mob state and both complete
32-byte naming buffers. Callback mutation probes cover weapon/phase changes,
post-hit mode/speed changes, and cancellation/send writes. These are synthetic
boundary audits, not claims that each real provider performs those mutations.

Native comparisons use the actual Actor and MobRuntime owners. An additional
native composition invokes the previously recovered user-hit coordinator with
real pooled party/user ledger additions, lethal native damage, actual Actor stop
and an observed required Death boundary; the polling tail then preserves the
stopped actor and schedules the expected post-hit deadline. No AttackMob damage
calculation or Death reward body is replaced by a production no-op.

Validation:

- Ghidra focused `--process` export: exit 0; exact names/addresses/signatures
  confirmed in its functions.tsv. Original SHA256SUMS: exit 0.
- Oracle generated under work/ before copying; independent regeneration matches
  the tracked 1,536-case fixture byte-for-byte (`cmp`, exit 0).
- `python3 tools/dev.py build`: exit 0, no compiler warnings;
  `work/mob-combat-scheduler-build.log`.
- Initial `python3 tools/dev.py test`: exit 0, **125/125 passed**, 30.82 seconds;
  `work/mob-combat-scheduler-test.log`.
- After adding native lethal-hit composition: build exit 0, no warnings,
  `work/mob-combat-scheduler-final-build.log`; final test exit 0,
  **125/125 passed**, 39.33 seconds,
  `work/mob-combat-scheduler-final-test.log`. This includes all original frame
  comparisons and the real native hit/Actor-stop composition check.
- `git diff --check`: exit 0. Shared build/test/Ghidra locks were respected.

Production/lifetime requirements:

- CZone::UpdateUserAttack holds the user-list lock (+118), uses its shared live
  cursor (+114, head +110), calls UpdateAttack with the same supplied tick for
  each user, discards the boolean, and advances from the live cursor after each
  call. Do not break on false, snapshot the user list, restore a saved cursor or
  sample a new clock per actor. This list visitor is not reimplemented here.
- Bind one retained serialized Actor to the connection/zone owner. Keep Actor,
  scene and bindings alive through all callbacks and the final motion write.
  A returned MobRuntime must survive AttackMob (including nested Death), naming
  cleanup and the following FSM read. Deferred corpse recycling must respect
  that lifetime. Never copy stale actor/mob state back after provider callbacks.
- Weapon presence must reflect the actual +1d8 equipment owner; update the live
  binding before returning from equipment/transform callbacks. Do not guess an
  inventory slot. Existing Actor attack/target/speed fields are sufficient for
  the implemented frame; no new canonical fields requested for those values.
- Bind `attack_speed`, transform removal, enable_mob, range, naming predicate,
  actual AttackMob, cancellation and self transport to real owners. In
  particular, EnableAttackMob is not just FindMob: its original checks zone,
  lookup, non-dead status and unequal user/template country (+12d / definition
  +78). Range must preserve the original helper's position/arithmetic behavior.
- Bind cancellation to existing `Actor::cancel_move` and its actual action world;
  bind stop to the existing Actor owner. Skills/PvP require their missing live
  skill selection/target coordination; they remain mandatory remainder providers.

SQL contracts unchanged; isolated SQL integration remains coordinator-owned.
Next independent block: StatusNextAttack 0048c970 and skill-phase coordination,
then normal user eligibility/dispatch. Production zone/connection wiring remains
open. The server remains non-playable.

## Scheduler continuation: StatusNextAttack and start notification

Implementation commit: `ec6ea4c`
(`Recover live next attack continuation and start delivery ordering`).

Recovered complete ps_game.exe CUser::StatusNextAttack **0048c970–0048ca4c**,
using the preceding focused Ghidra/PDB export and full disassembly. The bool is
read from the stack byte; actor is in ESI. It invokes SendAttackStart **004826f0**
only when continuing from mode 0, before its recovery/mode/phase writes.
SendAttackStart delivers exactly **two bytes `12 02` (0212)** to self, without a
user ID or zone-presence gate. No clock, movement cancellation, lookup, damage
or target ownership operation is performed by StatusNextAttack.

New `mob_combat_scheduler_next` uses existing Actor attack/action/status owners:

- Forced stop or zero previous_tick (+143c): invoke existing StatusAttackStop
  semantics. Modes 0/1 still retain their auxiliary fields; force is not a
  request to override those original guards. Resume deadline is untouched.
- Otherwise modes 1/2 retain mode. Other modes become 2. Only initial mode 0
  emits 0212 and then raises live signed recovery (+1274) to at least 2. This
  floor is not applied to every continuation mode.
- Set next_tick (+1440, actually a phase flag here) to 1. If the live current
  target is self-user (type 1, ID equal to actor ID), restore previous target
  type/ID from +1530/+1534. Zero previous type invokes the existing stop instead.
  Restored IDs/types are not normalized or looked up, and backup fields remain.
- Self delivery can change Actor fields synchronously. The original does not
  redo its initial previous_tick/force guard afterward: it continues the captured
  branch, floors live recovery, writes mode/phase, then reads live current and
  backup targets. A callback's nested stop therefore need not prevent outer
  continuation. No state snapshot is copied back over callback changes.

Existing canonical mappings suffice: ActorAttackState mode/recovery/previous_tick/
next_tick/resume_tick, EffectStatus current_target_type/current_target, and
ActorActionState previous_target_type/previous_target. No canonical header or
other owner edited. In particular, Actor::stop_casting is NOT used as a substitute:
that separate routine cancels other actions and samples a resume clock.

Original execution fixture: 2,048 complete StatusNextAttack calls, executing
actual SendAttackStart, StatusAttackStop and security-cookie returns. Only self
transport is a port. Includes 85 actual start sends and 43 nested forced stops
executed by original instructions without nested emu_start. Synthetic send
mutations audit live mode/recovery/phase/target/backup reads; these are not claims
that real connection delivery changes those fields. Both normal and nested
returns must balance the original stack. Mode/recovery signed boundaries,
zero/nonzero previous markers, self/nonself targets and retained deadlines are
compared against real Actor state. Unused world services fail in the native probe.

A native continuation-to-normal-poll composition restores a mob target on the
same Actor, consumes the phase written by StatusNextAttack and invokes the
existing scheduler's AttackMob port once with the expected deadline. This is
coordination evidence, not a production skill/target/damage provider.

Validation:

- `sha256sum -c SHA256SUMS`: all immutable executable/PDB entries pass, exit 0.
- Oracle generated under work/, copied before readers start, and independently
  regenerated byte-identically (`cmp`, exit 0).
- `python3 tools/dev.py build`: exit 0, no compiler warnings;
  `work/mob-combat-scheduler-next-build.log`.
- `python3 tools/dev.py test`: exit 0, **126/126 passed**, 29.39 seconds,
  including all 2,048 new original comparisons and existing scheduler/hit/stop
  suites; `work/mob-combat-scheduler-next-test.log`.
- `git diff --check`: exit 0. Shared wrappers/locks respected. SQL unchanged;
  final isolated SQL integration remains coordinator-owned.

Production contract: use the same retained, serialized Actor and existing self
transport owner as UpdateAttack. Keep actor/action/status storage alive after
0212 delivery until the continuation's final writes finish. Nested synchronous
calls must see the same state; do not restore snapshots or introduce a guard
recheck after send. Current target classification is separate from retained ID;
restoration here does not acquire/release a world target. Send failure/lifetime
handling remains the real connection owner's responsibility, never a no-op send.

Remaining independent work within mob_combat_scheduler: replace the mandatory
casting/skill-after-transform remainder stages with recovered skill dispatch
that invokes this continuation at the original boundaries, then recover normal
user eligibility/dispatch. Production zone/connection/world wiring remains open.
The server remains non-playable.

## Scheduler skill phases: live selection, targeting and continuation

Implementation commit: `47c290e`
(`Recover skill and casting dispatch on live actor scheduler owners`).

Recovered the remaining skill/cast coordinator paths of ps_game.exe
CUser::UpdateAttack **0048ca50** from the same Ghidra/PDB evidence:
mode-2 phase-2 dispatch **0048cab9–0048cbc8**, casting dispatch
**0048ce23–0048cf7e**. New MobCombatSchedulerSkillWorld implements these two
remainder stages while retaining the existing Actor scheduler entry. Normal-user
attack remains a mandatory separate provider. No canonical source/header changed.

Actual required action boundaries remain explicit:

| Path | User target | Mob/area target |
| --- | --- | --- |
| mode 2, phase 2 | UseSkillUser 0044f110 | UseSkillMob 0044f2d0 |
| mode 3 | AttackUser skill overload 00451c80 | AttackMob skill overload 00450cf0 |

The module does not implement those skill bodies or duplicate damage/FSM/targeting
owners. It supplies recovered ordering around them:

- Mode-2 phase 2 arrives after the existing speed/transform prefix; casting
  arrives after the initial deadline guard only. Neither prefix is repeated.
- Missing initial selected skill invokes existing stop and reason-10 0502
  delivery, returns false, and does not run continuation/cancellation. Missing
  mob/user lookup stops and returns false without an extra failure packet.
- Selection comes from live CUser +1444 and the +ac4 slot table. Mob lookup and
  IsBothmob precede a fresh skill read for naming/action. User eligibility
  00456780 receives the INITIAL selected definition's zero-extended +32 target
  type, then the skill is reloaded after lookup for the action. A callback can
  therefore change which skill executes without rerunning the earlier eligibility
  request. No stale selected skill is cached across those boundaries.
- For named mobs, actual user name and the current skill's definition name
  (+3) are copied before action. After action only byte zero of each previous
  attack context is cleared. Full retained tails survive. Area type 4 invokes
  the mob overload with a null target and the initial selected skill. Other
  target kinds perform no skill action, but still follow the continuation tail.
- Target completion is mob live state 2 or user +142c state 1 after the action.
  Mode-2 phase 2 skips StatusNextAttack if the callback leaves the actor in mode
  3. Otherwise it continues. It does NOT recalculate the attack deadline.
- Casting always invokes the recovered StatusNextAttack, then computes the live
  speed deadline from the supplied tick. Both accepted paths cancel movement
  and finally write motion +1448 = 1, overriding callback changes to that byte
  while retaining callback deadline changes after the deadline write.
- No normal weapon or normal-range check is added to these skill paths; actual
  skill bodies retain responsibility for their own application rules.

The selected binding is a non-owning projection (target type and string view),
not another skill catalog/runtime owner. Existing canonical passive
SkillDefinition lacks these active-dispatch fields. Production must bind each
view to its actual retained CSkill and immutable definition, and map action
providers back to that same runtime owner. Do not use the mob-AI skill catalog
or guess a user skill from passive-effect IDs. A disappeared selected binding on
an otherwise required reload is explicitly rejected, not silently treated as a
successful no-op. This unsupported lifetime transition is not claimed equivalent
to undefined/invalid original world state. Overlong names that exceed retained
32-byte context buffers are rejected rather than emulating a buffer overwrite.

Original fixtures: 2,048 COMPLETE UpdateAttack entry-to-return executions over
mob, user, area and no-action target kinds. Real SetPrevAttack 0048c920,
StatusNextAttack 0048c970, SendAttackStart 004826f0, StatusAttackStop 00489ed0,
GetAttackSpeedNow, CancleMove (zero pending move), and cookie returns execute.
Effects, eligibility/lookup, skill bodies and transport are observed required
ports. There are 913 skill actions, 1,365 cancellation tails and 278 sends.
Synthetic callback selection changes, mode/speed changes, self-target restoration
and cancellation/send mutations audit the live ordering; they are not assertions
about side effects of the real provider bodies. Native comparisons use the same
actual Actor/MobRuntime/target-Actor owners and recovered continuation, with
unexpected normal attack/range/movement services rejected by the test probe.

Validation:

- Original SHA256SUMS verified, exit 0.
- Generated under work/ before copying; independent oracle regeneration matches
  byte-for-byte (`cmp`, exit 0).
- `python3 tools/dev.py build`: exit 0, no compiler warnings;
  `work/mob-combat-scheduler-skills-build.log`.
- `python3 tools/dev.py test`: exit 0, **127/127 passed**, 31.47 seconds,
  including all 2,048 new skill/cast traces and previous normal/next/hit suites;
  `work/mob-combat-scheduler-skills-test.log`.
- `git diff --check`: exit 0. Shared wrappers/locks respected; no SQL changes or
  lifecycle commands. Final isolated SQL integration remains coordinator-owned.

Production contract: instantiate the expanded world to activate these recovered
stages, supplying real active-skill selection, action and eligibility providers.
Keep actor, target actor/mob, definition/name storage and action-selected runtime
skill valid through action and subsequent context cleanup/state read. Queries
must read current selection rather than a frame-entry snapshot. Continue with
the same Actor/action owners after nested delivery; no copy-back or inferred
recovery/reset beyond the original writes. Actual user-skill slot/index binding
and connection/zone dispatch remain owner-adapter dependencies. Normal PvP
eligibility and normal AttackUser coordination are the next independent work
within mob_combat_scheduler. The server remains non-playable.

## Scheduler normal-user branch / canonical team relations

Implementation commit: `942357c`
(`Recover normal user attack scheduler using canonical team relations`).

Recovered normal-user CUser::UpdateAttack **0048cce0–0048ce20** from the full
ps_game.exe Ghidra/PDB disassembly. MobCombatSchedulerUserWorld completes the
normal-user remainder on top of the previously recovered mob/skill/cast paths.
The expanded world now supplies all UpdateAttack coordinator branches; this is
not a claim that required world/attack/connection providers are connected.

New required providers are same-zone FindUser **00415f30**, a pure live map team
flag query, user EnableAttackRange **0044bf90** (argument zero), and normal
AttackUser **00456870**. Team decisions reuse existing canonical `same_team`
(CUser::IsTeam(CUser*) **0044bdf0**), not a second relations implementation.
Read canonical Actor/ZoneUsers source; existing appearance/status/relations
fields suffice. No shared source/header or other owner changed.

Recovered behavior:

- The existing normal prefix rejects a requested self ID before lookup. This
  branch captures the requested ID, then finds the target in the requester's
  current zone. Failed lookup stops and returns false without a packet or new
  deadline.
- Ordered rejection guards after lookup: target +142c state 1; invalid visible/
  attackable flags (+5774/+5775) unless captured request equals own ID; raw shape
  +1354 == 13; actual returned target ID equals own ID; then same-team relation.
  Each rejection stops and returns false without a failure packet. Permission
  checks use the captured requested ID even if lookup changes current_target.
- Use raw Actor.status().shape, not effective vehicle/copied appearance. Reuse
  the directional team helper: source duel override, reciprocal ready guild war,
  then map-team/country fallback. Do not add a separate country early guard or
  reverse the source/target arguments. Map team query is pure state access,
  not a dispatch/notification callback.
- Range follows eligibility. There is no eligibility recheck after range returns:
  its callback may change target state to 1 and AttackUser still executes when
  range returns true. Keep the captured target object instead of resolving the
  actor's potentially changed current_target again.
- Range failure restores the supplied poll tick (the original temporarily uses
  EBP for captured target ID), samples live attack speed, writes the wrapping
  deadline, stops, sends 17-byte reason-5 0502 and returns false. It does not
  cancel movement or clear the motion byte afterward.
- Successful AttackUser is followed by target's live state-1 check and optional
  existing stop. Then write deadline using live speed and the ORIGINAL poll tick,
  cancel movement, clear motion +1448 to zero and return true. Inner attack/stop
  changes to the attacker's mode do not suppress the final scheduling tail.
  Cancellation deadline changes remain; its motion changes are overwritten.

Original fixtures execute complete UpdateAttack entry-to-return paths with real
IsTeam, StatusAttackStop, GetAttackSpeedNow, CancleMove (pending move zero) and
cookie returns. Effects, lookup, range, AttackUser and self delivery are observed
required ports. The 2,048 cases include 1,665 lookups, 909 original relation calls,
819 range tests, 408 attacks and 411 range-failure packets. Inputs cover absent,
zero/max/self/alias identities, raw target flags/state/shape, directional duel and
guild relationships, map-team modes, signed speeds and tick wrap. Synthetic
lookup/range/attack/cancel/send mutations audit the specific captured/live reads;
these are not assertions that each real provider performs those mutations.
Native tests use actual retained Actor instances and canonical `same_team`.
Unexpected mob, skill, or movement-routing services fail the test probe.

Validation:

- All immutable executable/PDB SHA256SUMS pass, exit 0.
- Fixture generated under work/ before copying and independently regenerated
  byte-identically (`cmp`, exit 0).
- `python3 tools/dev.py build`: exit 0, no compiler warnings;
  `work/mob-combat-scheduler-users-build.log`.
- `python3 tools/dev.py test`: exit 0, **128/128 passed**, 34.94 seconds,
  including all 2,048 new normal-user traces and previous scheduler branches;
  `work/mob-combat-scheduler-users-test.log`.
- `git diff --check`: exit 0. Shared wrappers/locks respected. SQL unchanged;
  final isolated SQL integration remains coordinator-owned.

Production requirements: activate the expanded world rather than a generic
remainder stub. Bind lookup to the source's retained zone/instance, not a global
or numeric-map-only registry. Keep both Actor objects valid through AttackUser
and the following target-state read, and keep the source Actor/scene valid
through cancellation and the final motion write. Do not recycle a disconnected
actor during those nested callbacks. Obtain the map's live +7c team flag from
retained zone-definition ownership. Actual user range/attack, effects, skills
and direct connection transport remain required providers. No new canonical
Actor fields are needed for this branch, and no lifecycle/packet defaults were
introduced.

Remaining independent work within this prefix: recover GetAttackSpeed 0048a460
against existing Actor effect counters to replace that external query, then
assemble the real zone/connection adapters once the canonical retained visitor
and active-skill owner interfaces are supplied. The user attack coordinator is
recovered; production routing and actual providers remain open. The server is
still non-playable.

## Scheduler speed query: bind the existing canonical effect owner

Implementation commit: `bf51740`
(`Use canonical effect speed gate directly in actor attack polling`).

Read current canonical effects.cpp/effects.hpp and Ghidra/PDB evidence for
CUser::GetAttackSpeed **0048a460** / GetAttackSpeedNow **0048a4b0**. The first
getter already exists as canonical `attack_speed(const EffectStatus&)` and is
also used by the character speed serializer. Reuse it directly inside UpdateAttack
rather than adding another getter or another effect-state owner. Removed the
MobCombatSchedulerWorld::attack_speed virtual query: production adapters no
longer supply or override this local, side-effect-free operation. Other actual
world operations remain mandatory providers. No shared owner/header edited.

Exact field mapping and behavior:

| Original | Existing EffectStatus |
| --- | --- |
| +1344 | action_block_b |
| +1340 | action_block_a |
| +1428 | casting |
| +1348 | fixed_form |
| +12ec | skills.equipment.attack_speed |

GetAttackSpeed returns 255 when ANY of the four blockers is nonzero, including
negative wrapped counters. Otherwise it clamps signed raw speed to 0..9.
Movement block +133c and movement restriction +5778 are deliberately absent from
this gate. Conflicting PDB member names do not override these assembly reads.
GetAttackSpeedNow ignores blockers and always clamps raw speed to 0..9; the
scheduler deadline already implements that separate behavior. Do not substitute
the blocked getter when calculating a post-callback deadline.

New oracle executes 2,048 pairs of complete original getters without any body
ports (4,096 getter invocations), checking stack balance, no escape to external
code and byte-identical entire user storage afterward. Every blocker mask spans
signed speed boundaries and positive/negative blocker values; unrelated movement
counters also vary. Another 1,024 complete original UpdateAttack executions use
actual speed getters, stop, zero-pending-move cancellation and cookie returns.
Only effect/lookup/range/attack/transport bodies remain ports. They include 483
attack/cancellation tails and 486 disabled-gate packets. Synthetic range changes
can introduce all four blockers after the initial gate while changing raw speed;
the current frame still calls AttackMob and computes its deadline from raw speed.
This audits coordinator timing, not the inner attack provider's eligibility body.

Native tests compare canonical getter output and the image's clamped-now result,
then actual Actor polling state/events and deadlines. Previous scheduler fixtures
are retained byte-for-byte. Their native comparisons omit only Q observations,
which represented the former external query; all other action snapshots remain
checked. Old normal-mob fixture blocked inputs now populate the real Actor effect
counter. This is an interface migration, not a changed expected world trace.

Validation:

- All immutable executable/PDB SHA256SUMS pass, exit 0.
- New fixture generated under work/ before copying; independent regeneration
  byte-identical (`cmp`, exit 0).
- `python3 tools/dev.py build`: exit 0, no compiler warnings;
  `work/mob-combat-scheduler-speed-build.log`.
- `python3 tools/dev.py test`: exit 0, **129/129 passed**, 41.52 seconds,
  including the getter pairs, 1,024 actual-speed frames and all previous
  normal/skill/cast/user/next/stop/hit suites;
  `work/mob-combat-scheduler-speed-test.log`.
- `git diff --check`: exit 0. Shared wrappers/locks respected. SQL unchanged;
  final isolated SQL integration remains coordinator-owned.

Production contract: existing effect admission/removal must update the SAME
serialized Actor.status() used by speed publication and attack polling. Remove
old world speed overrides; do not maintain a second blocker snapshot or inject
callbacks at this pure read. Retain the Actor through the existing complete
scheduler frame, including effects introduced by later callbacks. No new
canonical fields are needed for the speed integration.

Next independent integration candidate within this prefix: the existing
Actor::cancel_shape_on_attack already owns RemApplySkillToMOB 00489560, and
Actor::cancel_move owns CancleMove. Bind these actual owners in a concrete actor
world adapter with original effect/removal/transport composition rather than
creating replacement providers. Same-zone retained actor visitation and active
CSkill slot/index ownership remain coordinator API dependencies. Attack/range,
transport and full zone/connection routing still need real production bindings;
the server remains non-playable.

## Scheduler concrete Actor ownership adapter

Implementation commit: `d9bb675`
(`Bind attack polling to actual actor effect movement and self delivery owners`).

Added MobCombatSchedulerActorWorld, deriving from the complete user/skill/mob
scheduler world and existing ActorActionWorld. Its three final methods bind
actual canonical owners directly:

- RemApplySkillToMOB **00489560** -> Actor::cancel_shape_on_attack and its existing
  EffectCycle, retained definition, replica and effect-ID services.
- CancleMove **0048a010** -> Actor::cancel_move with this actual action world.
- Self delivery **004d4d20** -> Actor::send_self / the required ActorScene.

No effect/action implementation, runtime list, replica, target resolver, FSM or
transport owner is duplicated. The adapter remains abstract: real lookup/range/
attack/skills/zone configuration and ActorActionWorld services are mandatory.
Ordinary movement cancellation requires send_move_cancel, NOT ActorScene's
prioritized proximity send or grid-cell send. No shared header/source edited.

New original oracle executes 768 complete UpdateAttack calls with REAL
RemApplySkillToMOB, list unlink, SendSkillRemove **00482a70**, SendShape,
SendRunMode, shared FreeSkill **0040a7c0**, CancleMove, speed getters, stop and
cookie returns. Attack/eligibility and actual transport/OS locks are ports.
Cases include no effect, removable hide, protected transform 127 and a noncandidate
buff; shape zero/nonzero; in/out-of-zone delivery; signed movement modes,
zero/nonzero pending motion, slot 0/10/out-of-range, and DWORD deadline wrap.
Unprotected transform stat rebuild is not newly claimed by this fixture; that
path remains delegated to the existing Actor/equipment owner.

There are 124 actual effect removals/shared-pool returns, 60 grid shape sends,
103 ordinary ranged movement-cancel sends and 314 self packets. Real callbacks
during 050e can set pending movement or remove the weapon binding; the scheduler
then sees those LIVE fields after effect cleanup. Movement-send callbacks can
change deadline/motion: the former survives and the scheduler's later motion zero
wins. These are synthetic boundary probes, not assertions about real connection
side effects. Required callbacks must retain the live Actor and bindings.

The native comparison uses the actual Actor EffectCycle, ActorActionState,
effect replica and ID release boundary through the new adapter. It validates
retained movement-slot arrays, effect membership, shape/template, packet routes,
complete ordered client/view/release events and final polling state. Scene
checks confirm that effect unlink AND replica erase precede client 050e.
Actual shared pool release executes in the original oracle and its free count
must match the observed ID-release count; the native ID allocator remains the
required ActorScene service. This fixture has no party membership; party delivery
continues to use the existing owner rather than a new scheduler implementation.

Backend limitation recorded honestly: the original 0902 removal request is
captured separately, including its exact bytes. Canonical Actor's existing
EffectObserver::erase_persisted updates its DB-agent replica for later persistence;
it does not emit that immediate backend packet. Tests check equivalent erased
replica state before client removal, NOT nonexistent native wire delivery.
This module neither invents a DB-send no-op nor changes canonical persistence.
Immediate backend transport equivalence still needs coordinator-owned integration.

Validation:

- Original executable/PDB SHA256SUMS verified, exit 0.
- Fixture generated under work/ and copied before readers start; independent
  regeneration byte-identical (`cmp`, exit 0).
- `python3 tools/dev.py build`: exit 0, no compiler warnings;
  `work/mob-combat-scheduler-actor-build.log`.
- `python3 tools/dev.py test`: exit 0, **130/130 passed**, 40.28 seconds,
  including all 768 new retained-owner polling traces and previous branches;
  `work/mob-combat-scheduler-actor-test.log`.
- `git diff --check`: exit 0. Shared wrappers/locks respected. SQL unchanged;
  final isolated SQL integration remains coordinator-owned.

Production contract: ActorScene and effect-ID services outlive Actor; its
immutable definition/equipment sources outlive retained effects. The action world,
connection/zone context and target objects survive the complete synchronous
poll, including sends after effect removal and movement cancellation. Scene's
noexcept delivery/release callbacks must follow the existing failure/lifetime
contract. Do not destroy/recycle an Actor on a nested disconnect while its frame
still writes state. Publish weapon-binding changes before returning from nested
callbacks. Preserve ordinary SendView position/radius routing for movement.

Dependency/next task: scheduler coordinator branches and reusable local Actor
operations are covered. A production world cannot be instantiated correctly
without coordinator-owned (1) retained whole-zone Actor visitation using the
original shared live cursor/insertion order, (2) live active CSkill slot/index
and equipment binding access, and (3) concrete scene/connection and target/action
providers. Do not substitute an ID-sorted snapshot or a second private Actor list.
Provide those canonical interfaces, then assign the production scheduler bridge
within this prefix. No independent scheduler coordination gap is currently
identified beyond these ownership/provider dependencies. Server remains
non-playable.

## Retained whole-zone attack polling — 2026-11-03

Implementation commit: 71febf346af9f3ec3ec09d47089bbafc7a594ba0. Explicit coordinator ownership expansion
covers canonical `zone_users.hpp/cpp` on this worker branch. Main and other
workers remain untouched; existing interfaces and all previous milestones remain.
Read current main continuity/parallel notes and inspected canonical ZoneUsers and
WorldUsers before implementation. WorldUsers indexes borrowed Actor/admission
references; it cannot manufacture the required connection/scene ownership lease.

Evidence: `ps_game.exe`, `CZone::UpdateUserAttack` **00421930** (complete 96-byte
loop), `CUser::UpdateAttack` **0048ca50**, `CZone::AddUser` **00415e30**,
`RemoveUser` **00415ec0**, `FindUser` **00415f30**, complete Ghidra/PDB exports
under main `work/reverse/ps_game.exe`. UpdateUserAttack holds +118 recursive
critical section across callbacks, starts +114 at +110 head.next, supplies the
same caller tick to each UpdateAttack and ignores its bool. After the call it
reads only the LIVE shared cursor.next, never the old actor. AddUser appends;
RemoveUser rewinds a matching current cursor to its predecessor before unlink.
FindUser uses the separate ID-tree cursor (+84), not the global cursor.
A completed nested whole-zone visit leaves the global cursor at head, so the
outer visit restarts from head.next; no cursor save/restore is faithful here.

Added retained `ZoneUsers::visit_all` and borrowed `find_actor`, recursive
membership locking, stable arrival ordinal in EXISTING Member entries, and one
owner-shared global cursor. Cell movement changes cell order but preserves zone
arrival order. No Actor snapshot, ID-order polling, second Actor list/index or
private per-call cursor. Next-member selection scans the existing ID map by
arrival ordinal: O(n) per step / O(n squared) per unmutated full pass. This is a
known performance limitation, not a claim of original linked-list complexity.
Cell visitor interfaces and shared cell cursors remain intact.

`mob_combat_scheduler_zone` composes this real owner with the previously recovered
Actor scheduler; each actor receives exactly the supplied tick and a false poll
does not stop traversal. Per-actor bindings and MobCombatSchedulerActorWorld are
mandatory live service references. Nothing implements a fallback world/action
provider. The owner service must supply a nonempty shared ownership token for
Actor AND its ActorScene before visitor entry; empty retention is rejected.
Retain/context accessors must not mutate membership. Visitor callbacks may remove
current/future users, append, change cells and perform nested lookups/traversals.
No Actor/member dereference follows the visitor. Zone and services survive the
whole traversal; all Actor state still belongs to the serialized world executor.

Complete-original fixture: 256 loops, non-ID insertion order, 3/4 initial users,
ordinary/wrapped DWORD ticks, nested lookup, current/future removal, append,
cell-independent coordinate mutation, nested whole-zone traversal, removal+append,
and removal+failed lookup+append. Original AddUser/RemoveUser/FindUser execute their
actual std ID-tree insert/search/delete and list code. Only allocation/free,
OS critical sections and the per-user UpdateAttack action boundary are ports;
there is no hand-coded replacement of the zone loop. Callback trampolines invoke
original functions and preserve registers. Removed actors become inaccessible
immediately, proving no later read by the original zone traversal. Every return
checks balanced stack, released global lock, final cursor=head and actual member
count/list. Initial harness run faulted on unmapped FS:0 in the original tree's
SEH; mapping the exception-frame page fixed the fixture, without patching tree
logic. Coordinate mutation proves global independence; native cell move also
checks existing cell membership operations preserve global order.

Native comparisons check complete ordered traces, nested lock depth and final
membership. Additional composition runs the actual Actor scheduler and detaches
and releases its registry owner inside AttackMob: Actor remains alive through
scheduler deadline/motion writes, then retires exactly once. A false poll between
active/current and appended actors cannot stop the walk. A cross-thread lookup
blocks during the callback and proceeds after the whole-visit lock releases;
empty retention is rejected and subsequent visitation still succeeds.

Validation:

- All original executable/PDB SHA256SUMS verified, exit 0;
  `work/mob-combat-scheduler-zone-hashes.log`.
- Oracle regenerated independently, byte-identical to tracked fixture (`cmp`,
  exit 0); 256 complete original loops. Fixture copied before native readers.
- Initial native build passed. An early concurrent test run reached the newly
  registered target before its executable was linked: 130 passed, new target
  Not Run. This scheduling mistake is superseded by sequential final validation.
- Final `python3 tools/dev.py build`: exit 0;
  `work/mob-combat-scheduler-zone-final-build.log`. Existing unrelated warnings
  in guild_catalog/guild_sql/guild_bank/guild_bank_save remain; no warning from
  the changed/new zone and scheduler sources.
- Final sequential `python3 tools/dev.py test`: exit 0, **131/131 passed**,
  **27.70 seconds**, including all 256 zone traces, actual retained scheduler
  composition, mutex/invalid-lease probes and previous milestones;
  `work/mob-combat-scheduler-zone-final-test.log`.
- `git diff --check`: exit 0. Shared wrappers/locks used. No SQL changes or
  database lifecycle commands; final isolated SQL suite belongs to coordinator.

Production integration requirements: wire the existing ZoneUsers instance from
the actual zone update path, pass its one caller tick, and provide an aliasing
owner lease from the real retained connection/actor/scene registry. Do not create
a second membership owner. Zone/scene/action-world/connection context and skill
and equipment backing must survive the complete poll, including reentrant sends
and disconnect. Acquiring a raw WorldUsers/ZoneUsers result alone does not retain
it. Nested disconnect must detach immediately but defer destruction/recycle until
all poll leases release. Hold the zone owner through nested traversals; recursive
membership locking does not serialize arbitrary Actor state on other threads.
Target providers still retain their own live targets. Active CSkill selection,
actual equipment binding, world actions, scene/network and root world assembly
remain coordinator/provider dependencies; this patch supplies the previously
missing canonical whole-zone visitation and scheduler bridge, not those services.
No additional Actor/shared-header changes are made or requested here. Server
playability and full final isolated SQL integration remain coordinator-owned.

Next proposal: coordinator supplies retained active CSkill/equipment binding APIs
and concrete per-actor action contexts, then assigns their production scheduler
assembly. No independent remaining zone-loop coordination gap is identified.

## Live skill-slot and weapon binding evidence — 2026-11-03

Evidence/test commit: `a2c2a37` (complete original packet selection and subsequent
polls). Read current main CONTINUITY/PARALLEL_WORK, previous `71febf3/cf52e64`
and canonical Actor/skills/inventory read-only. Existing retained ZoneUsers and
all previous behavior are preserved. No shared owner/header edits, main edits,
second production Actor/skill/equipment store or substitute providers.

**Production adapter is blocked by missing canonical ownership, not completed.**
Actor has no selected active-skill index, learned runtime-slot owner or CSkill
objects. `SkillReplica` is a DB-agent cache, `PlayerSkill` is a persistence record,
and EffectCycle owns applied effects; none is the original learned CSkill owner.
`SkillDefinitions` supplies passive/effect fields (including TypeDetail), but no
original skill name or complete active eligibility/action definition. Using the
mob-AI catalog or passive IDs would hide this gap. Actor owns retained pooled
inventory privately, but its public mutable `inventory()` also permits constructor
records with no retained GroundItem/CItem. Presence in that vector therefore
cannot safely implement original pointer presence. No speculative adapter added.

Evidence sources: `ps_game.exe` complete Ghidra/PDB exports
`work/reverse/ps_game.exe/{functions.tsv,disassembly.txt,decompiled.c,types.txt}`.
New oracle executes complete **CUser::PacketPC 0046a230**, real jump-table
opcodes **0511** (user skill) / **0517** (mob skill), followed by complete
**CUser::UpdateAttack 0048ca50**. Target validation, transform cleanup, range,
action and transport services are explicit fixture ports; actual packet guards,
selection writes, status/continuation, CancelMove and security-cookie paths run.
The target port leaves an existing mob target for polling; this proves selection
publication and downstream binding, not reconstruction of ChkTargetUser/Mob.

Recovered request contract (0046a89b..0046aae4):

- Dead mode 1 rejects both. User request slot ff with nonzero DWORD at packet+3
  calls RemApplySkill 00484b10 BEFORE casting/shape/eligibility guards; ff with
  zero DWORD falls through normal validation. Casting mode 3 otherwise rejects.
- Shape must be 0 or 13. GetAttackSpeed 0048a460 ==255, byte +1434==1 or
  DWORD +1460!=0 rejects. Request byte is zero-extended and compared SIGNED
  against +ac0 count. Missing slot +ac4[index] rejects. Definition +2f must be
  nonzero; optional required WORD level +24, job bit +27[job] and grow +2d gate.
- Actual target validation is ChkTargetUser 0044e140 / ChkTargetMob 0044e6a0.
  Failure leaves phase/index alone but still calls CancelMove. Early guards do
  not cancel. Success publishes mode 2 (with existing attack-start/recovery
  ordering), phase +1440=2 and selection +1444 from the LIVE packet byte.
  A ported nested callback changing that byte from 0 to ff produces selection
  255 despite prior count=2 validation; count is not checked again. This mutation
  is a synthetic boundary probe, not a claim that real target validation edits
  packet bytes. Request packet storage must survive the complete dispatch.

Polling reads +1444 then +ac4[index] directly, with no +ac0 revalidation
(0048cab9/cabf and 0048ce2c/ce32). Forced slot 17 and 255 both dispatch their
actual runtime pointer despite count=2. A null runtime at INITIAL polling check
stops and sends 0502 reason 10. Nulling a slot later within a dispatched action
is not thereby made safe: existing scheduler reload rejects disappearing skills;
provider callbacks must preserve the current runtime/definition lifetime.
Transform cleanup can change the selected index before that first skill read.
Normal phase checks +1d8 AFTER transform cleanup (0048cbd2); null sends reason 7.
Skill/casting phases do not impose that weapon-presence guard. Fixture verifies
normal removal/initial absence, skill removal and casting absence independently.
+1d8 is equipment pointer base +1c4 plus slot **5** *4, matching appearance slot5
in `CUser::EnterDead` **004584d0** equipment projection; no weapon-type,
count or durability check replaces the pointer test. Slot5 conclusion follows
array base/stride, not an inferred weapon category.

PDB CSkill is 52 bytes: runtime ID +8, skill ID +c, level +10, slot/number +14,
attack/reset/end/previous ticks +18/+1c/+20/+24, attacker +28 and stSkillInfo* +30.
PDB learned pointer array is **256** slots; 255 is an addressable slot, not a new
catalog lookup. Definition has 32-byte name +3, eligibility fields above, and
WORD wType +32 (existing TypeDetail mapping) used in EnableAttackUser 00456780.
LearnSkill **004842e0** updates an existing runtime's +30 definition and level
in-place; new skills append pooled CreateSkill runtime at +ac4[count], set
m_nNumber=count and increment count. Runtime identity and slot number are not
interchangeable with SkillID. UseSkillMob **0044f2d0** captures the passed CSkill
pointer across eligibility; AttackMob skill overload **00450cf0** and AttackUser
skill overload **00451c80** receive the runtime, not merely a name/type view.

New fixture has **112 complete request sequences** (28 profiles x two opcodes x
two starting modes), including each guard, signed count, ff cancellation, failed
target, live packet-index publication, slot clearing, forced index, ordinary,
skill and casting polls. **48 accepted-state polls** additionally compare native
existing scheduler final state/ordered events against original. Native input is
explicitly the observed post-request projection: it DOES NOT implement PacketPC
or claim a native active-skill owner. Runtime views in the unit are test doubles
only. This is evidence beyond previous skill dispatch fixtures, not another
production skill store. Initial harness range port used wrong stack cleanup and
casting action was unported; faults corrected using existing recovered calling
conventions (range pops 8; casting action pops 4), then all complete returns
balance. No failed partial fixture was copied into tests.

Validation:

- Original executable/PDB SHA256SUMS: exit 0,
  `work/mob-combat-scheduler-bindings-hashes.log`.
- Independent oracle regeneration byte-identical (`cmp`, exit 0); generated
  under work/ then copied before native readers.
- `python3 tools/dev.py build`: exit 0, no warnings in this incremental build,
  `work/mob-combat-scheduler-bindings-build.log`.
- Sequential `python3 tools/dev.py test`: exit 0, **132/132 passed, 50.41 s**,
  `work/mob-combat-scheduler-bindings-tests.log`. Prior retained zone tests pass.
- `git diff --check`: exit 0. Shared locks respected. SQL unchanged; full final
  isolated SQL integration remains coordinator-owned. Server is not playable.

Required minimal canonical ownership/API work before a concrete adapter:

1. Expose `Actor::retained_inventory_item(bag,slot)` returning the actual retained
   GroundItem (slot5/bag0 for +1d8), null only for true absence. An unbound vector
   record must fail explicitly rather than imply runtime pointer presence.
   Actor/pool/definition lifetime covers the query; callers needing an action
   reference retain it until return. The scheduler needs only this presence test.
2. Establish Actor's actual retained learned-user CSkill owner (it is absent,
   not merely private): signed learned count, 256 stable nullable runtime slots,
   mutable DWORD selected index, pooled lifetime, per-runtime slot number/ticks
   and live user-definition reference. Expose mutable selected index and a fresh
   slot lookup preserving identity; valid forced slots are NOT gated by count
   during polling. Define explicit failure for index outside 256 (original code
   has no bounds guard; do not invent modulo/clamping). Definitions/names and
   captured runtimes survive synchronous world callbacks; disconnect defers
   recycling until retained poll/action leases end.
3. Existing `MobCombatSchedulerBindings::weapon_present` bool is an input
   projection, not a concrete live equipment adapter. Replace its production
   read with a mandatory `weapon_present(const Actor&)` query at the exact
   post-transform location, or a coordinator-approved reference to the actual
   owner-backed presence. Taking one snapshot on zone entry is insufficient.
   Skill dispatch must pass the selected actual runtime handle to action
   providers; the existing target/name view alone cannot identify it. Requery
   selection at each already recovered reload site, with retained backing,
   rather than a scratch projection overwritten during nested callbacks.

Next assignment proposal: implement/authorize the canonical Actor learned-runtime
owner and retained-equipment accessor, then assign concrete scheduler binding
integration here. Current shared headers remain read-only; independent evidence
within this binding gap is now recorded, and a fake replacement owner would
violate scope. WAITING_FOR_ASSIGNMENT

## Canonical learned-user runtime owner — 2026-11-04

Implementation: **317980f**. Read current main CONTINUITY/PARALLEL_WORK and prior
binding report/commits. Explicit expanded `user_skill_runtime` scope only; Actor
and inventory remain worker02-exclusive/read-only. No main/other worker edits,
reset/rebase/merge/push/SQL lifecycle actions. Existing ZoneUsers cursor/leases and
all earlier damage/aggro/stop/scheduler milestones remain unchanged.

Implemented `UserSkillRuntime`, the actual learned-user owner: signed count,
256 nullable stable runtime slots, mutable DWORD selection, runtime instance ID,
skill ID/level/number, attack/reset/end/previous ticks, attacker type/ID and stable
borrowed user-definition reference. This is NOT a catalog or ID-only test view.
`UserSkillDefinition` borrows existing SkillDefinition plus original-name storage
from a mandatory definition provider; it stores no private lookup table or copied
passive definitions. Runtime objects are retained handles supplied by the actual
manager service, not independently generated IDs or a per-user competing pool.

`UserSkillRuntimeWorld` binds the same Actor and reuses the recovered scheduler's
existing requery sites. It returns the actual runtime as the target/name view,
refreshes that view from its current definition, and captures a shared runtime
handle before invoking mandatory actual action providers. All four user/mob and
use/casting action overloads receive that handle, never an inferred SkillID lookup.
Lookup callbacks can change selected slots or their definition; the next recovered
reload observes them. Captured action handles survive synchronous callbacks.
Cross-Actor world binding and indices outside the 256 native slots fail explicitly;
valid forced slots ignore learned count during polling. `request_slot` separately
implements PacketPC's signed learned-count admission check. Selection publication
remains a live DWORD reference for the existing/request owner to set AFTER its
actual target validation; full native PacketPC eligibility is not newly claimed.

Original sources: `ps_game.exe` Ghidra/PDB complete exports, functions/disassembly/
decompiled/types; CSkill pool bytes additionally inspected in original code.

- **PacketUserDBSetInfo 0046d780**, complete 0504 branch **0046e166..0046e216**:
  wire header is WORD opcode, DWORD account, BYTE count; each six-byte row is
  WORD SkillID, BYTE level, BYTE slot number, WORD delay seconds. Resolve actual
  GetSkillInfo **00402770** first, then reject ID zero. Allocation failure skips
  the row and continues. Success changes only info, ID, level, number and reset
  tick=`admission_tick+delay*1000` with DWORD wrap; other pooled ticks/attacker
  fields remain stale. Write by supplied slot number and increment count per
  successful allocation, including duplicates/sparse slots; no rollback/compaction.
- Missing definition performs diagnostics and closes with **0x68/detail0**;
  zero ID also closes. Original dispatcher returns handled=true even on those
  paths. Native `UserSkillLoadResult::disconnect_requested` distinguishes that
  semantic from loaded; root must still treat the packet as handled. Prefix
  publications survive failure. Diagnostics and real disconnect are mandatory
  services, not default success/no-op. Truncated/non-0504 input throws before
  mutation; original unchecked reads are not emulated as native memory errors.
- **CreateSkill 0040a780**, **SSyncHeap<CSkill>::Alloc 0040b180** and
  **FreeSkill 0040a7c0**: actual shared heap is CObjectMgr+114, lock+120; shared
  counter is manager+10. Recycled object retains ID and payload; increment the
  shared counter only if its ID is zero. Fresh heap allocation initializes only
  instance ID before manager assignment; stale payload is not silently cleared.
- **ResetCharacter 0044a050** writes selection zero at 0044a108 and count zero
  at **0044a2c3**, BEFORE its learned free loop at 0044a3ce. That loop is skipped;
  slots remain. `reset_character()` here implements only this skill-owner portion,
  not the rest of Actor reset. It must NOT eagerly recycle/clear learned slots.
- Complete **~CUser 004496a0**, learned loop **00449720..0044974b**, directly
  deletes nonnull entries below SIGNED learned count; it does not FreeSkill them.
  Sparse entries above count and duplicate-overwritten objects are unreachable
  allocations, not reusable pool objects. Native mandatory `strand` transfers
  those handles to manager accounting without recycling; `destroy` handles actual
  destructor-range objects, deferring physical destruction across captured leases.
  This preserves observable availability while avoiding an untracked native leak.
  Malformed accumulated counts above 256 do not reproduce original out-of-bounds
  deletion; native storage stays bounded, without clamping the stored count.

Original oracle executes WHOLE admission dispatcher, CreateSkill/FreeSkill (real
shared free list, locks/counter/heap logic), UpdateAttack **0048ca50**, optional
whole ResetCharacter and whole ~CUser. Only raw allocation, OS, definition lookup,
logging, connection/crypto teardown and actual world action/effect boundaries are
ports. Empty unrelated owner trees/list sentinels are valid inputs, not skipped
instruction ranges. Deliberately seeded allocator payload makes preserved fields
observable; no claim that real fresh heap bytes always equal that fixture value.

**64 original complete sequences** cover reuse retaining ID/ticks, allocation
failures, duplicate/sparse slot255, missing definition/zero ID with surviving
prefix, WORD delay/DWORD deadline wrap, forced slot0/255 normal skill/casting
polls, reset count/index/slot retention and exact destructor deletes. Native
comparisons check all ten runtime payload fields per populated slot, count/index,
ordered lookup/action/packet/delete traces, manager ID/free counts and scheduler
final state. Additional **12 actual-owner scheduler compositions** cover user,
mob and position skill/casting routes; selection changes in lookup, live definition
replacement, reset during captured action, cross-Actor rejection and forced index
failure. A retained-lifetime probe drops the external Actor handle and verifies
UserSkillRuntime keeps the same Actor alive until owner destruction.

Validation:

- All original executable/PDB SHA256SUMS: exit0;
  `work/user-skill-runtime-hashes.log`.
- Final independent oracle regeneration byte-identical (`cmp`, exit0);
  `work/user-skill-runtime-final-repeat.tsv`. Fixtures generated under work/ and
  copied only after stopping the superseded queued test, before final readers.
- Initial build rejected a test override of Actor world's final transform method;
  corrected test to mutate selection through existing lookup callbacks. A queued
  obsolete test was terminated before execution while expanding composed fixtures.
- Final `python3 tools/dev.py build`: exit0, no compiler warnings,
  `work/user-skill-runtime-verified-build.log`.
- Final sequential `python3 tools/dev.py test`: exit0, **133/133 passed, 31.99s**,
  `work/user-skill-runtime-verified-tests.log`. Previous zone and binding tests pass.
- `git diff --check`: exit0. Shared locks respected. SQL unchanged; coordinator
  owns final isolated SQL integration and whole-server validation. Not playable.

Production attachment and required services:

1. This variant retains `shared_ptr<Actor>` directly: bind one UserSkillRuntime
   alongside the same connection-owned Actor; no global side map or second Actor
   list. Constructor rejects null. A connection/world root retains the skill
   owner and UserSkillRuntimeWorld together, and zone's aliasing retention token
   must pin that root through each entire poll. World stores an owner reference;
   destroy world before owner. ActorScene, definition/name sources and manager
   services outlive owner AND all stranded/captured runtime handles. Do NOT put
   this strong-Actor variant in an Actor-owned member, which would form a cycle.
   No Actor.hpp change is necessary for this external retained-root attachment.
2. Supply real UserSkillRuntimeServices at the shared object manager. Main's
   MobAppliedSkillPool has private acquire/recycle and a different native type;
   unsafe casts/independent IDs cannot unify it with learned-user runtime. The
   required port must use the same allocation/free-list and global counter domain
   as mob/applied CSkill, preserve payload on reuse, account stranding separately
   and distinguish direct delete from recycle. No fallback allocator is provided.
   Central common-payload/pool adaptation is required before root instantiation;
   this milestone supplies ownership/dispatch, not a counterfeit manager backend.
3. Definition provider returns stable UserSkillDefinition references backed by
   the actual user definition source/name storage. Existing SkillDefinition can
   supply effects/TypeDetail, but its loader lacks names/full active eligibility;
   extend that canonical source centrally rather than borrowing mobAI definitions.
   Real action ports still implement eligibility, costs, cooldown, targeting,
   packets/world effects; pass captured runtime identity through them.
4. Wire 0504 admission to load with the caller's captured admission tick. Route
   reset/deletion to this owner exactly once. Actual retained weapon presence is
   still blocked on worker02's inventory accessor and the previously reported
   post-transform live scheduler query; no snapshot masquerades as +1d8 here.

Next independent block within this scope: recover LearnSkill **004842e0** upgrade/
append and existing runtime reuse in-place, including points, admission failures,
publication/callback ordering, using this owner instead of constructing another
skill container. Actual shared manager/definition/root connections remain central
integration dependencies, not evidence of overall server completion.

## Learned skill admission and in-place upgrades — 2026-11-04

Implementation **c813863**. Read current main continuity/parallel notes and
`317980f/fc1a9eb` owner milestone; continued its explicit next independent block.
All code stays under `user_skill_runtime*`, private CMake and this report. Actor,
inventory, main and other workers remain untouched. Prior owner, zone cursor,
retention and scheduler implementations are preserved.

Added `UserSkillRuntime::learn(PlayerCore&, int32 ID, int32 level,
UserSkillLearnWorld&)` operating on the SAME actual retained runtime slots.
PlayerCore supplies the existing live WORD skill-point owner; mismatched Actor
identity fails explicitly. User-definition projection now also borrows original
required-level, country, six job flags, grow, points and type-show metadata. No
second skill/catalog container, passive-ID inference or competing allocator/ID
counter. All allocations use the previous mandatory shared manager port.

Original evidence, `ps_game.exe` complete Ghidra/PDB exports:
**LearnSkill 004842e0**, **EnableLearnSkill 00484080**, **SendGetSkill 00484260**,
**SetGameLogMain 0045b0d0**, **ChkGetSkillPassive 00489a40**, **CreateSkill
0040a780**, **GetSkillInfo 00402770**. Country dispatch table at **00484244**
confirms the six family predicates, rather than substituting country equality.

Recovered/implemented behavior:

- Requested levels outside signed 1..3 send 0209 reason3 immediately. Missing
  definition also sends reason3. Scan slots in physical index order below signed
  learned count, skip holes and choose first matching runtime SkillID. Same/higher
  existing SIGNED runtime level rejects with reason3; a corrupt ffffffff level
  compares as -1, not the largest unsigned level. Missing skill with requested
  level other than1 silently returns; no invented sequential-level requirement.
- EnableLearnSkill performs a SECOND live definition lookup. Missing second
  definition silently returns. Check WORD points (reason1), WORD actor level
  (reason2), then family predicate (silent reject), job/grow/type-show (reason3).
  Definition country0/1/2 permits family0/1/(0 or1), country3/4/5 permits
  family3/2/(3 or2); country>5 imposes no family check. Grow0 requires definition
  grow0; grow1 permits0/1; grow2 rejects only definition grow3; grow>=3 imposes no
  grow restriction. Type-show zero rejects, not a passive-ID substitution.
- Eligibility uses the second definition's cost, but actual point deduction
  uses the FIRST captured definition's BYTE cost with WORD wrap. Test changes the
  second lookup to cost0: initial points1 then deduct first cost7 ->65530.
  New allocation happens AFTER deduction; failure returns silently without refund,
  runtime publication, DB/client success packet or game log.
- New runtime writes info/ID/level1/number=current count, samples actual tick,
  rereads live count for insertion and increments that count. Preserves other
  pooled fields/ID. Upgrade changes the existing runtime's definition/level in
  place, samples tick and rereads physical slot before setting reset tick.
  No new allocation or ID occurs on upgrades. Clock callback resetting the skill
  owner demonstrates original old-number/new-live-insertion-index distinction.
- Call actual ChkGetSkillPassive owner AFTER publication. This required provider
  can change runtime ID/level/number/definition and points before notices. New
  notices capture those runtime ID/level/number values; upgrade notices retain
  requested ID/level but reread live runtime number. No private passive stat owner
  or default success/no-op callback is introduced.
- Actual ordered notifications: backend **0801/11 bytes** (account DWORD,
  remaining-points WORD, SkillID WORD, level BYTE), client **0209/7 bytes**
  (result0, slot BYTE, ID WORD, level BYTE), then SetGameLogMain context and
  game log **0309/110 bytes**. DB/client arguments stay captured across nested
  sends. A DB callback can change live points without rewriting an already-built
  packet. Log context runs after both sends and must observe current world state.
- Log tail uses requested ID and new level1/requested upgrade level, CURRENT
  definition name after context callback, and FIRST requested definition's cost.
  New path keeps its allocated runtime handle; upgrade rereads physical slot at
  log time. Runtime handles and definition/name backing survive callbacks.

World services are mandatory: actual clock, actual passive/stat/network owner,
backend transport, shared log-header population and game-log transport. Client
packets use the actual bound Actor::send_self. populate_log receives only shared
header bytes [2,74), leaving the recovered tail to this coordinator. Root must
populate actual current account/name/identity/time/map/position there; the test
provider is not production wiring. Existing user-definition provider must populate
metadata from stSkillInfo +24/+26/+27/+2d/+2e/+2f; zero metadata is not permission.

Validation evidence:

- New oracle executes WHOLE original LearnSkill, EnableLearnSkill, SendGetSkill,
  SetGameLogMain and CreateSkill. Initial existing objects are loaded through the
  complete 0504 dispatcher; shared pool/ID logic is actual original code. Only
  definition lookup, raw allocation/OS time/locks, actual passive callback and
  transport boundaries are ports. Original scratch stack is zeroed input; full
  110-byte log matches that controlled input, not a claim that arbitrary original
  untouched padding bytes are deterministic. Native packets zero their padding.
- **192 complete original/native comparisons**: 32 profiles x absent/matching/
  other existing skill x ordinary/wrapped tick. Cover invalid levels/definitions,
  no-downgrade, skipped levels, allocation failure after deduction, each admission
  gate, family combinations/wildcard, signed runtime level, stale field/ID
  preservation, passive/runtime mutation, backend/client/context callbacks,
  second-lookup failure/cost change and live count reset during clock callback.
  Compare every runtime payload field/definition identity, points/count/selection,
  ID counter, exact packets/logs and ordered callback-state snapshots.
- Independent fixture regeneration byte-identical (`cmp`, exit0), generated under
  work/ then copied before readers. Original executable/PDB SHA256SUMS exit0;
  `work/user-skill-runtime-learn-hashes.log`.
- First build/test passed **134/134**,42.68s with one new misleading-indentation
  warning. Braced the statement and made test-manager stranding retain objects
  according to the required manager contract; no original behavior was changed.
- Final `python3 tools/dev.py build`: exit0, no warnings,
  `work/user-skill-runtime-learn-final-build.log`.
- Final sequential `python3 tools/dev.py test`: exit0, **134/134 passed,47.19s**,
  `work/user-skill-runtime-learn-final-tests.log`, including prior actual-owner
  scheduler and zone/binding fixtures. `git diff --check`: exit0.
- Shared resource wrappers respected. SQL unchanged; coordinator owns final full
  isolated SQL integration. Test counts do not imply server playability.

Integration/lifetime requirements and limits:

The same retained connection root supplies Actor, PlayerCore and UserSkillRuntime;
keep all three plus action world alive through the whole synchronous learn/send/
log frame. Preserve the existing strong-Actor owner variant without an Actor-owned
cycle. Definition storage/names, global manager, scene and transport outlive every
captured runtime. Reentrant passive/transport callbacks must not destroy/recycle
captured runtimes or replace stable definition backing. Missing slots during a
recovered reload fail explicitly instead of dereferencing null. Out-of-range job
or append index, overlong names and int SkillIDs outside native WORD definition
space fail explicitly; original out-of-bounds memory access is not emulated.
The new function does not execute packets on arbitrary invalid owners or roll back
already-applied points/publications when real provider callbacks fail.

Central shared-manager backend, stable full user-definition source, root assembly
and live weapon-presence binding remain the previous documented dependencies.
No Actor/inventory accessor changes are needed for this block. Actual passive
service must delegate to the existing canonical effects/equipment owner; no
counterfeit DB/log sink is supplied. Native full skill-reset request/persistence
coordination remains separate from ResetCharacter's recovered skill-owner reset.

Next independent task proposal within user_skill_runtime: recover
**SendCharacterSkill 00483540** and learned cooldown serialization/restore against
this same runtime owner, preserving sparse slot/count behavior and shared tick
sampling, then connect actual admission/output paths centrally. Server objective
remains open.

## Learned client snapshot / signed cooldown output — 2026-11-04

Implementation **979e1a2**. Reviewed current main continuity/parallel notes and
`c813863/df90d11` learning milestone; completed its next independent client-output
block. Existing canonical owner/learn/scheduler/zone code is reused unchanged.
Only user_skill_runtime_snapshot files, fixture/test/tool, private CMake and this
report changed. No shared Actor/inventory/main/other worker edits.

Source: `ps_game.exe`, complete Ghidra/PDB exports, **CUser::SendCharacterSkill
00483540..004835e7**. New `user_skill_runtime_snapshot` sends actual client
**0108** (wire prefix `08 01`), using existing runtime fields directly, with no
SkillID-to-definition inference, replacement owner or skill/catalog copy.

Recovered exact behavior:

- Write opcode before GetTickCount. Sample clock exactly once, then snapshot the
  SIGNED learned count. Clock port mutations before that snapshot are visible.
- Loop by physical index, not by populated count; each index reserves an 8-byte
  row. A nonnull runtime writes low WORD ID, BYTE level, BYTE runtime number and
  DWORD remaining seconds. Null slots leave all eight existing scratch bytes
  untouched. Do not compact, synthesize an empty runtime or substitute index for
  the runtime's m_nNumber. Definitions are not read by this function.
- Remaining seconds = max(truncate_toward_zero(int32(reset_tick-now))/1000,0),
  with DWORD-wrapped subtraction. This follows original signed IMUL/SAR rounding,
  not an unsigned deadline comparison or ceiling. No definition reset-time cap.
- Header count/length use the LOW BYTE of loop count. At signed count256 all
  256 rows are processed but header becomes zero and only the three-byte header
  is transmitted. Count0 sends that header too; no early-return approximation.
- Build complete packet before synchronous Actor::send_self; no Actor/runtime
  dereference follows its callback. Retain root/owner/Actor/scene for the frame.

The API requires a caller-initialized 2051-byte scratch span and a mandatory clock
provider. This explicitly represents original preexisting stack input for holes;
it never reads uninitialized native memory or invents a default zero-hole policy.
Scratch is packet workspace, not a second skill store. Root chooses/initializes
its workspace under its protocol policy. Count>256 throws rather than reproducing
original stack overwrite; negative count does not iterate and sends count0.
Negative-count corruption is not manufactured via a new public owner mutator.

Oracle executes WHOLE **PacketUserDBSetInfo 0046d780** 0504 admission, actual
**CreateSkill 0040a780** / shared heap / ID counter and WHOLE SendCharacterSkill.
Only definition lookup, raw allocator/OS/clock and transport are fixture ports.
**192 complete original/native comparisons** cover counts0/1/4/16/255/256,
contiguous/permuted/only-slot255/duplicate slots, four caller ticks, two initialized
scratch patterns, DWORD subtraction/sign boundaries, subsecond rounding and
low-width field truncation. Compare exact packet bytes, clock-before-count order,
final count/selection and FNV of ALL 2051 workspace bytes, including rows processed
but not sent at count256. Synthetic clock/send callbacks reset count/selection to
probe ordering/lifetime; this does not assert that real OS GetTickCount reenters.

Validation:

- Original executable/PDB SHA256SUMS exit0;
  `work/user-skill-runtime-snapshot-hashes.log`.
- Independent oracle regeneration byte-identical (`cmp`, exit0); generated in
  work/ and copied before readers. No original binary modification.
- Initial build failed on test token `0x1fe+i` (C++ pp-number lexing). Corrected
  whitespace; initial attempt is not claimed passed.
- Final `python3 tools/dev.py build`: exit0, no warnings,
  `work/user-skill-runtime-snapshot-final-build.log`.
- Final sequential `python3 tools/dev.py test`: exit0, **135/135 passed,28.70s**,
  `work/user-skill-runtime-snapshot-final-tests.log`, including all prior owner,
  learning, scheduler and retained-zone comparisons. `git diff --check`: exit0.
- Shared resource wrappers respected. SQL unchanged; coordinator retains final
  full isolated SQL integration. This is not evidence of server playability.

Production connections: invoke this function in actual character-output/admission
ordering, with retained root, real clock and initialized scratch; it delivers via
existing ActorScene transport. No Actor/shared header changes required. Shared
CSkill manager backend, full user-definition/name source and root world assembly
remain previous documented dependencies; no fake providers added here.

Next independent block: learned persistence phase of **SendDBAgentCharGetInfo
0046c980**, **0046cab9..0046cb63**. Its backend0804 uses unsigned absolute deadline
comparison, integer seconds capped by original definition WORD reset time +4a,
and three-byte physical rows. It must not reuse this client's signed/unbounded
DWORD formula. Complete original surrounding function and actual backend owner
must be recovered before composing that phase; existing DB-agent SkillReplica
save owner is not duplicated. Full cooldown persistence/restore equivalence is
not newly claimed. Overall server objective remains open.

## Learned cooldown persistence / existing cache round trip — 2026-11-04

Implementation **a6d1d1c**. Reviewed current main continuity/parallel notes and
`979e1a2/d33f9a9`; completed its next learned-persistence block. Reused canonical
UserSkillRuntime and existing DB-agent SkillReplica; no second game skill store,
replacement cache, SQL procedure owner or per-user ID generator. Only owned
user_skill_runtime files, private CMake and this report changed. Shared Actor,
inventory, main and other workers remain untouched.

Original evidence: `ps_game.exe` complete Ghidra/PDB exports,
**CUser::SendDBAgentCharGetInfo 0046c980..0046cef4**, learned phase
**0046cab9..0046cb63**, stSkillInfo WORD reset time **+4a**. Existing DB-agent
**PacketUserSkill 00406100** / SkillReplica::apply already owns receipt of0804;
this milestone composes that owner rather than implementing another receiver.

Implemented `user_skill_runtime_persist` as ONLY the learned phase:

- Require real clock and DB transport, plus initialized 775-byte caller packet
  scratch. Sample clock BEFORE opcode/account/count reads, unlike client0108.
  Capture live Actor account and signed count after that callback. Return the same
  sampled tick so the enclosing root can reuse it for later effect/quest phases.
- Loop by physical index below signed count; null slot rows remain untouched.
  Each populated row writes BYTE runtime number (not physical index) and WORD
  delay. Use absolute UNSIGNED `reset_tick > now`; otherwise delay0 without
  dereferencing the definition. Future delay is integer `(reset_tick-now)/1000`,
  capped by the retained definition WORD reset_seconds. No ceiling, signed delta
  or client0108 formula. An invalid future definition fails explicitly.
- Read LIVE count again at tail, take its low BYTE and send backend **0804**
  (wire `04 08`), DWORD account, BYTE count, then count*3 bytes. Low BYTE zero
  sends nothing: count256 processes all rows but emits NO packet. This differs
  from client0108's three-byte header at256. Do not clamp/compact count or holes.
  Signed iteration and low-byte tail count remain distinct even on malformed
  negative count; fixtures do not introduce a corruption-only public setter.
- Required initialized scratch preserves caller-provided bytes in holes without
  uninitialized native reads or a fabricated zero-row policy. Count>256 fails
  instead of reproducing original stack overwrite. No owner/Actor/runtime read
  follows synchronous DB delivery. Existing retention/serialized executor contract
  applies throughout the call, including clock and transport callbacks.

The user-definition projection gains reset_seconds, backed by actual stSkillInfo
+4a; central definition provider must populate it, not infer it from passive IDs,
client seconds or mobAI definitions. Zero is a legitimate original cap. No new
catalog ownership or lookup table was added.

Original execution fixture runs WHOLE SendDBAgentCharGetInfo, not a patched-out
instruction range. Resources0606 and position0607 are emitted first; equipment,
applied-effect, quest and quickslot owners are valid empty inputs. Shared-list
heads/locks are initialized, and full stack probe/security-cookie/return execute.
It checks surrounding output is exactly0606 then0607 for those inputs. Captures
learned workspace at **0046cb63**, then still executes the remaining whole body:
that checkpoint is necessary because later phase headers reuse/overwrite part
of the original stack workspace. Native comparison is ONLY learned phase; it
DOES NOT claim native whole-character resources/position/effect/quest output.

**192 complete original/native phase comparisons** cover counts0/1/4/16/255/256,
contiguous/permuted/slot255/duplicate placement, two initialized scratch patterns,
four ticks and signed-boundary/wrapped deadlines. Definition caps0/1/7/65535,
nullable expired definitions, low-byte runtime numbers, clock-reset count/account
mutation and DB-send reset/account mutation are included. Exact packet bytes,
clock-before-header order, final owner/account state and ALL775 workspace bytes'
FNV agree, including processed-but-unsent rows. Mutations are synthetic boundary
probes, not assertions that OS time itself resets the user.

**116 additional native compositions** deliver each emitted0804 to the actual
existing SkillReplica. Verify its indexed last-row-wins delay updates and ignored
out-of-range numbers, including caller-initialized hole rows. Then retire the old
game owner, serialize that cache's learned rows into0504, restore through actual
UserSkillRuntime::load and check WORD-delay/DWORD-deadline wrap. Never run two
production game owners for the same Actor concurrently. These compositions reuse
previously recovered DB-agent/admission implementations; no new original
ps_dbAgent execution or full SQL round-trip is claimed by this fixture.

Validation:

- Original executable/PDB SHA256SUMS exit0;
  `work/user-skill-runtime-persist-hashes.log`.
- Independent oracle regeneration byte-identical (`cmp`, exit0), generated under
  work/ and copied before readers. Original binaries unchanged.
- `python3 tools/dev.py build`: exit0, no warnings,
  `work/user-skill-runtime-persist-build.log`.
- Sequential `python3 tools/dev.py test`: exit0, **136/136 passed,46.85s**,
  `work/user-skill-runtime-persist-tests.log`, including all prior owner/learn/
  snapshot/scheduler/zone fixtures. `git diff --check`: exit0.
- Shared-resource wrappers respected. SQL unchanged; coordinator owns full final
  isolated SQL integration. No gameplay-completion inference from test counts.

Production boundary: root must first execute the ACTUAL resource0606, position0607
and changed-equipment-quality070d owners, then call this learned phase, then use
its returned tick for actual applied-effect/quest/quickslot phases. Those empty
fixture owners are not no-op production defaults. Supply real DB-agent routing
for the captured account and retain root/Actor/skill/scene/definitions/manager
through callbacks. Existing DB-agent SkillReplica receives0804 and its existing
SkillStore persists the cache inside the existing whole-character transaction;
do not bypass either with SQL writes from the game owner.

Shared CSkill manager adaptation, full stable user-definition source, root save/
admission assembly and live weapon binding remain documented central dependencies.
No new Actor/shared-header patch is requested here. Next independent proposal:
recover runtime cooldown/ready checks in **EnableUseSkillUser 0044eb40** and
**EnableUseSkillMob 0044ee50**, composing existing Actor resource/target providers
rather than duplicating skill damage or effects. Full server objective remains open.

## Runtime readiness suffix after cost/equipment — 2026-11-04

Implementation **ed28b5b**. Read current main continuity/parallel notes and prior
`a6d1d1c/4a0a2cc`. Continued its next runtime validation block. Existing actual
UserSkillRuntime/handles/Actor state are reused; no second skill/target/resource
owner, ID generator, global side map or catalog. Actor/inventory/main remain
read-only. Changes confined to user_skill_runtime*, private CMake and this report.

Source: `ps_game.exe` complete Ghidra/PDB exports, WHOLE
**EnableUseSkillUser 0044eb40**, **EnableUseSkillMob 0044ee50**,
**SendUseSkillError(CSkill*) 0044ea30**. Native functions are explicitly named
`*_after_equipment`: implement ONLY suffixes starting **0044ece3** / **0044ef5d**,
not complete native eligibility. Caller MUST first execute the actual resource/
equipment prefix and pass the originally captured retained definition. Cost
arithmetic, shield/weapon compatibility, spending resources and use/attack effects
are not silently replaced by success/no-op providers or claimed implemented here.

Recovered behavior:

- Capture actual runtime handle and original definition; runtime/definition/Actor/
  scene/world/target backing survives complete synchronous validation callbacks.
  Positive Actor SkillStatus.cool_time multiplies by1000 with DWORD wrap. If
  reduction>=runtime reset tick, bypass time comparison. Otherwise compare
  unsigned adjusted deadline against CALLER tick. Negative reduction ignored.
  Future deadline sends reason9 (user0511/mob0517), returns false.
- stSkillInfo WORD **+52** required skill does NOT mean merely learned. Scan
  physical slots below signed count, skip holes, choose FIRST matching SkillID;
  no match sends reason8 with NO fresh clock sample. On match GetTickCount samples
  a NEW tick, then rereads that physical slot even if callback reset count to0.
  Required runtime reset tick must be unsigned GREATER than fresh tick to pass.
  An expired/equal first match fails even if a later duplicate is still future.
- Check original captured definition BYTE **+30** attack kind against existing
  Actor.status.skills.attack_disabled. Mapping is corroborated by original
  SetSkillAbility **00486050/0048605a/00486064**, offsets+13a4/+13d0/+13fc with
  44-byte stride, and canonical skill ability types32/33/34. Nonzero (including
  negative) fails reason10. Native rejects attack kinds outside its actual three
  status groups rather than reading arbitrary memory; fixtures exercise kind1.
- Nonnull target calls actual range provider with original definition BYTE
  **+4c**; no target bypasses only range. False returns reason5. User and mob
  range overloads remain distinct mandatory providers, not generic distance math.
- Error packets are exact20-byte0500-family actions: actor DWORD, zero target,
  live runtime ID WORD/level BYTE, zero resource words. Prerequisite reason8 and
  blocked reason10 use **0511 even in the mob overload**. Mob cooldown9/range5
  use0517. Range/clock callbacks can change runtime ID/level before packet capture;
  self-send callbacks can change owner state after it. No state is rewritten from
  an earlier snapshot. Validation itself does not spend costs/set cooldown ticks.

UserSkillDefinition now carries only the missing borrowed metadata required_skill,
attack_kind and attack_range. No ID lookup substitutes a runtime or its original
captured definition. Required UserSkillReadyWorld supplies real fresh clock and
user/mob range operations. Missing prerequisite slot after clock callback fails
explicitly. Stable definition storage remains required even if a runtime changes
its current definition during callbacks; the suffix intentionally uses the
original definition captured by the enclosing eligibility frame.

Evidence/validation:

- Oracle executes WHOLE both original overloads, actual error helper, original
  0504 admission/shared CreateSkill logic and balanced cookie/return paths. Actual
  original resource/weapon prefix executes with valid inputs: zero costs,
  SP/MP100000 and a retained compatible weapon. It is NOT skipped or replaced by
  a hand-coded prefix. Native comparison is ONLY the supported suffix from those
  inputs; resource-rejection/weapon-rejection equivalence is NOT newly claimed.
- **256 original/native comparisons**: both overloads,16 profiles,four caller
  ticks,baseline/reentrant callback passes. Cover positive/negative/wrapped
  cooldown reduction, unsigned deadline wrap, missing/expired/future required
  skill, fresh-vs-caller tick distinction, first duplicate, clock mutation of
  required deadline, count reset with physical-slot reread, blocked group,
  range rejection/null target and live ID/level changes during clock/range/send.
  Compare bool, exact ordered packets/clock/range state snapshots and final
  owner count/selection/runtime fields. Boundary callback mutations are synthetic
  probes, not claims that real OS GetTickCount resets users.
- Independent fixture regeneration byte-identical (`cmp`,exit0); generated in
  work/ and copied before readers. All original executable/PDB SHA256SUMS exit0;
  `work/user-skill-runtime-ready-hashes.log`.
- `python3 tools/dev.py build`: exit0,no warnings;
  `work/user-skill-runtime-ready-build.log`.
- Sequential `python3 tools/dev.py test`: exit0,**137/137 passed,44.18s**;
  `work/user-skill-runtime-ready-tests.log`. Prior owner/learn/snapshot/persist/
  scheduler/zone tests pass. `git diff --check`: exit0.
- Shared wrappers respected. No SQL changes/lifecycle actions; coordinator owns
  final full isolated SQL integration. Server playability not inferred.

Canonical dependency update: main Actor.hpp NOW exposes
`retained_inventory_item(bag,slot) -> optional<ActorInventoryItem>` with actual
GroundItem*/pool*, plus inventory_slot_empty. This supersedes older reports that
said the accessor did not exist centrally. This worker's older checkout does not
have it, and exclusive Actor/inventory ownership remains02; no unsafe header copy,
merge/reset or ignored-snapshot production fallback added. The eventual actual
weapon/shield prefix should use that real API: bag0/slot5 maps+1d8, slot6 maps+1dc.
Borrow expires on release/reuse; retain Actor/definition/pool and revalidate item
identity across callbacks. A value-only inventory record must not imply pointer
presence. No additional canonical field is requested for this suffix.

Remaining production work: bind actual resource/equipment prefix before invoking
these helpers; wire them into actual retained runtime action providers. Cost
prefix uses x87 reduction/truncation and signed SP/MP comparisons; its numeric
semantics/definition weapon-table indexing require dedicated original execution
before native choices, not an assumed integer-percent helper. Actual shared
CSkill manager, full user definitions, root assembly and remaining action logic
remain central dependencies. Next independent proposal: recover that cost prefix
with controlled x87 precision/overflow fixtures, then actual retained-equipment
adapter once the coordinator supplies the canonical accessor dependency. Full
native EnableUseSkill and gameplay completion remain open.

## CRT53 resource-cost prefix — 2026-11-04

Implementation **165eb33**. Read current main continuity/parallel notes and prior
`ed28b5b/c11c253`; continued its resource-prefix evidence task. Existing Actor
resources, SkillStatus modifiers and actual UserSkillRuntime handles are reused.
No Actor/inventory/main/other-worker edits, shadow resource store, catalog, pool
or ID generator. Only user_skill_runtime files/private CMake/report changed.

Original evidence: `ps_game.exe` Ghidra/PDB symbols and complete exports, original
code bytes for CRT functions not decompiled in the export. **mainCRTStartup
00505125** pushes1 at **0050525f**, calls **_cinit 0050a6c7** at00505261.
_cinit dispatches original pointer **[005405c8]=00506081 (_fpmath)**, whose true
argument calls **_setdefaultprecision 0051136e** -> **_controlfp 0051a397** with
precision53 selection. Oracle executes the ACTUAL precision helper from reset
control037f and verifies PC53/nearest (observed023f). This resolves the previous
precision question; no assumed integer-percent formula or host extended default.

Implemented `user_skill_runtime_resource_cost` and
`user_skill_runtime_check_resources`: ONLY SP/MP availability prefix of
**EnableUseSkillUser 0044eb40** / **EnableUseSkillMob 0044ee50**. User-definition
projection now carries borrowed original WORD SP/MP costs **+44/+46**. Modifiers
are actual Actor.status.skills.sp_cost_reduction/mp_cost_reduction, confirmed by
**SetSkillAbility 00485f29/00485f33** (+127c/+1278). Available raw resources are
Actor.resources().sp/mp, interpreted SIGNED as original +1240/+123c comparisons.

Exact arithmetic: FILD then FSTP float32 each operand; perform cost/100 followed
by multiplication at precision53; FIST toward zero for discount; wrapped DWORD
cost-minus-discount then signed comparison. C++ uses explicit float32 operands,
separate volatile double stages, defined INT_MIN for integer-conversion overflow
and bit-cast DWORD wrap. No clamp of negative modifiers, percentages>100 or
negative resulting requirements. Original cost29/reduction100 requires **1**, not
0: precision53 product truncates below29. Values beyond float32 integer precision
and FIST range follow original bits, including wrapped negative requirements.

Check SP first, then MP only if SP passes. Failure sends exact20-byte reason6
(0511 user /0517 mob), current Actor ID and captured actual runtime ID/level,
zero target/resource payloads. Client delivery uses Actor::send_self. No resources
are spent or changed, no clock is sampled and no weapon/range/cooldown gate is
bypassed. Caller must still execute the real equipment middle before invoking the
previous readiness suffix. This is NOT full native EnableUseSkill/UseSkill.

The numeric helper requires host FE_TONEAREST, matching recovered default mode,
and fails explicitly otherwise without changing caller rounding control. Tests
probe FE_DOWNWARD rejection/restoration. Alternative original precision/rounding
modes and exact x87 exception/status-flag reproduction are NOT claimed; explicit
FIST-overflow materialization matches numeric bits but does not emulate its
masked floating exception flag. Canonical build uses ordinary C++20 FP settings,
not fast-math/contraction that would change the staged operations.

Validation:

- **4096 original/native cases**, including complete cross-product of10 boundary
  costs x15 boundary modifiers, both overloads, mixed SP/MP/resource values and
 2048 deterministic random cost/modifier pairs. Per case execute WHOLE original
  overload once at max signed resources to observe both exact costs, then again
  at candidate resources to compare bool/error bytes: **8192 complete original
  overload calls**, plus actual CRT setup. Original equipment compatible, no
  required skill/block/target: remaining original gates execute valid paths, not
  skipped instructions. Native comparison is ONLY resource prefix. Native tests
  load actual runtime owner, compare8192 scalar costs,4096 outcomes/exact packets,
  unchanged resources and rounding control. Original restores its control word
  after every complete call.
- Fixture regenerated independently byte-identical (`cmp`,exit0), generated in
  work/ then copied while no target reader ran. All original executable/PDB
  SHA256SUMS exit0; `work/user-skill-runtime-cost-hashes.log`.
- Build exit0/no warnings; `work/user-skill-runtime-cost-build.log`. Final
  `python3 tools/dev.py build` exit0/no work,
  `work/user-skill-runtime-cost-final-build.log`.
- Initial matrix suite **138/138**,38.30s; after explicit full boundary cross
  product, suite **137/138**,42.37s with unrelated world-network-unit failure:
  `Replacement disconnect leaked registry state`. Cost unit passed. Log preserved
  at `work/user-skill-runtime-cost-final-tests.log`; no network source modified.
- Bounded recheck was requested through wrapper with CTest filter/repeat options,
  but tools/dev.py does not forward arguments for action=test (only action=run).
  It actually ran FULL suite **138/138**,35.16s, not three focused repetitions;
  `work/user-skill-runtime-cost-network-repeat.log`. Following full verification
  also passed **138/138**,**34.27s**;
  `work/user-skill-runtime-cost-verified-tests.log`.
- Network failure is not claimed fixed. Read-only inspection finds assertion
  immediately after replacement.closed() at tests/world_network_unit.cpp:78;
  a teardown-order race is a hypothesis for coordinator audit, not proven cause.
  The isolated cost helper has no static initialization/global FP-control writes.
- `git diff --check` exit0. Shared locks respected. SQL unchanged; full isolated
  SQL integration/final server checks remain coordinator-owned. No gameplay claim.

Integration: bind real full user-definition costs, retain Actor/PlayerCore/runtime/
scene/definition through synchronous validation/error delivery, and call this
prefix BEFORE actual retained-equipment checks and readiness suffix. Existing
main retained_inventory_item API closes the old accessor absence centrally;
worker checkout is older and shared Actor/inventory remain02-exclusive. No raw
header copy/merge or ignored-snapshot dependency was introduced. Actual weapon
kind/table indexing, actual cost spending and action/tick publication remain open;
do not equate successful availability check with casting or effect execution.
Shared CSkill manager/full definition/root providers remain explicit dependencies.

Next independent proposal: recover actual runtime tick/definition capture in
**AttackMob(CSkill*) 00450cf0** / **AttackUser(CSkill*) 00451c80**, composing real
existing damage/effect services; no duplicate damage or resource owner. Concrete
retained equipment middle should follow after canonical accessor dependency is
available in the integration base. Overall server objective remains open.

## Resolved runtime attacks and actual casting adapter — 2026-11-04

Implementation **d929764**. Read current main continuity/parallel notes and
`165eb33/2bd193e`; completed its proposed resolved-attack runtime block. Existing
UserSkillRuntime/handles, Actor state/speed delivery and scheduler are reused.
Only user_skill_runtime*, private CMake and report changed. No Actor/inventory/
main/other-worker edits, shadow runtime/catalog/target owner or ID generator.

Original `ps_game.exe` complete Ghidra/PDB evidence:
**AttackMob(CSkill*) 00450cf0..00450def**, **AttackUser(CSkill*)
00451c80..00451d6c**, user dispatch table **00451d70**, **SendUseSPMP 00481ca0**,
**SendSpeed 004827b0**, **GetAttackSpeed 0048a460**, **GetMoveSpeed 0048a4d0**.

Implemented behavior:

- Capture the ACTUAL runtime and its original definition. Publish reset tick
  BEFORE effects. Nonpositive Actor SkillStatus.cool_time -> caller_tick +
  definition reset_seconds*1000. ANY positive cool_time, independent of amount,
  sets reset tick to caller_tick when WORD reset_seconds<=**500**; >500 retains
  full seconds*1000. DWORD wrap preserved. No guessed subtract/clamp formula.
- Mob effect1/target3 delegates attack-select00450e20; target7 attack-range
 00450f70. Effect3/target3 delegates debuff0044fd90 with BOTH original bool flags
  true; target6 around00451980 with NO mob target argument; target7 debuff-range
 00451570. Other combinations skip effects but still execute cost/disable tail.
- User effects1..6 delegate their EXISTING owner boundaries: SkillAttack00451d90,
  SkillHeal00452b50, SkillDebuff00452bf0, SkillBuff00453470, SkillField004534e0,
  SkillOnce00453610. Other values skip effect callback, not the remaining tail.
  These are required effect services, not reconstructed damage implementations.
- After effect callback, reread SP/MP costs from ORIGINAL captured definition.
  If either nonzero, require actual SendUseSPMP service with captured WORD costs.
  Runtime->definition may now point elsewhere; it does not replace frame info.
  No availability-check substitute, refund or resource-owner copy is introduced.
- After spending, test live captured definition disable_seconds WORD **+50**.
  If nonzero, sample a FRESH real tick, reread that WORD AFTER clock callback,
  publish caller-independent deadline then call actual Actor::speed_changed.
  Even if callback changes disable_seconds to0, publication/send still occurs.
  Source +1428 is ALREADY canonical `Actor.status().casting`: corroborated by
  identical nonzero guards in both original speed getters and existing
  effects.cpp attack_speed/move_speed. Store DWORD bits using int32 bit-cast;
  no new Actor field or transient private deadline introduced.
- Existing Actor speed owner suppresses during initialization/out-of-zone and
  emits its actual speed packet otherwise. Clock writes are overwritten by later
  deadline publication; nested speed delivery may override deadline/runtime
  reset tick afterward, and this coordinator does not restore earlier values.

`UserSkillRuntimeAttackWorld` concretely binds the existing retained runtime
scheduler's final casting action methods to these coordinators, preserving the
captured shared handle and original Actor identity. Its UseSkill preparation,
normal actions, targets and remaining world providers remain ABSTRACT. No
fallback action world is added. Eligibility/resource/weapon checks must precede
these resolved attacks via their actual callers; this is not full UseSkill.
User definition projection gains only target_kind BYTE **+56** and disable_seconds
WORD **+50**, distinct from existing WORD TypeDetail/target_type.

Validation:

- **768 complete original resolved-attack comparisons**: both overloads,effects
 0..7,target kinds3/6/7/0,reset0/1/499/500/501/65535,nonpositive/positive cooldown
  modifiers, ordinary/wrapped ticks, costs0/nonzero, disable and original zone/
  initialization suppression. Mutations independently replace runtime definition,
  overwrite runtime reset, change original definition costs/disable, change
  disable during clock and overwrite published state during speed callback.
  Compare complete ordered effect/spend/clock/view snapshots and final fields.
- Oracle executes WHOLE both Attack functions and actual SendSpeed/GetSpeed
  helpers. Actual effect, SendUseSPMP, OS time and PSendView are ports only;
  this does NOT claim their damage/resource bodies newly executed. Every effect
  hook checks exact actor/target/captured-definition calling convention and
  debuff true/true flags. Around has no target. Speed route checks actual zone,
  position,radius60,source ID,channel5 and complete8-byte packet. Native compares
  existing Actor's packet/broadcast boundary; production Scene must route it.
- **8 additional native compositions** run actual scheduler mode3 for user/mob,
  reset7/501 and cooldown flag0/1, through the new final adapter, actual runtime
  capture and resolved coordinator. Check reset publication and scheduler
  mode/deadline/motion. These composition tests are separate from the direct
  original Attack fixtures; no new whole-original combined UpdateAttack claim.
- Original executable/PDB SHA256SUMS exit0;
  `work/user-skill-runtime-attack-hashes.log`. Independent oracle regeneration
  byte-identical, including final calling-convention assertions (`cmp`,exit0);
  `work/user-skill-runtime-attack-final-repeat.tsv`. Fixtures generated under work/
  and copied before readers; original binaries unchanged.
- `python3 tools/dev.py build`: exit0,no warnings;
  `work/user-skill-runtime-attack-build.log`.
- Sequential `python3 tools/dev.py test`: exit0,**139/139 passed,44.26s**;
  `work/user-skill-runtime-attack-tests.log`. Previous cost/ready/persist/snapshot/
  learn/owner/scheduler/zone tests pass. `git diff --check`: exit0.
- Shared wrappers respected. SQL unchanged; coordinator owns final full isolated
  SQL integration and whole-server validation. No gameplay completion claim.

Production/lifetime contract: retain owner/Actor/scene, targets, action services
and original definition/name backing for ENTIRE frame. Retaining runtime alone
is insufficient for a definition swapped out during nested effects; catalog/root
backing must survive. Services must execute actual enum-specific effect owners
(including debuff true/true flags), actual SendUseSPMP and same process clock;
there are no default no-ops. Spend must see LIVE Actor modifiers/resources after
prior effects. Root Scene routes speed through real retained zone/view services.
Callback must not destroy/recycle retained subjects/catalogs before returns.

Remaining dependencies: actual effect/spending providers, UseSkill preparation,
shared CSkill manager/full user definitions/root assembly and retained equipment
middle. Main already has the retained item accessor as previously documented;
worker base still lacks that dependency and shared headers stay02-exclusive.
No new canonical field needed for disable publication: use existing casting bits.
Expiry scheduling is NOT implemented by this publication coordinator. Next
independent proposal: recover +1428 expiration in **ChkKeepSkill 00487190**,
including fresh-clock/equality/send ordering, and compose its skill-disable stage
with existing applied-effect tick owner without duplicating that effect list.
Overall server objective remains open.
