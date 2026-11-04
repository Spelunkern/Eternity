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
