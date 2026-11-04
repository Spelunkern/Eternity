# Worker 05: party experience state and reward range

## Evidence and implementation

Immutable `ps_game.exe`: `CParty::ChkPartyExpByLevel` 00444d80,
`CParty::GetPartyExpByLevel` 00444e40, `CUser::EnableApplyRange` 0045de60.
Focused Ghidra export additionally recovered `SVector::Distance2` 0041b1f0
under ignored `work/exports/05-party-state/ps_game.exe`.

`PartyRuntime` preserves the previous experience maximum for zero/one members
and kinds other than 0/1. Otherwise it computes the maximum unsigned 16-bit
level; raid kind 1 skips absent slots, normal kind 0 requires all slots present.
An invalid normal projection throws before changing state (the original would
read a null pointer). The member span projects the original signed count and
ordered slots; corrupt negative counts are outside this valid owner interface.
Level factors use signed wrapped 32-bit subtraction and thresholds 8/16/32/48.

Reward eligibility checks only the candidate's life status != 1 and DWORD
+0x5758 == 4, then matching +0xe0 identity and +0x164 instance. Distance uses
X/Z only, float-rounded differences, double accumulation/sqrt, float result
and signed range converted to float. Original unordered x87 comparison accepts
NaN; this is preserved. No world service has been replaced with a no-op.

## Validation

`python3 tools/dev.py build`: exit 0; 379 build steps completed. No party
module warnings; existing guild sources emit indentation/range-loop warnings.
`python3 tools/dev.py test`: exit 0; all 120 tests pass, 47.66 seconds;
`party-runtime-unit` passes all 1,508 original comparisons plus rejection of an
invalid normal-party projection. Logs: ignored `work/party-runtime-build.log`
and `work/party-runtime-test.log`. Repeated oracle generation matches byte for
byte (`cmp`, exit 0). `git diff --check`: exit 0.
SQL lifecycle and final integration are owned by the coordinator; this module
has no SQL operations or schema changes. No isolated SQL suite was run here.
Original EXE/PDB manifest verified with `sha256sum -c SHA256SUMS` (exit 0).
The oracle executes all four original functions including real CRT sqrt;
only Windows CRT TLS is supplied as an isolated zeroed block. 1,508 cases cover
state retention, empty raid slots, level extremes, wrapped arithmetic, range
status gates, X/Z boundaries, ignored Y, NaN/infinity and float range rounding.
Generated fixture was staged under ignored work before copying into tests/data.

## Integration dependencies and canonical interfaces

No canonical headers changed. Production party owner must own ordered member
slots, call `check_experience_level` at recovered mutation/level-change sites,
and expose the retained maximum to `MobDeathParty::maximum_level`. Discovering
all mutation call sites and implementing party admission/removal are outside
these three assigned functions.

`MobDeathWorld::in_reward_range(credited, member, radius)` should project member
as candidate and credited as reference into `party_reward_in_range`. Its live
user owner must supply DWORD +0x142c life status, DWORD +0x5758 state, +0xe0
identity, WORD +0x164 instance and +0xd0 XYZ; current `MobDeathUser` lacks these.
The semantic owner/name of +0x5758 remains unresolved: the decompiler incorrectly
labels it UID, so this implementation deliberately exposes `reward_state`.
Existing mob_death.cpp already has equivalent level-factor arithmetic; the
coordinator can delegate it to PartyRuntime when integrating party ownership.
This module and comparisons do not establish playable gameplay.

Implementation commit: `ec70b402d8e8d914fbbe73b389dddb0c8d240ec1`.

## Block 2: LevelUp integration evidence

The next independent block verifies the existing native `notify_level_up` party
port with the real `PartyRuntime`, rather than duplicating the LevelUp owner.
`tools/party_runtime_level_up_oracle.py` executes the original CUser::LevelUp
0048ad70 slice 0048af5c..0048b006 and its unmodified ChkPartyExpByLevel body.
Only critical-section calls and SendParty transport are observed ports. The
256-case fixture compares lock/recheck/recalculation/send/unlock ordering,
exact eight-byte party notices (0c09 normal / 0c13 raid), retained/recomputed
maximum, and maximum observed synchronously by transport. It includes absent
membership, membership removed during lock acquisition, one-member retention,
raid holes and previously retained signed-boundary maxima. Native comparison
calls the existing full notify_level_up with a required test world whose
update_party_experience delegates to PartyRuntime; unrelated providers are
outside this slice comparison. No production provider is added or bypassed.

