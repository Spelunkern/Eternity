# Worker 07: quest end requests and item requirements

## Evidence and implementation

Source: immutable `originals/ps_game/ps_game.exe` (SHA256
`0fb7c9f13b5bcf186118f8bfe73ab214226b93e8aa502b8e6fded4a8312bc547`).
Ghidra's complete `work/reverse/ps_game.exe/{decompiled.c,disassembly.txt}`
exports in the coordinator checkout were read only. Assembly resolves the
misidentified register/stack arguments in the decompilation.

- `CUser::QuestEndRequest` **0047f2a0**: bounds/active lookup and end-kind dispatch.
  Click completion requires a configured end NPC type. Unsupported kinds return.
- `QuestEndChkClickNPC` **0047f630**, `QuestEndChkItemPick` **0047f670**,
  `QuestEndChkKillMOB` **0047f6e0**, `QuestEndChkKillPC` **0047f740**:
  exact NPC type/ID checks; item and kill checks allow zero NPC; kills forward
  NPC argument zero, while click/item checks forward the request NPC value.
  Item completion consumes unless both 0x15d and end NPC type are nonzero.
- `QuestFindItem` **0047f790**: first matching slot in unlocked bags 1..N,
  slots 0..23; optional single-byte decrement (including zero-to-255 wrap),
  DB 0x0702 and client 0x0206 delivery, then release/clear on zero.
- `QuestFindItem` **0047f8e0**: first-match classification among three requirements;
  all totals checked before mutations; original counter overwrite/truncation;
  descending reward-dependent consumption override (0x163/164 + 32*i),
  full removal of matching effect-27 stacks, ordinary consumption in slot order,
  DB-before-client-before-release ordering. Failed checks preserve counters/items.

Public functions and required providers are in `quest_requests.hpp`.
No QuestEnd rewards, removal, selection or success-result implementation is
included. No database connection or world-entry behavior is added.

## Canonical integration needed

No shared canonical header was edited. `QuestRequestInventory` is an explicit
projection of unlocked bags excluding equipment, with 24 slots per bag. The
production adapter must retain original item ownership and project presence,
type/ID/count and item-definition effect, route account ID to DB packets, route
client packets to the requesting user, and release the actual item at the required
boundary. Keep the projection and owning inventory synchronized during callbacks.
`QuestRequestWorld` must resolve the positional definition, active quest and
current-zone NPC, and call the coordinator's real QuestEnd with the provided
NPC argument and success flag. The immutable definition owner must outlive the call.
All service methods are required virtual providers, with no default no-ops.

Worker 06 may use `quest_find_item` for its item-give/pick checks. Coordinator
QuestEnd may use `quest_find_items`; pass the real runtime counter array because
counter updates and reward checks depend on its initial contents. Both original
call sites alias the output to quest+0x1c (0047ec12, 0047f6be); the oracle
retains this alias, including during reward-dependent counter overwrites.

## Validation

- `python3 tools/dev.py build`: exit 0, final log
  `work/quest-requests-build-final2.log`, no compiler warnings.
- `python3 tools/dev.py test`: exit 0, **121/121 passed**, 44.61 seconds;
  `work/quest-requests-test-final.log`. Both new native comparisons pass:
  768 item cases and 1,024 request cases (1,792 original executions).
- Both original-execution generators: exit 0 using `.local/oracle-env/bin/python`.
- `python3 -m py_compile` for both new generators: exit 0.
- `git diff --check`: exit 0.
- Original executable/PDB manifest: `sha256sum -c SHA256SUMS`, exit 0,
  all entries OK (`work/quest-requests-hashes.log`).

`tools/quest_requests_oracle.py` executes original item routines: 768 cases of
unlocked/empty bags, slot scanning, duplicate requirements, byte boundaries,
consumption, effect-27 stacks, reward overrides, packets and final inventory.
`tools/quest_requests_end_oracle.py` executes original dispatch/check instructions:
1,024 cases, observing QuestEnd and injecting NPC/active/item-result providers.
Native request tests compose the actual new item routine with equivalent live
inventory requirements and verify the request consumption flag, rather than substituting a native item-check stub.
Fixtures are generated under ignored `work/` before copying into tests/data.

SQL integration and production adapters belong to the coordinator; no SQL contracts
or fixtures change. This subsystem has no SQL connection and does not make the
server playable.

Implementation commit: `60be7af9ad8e99411e8fb06b39ce59870096481d`.
