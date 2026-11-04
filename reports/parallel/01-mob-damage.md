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