Additional original integration sites from the full Ghidra disassembly:

| Executable/symbol | Recalculation call address |
| --- | --- |
| ps_game.exe CParty::EnterRaidParty 00445a30 | 00445f44 |
| ps_game.exe CParty::EnterNormalParty 00445fe0 | 004463d9 |
| ps_game.exe CParty::LeaveRaidParty 00446510 | 00446808 |
| ps_game.exe CParty::LeaveNormalParty 00446960 | 00446bb0 |
| ps_game.exe CParty::ExpelRaidPartyUser 00446cd0 | 00446fd7 |
| ps_game.exe CParty::ExpelUser 00447120 | 00447327 |
| ps_game.exe CUser::LevelUp 0048ad70 | 0048af9d |

LevelUp acquires party +0x118 at 0048af71, rechecks user +0x173c at
0048af77, immediately unlocks an abandoned party at 0048af80, then rereads
membership at 0048af93. It recalculates at 0048af9d before SendParty 004455d0
at 0048afe5 and releases at 0048b000. The production LevelUpNoticeWorld owner
must wire update_party_experience to the live ordered member projection while
holding that same membership lock. This is now validated composition evidence,
but a production owner is still missing.

The reward-state field +0x5758 additionally transitions from 3 to 4 only after
successful CZone::EnterUser (00415f80) in CWorld::ZoneEnterUserMove 0040d000
(write 0040d04e). This supports a world-admission gate; its complete enum and
canonical field ownership remain unproven, so the API name is unchanged.

Block 2 validation: `python3 tools/dev.py build` exit 0 (two incremental
steps, no warnings); `python3 tools/dev.py test` exit 0, 121/121 passed in
43.24 seconds, including both party suites (1,508 + 256 original comparisons).
Logs: ignored `work/party-runtime-round2-build.log` and
`work/party-runtime-round2-test.log`. Repeat fixture generation matches byte
for byte; `sha256sum -c SHA256SUMS` and `git diff --check` exit 0. SQL is unchanged;
final isolated SQL integration remains coordinator-owned.

Remaining independent scope in the originally assigned three functions is
complete. Next useful scope proposal: reconstruct party membership mutation
and ownership, beginning with EnterNormalParty 00445fe0 and LeaveNormalParty
00446960, using the call sites above. This requires coordinator confirmation
that broader party lifecycle scope is assigned, plus eventual canonical user
membership/lock and transport interfaces. No canonical file was changed.

Block 2 evidence commit: `f34712aecf0f3ca7ad8c44d5cc62cf942151f617`.

## Block 3: normal-party entry and leave

Coordinator explicitly extended ownership to `party_runtime_membership`.
Recovered PDB/Ghidra inventory signatures in immutable ps_game.exe:
`bool CParty::EnterNormalParty(CUser*)` 00445fe0 (inventory body size 1,158 bytes) and
`bool CParty::LeaveNormalParty(CUser*)` 00446960 (inventory body size 869 bytes).
Full source evidence: read-only `work/reverse/ps_game.exe/functions.tsv`,
`decompiled.c` and `disassembly.txt` in the coordinator checkout.
The disassembly resolves misleading decompiler field names and stack offsets.

New native files implement the actual membership coordinator using explicit
user/party projections and required `PartyMembershipWorld` providers. Existing
party state/LevelUp code and canonical headers were not modified.

- Entry rejects count >= 7 before clock/locks; otherwise samples the clock,
  locks, writes the new pointer at slot[count] and sets user +0x173c BEFORE
  notifying existing members. The 0b04 notification excludes the joining user
  because count is still unchanged. The 0b01 roster contains leader BYTE,
  existing count BYTE, and existing members in order. Only after sending it
  does entry increment count, clear user +0x1744, refresh maximum and unlock.
  Then GetPartyType, optional 0520 scene notification, log context and 0f01 log.
