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

## Block 5: shared buffer delivery for membership notifications

Recovered immutable ps_game.exe `CParty::Send` 004455d0, `SConnection::Send`
004d4cf0, `SPacketBufferSend::AllocBuffer` 004df9f0 and the required
`SAgent::SendRequest` 004d8950 boundary. New focused Ghidra exports of allocation,
Free 004df070 and SendRequest are ignored under
`work/exports/05-party-state/ps_game.exe`; full CParty/connection assembly is in
the coordinator's read-only full reverse export. The explicit pool/worker
boundaries remain required real providers, not no-ops.

`party_runtime_membership_send.hpp/cpp` adds a real `send_party` implementation
on `PartyMembershipSendWorld`. Allocation selects capacity 32 for payload <=30,
128 for <=126, otherwise 2048. It initializes reference count 1, custom-reference field 0,
copies payload once and writes the little-endian total length including its two
bytes. Allocation failure returns before locking. The original payload must fit
the 2048-byte pool; native input >2046 throws instead of reproducing the original
out-of-bounds write. Normal membership's largest valid roster is 2014 bytes.

The party lock is recursive. Kind 0/1 selects the loop once; other kinds send to
nobody but still allocate/lock/unlock/release. Ordered count/slots are reread on
each iteration and holes skipped, so live enqueue callbacks can shrink/extend
the active prefix or replace subsequent users. Only socket DWORD +0x68 == -1
suppresses a connection send; all other values enqueue. Each connected enqueue
increments the shared atomic reference first. Party unlock precedes release of
the sender reference; Free is called only when the atomic decrement reaches
zero. Async worker owners retain their own references until completion. The
normal adapter exposes seven slots; this does not implement raid ownership.

Required pool and worker providers: allocate requested size class (nullable),
queue the same retained buffer to the actual connection/worker, and return the
buffer to its real pool at zero references. `free_send_buffer` may destroy the
object; the implementation never reads it after that callback. Atomics preserve
original 32-bit Interlocked wrap/full-order semantics. The projected socket is
separate from a native file descriptor; the canonical connection owner must
supply the original valid/disconnected state, rather than use a default socket.
Registry/guild/log/scene/direct-user transport providers remain independent.

`party_runtime_membership_send_oracle.py` executes full original CParty::Send,
AllocBuffer, SConnection::Send and SendRequest; only pool allocation/free,
SendPost worker, locks and atomic OS intrinsics are ports. 1,024 cases compare
all events, recipient identities, complete frame, reference/custom-reference state and
pool return. Cases include allocation failure, kind gates, signed count <=0,
holes, disconnected/zero sockets, all size-class boundaries, maximal payload,
mutation of caller bytes after allocation, count/slot/socket/kind changes during
enqueue, and explicit synchronous/asynchronous completion models. A native
post-comparison check releases any retained async shares and requires one pool
return. It does not claim to implement asynchronous OS I/O completion itself.

The full membership oracle's `--real-party-send` mode adds another 512 original
complete entry/leave executions with real GetPartyType AND CParty::Send /
AllocBuffer / SConnection::Send / SendRequest. The native fixture composes the
same recovered default providers and verifies exact notifications, recipients,
frames, references and nested locks. Only actual worker completion is modeled
synchronously at its required port. Earlier observed-port and real-type suites
remain separately registered and unchanged in meaning.

Block 5 validation:

- Final `python3 tools/dev.py build`: exit 0, no warnings
  (`work/party-runtime-membership-send-reviewed-build.log`). PDB member +0xc is
  explicitly named m_nCustomRef, now exposed as custom_references; it is not
  inferred to be a send offset.
- Final `python3 tools/dev.py test`: exit 0; 126/126 pass in 30.92 seconds
  (`work/party-runtime-membership-send-reviewed-test.log`). The earlier complete
  suite also passed (41.20 seconds) before this identifier-only PDB correction.
- Focused C++20 ASAN/UBSAN builds/runs in dev containers under the shared build
  lock: all 1,024 send/ownership and 512 complete membership/send comparisons
  pass, exit 0, no diagnostics
  (`work/party-runtime-membership-send-final-sanitized.log`).
- Both new original fixtures regenerate byte-identically (`cmp`, exit 0).
  Original EXE/PDB hashes and `git diff --check`: exit 0. New outputs stay under
  ignored work/. SQL/schema/lifecycle are unchanged; final isolated SQL suite
  remains coordinator-owned and was not run in this worker block.

Needed integration: supply the canonical connection's DWORD +0x68 to the new
send_socket projection, allocate/free real 32/128/2048 pool objects, and enqueue
frames through the worker while retaining one reference per send until its
actual completion/cancellation. Never destroy a buffer while those references
remain. User roster transport (SConnection::Send(void*,int) 004d4d20), scene/log
transport, registry/guild owners and production dispatch remain pending.

Block 5 implementation commit: `a06d652ef0fe90002a24a66117b80bc61273e2d9`.

## Block 6: direct roster transport and reference transfer

The entry/leave bodies were already complete, so this block replaces their last
packet-assembly/connection-send abstraction: immutable ps_game.exe
`SConnection::Send(void*,int)` 004d4d20 (PDB/Ghidra inventory body size 107
bytes). New `party_runtime_membership_user_send.hpp/cpp` implement the direct
send and provide `PartyMembershipUserSendWorld::send_user`. Buffer allocation/
copy is factored from the already recovered broadcast implementation and reused;
existing broadcast/classification/LevelUp behavior remains unchanged.

For lengths <=2048, a disconnected socket (-1) returns BEFORE allocation.
Otherwise allocation initializes one reference and the original frame, then
that initial reference is transferred directly to SendRequest/worker. There is
NO broadcast-style increment or sender release. The socket is not rechecked
after allocation, even if its provider changes connection state. Allocation
failure returns without delivery. No party or effect lock is acquired.

