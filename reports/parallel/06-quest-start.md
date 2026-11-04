# Quest admission and start

Status: implemented standalone admission/start coordinator; production quest/world
ownership adapters remain required. This does not make the server playable.

## Evidence

Read-only Ghidra exports: `/home/franco/Projects/Eternity/work/reverse/ps_game.exe/`
(`decompiled.c`, `disassembly.txt`, `types.txt`). Executable: `ps_game.exe`.

- `CUser::QuestEnableStart` **0047f080**: enabled byte `+1cc`, inclusive level
  bounds `+102/+104`, country selector `+106`, job flags `+10a`, grow `+107`.
  Country selectors 3 and 4 mean user countries 3 and 2 respectively. Unknown
  selectors bypass country checks. Grow 2 rejects only requirement 3; grow >=3
  bypasses grow checks. Sex, party and party-job fields are not checked.
- `CUser::QuestFindFinishSuccess` **0047e520**: any nonzero successful entry
  matching any nonzero ID from `+118`, `+110`, `+112` satisfies prerequisites.
  Despite their recovered names HG/VG, the last two fields are unsigned quest IDs
  at these call sites (`MOVZX`). Zero IDs do not match.
- `CUser::QuestStartRequest` **0047f220**: unsigned 16-bit, one-based positional
  definition lookup, then dispatch types 0..4; unknown types silently stop.
- Checks **0047f390**, **0047f400**, **0047f490**, **0047f500**, **0047f570**:
  NPC existence/type/ID; active/finished rejection; admission; item requirements;
  allocation; start. Item-give consumes before allocation without rollback;
  item-pick checks without consuming. Pick/use compare the full 32-bit request
  target with the definition's byte item ID. Use requires matching last item and
  unsigned `tick <= deadline`, including equality. Previous-quest starts require
  an NPC even with wildcard NPC type, check prerequisites before duplicates, and
  deliberately pass no NPC to QuestStart.
- `CUser::QuestStart` **0047ea70**: exact eight-byte client packet
  `0902, npc runtime handle (u32), definition ID (u16)`, followed by required
  start-log delivery. Common log data are captured before the client send. `QuestGetStartItem` **0047eb70** calls each nonzero type in
  three ordered item/type/count triples, ignores failures and continues.

## Implementation and boundaries

`quest_start.hpp/cpp` uses an immutable 464-byte definition projection and a user
projection. Valid job domain is 0..5; invalid jobs throw instead of reproducing
out-of-record reads. Definitions must remain alive while an owner retains their
references. No existing canonical header was modified.

Required `QuestStartWorld` methods have no default bodies: NPC lookup, active and
finished lookup, inventory requirement/consumption, clock, quest allocation/add,
client send, start logging, and start-item grants. The log provider owns common
log-header construction and original `0x0501`/144-byte transport serialization;
this coordinator verifies its call ordering, not its payload. These are explicit
unimplemented production dependencies, not successful gameplay services.

Canonical integration changes needed:

- Project user level (`+136`), country (`+12e`), grow (`+12f`), job (`+134`), last
  use deadline (`+57d0`) and item (`+57d4`), plus finished ID/success entries.
- Supply positional quest records and live NPC runtime handle/type/ID.
- Implement `QuestStartWorld::add` with the canonical quest owner. Evidence
  `QuestAdd` **0047e010**: allocation failure returns false; initializes success
  and three counters to zero, start tick, optional end tick using wrapping
  `minutes*60000+tick`, optional success tick, active-list insertion, kill-PC/MOB
  counters for end types 3/2, and DB packet `0x0a01` (billing u32, ID u16,
  truncated maximum minutes u16). Do not infer an admission capacity limit here.
- Connect inventory and logging providers and production dispatch to this module.
  Core QuestEnd/removal/ownership remain the coordinator's domain.

## Validation

`python3 tools/dev.py build`: exit 0 (379 build steps). No warnings in owned
sources; the full baseline emits existing guild-source warnings.
`python3 tools/dev.py test`: exit 0; **120/120** tests passed in **35.25 s**.
`quest-start-unit`: **1,536** exact original admission/request traces and **479**
successful starts matched, including the exact client packet and provider order.
Logs: ignored `work/quest-start-build.log`, `work/quest-start-test.log`, and
`build/Testing/Temporary/LastTest.log`.
`git diff --check`: exit 0.

The original-execution generator completes
1,536 deterministic cases, executing original admission, prerequisite-list
traversal, request/check functions, packet construction and start-item loops.
World lookup, inventory, allocation and transport are observed ports. Cases
include failure ordering, all start types, boundary ticks/levels, prerequisite
success values 0/1/2, alternate prerequisites, failed allocation after consumption,
unknown country/start selectors and zero-count item grants.

`sha256sum -c SHA256SUMS`: exit 0, all original EXE/PDB hashes OK.
Generator: `.local/oracle-env/bin/python tools/quest_start_oracle.py`, exit 0;
fixture generated under ignored `work/` then copied to the owned fixture path.
A second generation compares byte-for-byte identical (`cmp`: exit 0).
`python3 -m py_compile tools/quest_start_oracle.py`: exit 0.
SQL lifecycle/final isolated SQL integration are reserved for the coordinator;
this module introduces no SQL contract/schema changes.

Implementation commit: `f10cfb7b8b64e9cfbd32b5600a5a9f62f5e47872`.