- Per-member wire data: ID DWORD, name[21], level WORD, class BYTE, current/max
  HP/MP/SP WORD pairs, instance WORD, XYZ float bits and effect count BYTE.
  Visible effects append skill WORD, level BYTE and remaining seconds DWORD.
  Definition +0x97 == 1 is omitted; other values remain visible. Expiry uses
  signed wrapped (deadline - tick) / 1000, clamped at zero. Both serializers
  cap at 40 visible effects, preserving list cursor at the last included node
  on cap, or the sentinel at exhaustion. Each traversal uses the user effect
  lock. Native name padding is initialized to zero; original strcpy leaves
  bytes beyond NUL unspecified, and fixtures initialize original stack to zero.
- Leave locks and searches only active ordered slots. Missing user returns
  false after unlock, with no notifications. A found user has membership
  cleared BEFORE 0b05 broadcast (whose recipients still include the leaver).
  Only pUser pointers shift left; slot Index fields and the inactive last
  pointer are retained. Item cursor decrements only if removed_index + 1 is
  strictly less than the original cursor (00446a16..00446a29).
- Count zero unregisters the party through its real manager owner. Count one
  recursively removes the remaining user, including nested locking, packets,
  manager unregister, scene updates and logs. With >=2 remaining users, removing
  the leader resets leader index to 0 and emits 0b07 after experience refresh;
  removing an earlier nonleader decrements the leader index without 0b07.
  Guild-creation cleanup runs after recursion/count handling and before refresh.
  Unlock precedes departed-user and changed-leader 0520 scene updates and 0f02
  log delivery. One/zero-member experience maxima remain retained as before.

Required provider boundaries are explicit: recursive party locks, effect locks,
clock, party/user/scene transport, GetPartyType, actual party-manager unregister,
actual guild-creation cleanup, SetGameLogMain context and game-log transport.
There are no production no-op providers. Registry/pool, guild creation and
GetPartyType bodies are not claimed as reconstructed by this block.
Callbacks must not throw, destroy membership/users or invalidate projections;
Windows recursive critical-section semantics are required for nested leave.
Normal kind 0 and non-null active slots are required; raid conversion/leave,
invitations, party allocation and manager lifetime remain separate work.

The original-execution oracle runs both complete entry/leave functions and the
real ChkPartyExpByLevel. It observes clock, critical sections, transport,
GetPartyType and log-context ports; manager unregister block
00446a37..00446a7d and guild cleanup block 00446ac1..00446bae are explicit owner
ports. It does NOT substitute the membership algorithm or packet assembly.
512 cases compare every port event with live count/leader/item cursor/maximum,
all seven pointers (including stale tail), eight membership flags/request bytes
and eight effect cursors, exact payloads, result and final state. Cases cover
0..7 members, leader/target positions, missing users, full entry,
rejection of count 8 and INT_MAX before any member read,
recursive dissolution, guild cleanup, no-zone users, short/full names, empty and
48-effect lists, filtered effects, 40-visible cap, clock wrap and signed expiry.
Slot Index values are separately checked unchanged by native comparisons.

Block 3 final validation:

- `python3 tools/dev.py build`: exit 0; final incremental rebuild/relink completes
  with no warnings (`work/party-runtime-membership-final-build.log`).
- `python3 tools/dev.py test`: exit 0; all 122 tests pass in 35.19 seconds,
  including previous state/LevelUp and all 512 complete membership comparisons
  (`work/party-runtime-membership-final-test.log`). The preceding suite also
  passed before adding the abnormal-full-count guard cases (42.07 seconds).
- Focused standalone C++20 ASAN/UBSAN comparison, compiled/run in the dev image
  under the same shared `resource_lock(ROOT, 'build', 2)`: exit 0; all 512 pass,
  no compiler/sanitizer diagnostics (`work/party-runtime-membership-final-sanitized.log`).