Length >2048 follows the recovered error path: DWORD +0x18 becomes 12 only if
previously zero, DWORD +0x1c retains its previous value, then a live socket is
captured and closed through a required provider. The socket field is written -1
AFTER that provider returns, regardless of callback mutation. A previously
closed connection still receives the conditional error update but is not closed
again. These two fields remain explicit projections; their wider lifecycle
semantics/PDB names are unresolved. New close_send_socket is mandatory.

The original accepts 2047/2048 payload bytes before copying into a 2048-byte
pool plus length prefix, which overruns that original buffer. The safe allocator
continues rejecting this range for open connections. Original fixtures exclude
that unsafe copy; normal-party rosters never exceed 2014. Valid payloads through
2046 and the >2048 error/close branch are recovered and compared exactly.

`party_runtime_membership_user_send_oracle.py` runs the entire original direct
send, real AllocBuffer and SendRequest. Pool allocation, closesocket and worker
SendPost/completion are explicit ports. 1,024 cases compare order, errors,
socket/reference/custom-reference state, full frames and provider events. Cases
include zero/disconnected/nonstandard sockets, allocator failure, size-class
boundaries, maximal safe payload, oversized close, retained errors/details,
changes to caller bytes/socket during allocation, synchronous completion and
retained asynchronous worker reference. Native post-comparison release verifies
one pool return. Callback-driven close changes verify the final socket write.

The full membership oracle's new `--real-user-send` mode executes another 512
entry/leave scenarios with real ChkPartyExpByLevel, GetPartyType, CParty::Send,
both SConnection send overloads, AllocBuffer and SendRequest. Native composition
uses both recovered delivery providers. Roster transfers observe reference 1;
broadcast enqueues observe reference 2. Pool/worker, close, scene/log and registry/
guild services remain mandatory real integration dependencies. Existing fixtures
remain registered separately, including their earlier external-port contracts.

Block 6 validation:

- `python3 tools/dev.py build`: exit 0, no warnings in initial or final build
  (`work/party-runtime-membership-user-send-final-build.log`).
- `python3 tools/dev.py test`: exit 0; 128/128 pass in 45.59 seconds, including
  all earlier state, LevelUp, classification, broadcast and membership suites
  (`work/party-runtime-membership-user-send-final-test.log`).
- Focused C++20 ASAN/UBSAN comparisons in dev containers under the shared build
  lock: all 1,024 direct-send and 512 complete membership/delivery cases pass,
  exit 0, no diagnostics
  (`work/party-runtime-membership-user-send-final-sanitized.log`).
- Both new original fixtures regenerate byte-identically (`cmp`, exit 0).
  Original EXE/PDB manifest hashes and `git diff --check`: exit 0. Generated
  outputs remain ignored under work/. No SQL/schema/lifecycle operations were
  added; the final isolated SQL suite remains coordinator-owned, not run here.

Block 6 implementation commit: `df2e00b09fafaa4e8f080defcd134608fd73e58b`.

Remaining integration requires the actual connection owner for socket/error
projections, real send-buffer pools and worker completion/cancellation, plus
serialized party-manager and guild-creation owners. This worker has no canonical
Actor/connection/registry write ownership. Normal membership and its delivery
coordinators are now reconstructed; proposed next scope is party creation/
registry lifetime (CParty constructor/Init and manager allocation/removal), or a
coordinator-owned production adapter using these interfaces. Do not substitute
placeholder pool/worker services or infer server playability from fixtures.

## Block 7: party registry creation and lifetime

Expanded scope `party_runtime_registry` reuses the existing PartyNormalMembership,
PartyRuntime and normal entry/leave bodies. No canonical Actor/connection header
or earlier membership source was changed. Focused locked Ghidra exports are in
ignored `work/exports/05-party-state/ps_game.exe`. Immutable ps_game.exe evidence:

| Symbol | Address |
| --- | --- |
| CParty constructor / destructor / Reset | 00444c60 / 00444c90 / 00444ca0 |
| CPartyMgr constructor / destructor | 004473f0 / 00447450 |
| CPartyMgr CreateParty / RemoveParty / FindParty | 004474b0 / 00447540 / 00447560 |
| SSyncHeap<CParty> Alloc / Release | 004475f0 / 00448620 |
| SSyncMap Add / Remove | 00447780 / 004477c0 |

There is no CParty::Init in the inventory: Reset is the initialization body
called by constructor/CreateParty. Constructor initializes its recursive critical
section with spin 4000, sets ID zero, then resets. Reset retains ID, intrusive
links and lock; clears count/leader/division/item cursor/maximum, all 30 pointers,
kind and auto-join; sets every slot Index to 65535 and subleader index to 1.
`PartyRegistryParty` extends the existing normal owner with the retained extra
23 slots and registry fields, without introducing a second membership body.

Heap Alloc holds its lock. An available node is popped through +4, available
count decremented, and all object fields/ID retained. Otherwise actual object
allocation is required; success initializes the object, then the allocation
attempt counter increments. Failure ALSO increments that counter before unlock.
The real native object allocator must supply valid typed storage, not a 304-byte
binary overlay (304 is the original 32-bit allocation size).

Manager initialization clears heap bookkeeping, constructs the map owner and
sets the atomic ID counter to 1. Create rejects a null user before allocation.
Only a zero-ID object consumes InterlockedIncrement, including DWORD wrap to 0;
a retained nonzero ID is reused. Reset then applies the caller's item-division
DWORD. That parameter is not a raid flag: Reset has already cleared kind, so the
serialized creation path enters normal membership. Actual existing entry runs
BEFORE map publication, including roster/scene/log callbacks; its bool result
is ignored by the original. Map insertion keeps the earlier value on duplicate
ID, yet Create still returns the new object. ID 0 is not normalized or retried.

