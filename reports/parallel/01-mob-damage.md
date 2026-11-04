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