- Repeated original fixture generation matches byte for byte (`cmp`, exit 0).
  Original EXE/PDB `sha256sum -c SHA256SUMS` and `git diff --check`: exit 0.
  All generated executables/logs/intermediate fixtures remain in ignored work/.
- No SQL/schema/lifecycle operations were added. Isolated SQL integration remains
  coordinator-owned; it was not run in this worker block.

Block 3 implementation commit: `ef3c489918c908e312df1f9e3a7ebe373ec659ee`.

Needed canonical integration: provide live user ID/name/class/resource/instance/
XYZ/effect-list projections and pending-request byte, membership back-reference,
normal party count/leader/item cursor/seven ordered slots and retained maximum.
The actual party manager must keep the party alive through outer recursive
leave and unregister it without destroying active call frames. Guild-creation
cleanup must preserve the original reset/list/pool contract behind the required
callback. Map production transport to each exact ordered packet callback;
refresh the live maximum before sending subsequent reward/LevelUp queries.

## Block 4: real membership classification provider

The repeated normal-membership assignment is already implemented in block 3.
This block removes one remaining integration abstraction: the actual
`ps_game.exe CUser::GetPartyType` 0048a330 (PDB/Ghidra inventory body size 180
bytes). New `party_runtime_membership_type.hpp/cpp` implement it and supply
the real default body of `PartyMembershipWorld::party_type`; production owners
can inherit this behavior instead of inventing a classification callback.

The user +0x173c pointer is first captured and locked. If membership disappears,
that captured party is unlocked immediately and discarded. Membership is then
reread and that current party is locked AGAIN, including the recursive A/A case.
If membership changed A/B, both captured identities are retained: unlock B then
A. Classification reads the second captured party after its lock callback;
there is no further membership recheck. Return 2 iff count > 0 and selected
leader slot equals this user, otherwise 1 for a captured current party and 0
without one. Kind is ignored, and nonpositive counts still return 1 when the
party pointer exists. Changes during unlock cannot retroactively change the
selected return branch. As in the original, all captured party objects and a
valid selected slot must remain alive/readable through matching unlocks.

`party_runtime_membership_type_oracle.py` executes the entire unmodified
original function, replacing only Windows critical-section ports. 1,024 cases
inject membership/count/leader changes on each reached acquire/release across
two parties, including detach, switch, restoration during early release,
recursive locking and signed count boundaries. Native comparisons check every
lock event, live state, result, and balanced final locks. Results: 428 none,
394 member, 202 leader; 342 zero-callback, 86 two-callback and 596 four-callback
executions. All other test-world providers fail if reached.

The existing membership oracle now also supports `--real-party-type`, leaving
GetPartyType intact and observing its returns after real lock processing.
A separate 512-case fixture and CTest execute complete EnterNormalParty /
LeaveNormalParty plus real ChkPartyExpByLevel AND GetPartyType. The native
comparison delegates the classification call to the real default provider.
The earlier observed-port fixture remains unchanged and independently tested.
This does not supply registry/guild/transport implementations or a production
user/party owner. Next direct membership dependency is CParty::Send 004455d0;
manager lifetime, guild cleanup and canonical projection wiring remain open.

Block 4 validation:

- `python3 tools/dev.py build`: exit 0, no warnings in final incremental build
  (`work/party-runtime-membership-type-final-build.log`).
- `python3 tools/dev.py test`: exit 0; 124/124 pass in 37.07 seconds, including
  all earlier party state, LevelUp and observed-port membership comparisons
  (`work/party-runtime-membership-type-final-test.log`).
- Focused C++20 ASAN/UBSAN builds/runs in the dev image under the shared build
  lock: 1,024 classification and 512 live-classification membership comparisons
  pass, exit 0, no diagnostics
  (`work/party-runtime-membership-type-final-sanitized.log`).
- Both new original fixtures regenerate byte-identically (`cmp`, exit 0).
  Original EXE/PDB hashes and `git diff --check`: exit 0. Generated outputs
  remain ignored under work/. No SQL changes or lifecycle operations; final
  isolated SQL integration remains coordinator-owned and was not run here.

Block 4 implementation commit: `d69cb64e603f0fadef8f106162be0e0d5d5cd681`.