RemoveParty only erases the CURRENT ID key, even if that key maps to another
object. Null is a no-op. Find locks, captures the matching raw pointer, unlocks,
then returns it. Neither operation owns or destroys the party. The direct
`unregister_registry_membership` bridge composes real recursive LeaveNormalParty
with this manager; after inner unregistration the object must survive remaining
inner/outer refresh, scene and log callbacks. It is not returned to the heap.

Manager destruction clears map nodes and destroys the map lock BEFORE heap
Release. Release destroys/deallocates only the available chain, advancing head
before each object's lock destruction/free, then zeroes heap bookkeeping and
unlocks. Active/unregistered objects outside that chain are not implicitly
freed. No producer returning parties to this available chain was recovered in
these functions; external fixture seeds are explicitly labeled H/Q, not claimed
as a reconstructed Free routine. Do not invent recycling on unregister.
Ghidra's incorrect noreturn annotation for operator_delete hides trailing cleanup
in Release; real original bytes at 00448648 include ADD ESP,4 and continue the
loop. The final fixture exercises two nodes, not the misleading one-node view.

The oracle executes complete constructor/reset, manager constructor/create/
remove/find/destructor, heap Alloc/Release and real original std::map insert/
find/erase. Creation and recursive removal also execute real normal membership,
ChkPartyExpByLevel and GetPartyType. Ports are OS locks/atomics/clock, raw CRT
memory, existing transport and log context. Complete normalized state is compared
after each action; all callback snapshots use FNV-1a64 over the same complete
state, plus exact payloads. Cases cover nullable user/object allocation, retained
IDs/links, all 30 slots, wrap/zero/collisions, publication timing, direct and
recursive unregistration, allocator failure, reuse and two-node destruction.

Block 7 validation:

- Native `tools/dev.py build`: exit 0, no warnings in final build
  (`work/party-runtime-registry-final-label-build.log`).
- Final clean `tools/dev.py test`: exit 0, all 129 pass in 44.98 seconds
  (`work/party-runtime-registry-clean-retry-test.log`).
- The preceding two-node full suite FAILED world-network-unit with
  "Login connection was not closed" (128/129, 50.11 seconds;
  `work/party-runtime-registry-reviewed-test.log`). The message comes from the
  shared PeerSocket::closed three-second wait, reused for several connections;
  it does not identify which close failed. No cause/race is proven and no
  network files were changed. This failed run is not counted as a pass.
  The earlier one-node full suite also passed (129/129, 45.12 seconds).
- Registry comparisons pass: 64 retained lifetimes / 1,501 state/action
  comparisons, including 1,248 complete original top-level calls and explicitly
  marked external setup actions. Both original free-chain nodes are exercised.
- Focused C++20 ASAN/UBSAN under the shared build lock: exit 0, all registry
  comparisons pass, no diagnostics (`work/party-runtime-registry-reviewed-sanitized.log`).
- Repeat fixture generation matches byte for byte. Original EXE/PDB hashes and
  `git diff --check`: exit 0. All generated projects/logs/executables remain
  ignored under work/. No SQL/schema/lifecycle change; full isolated SQL remains
  coordinator-owned and was not run here.

Block 7 implementation commit: `f472fc015a37c3e5a882ba3498e56e650fe376a3`.

Production contract: implement real recursive heap/map/party lock ports and
actual object allocation/deallocation; retain objects independently of map
entries. Use this owner for ordered normal members and experience state, and
project it to canonical Actor/connection/reward owners under their serialized
lifetime rules. Registry lookup returns a borrowed pointer, not ownership. Bind
unregister through the bridge without delete/reset/recycle. Shutdown must respect
the available-chain contract and outstanding actor/callback references. The
source of any pool-return producer remains missing evidence, not a guessed
service. Real network, guild, actor and world adapters remain coordinator work.

## Block 8: force-disband and bounded pool-return audit

Recovered `ps_game.exe CParty::BreakParty` 00444fa0, a remaining registry
unregistration path. `party_runtime_registry_break.hpp/cpp` implement it using
the existing normal owner, full 30-slot backing and registry RemoveParty. Two
small public wrappers expose the already recovered scene/log emitters; their
bodies/serialization are reused, not duplicated. No canonical header changed.

BreakParty locks the party, scans live count and ordered slots, skips holes and
clears each selected user's membership before its optional 0520 scene delivery.
It reloads the CURRENT slot after that callback for SetGameLogMain/0f02 log;
replacing that slot can make scene and log refer to different users. It rereads
count after each iteration and does not deduplicate repeated users. Only after
all callbacks does Reset clear all 30 slots/indices and ordinary state, retaining
ID/links/lock. Current-ID map removal follows Reset, still under the party lock;
map unlock precedes party unlock. Empty or missing registry entries still return
true. This path emits no 0b05/0b07 and performs no recursive Leave, guild cleanup,
heap return, object free or ID increment. A callback changing ID before logging
also changes the final removal key; the earlier published key can remain.

The new `--break` oracle mode executes full unmodified BreakParty, manager/map
operations, Reset and GetPartyType. The real original tree is used. It covers
256 lifetimes / 2,304 state/action comparisons, including 1,792 complete original
top-level calls; W/Q are explicitly labeled input projections. Callback cases
shrink/grow count, replace the current slot, clear a future slot and change ID
inside log-context delivery. Counts include negative/zero/1/2/7/15/30, masks
include holes/empty/full, kind varies, and guild-creation state is nonzero to
ensure no cleanup is introduced. Complete owner state, hashed callback snapshots
and exact payloads are compared as in block 7. Original execution with nonzero
guild state regenerates the same expected data, not an inferred result.

Bounded lifetime audit: GNU objdump over the entire immutable executable .text
found four instruction operands in manager/heap range [00547688,005476b0):
004679fc / 00468099 (CUser::PacketParty 004676c0, CreateParty calls at
00467a01 / 0046809e), 0051fae0 ($E6 startup) and 0051ff70 ($E7 shutdown).
No direct absolute operand for the heap fields was found. Focused Ghidra exports
confirm $E6 calls manager construction then atexit($E7), and $E7 calls manager
destruction. Existing manager/normal-leave/BreakParty bodies unregister without
returning a node to the heap; the inventory has no SSyncHeap<CParty>::Free.
This is bounded evidence, not proof that no indirect alias/caller exists. Raw
assembly is ignored in `work/party-runtime-registry-all-text.asm`; focused Ghidra
outputs stay under `work/exports/05-party-state`. Do not add speculative recycling.

Block 8 validation:

- `python3 tools/dev.py build`: exit 0, no warnings
  (`work/party-runtime-registry-break-final-build.log`).
- `python3 tools/dev.py test`: exit 0; all 130 pass in 47.29 seconds
  (`work/party-runtime-registry-break-test.log`).
- Focused C++20 ASAN/UBSAN under shared build lock: both 64-cycle registry and
  256-cycle BreakParty comparisons pass, exit 0, no diagnostics
  (`work/party-runtime-registry-break-sanitized.log`).
- Repeated new fixture generation is byte-identical; nonzero guild-state
  execution is also byte-identical. Original EXE/PDB manifest verification and
  `git diff --check` exit 0. Generated exports/logs/binaries stay ignored under
  work/. No SQL/schema/lifecycle operations were added; full isolated SQL
  integration remains coordinator-owned and was not run here.

Block 8 implementation commit: `ed8ec71fe4cba5ee89f0d530305a3439cdbe771e`.

Remaining production dependencies are the real object/recursive-lock backend,
canonical Actor/connection party projection, and registry/guild/network dispatch
adapters. The assigned constructor/create/remove/free paths and known force-
disband path are now recovered. A concrete next scope is canonical serialized
party-handle access followed by CUser::PacketParty 004676c0 request dispatch;
canonical Actor/connection headers remain coordinator-owned. No gameplay
completion is inferred from these comparisons.

## Block 9: retained normal party request dispatch

Expanded `party_runtime_requests` recovers immutable ps_game.exe
`CUser::PacketParty` 004676c0 normal request caller paths. Source: full Ghidra
exports in the read-only coordinator reverse tree; focused assembly is ignored
under `work/party-runtime-requests-asm.txt`. New request code reuses the existing
registry CreateParty, normal entry/leave, experience, scene/log emitters and
required membership transport. It does not introduce another membership owner.

`PartyRequestUser` holds references to the SAME PartyMembershipUser and persistent
PartyRequestState, plus a required shared lifetime pin to their Actor/connection
context. Copies copy the pin/references only. Lookup and retain_member providers
must return these retained live views; an expulsion retainer changing member
identity is rejected explicitly. No canonical Actor/connection header was edited.

Recovered behavior:

- 0b02 ID invitation: self/activity checks precede CPartyLock; leader/count <7
  checks precede zone lookup. Errors are 0b03 + reason BYTE + UID DWORD: self/
  missing/country 4, activity 1, leader/capacity 2, occupied/pending 3, same party
  5. Target invitation word +1740 is written BEFORE GetTickCount; deadline +5834
  gets wrapped tick +30000. Reply rereads inviter UID after that callback.
- 0b09 name invitation forces input byte 22 to NUL BEFORE locking. Global
  FindUserZoneMove(name) precedes leadership/activity checks; EqualZone 0044bd20
  checks TARGET state ==4, zone identity and instance, then country. The name
  reply is 23 bytes and rereads inviter name after clock sampling. Native unused
  name padding is zeroed, as in the existing serializers.
- 0b03 returns early without clearing pending data if no inviter or already in
  party. Otherwise it looks up the saved inviter in the current zone, then clears
  inviter/deadline even on a missing result. It does not check expiry here.
  Activity errors/rejections are sent to the inviter with responder UID. Zero
  accepts; any nonzero byte rejects with reason 0. It locks the inviter's captured
  party, creates a normal party if absent, verifies existing leadership and enters
  through the existing owner. Failure unlocks before error 1 delivery.
- 0b05 leaves directly; 0b0d takes the captured-membership guard first. The guard
  matches CPartyLock 00444d20: it retains the original party lock, rechecks null
  membership after acquisition and releases that captured identity on exit.
- 0b07 uses recovered IsPartyBoth 00444e90 / SetPartyBoth 00445470 ordering.
  Leader notification broadcasts while locked, then reloads new/old leader slots
  after the send callback and emits their scene updates after inner unlock.
  Failed selection replies with 0b07/UID 0. 0b06 preserves leader and FindUser
  00445520 guards, including reload of the selected slot AFTER lookup unlock;
  its ExpelUser 00447120 body remains a mandatory explicit provider.
- 0b0c is the original no-op. Unknown normal codes close with reason 9/detail 0.
  Known raid control codes 0b0e..0b18 and 0b1a are explicit full-dispatch providers;
  normal accept/leave encountering kind 1 requires actual raid enter/leave
  providers. No fake success or fallback normal membership is supplied.

Original fixture executes the ENTIRE unmodified PacketParty caller, actual
CPartyLock/leader/find/SetPartyBoth/EqualZone/error helpers, registry create/map,
normal membership and GetPartyType. Ports are lookup, OS/raw memory, transport/
log context and explicitly missing expulsion/raid bodies. A saved-register x86
trampoline enters real nested PacketParty(0b03) from invitation send, not a
modeled join. Each such case verifies a real joined party, then sends a second
real PacketParty leave against the same retained registry/users. Tests include
self-invite precedence, busy/full/nonleader/pending/country/zone/missing cases,
reject/accept, allocator failure, captured-lock detachment and live clock-field
updates. Whole function returns are checked at the original return sentinel.

Block 9 validation:

- Final `python3 tools/dev.py build`: exit 0, no warnings
  (`work/party-runtime-requests-final-build.log`). Initial build failed because
  optional reference-bearing bindings are nonassignable; corrected via emplace.
  The failed compile is not counted as success.
- Final `python3 tools/dev.py test`: exit 0, all 131 pass in 42.94 seconds
  (`work/party-runtime-requests-final-test.log`). The preceding 1,024-request
  fixture suite also passed (44.05 seconds) before self/leave extension.
- Final fixture: 1,024 cases / 1,070 top-level whole PacketParty invocations /
  46 literal nested accepts (1,116 original PacketParty executions total), with
  46 follow-up leave requests on those same live owners. Each nested accept
  asserts actual membership before the following leave; it is not a success port.
- Focused C++20 ASAN/UBSAN in dev containers under the shared build lock: exit 0,
  the complete extended comparison passes, no diagnostics
  (`work/party-runtime-requests-final-sanitized.log`).
- Repeated final original generation, including exact return-sentinel checks,
  matches byte for byte (`cmp`, exit 0). Original EXE/PDB manifest and
  `git diff --check`: exit 0. Generated outputs remain ignored under work/.
  No SQL/schema/lifecycle operation; full isolated SQL remains coordinator-owned
  and was not run in this worker block.

Block 9 implementation commit: `02d44c22b7c5cf8074a2c6a0dd8a3c1a3f206085`.

Minimal canonical binding contract after read-only Actor review: expose a retained
`PartyRequestUser retain_party_user(actor_uid)` on the existing connection/user
owner, referring to one persistent PartyMembershipUser and PartyRequestState.
The pin must retain Actor, its bound scene and those records across all nested
calls. Existing ActorScene::party_kind/send_party must use that SAME member.party
handle; the current flag/send-only interface cannot supply a retained handle.
Bind original UID (+dc), live level/name/resources/instance and request metadata
(+12d country, +15e0 activity, +5758 state, +e0 zone identity, +1740 inviter,
+5834 deadline) through the serialized owner. Update authoritative records in
place during Actor/world changes; never build a detached per-lookup member copy.
Member.pending_request (+1744) is separate from the inviter DWORD. The dispatcher
requires validated decoded packet sizes; malformed/truncated shapes and the
remaining expiry scheduler are not claimed as recovered here. Normal ExpelUser,
full raid dispatch and production adapters remain substantive next work.

## Block 10 — actual request expulsion owner

Recovered `ps_game.exe CParty::ExpelUser 00447120..004473b4` from the
complete Ghidra disassembly/decompilation inventory. The decompiler confuses
party/user registers: assembly establishes ECX=user and stack argument=party.
`party_runtime_requests_expel` now supplies the real default
PartyRequestWorld::expel_user, replacing the previously required opaque body.
It uses the existing authoritative PartyRegistryParty/PartyMembershipUser,
normal Leave owner, unregister provider and shared scene/log serializers.
No Actor, connection, other worker or main checkout source changed.

Recovered ordering:

- Lock party, search live count by pointer identity; missing member unlocks and
  returns false without packets or state changes.
- Capture target UID, clear target.member.party, broadcast six-byte 0b06 while
  the removed user is still in the active slots, then compact pointer slots and
  decrement count. Tail pointers and all indices remain untouched.
- Zero count unregisters through the existing map owner. One remaining member
  invokes real LeaveNormalParty, or the mandatory LeaveRaidParty provider when
  kind==1, while the outer recursive party lock remains held.
- Guild creation uses the same required cleanup boundary as normal Leave;
  refresh experience, unlock, emit departed user's 0520 scene notification,
  then fill/send 0f02 log with the live party ID.

ExpelUser does NOT adjust leader or item_cursor and does NOT emit 0b07 itself,
including self-expulsion. Recursive Leave can change those values or emit its
own packets. The native owner preserves this difference instead of treating
expulsion as voluntary Leave. Unregister must not recycle/free the party or
user: the outer call still uses both after recursive removal and map erasure.
The normal registry allocation/publication and send owners are unchanged.
Retain_member is a lifetime acquisition port, not an original gameplay call:
it must return the exact selected live record without observable callbacks or
membership changes. It pins the selected remaining user across nested Leave.

The request oracle no longer patches ExpelUser. Its original instructions,
normal recursive Leave, GetPartyType, experience refresh and actual manager
map operations all execute. Expanded fixtures include original whole requests
and consecutive direct original ExpelUser calls on the same objects, including
absent/repeated targets, self-expulsion, count-one unregister, count-two recursive
removal, populated/full parties and nonzero cursor/indices. The prior literal
nested invitation accept and subsequent leave sequences remain present. Raid
Leave is explicitly a required observed provider, not simulated success or a
normal-party substitution. Guild cleanup remains a required provider; this
matrix does not claim a recovered nonempty guild-creation implementation.

Validation (final results recorded below):

- Original matrix: 2,048 scenarios; 2,094 whole PacketParty calls, 2,048 whole
  direct ExpelUser calls, plus 46 literal nested PacketParty accept executions.
  Direct return values: 245 true, 1,803 false. Traces contain 622 original 0b06
  broadcasts and 538 original 0b05 broadcasts across the complete composition.
  State snapshots at every port, final state, return byte, packet bytes and
  balanced recursive locks are compared, not just operation counts.
- Repeated original generation matches all execution rows byte for byte;
  fixture command header was clarified to document direct E calls.
- Initial focused launch raced the queued rebuild and used the earlier parser,
  reporting `Invalid request fixture/owner state`. It is not counted as a pass;
  after rebuild, the full extended comparison passed.
- Final locked build: exit 0 (`work/party-runtime-requests-expel-verified-build.log`).
- Focused ASAN/UBSAN C++20 build and the complete 2,048-case comparison:
  exit 0, no diagnostics (`work/party-runtime-requests-expel-verified-sanitized.log`).
- Original EXE/PDB manifest, exact final fixture `cmp`, and `git diff --check`:
  exit 0 (`work/party-runtime-requests-expel-final-hashes.log`).
- Final locked native tests: 131/131 passed, 38.90 seconds, exit 0
  (`work/party-runtime-requests-expel-verified-test.log`). Earlier complete pass:
  131/131, 34.64 seconds. No SQL operations were performed.

Production integration still requires the retained Actor binding documented in
Block 9, live recursive lock/lookup/transport/log/pool adapters, complete raid
request/entry/leave providers and guild cleanup. No new canonical data field is
needed by expulsion: use the SAME live member.party and registry-party handle.
Remaining independent scope includes original raid requests and invitation
expiry scheduling evidence; regular raid gameplay is not declared implemented.
SQL integration/lifecycle remains coordinator-owned and was not run for this
memory/protocol block. These comparisons do not establish a playable server.

Block 10 implementation commit: `709df36b18f1933e8ccb881d6ccf904571ec15ac`.

## Block 11 — expulsion callback reentrancy

The repeated coordinator assignment's normal ExpelUser body was already
implemented in Block 10. This block closes a distinct evidence gap: a real
nested PacketParty leave from the expulsion's transport or scene callback,
not merely a nested invitation accept or modeled recursive state changes.

Original `ps_game.exe CParty::ExpelUser 00447120` emits its 0b06 at 004471b3,
then compacts using the LIVE count and unconditionally decrements at 004471d4.
If that broadcast synchronously invokes PacketParty 004676c0 leave on the other
member of a two-user party, real LeaveNormalParty 00446960 recursively removes
both users and unregisters the party before ExpelUser resumes. The outer count
then becomes -1. Original normal ChkPartyExpByLevel's signed <=1 branch
(00444df0 to 00444e2f) retains the previous maximum. A subsequent ExpelUser on
that same retained object returns false through its signed nonpositive-count
guard (00447152..00447176). Block 10 native code instead rejected negative count
in refresh, and its equality-only exhausted-search check missed that guard.
Both are now corrected. No clamp, extra unregister or invented recycling is
added. The party remains retained through the outer scene and log callbacks.

The oracle now adds 512 expulsion scenarios. Saved-register x86 trampolines
execute actual nested PacketParty leave from the 0b06 CParty::Send boundary
(RET 0) or CZone::SendView boundary (RET 12), then resume the original instruction
stream and return ABI. Event W records the selected live user before that call.
Native callbacks invoke the same dispatcher with the same retained live binding.
This tests callback-driven compaction before the outer compaction, recursive
unregister, state changes before the outer log, and repeated direct removal
on the SAME objects afterward. Required raid/guild providers remain unchanged;
no Actor/connection/zone or inventory owner changes were needed.

Expanded matrix: 2,560 scenarios, 2,606 whole top-level PacketParty executions,
3,072 whole direct ExpelUser executions, 46 nested accepts and 282 nested leaves
(328 reentrant scenarios). Twenty-six scenarios produce the original negative
count and retain the original experience maximum. Final native comparisons
include every packet, callback-state hash, full final projection, direct return
byte and lock balance, including the signed count bit pattern.

Validation:

- Full repeated original fixture generation: byte-identical (`cmp`, exit 0).
- Original EXE/PDB manifest: exit 0
  (`work/party-runtime-requests-reentrant-hashes.log`).
- Locked focused C++20 ASAN/UBSAN build and all 2,560 comparisons: exit 0,
  no diagnostics (`work/party-runtime-requests-reentrant-sanitized.log`).
- Locked native build: exit 0, no compiler diagnostics
  (`work/party-runtime-requests-reentrant-build.log`).
- Locked native tests: 131/131 passed, 33.35 seconds, exit 0
  (`work/party-runtime-requests-reentrant-test.log`).
- `git diff --check`: exit 0. No SQL/schema/lifecycle operations performed.
Implementation commit: `c656778bd73bf0dedcb048ff6151ca85271d7c30`.

Integration contract remains Block 9/10's single retained membership owner,
recursive locks, real transport/log/lookup/pool and explicit raid/guild providers.
A callback may change live membership while an outer call is still active;
unregister must retain the party and selected users until that call unwinds.
Do not normalize the recovered signed count in this owner. This block does not
claim safe original behavior for later unrelated operations on a negative-count
party, or provide the missing production Actor binding. SQL lifecycle/full SQL
integration stays with the coordinator. Server gameplay completion is open.

## Block 12 — actual regular raid entry and leave

Resolved complete `ps_game.exe CParty::EnterRaidParty 00445a30..00445fda`,
`LeaveRaidParty 00446510..00446958`, and `GetEmptySlot 00446480..004464b2`
from the complete Ghidra symbol/disassembly inventory. Both membership routines
use ECX=party, stack argument=user (unlike ExpelUser's recovered convention).
New files use only party_runtime_raid; existing canonical Actor/inventory/zone
headers and all other owners remain untouched.

The native functions operate on the existing PartyRegistryParty's SAME ordered
30-entry array (seven inherited entries plus 23 extra entries), indices, count,
leader, subleader, cursor and experience state. No second membership owner or
raid success stub was added. PartyRaidRequestWorld implements the inherited
required enter_raid/leave_raid routes, so the existing PacketParty dispatcher
and its default actual ExpelUser now compose with actual raid membership.
Full other raid request opcodes remain an explicit required provider.

Recovered entry behavior:

- Count >=30 returns false before clock or locks. Otherwise clock then party
  lock; GetEmptySlot scans index values of ALL entries below count, including
  holes, returning the lowest unused 0..29 or -1. An absent user pointer does
  not make its index available. A -1 result unlocks and returns false.
- Append pointer AND new index at live count, set user.party before sends. When
  count>0, broadcast 0b0c and send the identical 0b0c directly to the new user,
  BEFORE count increment. This member record includes the wire index and up to
  40 visible effects using the same original signed wrapped deadline arithmetic.
- Capture 0b0b roster header once: continuation, leader/subleader wire indices,
  division WORD, raid flag, auto-join flag and count. Scan all 30 pointer slots,
  skip holes, include the joining member, and send chunks of six with at most
  35 effects per record. Full-chunk continuation depends on LIVE count+1 modulo
  six, not which chunk is last. A trailing partial chunk is sent only on reaching
  count+1 non-null entries; scan exhaustion alone does not flush it. Inactive
  tail pointers are not cleared or ignored by this scan.
- Increment count, clear pending byte +1744, refresh max experience, unlock;
  then shared 0520 scene notification and 0f01 log. Inviter DWORD is separate.

Recovered leave behavior:

- Lock, scan pointer identity below live count, skip holes; missing returns
  false with unlock only. Capture leader/subleader-removal flags, clear party,
  and broadcast 0b0d while the leaver is still in the recipient array.
- Compact BOTH pointers and indices, leaving inactive tail untouched. For each
  shifted source position decrement matching leader, ELSE matching subleader.
  Decrement item cursor only when removed_position+1 < cursor, then --count.
- Zero count unregisters; one remaining member recursively invokes raid Leave
  when current kind==1, otherwise the existing normal Leave owner. No recycling
  or free. With >=2, removed leader promotes the shifted subleader; replacement
  subleader is the first non-null array position excluding the leader. Preserve
  holes and the original retained indices when no replacement exists.
- Required guild cleanup precedes max refresh. Leader change broadcasts 0b17,
  reloads the selected leader after that callback; subleader change broadcasts
  0b13. Unlock, scene for departed user then changed leader (not subleader),
  then shared 0f02 log. Recursive notifications are retained, including original
  inactive-tail behavior after dissolution.

PartyRaidMembershipWorld supplies 30-slot GetPartyType with the recovered
capture/recursive-lock/recheck algorithm and the 30-slot CParty::Send loop.
It REUSES existing shared buffer allocation/copy/release and direct connection
send, with real pool/worker/close providers still mandatory. The original seven
slot send/type defaults are not sufficient for raid: production must select
this world adapter or an equivalent real 30-slot provider. Existing normal
parties can use it because every bound party is an actual PartyRegistryParty.

Validation:

- 256 retained original sequences: 768 whole PacketParty calls (accept, leave,
  and leader-driven expulsion), 256 direct EnterRaidParty, 512 direct
  LeaveRaidParty and 256 direct ExpelUser calls, plus 84 literal nested
  PacketParty calls. Bodies, registry/map operations, experience and 30-slot
  GetPartyType execute in the original EXE. No raid membership body is patched.
- Matrix spans counts 0..30, nonsequential/duplicate wire indices, pointer holes,
  leader/subleader beyond inherited seven slots, full capacity, multi-chunk
  rosters, 65-effect users exercising both truncation limits and shared effect
  cursor, tick wrap, automatic join flag, recursive unregister and callback
  leave during scene/expulsion. Callback snapshots include all 30 slots/indices,
  signed count, leader/subleader/cursor/max, auto-join, membership/request/guild
  state and effect cursor. Full packets, return values and final state match.
- Traces include 1,233 raid roster packets, 468 broadcast AND direct 0b0c pairs,
  363 raid departure broadcasts, 475 composed expulsion broadcasts and 124
  required guild-cleanup calls. Guild cleanup is an explicit input projection
  of inline world blocks 0044670a..00446806, 0044724d..00447325 and the existing
  normal cleanup 00446ac1..00446bae; its implementation is not claimed recovered.
- Separate 512 complete original CParty::Send/AllocBuffer/connection/agent
  comparisons cover 30 slots, holes, socket -1 versus 0, allocation failure,
  live count/slot/socket/type mutations and immediate/delayed completion. Native
  uses existing shared buffer allocator and releases, tests ensure exactly one
  recycle after the final reference. No production no-op pool/worker exists.
- Locked native build exits 0, no diagnostics
  (`work/party-runtime-raid-complete-build.log`, incremental final harness build
  `work/party-runtime-raid-guild-build.log`). Initial focused expansion used a
  stale hash writer compiled with 32 bytes instead of eight; the rebuilt
  harness corrected that test bug before final comparisons.
- Final locked tests: 133/133 passed, 42.78 seconds, exit 0
  (`work/party-runtime-raid-final-test.log`). Earlier 133/133 pass: 46.46 seconds.
- Locked focused C++20 ASAN/UBSAN compilation and both matrices: exit 0, no
  diagnostics (`work/party-runtime-raid-sanitized.log`). Re-run with final added
  PacketParty-to-expel sequences also exits 0, no diagnostics
  (`work/party-runtime-raid-final-sanitized.log`).
- Both repeated original fixtures compare byte for byte (`cmp`, exit 0),
  including return-sentinel assertions on whole calls. Original EXE/PDB manifest
  and `git diff --check`: exit 0 (`work/party-runtime-raid-final-hashes.log`).
  Generated assembly, fixtures and logs stay in ignored work/. Full isolated SQL
  integration remains coordinator-owned and was not run in this memory block.
Implementation commit: `ba4f407a3ce54019f686fb3500a51d716f02ae79`.

Required production contract: keep one retained registry party and live
PartyMembershipUser per Actor; the root must hold selected parties/users across
ALL nested callbacks and recursive Leave, including after member.party clears
and map unregister. A raw map/slot pointer is not lifetime ownership. Use the
Block 9 retained Actor binding and new raid-aware world/request adapters; no new
canonical field or duplicate party handle is requested. Scene/log/guild cleanup,
lookup, recursive locks, real transport/pool/worker and remaining raid request
provider are required. Guild cleanup remains a projected mandatory boundary,
not a recovered guild implementation. No SQL lifecycle operations or main edits.
Regular raid creation/conversion/opcode dispatch and root Actor assembly remain
open; component tests do not establish client playability.

## Block 13 — raid membership with actual original transport composition

The repeated raid entry/leave assignment is already implemented in Block 12.
This block closes its transport integration evidence gap without duplicating or
changing a production membership/send body. The new `--live-send` oracle mode
executes the SAME entire original raid/request/expulsion sequence with actual
`ps_game.exe CParty::Send 004455d0`, `SConnection::Send 004d4cf0/004d4d20`,
`SPacketBufferSend::AllocBuffer 004df9f0` and `SAgent::SendRequest 004d8950`.
The earlier matrix observed the party/direct send boundaries; the new mode
leaves their original instructions intact.

Only size-class pool allocation (004df710/004df7b0/004df850), pool return
(004df070), worker SendPost (004dc1b0), atomic/lock OS operations and the existing
lookup/scene/log/guild boundaries are providers. Worker completion explicitly
releases its owned reference immediately; it does not model an asynchronous
network connection. The separate 512-send matrix still covers delayed
completion. Original whole-function return sentinels remain asserted.

The native fixture world now derives from the EXISTING PartyRaidMembershipWorld
and delegates to its actual party and direct send methods in this mode. It
retains the same live user records, request state, registry, buffers and party
objects through all nested calls. No second membership or transport owner was
added. Observed E events compare user identity, reference count BEFORE completion
and every byte of the length-prefixed frame; B asserts zero references. This
checks recipient inclusion/exclusion during entry/departure/expulsion rather
than comparing just a serialized broadcast payload.

Validation:

- 64 retained sequences: 192 whole original PacketParty calls, 64 EnterRaidParty,
  128 LeaveRaidParty, 64 ExpelUser calls, plus 10 literal scene-callback nested
  leaves. The prior broadcast-callback reentrant cases remain in the original
  256-sequence matrix; this live-send matrix selects ordinary broadcast execution
  instead of the old observed broadcast callback trampoline.
- Socket inputs are -1/0/5; only -1 suppresses an actual connection send. Count,
  holes, 30-slot recipient state, header/roster/effect truncation, guild cleanup,
  recursive unregister and experience/state comparisons remain in place.
- Original traces: 538 pool allocation attempts, including 41 explicit failures;
  2,650 worker deliveries carrying 397,170 frame bytes; 497 final zero-reference
  frees. Every callback-state hash, final state, packet and return byte matches
  native, including continued membership progression when allocation fails.
- Locked native build: exit 0, no diagnostics
  (`work/party-runtime-raid-live-build.log`).
- Locked native tests: 134/134 passed, 45.63 seconds, exit 0
  (`work/party-runtime-raid-live-test.log`).
- Locked focused C++20 ASAN/UBSAN build and both 256-sequence observed-boundary
  and 64-sequence live-send comparisons: exit 0, no diagnostics
  (`work/party-runtime-raid-live-sanitized.log`).
- Exact repeated live-send fixture `cmp`, original EXE/PDB manifest and
  `git diff --check`: exit 0 (`work/party-runtime-raid-live-hashes.log`).
  Generated fixtures/logs remain ignored under work/. No SQL operations; full
  isolated SQL integration remains coordinator-owned.

Implementation/evidence commit: `5fc2198bef33c19385743deb644ee42608e4eac6`.

Production dependency remains unchanged: retained Actor binding and the actual
root world must supply the mandatory pool, worker, socket close, lookup,
recursive lock, scene/log and guild services. These test providers are explicit
observations, not production services. The integration must select the existing
raid-aware world adapter for all registry parties; its 30-slot broadcast and
classification are now verified in membership/request compositions. No canonical
Actor/zone/inventory change, extra party field, guessed heap return or main edit
was needed. Actual raid creation/conversion and remaining PacketParty raid
opcodes are concrete next scope; playable-server completion is not claimed.

## Assignment checkpoint — repeated EnterRaidParty/LeaveRaidParty scope

Re-read current coordinator continuity/parallel ownership and reviewed commits
ba4f407/1c14552 and 5fc2198/d38c171. Coordinator continuity now records the
regular raid membership review and queues it during the fourteenth integration
freeze. The repeated assignment's actual entry/leave and PacketParty/ExpelUser
compositions are implemented and verified in Blocks 12/13, including actual
original transport composition. No additional implementation gap in those
assigned routines was identified in this checkpoint; no tests were rerun or
new gameplay coverage claimed. Worker tree was clean before this report update;
`git diff --check` passes. This is a documentation-only handoff.

Concrete next-task proposal within a newly confirmed raid dispatch scope:
recover `ps_game.exe CParty::SetRaidParty 004458f0` and its callers in
`CUser::PacketParty 004676c0`, composing normal-to-raid conversion with the SAME
registry/membership owner and actual send adapter. Resolve the related leader
operations (`SetRaidPartyBoth 004451e0/004452f0`,
`SetRaidPartySubBoth 00445220/004452a0`) and distinguish raid-specific expulsion
`ExpelRaidPartyUser 00446cd0` from the already recovered normal-request
ExpelUser. Addresses are confirmed by complete Ghidra functions.tsv inventory;
these bodies are NOT claimed implemented by this checkpoint.

Separately, coordinator/root owners must supply the retained Actor binding and
mandatory real pool/worker/lookup/scene/log/guild services documented above.
Canonical Actor/inventory/zone sources remain read-only. Further repeated tests
of the same entry/leave functions do not close those production dependencies.
