# Movement continuation evidence

Executable: `ps_game.exe`, SHA-256
`0fb7c9f13b5bcf186118f8bfe73ab214226b93e8aa502b8e6fded4a8312bc547`.
The continuation and its side effects below are static disassembly; the two
geometry helpers and native membership subset already have oracle coverage as
described in `research/protocol.md` and `tests/maps_unit.cpp`.

- `PacketPC` `0046a47b..0046a497` passes the packet XYZ and current zone to
  `00416550`; a false result exits the movement branch. On success it stores
  packet direction at user `+166`, samples the clock into `+57cc`, and stores
  packet motion at `+1435` (`0046a49d..0046a4b7`).
- `00416550` acquires the user lock at `+5e98`, requires location DWORD `+5758`
  equal to 4, and calls `00415360` before writing XYZ to `+d0/+d4/+d8`
  (`00416550..004165b6`). `00415360` is the previously recovered CMap::IsSafePos:
  zero-inclusive, map-size-exclusive X/Z checks with original x87 NaN behavior.
  Precise zone-pointer preconditions are still to be traced.
- After the XYZ write, it consults the current cell with `004153c0` and, when
  needed, calculates target cell indices, checks their bounds, calls `0040f200`,
  `00423560`, and `0041f350`, then updates user cell coordinates `+e4/+e8`
  (`004165bc..0041669f`). `004153c0` is the previously recovered overlapping
  CCell::IsInnerCellView. A target-cell bounds failure calls `004d43a0` with
  reason 9 and returns false (`004166bc..004166db`). Therefore a naive wrapper
  that rejects all invalid cell transitions before any XYZ write cannot be
  assumed equivalent. Existing `ZoneUsers::move_at_position` already stores the
  position in this order; production close and cell notifications are separate.
- Successful return continues into status/motion handling and a 21-byte `0501`
  view publication (`0046a4b4..0046a553`). A failed move skips that continuation.

Next: compose the existing geometry helpers with the actual `0040f200`,
`00423560`, and `0041f350` effects, including same-cell, crossing, failed bounds, observer order,
reentrancy and user lifetime. Compare original traces before integrating the
existing `ZoneUsers::move_at_position`; no claim of movement or persistence
completion follows from this static evidence.

## Cell-change publication, initial static evidence

`ps_game.exe` `0041f350` computes two clipped rectangles extending two cells
in each axis around the supplied cell coordinates (`0041f350..0041f404`). It
constructs a six-byte `0202` containing the moving user ID and allocates its
shared send buffer (`0041f404..0041f42c`). The first loop visits cells of one
rectangle outside the other and calls `004243d0` and the already identified
`CCell::SendCellInfoLeave` `00424130` (`0041f432..0041f4bf`). The subsequent
path constructs a `0201` appearance payload and later calls `004243d0` and
`CCell::SendCellInfoEnter` `00423aa0` (`0041f4e3..0041f6bc`). Buffer allocation,
visibility predicates, exact direction/loop ordering, reentrant cell traversal,
and all related packet lengths still need execution-based recovery. The earlier
mob-only `SendMoveMob` tests do not establish user-notification equivalence.

## Existing owners and integration gaps

- `ZoneUsers` owns the canonical user/cell membership and shared cell cursor;
  `visit_cell` is live and may reenter. The existing geometry/membership tests
  cover user insertion/removal, but `move_at_position` does not expose the old
  and new cell to a publisher or invoke any visibility transition.
- `ZoneNpcs`, `ZoneMobs`, `ZoneCorpses` and `ZoneItems` already expose portions of
  cell snapshot behavior, with entity-specific ordering and lifetime contracts.
  These are not a complete `CCell::SendCellInfoEnter/Leave` assembly: user,
  object and transport owners must be bound together in original order.
- `ZoneActorEntryServices::cell_info` is a caller-supplied service, not a real
  implementation of cell snapshots. `VerticalSceneServices::cell_view` is also
  a required port, not evidence of a production broadcaster.
- `ZoneUsers::move` inserts the destination member before unlinking the source,
  with no callback between operations; `00416550` calls source unlink before
  `CCell::EnterUser`. Matching observable reentrancy and insertion failure still
  requires actual cell notification and allocation ownership, not simply calling
  `move_at_position` followed by a synthetic packet.

## Initial executed probe

`tools/vertical_user_move_probe.py` executes the original `00416550` and real
geometry helpers for four controlled cases, with locks, cell unlink/link,
notifications and close replaced by recording ports. In the chosen initial cell,
(20,7,20) stays in place, (65,7,20) calls unlink, link and notification in
that order, and invalid X or non-world location return false without changing
position. During all three crossing callbacks XYZ already contains the target,
while the public cell coordinates still contain the old cell. The probe asserts
these observations and exits successfully. These four cases do not verify actual
list mutation, recipient selection, notification bytes, errors or concurrency.

The executed probe also checks 64 finite X/Z combinations around zero, map
size and the overlapping cell boundary (50), asserting result, position,
reported cell and callback sequence. It uses a valid 5x5 grid and ported
callbacks. Nonfinite coordinates, inconsistent grids, real cell lists and
notifications remain outside this matrix.

Four additional original executions cover NaN, infinity and an inconsistent
zone grid. NaN X with Z inside the current view succeeds and stores NaN; NaN X
with Z outside that view writes position then invokes the ported close path.
Positive infinity X is rejected before position mutation. A finite target in an
inconsistently narrow grid also stores position before invoking close. These are
isolated edge cases; the probe does not exercise actual close or list callbacks.

`tests/zone_users_unit.cpp` now runs the same 64 finite boundary inputs against
native `ZoneUsers::move_at_position`, checking return, position and destination
membership, plus same-view and cross-view NaN position behavior. The focused test
passed with current `zone_users.cpp` and `maps.cpp` linked against existing static
libraries. This compares the observed cases, not the full original movement
routine: close, live lists, view packets, concurrency and SQL remain unverified.

`tools/vertical_user_view_probe.py` executes original `0041f350` with allocation,
reference counting, per-cell broadcast and cell snapshots ported. Four normal
cases (horizontal, diagonal, clipped edge and unchanged cell) match an explicit
row-major rectangle-difference trace. With the tested zeroed user state, leaving
cells receive `0202` and a leave-snapshot callback, then newly visible cells
receive `0201`, `051c`, an enter-snapshot callback, and a later `051d` pass.
Four buffers are requested even for unchanged cells. Probe checks opcodes and
cell order only; full payload bytes, live recipients, allocation failures,
other user states and callback reentrancy remain unverified.

The normal-state probe now checks 625 source/destination pairs sampled across
corners, edges and interior of a 10x10 grid. All match the same explicit ordered
rectangle-difference model and request four buffers. As before, callbacks and
allocations succeed as ports; payload bytes and other user states are not covered.

For a normal horizontal crossing, the original probe now checks all 16 failure
masks of its four buffer allocations. Failing allocation 1 omits the entire
old-cell `0202`/leave-snapshot pass; failure of either allocation 2 or 3 omits
the new-cell `0201`/`051c`/enter-snapshot pass; failure of allocation 4 omits
only the separate `051d` pass. All four allocations are attempted under these
conditions. The test asserts exact callback/cell/opcode order; actual buffer
management, packet contents and user-state variants remain outside its scope.

`user_view_difference` now implements the ordered old-only/new-only cell
traversal over clipped rectangles, with explicit callbacks. A fixture generated
from the actual 0041f350 snapshot callbacks compares 625 cases and passes a
focused native test. It implements traversal only: the four allocations, actual
buffer/packet behavior, per-cell snapshots, live recipients, reentry and the
additional appearance pass still require the real world owners.

`publish_user_view_difference` now orchestrates allocation, leave/enter snapshot
passes, buffer releases and the conditionally requested shape pass through required
services. The caller supplies original packet construction, real buffers,
recipients and shape-state predicate. A native focused test passed the 625 original
traversals and all 16 observed allocation-failure traces for the tested normal
crossing, plus suppression of shape allocation. These provider-bound passes are
not yet connected to the live world or verified for callback reentry/user variants.

The original probe now covers six shape-state cases including zero/nonzero shape,
vehicle mode 2 with summed shape values, and changing the live shape during an
enter-snapshot callback. It confirms the optional 051d decision and byte are read
after that callback. Native publication fixture separately verifies its shape
predicate is sampled after enter callbacks. The actual shape provider and full
packet delivery are still not connected.

`user_view_leave_packet` and `user_view_shape_packet` now construct the 0202
and optional 051d from the actual Actor. Original shape probing includes a vehicle
sum of 256: the full nonzero DWORD controls emission, while only the low byte is
sent (zero). The native focused scene test verifies that case, invisibility and
leave bytes. Appearance/equipment buffers, actual allocation and delivery remain
separate and the new builders are not yet bound to production movement.

The moved-user `0201` builder now uses the actual Actor ID/vehicle state and
caller-supplied live permission, direction, position and guild level. Two original
executions validate the exact 29-byte payload for permissions 10/11 and vehicle
mode 3/ordinary mode; the native scene fixture passes matching examples. The
source must sample these values after leave callbacks; no live zone provider,
real broadcast buffer or packet-order integration is implemented yet.

Eight original 0041f350 executions now verify the complete 051c speed packet
for normal, clamped, negative, action-blocked, casting, fixed-form and restricted
movement states. The existing native `character_speed` is reusable; four matching
native scene assertions passed. Real allocation, late sampling, recipient delivery
and production movement remain unconnected.

`UserViewActorPublication` now supplies the 0202/0201/051c/051d packet builders
from the Actor and a required live-state/transport interface. A focused fixture
verifies appearance and speed reflect changes during leave callbacks, and shape
reflects changes during enter callbacks. This is not yet a production broadcaster
or complete cell-snapshot implementation.

`tools/user_cell_broadcast_probe.py` executes original `004243d0` over a
three-user intrusive list. With the send port unchanged it visits all three;
when the send port models `0040f225` unlink writes for the current user, the
broadcast stops after that user because the original reads its now-cleared next
link. Locks and send are ports and unlink writes are modeled, not executed as a
nested original call. `ZoneUsers::visit_cell` instead repairs its shared cursor
when the current member unlinks and can continue, so it must not be wired as an
assumed-equivalent broadcast without a retained-node/next-link design and further
original tests (other-node removal, appends, reentry and lifetime).

The 004243d0 probe now covers normal traversal, removal of current/next/tail,
append and removal/reinsertion of current; send and lock remain ported and the
list edits model original pointer writes. `ZoneUsers::broadcast_cell` implements
a separate pinned local-link traversal; focused native tests pass normal, current/
next removal, append and current reinsert. It explicitly rejects current-actor
cross-cell reentry pending recovery and requires retention without membership
mutation. Actual transport, nested broadcasts and complete lifetime behavior
are not yet integrated or proven.

The cell broadcast probe now enters a nested real `004243d0` call while the
first outer send is ported. Without mutations, original and native both visit
`outer 0, inner 0/1/2, outer 1/2`; the focused native test passed. Locks/send are
still ports, and nested mutation, cross-cell transfer and real recipient
lifetimes remain unverified.

The nested original 004243d0 probe now also models removal of the current or
next user during the first inner send. The observed sequences are respectively
`0,0` and `0,0,2,2`; the native local-link broadcaster passes corresponding
cases. The unlink is modeled as original pointer writes rather than a nested
original Remove call; cross-cell movement, actor recycling and actual socket
retention remain unverified.

`UserViewCellTransport` composes the visibility coordinator with the canonical
ZoneUsers local-link broadcaster, while requiring external owner services for
packet buffers, actor retention, socket enqueue and cell entity snapshots. A
focused fixture with actual Actor/ZoneUsers membership verified old-only recipient
gets 0202, new-only recipient gets 0201 and 051c, and overlapping recipient gets
neither. Buffer ownership and snapshots in that fixture are simulated; live
session/socket routing, cross-cell reentry and production integration remain open.

The move probe now reenters actual `00416550` during the ported notification:
an outer move toward cell (1,0) triggers an inner move to position (120,9,20)
with target cell (3,0). After both return, XYZ remains the inner position but
the outer call overwrites the public cell coordinates with (1,0). List unlink,
link and notification are ported, so actual membership and nested-list safety
are not established. A native implementation must not silently overwrite live
position with the outer destination or assume public cell always matches it.

`tools/user_move_nested_lists_probe.py` executes the real 0040f200 unlink and
00423560 link during the nested 00416550 scenario, with locks and notification
ported. In this constructed reentry, the link ends in cell (3,0), public cell
ends (1,0), and counts for cells 0..3 become `ffffffff,1,0,1` while cells 0 and
1 are both empty. Thus this deliberately reentrant fixture exposes inconsistent
original counts; it does not prove production callbacks permit this reentry or
justify recreating unsafe behavior without the real lifecycle/locking contract.

## Full cell snapshot ordering (static disassembly)

The original `CCell::SendCellInfoEnter` traverses the first five cell lists in this
order: users (`cell+08`, `0201`, beginning at `00423ab7`); corpses
(`cell+b8`, beginning at `00423ccb`); mobs (`cell+34`, `0601`, beginning at
`00423ea7`); NPCs (`cell+60`, `0e01`, beginning at `00423f99`); and ground
items (`cell+8c`, `0401`, beginning at `00424018`); the sixth list is doors
(`cell+dc`, `0403`, beginning at `004240a7`). The corresponding
`SendCellInfoLeave` traverses those same lists in the same order at `0042413a`,
`004241b0`, `0042420e`, `00424270`, and `004242be`, respectively. Its first
four packet opcodes are `0202`, `0406`, `0602`, and `0e02`. These are static
observations of the original executable, not an integrated runtime trace.

The existing mob, corpse and item snapshot methods cover only individual list
blocks. `ZoneNpcs` exposes its cell members but no equivalent snapshot sender,
and the user snapshot must reproduce all original filtering, extra packets and
live session state. An implementation that sends only the three existing blocks
would omit visible entities; preserve this ordering when those missing blocks
and the actual session queue are recovered and tested together.

`tools/npc_cell_snapshot_probe.py` now executes the original whole-cell enter
and leave routines with zero through three NPCs linked into the real cell list.
The 23-byte `0e01` and six-byte `0e02` packets match the existing NPC packet
fields and insertion order. Two additional synthetic send callbacks overwrite
the shared NPC list cursor with the last node; both original loops then stop
following the first delivery. Locks and the connection send are ports, and this
artificial cursor mutation does not establish that a live socket callback can
cause reentry. Nonetheless, `ZoneNpcs` currently stores cell members in vectors
and has no shared traversal cursor: blindly iterating a copied vector would not
reproduce this original behavior. Recover and test actual nested list operations
and lifetime rules before wiring the NPC block into production snapshots.

`ZoneNpcs::send_enter` and `send_leave` now construct the recovered packets
and traverse an insertion-ordered cell list using a shared cursor. Unlinking the
current NPC rewinds that cursor to its predecessor, as in `00416ebf..00416ec8`;
nested traversals share it. The focused C++ test checks normal order, removing
the current NPC, and a nested traversal, alongside four existing original NPC
initialization cases. The separate original probe covers ordinary packet bytes
and synthetic cursor changes, not the new C++ nested scenario directly. The
zone owner must serialize access and retain current NPCs during callbacks.
The full snapshot assembler, real session wiring, complete build, SQL tests and
client validation remain outstanding.

The NPC probe now also executes a real nested `SendCellInfoEnter` through a
register-preserving trampoline in the send port. With three NPCs the original
packet IDs are outer first, inner first/second/third, then outer
first/second/third, matching the native nested snapshot test. A second case
calls original `LeaveNpc` `00416e90` from the send port for the current NPC;
the remaining original packets are second/third, also matching the native test.
The global NPC-index removal and outgoing view broadcast are ports in that
removal case; the actual original cell unlink and cursor updates execute. These
are intentionally induced reentrant callbacks, not proof of normal production
reentry. After the native cursor changes, the focused movement test passed
1,536 original movements and 12 paths with 144 consecutive steps.

## User entry snapshot speed packet

`CCell::SendCellInfoEnter` writes opcode `051c` at `00423ac8` and the two
speed bytes at `00423c34`/`00423c8f`, but does not initialize its four ID
bytes on this path before the eight-byte send at `00423c9c`. The original
execution probe constructs a user in the actual cell list and fills the call
stack with `55` or `aa` before entry. It observes respectively
`1c05555555550000` and `1c05aaaaaaaa0000` for the same user ID `12345678`.
This confirms those bytes depend on prior stack contents in the controlled
case. The original transport and locks are ports. This is distinct from the
movement publication's `051c`; do not silently reuse `character_speed(id,...)`
for cell-entry snapshots as if the two original paths were identical. A native
implementation must explicitly choose and document how to handle these original
indeterminate bytes and validate the effect with an original client.

`user_cell_snapshot.hpp` now implements the user-list portion of the original
cell snapshots over `ZoneUsers::visit_cell`: per user, enter sends `0201`, an
optional `051d`, then `051c`; leave sends `0202`. It retains the actor over
callbacks and samples shape/speed after preceding sends. The owner explicitly
supplies the four indeterminate speed bytes once per snapshot; no production
policy or client verification is implied. A focused scene test exercises actual
zone membership, ordered appearance/shape/speed packets, and leave. It passed
with current source files linked against existing static libraries; the complete
cell assembler and production session integration remain outstanding.

`CellSnapshot` initially composed five live registry snapshot operations in the
original user, corpse, mob, NPC, item order for both entry and leave. A focused
scene test creates one actual member of each registry and confirms the emitted
opcode sequence `0201 051d 051c 0405 0601 0e01 0401` on entry and
`0202 0406 0602 0e02 0402` on leave. This test links current changed zone source
against existing static libraries. Registry serialization, actor and entity
lifetimes, an explicit policy for the original indeterminate bytes, real session
queue wiring and end-to-end original-client validation remain owner obligations.

`UserViewSnapshotTransport` joins moving-user publication to the complete cell
snapshot, while `UserCellSnapshotChannel` delivers its direct packets to a
provided `WorldChannel` with explicit actor retention, appearance and speed-byte
providers. The focused scene test invokes the real view-difference publication
with a corpse in a departing cell and an NPC in an entering cell; their removal
and appearance arrive in order at a test channel. This exercises composition,
not production world ownership or a live client. The actual connection/session
must supply the channel, entity registries, retention and byte policy, and the
movement routine still needs to invoke this path.

Initial map entry calls `CZone::SendCellInfo` `0041f290`, which visits the
clipped radius-two rectangle with Z outer and X inner and sends each complete
cell snapshot. `tools/cell_entry_view_probe.py` executes the original routine
for all 25 centers of a 5x5 grid, porting only the per-cell sender; native
`visit_cell_entry_view` matches those traces. `bind_entry_cell_snapshots` binds
that traversal to the existing `ZoneActorEntryServices::cell_info` callback.
A focused scene test checks every center and invokes the bound callback with
real entity registries. Production must still create and bind these services
with the correct session and zone owners; no original-client connection or
full runtime integration has been demonstrated.

The focused scene test now invokes `enter_zone_actor` with a real `RuntimeZone`,
`ZoneUsers`, environment, actor/scene, and all five cell registries. Its bound
entry-view service delivers a corpse in the entry cell and an NPC two cells
away through `UserCellSnapshotChannel`; the test also checks admission, view,
obelisk and NProtect callbacks. The channel and remaining external services are
fixture implementations, and existing static libraries were reused. This is
not a production session, SQL integration, full build or original-client test.

The focused `vertical_protocol_gate_unit` now composes retained character loading,
world wait/zone wait, the client's `0201` gate, actor admission, and the full
cell-entry view with real zone registries. It verifies corpse and NPC snapshot
packets arrive through a provided channel, and all 60 original gate vectors
still pass. Socket, database, world services and channel in this test are
fixtures; the production world application still needs its own owner wiring.

The TCP handoff fixture now binds the full entry-view snapshot to a real
`WorldChannel` and populated zone registries. The connected peer receives the
corpse, entering user appearance/speed and NPC packets in cell traversal order
in both original tested AES-first and XOR-first handoff scenarios. The existing
invalid admission, input overflow and disconnect cases still pass. Character,
zone and external gameplay services are fixtures, so this does not establish
production world wiring or original-client compatibility.


The prior five-list snapshot was incomplete: `004240a7..00424112` also
visits the cell door list and sends four-byte `0403` (low-byte door ID and
open state) after ground items. Leave has no corresponding door block. The
original probe now executes two doors and verifies both entry packets and an
empty leave. `CellSnapshot` now requires the actual `ZoneEnvironment` and
sends its doors after items. Its per-cell shared cursor supports nested sends;
the current implementation requires serialized access and no structural
mutation of the door vector during callbacks. The focused scene test includes
an open door and passes with current source files; full production integration
and real-client validation remain outstanding.

The original probe now induces a nested `SendCellInfoEnter` from the first door
send: two doors produce IDs `12,12,13,12,13`, matching the native shared-cursor
unit test. The focused environment test also passed 384 original door, 512
weather and four synthetic environment preparations. The TCP handoff fixture now
includes a real initialized door and confirms its `0403` arrives after the corpse
in that cell and before subsequent cells in both AES-first and XOR-first cases.
The production session and original client remain unverified.

`vertical_move_user` now connects the existing canonical position/cell update
with the recovered view publication for the serialized, non-reentrant path.
It checks location and zone identity, applies the original safe-position order,
invokes close reason 9 after a failed changed-cell conversion, publishes while
public admission cell remains old, then stores the new public cell. A focused
scene test exercises invalid location/position, same-cell movement and a cell
crossing delivering real corpse/NPC snapshots. This is not complete PacketPC:
direction, motion, clock and `0501` continuation remain separate, and nested
movement/detach of the subject is explicitly outside this path's contract.
The existing 64-case original position fixture validates the underlying
geometry, not every composed side effect; production world wiring remains open.

`tools/vertical_movement_continuation_probe.py` executes original
`0046a49d..0046a558` across 60 motion, attack-motion and vehicle combinations,
with the clock, two status routines and prioritized transport ported. It checks
live field writes, status dispatch, vehicle cancellation, priority 7 and the
complete 21-byte `0501` view packet, including raw coordinate bits.
`vertical_move_continuation` implements these steps using the canonical actor,
entry direction and explicitly owned motion/tick state. The focused native
scene test checks the same 60 combinations. The actual
priority view and the subsequent guild/interaction/trade/PK effects are required
services, not implemented by this adapter. They and the full packet dispatch
still need production wiring and original-client validation.

`vertical_move_packet` composes the previously recovered stand/life/distance
prefix, speed check and correction, serialized position/cell move and success
continuation. It deliberately rereads packet XYZ after the check, as the original
at `0046a47b` does. The focused scene test verifies distance correction and the
ordered accepted path with actual actor/zone membership. Speed-table ownership, priority transport, remaining interaction/trade/PK effects and close are
required live services, and original nonfinite/reentrant limitations of the
individual stages remain. This composition is not yet used by the production
world application and has not been validated with an original client.

The movement speed selector `0046a36d..0046a3a8` and `GetMoveSpeedIndex`
`0048a530` now execute in `tools/vertical_movement_speed_select_probe.py`: 54
combinations cover modes 0/1/2/3/4/-1 and signed index extremes. Vehicle mode
2 selects 8.0; mode 3 adds 10000.0 to table entry 4; other modes index the
five-entry table with the raw equipment move-speed clamped to 0..4. The
executable's initial table is 3,4,5,6,7, but it resides in writable process data,
so native selection requires the owner to supply the live table. The focused
native scene test matches the 54 cases and the composed movement path. Actual
world table initialization and original-client validation remain outstanding.


`vertical_status_move` now implements original `0048a090` directly on the
canonical actor: a casting-mode attack restores the previous target when the
current target is self, then stops or schedules attack resumption depending on
the previous attack tick. `tools/user_status_move_probe.py` executes the real
original status and attack-stop routines with only the clock ported; the native
scene test matches 60 mode/target/tick cases. The remaining priority transport
and interaction/trade/PK completion services still require real world owners.

Original `CUser::PSendView` `004bd950` does not return ordinary send success:
it sends within radius 80 and increments signed user counter `+f8`; on the
fourth or later signed count it resets the counter, also invokes the 80..120
ring and returns true. Without a zone it does neither and returns false. The
14-case `user_priority_view_probe.py` executes the original coordinator with
both zone broadcasts ported, checking packet parameters, counter wrap and
return. `user_priority_view` and the focused native test match these cases;
`finish_move` now explicitly receives the wide-pass flag. Actual radius
selection, priority queues and socket ownership remain required services and
are not established by this coordinator test.

`user_priority_cell_probe.py` executes original `CCell::PSendRange` `00424780`
for 24 combinations of IDs and positions around radius 80. Original source and
extra IDs take immediate send even outside the radius; other users at distance
<=80 take the prioritized path, while more distant users do not. Locks, sqrt
CRT and both send routines are ports. `user_priority_cell` binds this selection
to the canonical `ZoneUsers` local-link traversal and retained actors; a scene
fixture checks special IDs and both sides of the 80 boundary. Actual shared
buffer ownership, socket availability, queue capacity and distance-based
priority ordering remain required services, as do the outer rectangle and ring
paths. The distance math has not been exhaustively compared for nonfinite or
rounding-edge inputs.


The original `CUser::PSend` `004bd6f0` and queue insertion `004e02a0`
combine `(priority << 16) | trunc(distance)` into a signed ordering key; equal
keys retain insertion order. The `user_priority_queue_probe.py` executes both
original functions with lock, atomic and node-allocation ports, covering ordering,
reference acquisition, allocation failure, disconnected socket and the 2048-byte
queue threshold. `UserPriorityQueue` implements that insertion and ownership over
the existing shared packet pool; the scene test covers stable ordering, draining,
disconnected sockets and the 2048-byte threshold. Queue drain integration with
actual `WorldChannel`, original asynchronous scheduling and production world
ownership remain unverified. This corrects the earlier description of distance
as a deadline: it is part of an ordering key.

Original priority extraction `004e04c0` first allocates a send batch and moves
up to 128 queued references, subtracting their framed lengths. The oracle now
checks a failed allocation preserves all 130 queued packets and that successful
extraction yields 128 then 2 without changing packet reference counts.
`UserPriorityQueue::take_batch` reproduces those boundaries and ownership;
`deliver_priority_batch` transfers each frame to a supplied WorldChannel and
releases the queue reference. A focused test checks two batches and channel
copies. Native WorldChannel posts packets individually, whereas the original
uses a gathered send; exact scheduling and production integration remain open.

`UserPrioritySharedCellServices` now connects the actual zone recipient
traversal to shared buffers and each recipient's priority queue, requiring the
world owner to supply the retained matching connection and queue. A focused
fixture checks immediate source delivery, deferred recipient delivery, reference
lifetime and batch release. A separate existing WorldServer TCP fixture now
sends a queued batch through a real encrypted WorldChannel and verifies the peer
receives its payload. Queue scheduling, actual per-session registry wiring, full
radius/ring traversal and the original client remain unverified.

The original `CCell::PSendRing` `00424880` includes both inner and outer
boundaries and directly sends the two special IDs regardless of distance. The
probe covers 30 combinations around 80 and 120, so a user exactly at 80 can be
sent in both near and ring passes. `user_priority_area_probe.py` also executes
the original near/ring zone traversal for ten finite positions, with buffer
allocation and cell sends ported; native `user_priority_area` reproduces the
clipped, row-major cell rectangle and separate shared buffer per pass. Focused
scene tests compose real zone membership, the shared pool, per-user priority
queue and a test channel. Full priority-view coordinator binding, queue timing,
nonfinite arithmetic and production session ownership remain unverified.

`UserPriorityZoneView` now composes the recovered cadence with the near/ring
cell traversal and per-recipient queue services; `VerticalMovePriorityServices`
binds PacketPC's priority send to that shared per-user counter. A focused test
uses actual user membership and shared buffers across four sends: the radius-80
recipient receives four near entries plus the fourth ring entry, the radius-120
recipient receives the fourth ring entry, and the source receives five immediate
sends. It drains/releases both queues after each call. Actual connection-owner
lifetime, scheduling, remaining movement effects and production wiring remain
required; this test is not an original-client test.

`vertical_move_interaction_probe.py` executes `0046a59f..0046a64d` in 20
combinations with real distance and exchange-reset code; socket send, PvP and
CRT sqrt are ports. A planar distance strictly greater than 10 decrements the
current NPC's user count and clears it, and resets both active trade records
before sending `0a04 02` and `0a04 03`. Equality and NaN do not cancel. The
native interaction adapter reuses canonical trade state, requires retained NPC
and peer owners, calls real guild/PvP providers in original order, and passes a
20-case focused scene test. `VerticalMoveWorldServices` binds this helper to the
priority-view continuation; guild-mark/PvP providers, actual world session and
full end-to-end behavior remain outstanding.

`vertical_move_pvp_probe.py` executes original `PvPChkLeaveByMove` `0047bef0`
for 50 state-five/other-state cases around distance 40, including missing peers
and NaN. Zone lookup, end-duel, transport, stack check and CRT sqrt are ports.
The preparation-state branch cancels only above 40, ends/sends to a found peer
before ending/sending to the moving actor, and still ends the actor if the peer
is missing. `vertical_move_pvp` matches these focused cases while explicitly
requiring the active duel and guild-duel branches from real world providers.
This does not reconstruct those remaining branches or establish production
integration.

The active state-3/4 movement branch now has 16 executed original cases with
wager drop, debuff cleanup, damage reset, sends, logging, lookup and sqrt as
ports. It drops/cleans the peer first when found, always drops/cleans the moving
actor, sends `2406 01` with the mover's ID to each, then logs the mover and
optionally the peer. The absent-peer case still cleans the mover. The native
`vertical_leave_active_duel` composes real wager drops and `Actor::end_duel`
with explicit peer retention and initial log-stack bytes; a focused duel test
checks present/missing peer ordering and all 768 pre-existing duel cases pass.
`VerticalMovePvpInteractionServices` connects the PvP check to the movement
interaction continuation. The guild-duel branch, production services and full
original-client behavior remain unverified.

`vertical_move_guild_pvp_probe.py` executes the original guild movement branch
`0047c161..0047c1dd` and the actual captured-guild lock helper `0042b800` in
192 cases, with lock/unlock, guild leave and CRT sqrt ported. It checks enemy
ID, state -1, strict distance >40, guild states 3/4, missing guild and guild
disappearance after locking. `vertical_move_guild_duel` implements that gate and
requires a real captured-lock/current-guild/leave provider; the matching native
scene matrix passes. `VerticalMoveGuildPvpServices` composes this gate with the
individual-duel movement services. The actual guild leave implementation,
production lock owner, full world session and original-client test are still
outstanding.

`guild_duel_leave_probe.py` executes original `CGuild::PvPLeaveGuild` `00438160`
for 64 combinations of participant count, leader, mode and opponent presence.
Lookup, locks, broadcasts, whole-guild reset and individual cleanup are ports.
`guild_duel_leave` implements its individual and terminal branches, reusing
actor effect/damage cleanup and requiring real guild locks, sends and member
reset services. The focused native scene matrix matches those 64 cases;
`GuildDuelMovementAdapter` joins the movement gate to retained guild/member
state. Actual catalog ownership and `00438070` reset-all implementation remain
required; the native state projection is not yet bound to production guilds.

`guild_duel_reset_members_probe.py` executes original `00438070` on a real
three-node in-order guild tree in 16 null-record/missing-user combinations;
debuff/damage cleanup and locks are ports. A null record terminates traversal,
while a null online user is skipped. `guild_duel_reset_members` now traverses
canonical `GuildMembership::online`, resolves and retains real actors, and
clears effects, damage and the original guild duel fields. The focused scene
test checks the null-record stop. `GuildDuelMembershipWorld` binds this to the
existing duel-leave coordinator with required live membership services. The
previous `+54fc` duplicate was corrected: it is the canonical
`Actor::relations().guild_duel_ready`, not separate session storage; the distinct
`+54f8` byte remains in the retained guild movement state. Live callback tree
mutation, production guild locks and original-client integration remain open.

`guild_duel_broadcast_probe.py` executes the original `00438320` and `004383c0`
with a real three-member tree for 16 guild-status, buffer-failure and null-row
cases. Both require a nonzero guild duel status and one shared allocation;
`00438320` filters the `+54f8` readiness byte, while `004383c0` filters the
`+54e4` opposing guild ID. `guild_duel_broadcast` now applies those filters
over canonical guild membership, and `GuildDuelMembershipWorld` binds the two
send paths and reset to the leave coordinator. The focused native matrix passes.
Actual guild/connection lookup, lock lifetimes, shared-buffer provider and
production world ownership remain necessary, and reentrant membership mutation
is not yet implemented.

The canonical `GuildDuelState` is now stored in each `LoadedGuild`, with an
identity pointer to that same retained catalog object. `GuildDuelCatalogWorld`
resolves the other guild and membership directly through `GuildCatalog` and
rejects foreign state, avoiding a parallel guild index. The catalog test now
checks this identity after every one of 768 original DB packet vectors across
96 sequences; it passes with the changed catalog source. This does not create
the production world executor, guild locks, actor/session mapping or duel
start/update handlers, and the complete build and original client remain
unverified.

`LoadedGuild` now owns the recursive mutex used by `GuildDuelCatalogWorld`;
`GuildDuelCatalogMovement` captures that lock from the actual `GuildUser`'s
membership, rechecks disappearance and rereads current membership as in
`0042b800`. A focused scene test loads a catalog guild, attaches a real member,
and executes distance-triggered finalization through catalog lookup, captured
lock, filtered broadcasts and canonical member cleanup. The 96-sequence catalog
oracle still passes and checks recursive lock/identity. This is not yet a
complete locking solution: production must use the same mutex for every guild
mutation and supply retained actor/session bindings. World startup, real client
and full build/integration tests remain outstanding.

`LoadedGuild` binds its canonical membership to the same recursive mutex used
by duel operations. Membership connect/disconnect, broadcasts and roster sends
now acquire it; guild-member catalog insertion also takes it. A focused
recursive-broadcast test and the original 72-sequence/648-operation membership
fixture pass, as do 96 catalog sequences and the composed guild-duel scene test
with current source files. Direct public map access, other guild resources,
actor/session lifetime and cross-owner world scheduling still require the same
serialization contract; no complete production lock audit or full build has
been performed.

`VerticalMoveSession` now binds the reconstructed movement packet stages to the
same retained actor, WorldAdmissionUser, zone and admission state. The TCP
handoff fixture dispatches a client `0501` after real admission, checks that
canonical position becomes 45 and receives the composed 21-byte movement
broadcast over the encrypted WorldChannel; both handoff orders and existing
failure/overflow/disconnect cases pass. The test provides stand, speed table,
priority and remaining interaction services as fixtures, so production world
ownership, full behavior and original-client validation are not established.

### In-world disconnect continuation

Original `CWorld::LeaveUser` (`0040ca20`), for location 4, was probed across 24 combinations of death, final-save flag, logout type and zone presence (`tools/world_disconnect_probe.py`). A first disconnect with logout type zero retains the world user, changes logout type to 2 and sets the logout tick to current tick plus 10,000 ms. A subsequent non-final call requests persistence; the final-save branch removes the world index and invokes the full zone leave when applicable, then continues into the original tail. Death handling precedes those branches. The probe stops at that tail and substitutes callbacks; it does not establish the effects of the complete zone leave or the tail.

`vertical_disconnect.hpp` models that branch with explicitly supplied services and preserves the zone/user lock order. `vertical_disconnect_unit.cpp` checks the 24 branch combinations and the retained-user movement gate. The service callbacks for zone removal, persistence and the final tail are still integration boundaries, and this adapter is not yet connected to the production session or the TCP fixture. A real client disconnect and complete cleanup remain unverified.

The in-zone half of `CZone`'s logout deadline update (`ps_game.exe` `004218ba..00421926`) visits the live zone-user list, compares nonzero deadlines as unsigned values against the supplied tick, clears a due deadline and then invokes `CWorld::LogoutUser` (`0040c860`). This half does not filter by logout type. The independent zone-waiting-list half requires type 2 and is implemented by `WorldAdmission::update_zone_wait_logout_deadlines`. `tools/zone_logout_update_probe.py` checks 80 original cases per traversal, including the unsigned wrap boundary, and `zone_logout_update.hpp` applies the in-zone half to the real `ZoneUsers` cursor with explicit owner retention. The unit test verifies all 160 matching cases and removal during each callback. The logout callback itself, scheduler wiring, persisted save and retained session ownership are still required.

`vertical_disconnect_tcp_unit.cpp` exercises a real `WorldServer` socket through selection, zone admission, disconnect, retained actor ownership, deadline expiry and final release. Its world owner retains the entire fixture session after the network application is destroyed; it verifies the same actor remains in the world and zone until a simulated save acknowledgment. Save preparation, database request, full zone leave and final tail are test doubles, and this ownership composition is not yet wired into the production roster application. The test does not establish production persistence or a playable server.

The DB-agent disconnect packet is `0302` plus little-endian account ID, generated by `ps_game.exe` at `0046c660`; `tools/user_disconnect_db_packet_probe.py` checks five IDs against that original routine. The TCP fixture now routes this packet to the reconstructed `UserLifecycle`, dispatches its job 9 through the recovered exit-save orchestration, and checks the `0302`/`0303` DB-agent notifications and ordering. Persistence, social operations, zone leave, notification delivery back to the game world and final acknowledgment are still simulated; the production server is not wired to this path. The handoff fixture also now retains its map geometry for the full zone lifetime, fixing a dangling reference that caused intermittent invalid-cell failures.

The game-side DB-agent `0302` response handler (`ps_game.exe` `00408d30..00408dfd`) looks up the account twice, with the first user's recursive lock held for the second lookup. It atomically increments `CUser+57bc`; only a resulting value of 1 triggers further action. A closed socket invokes `CWorld::LeaveUser`, an open socket closes with reason 104 unless transfer state `+57e0` is 1. `tools/db_disconnect_ack_probe.py` and `vertical_disconnect_unit.cpp` compare 54 original branch combinations, including missing users and DWORD wrap. The TCP fixture now routes the lifecycle's `0302` through this handler to the final disconnect; DB writes and full zone cleanup remain substituted, and the production application remains unwired. An exit notification does not by itself prove that a SQL save succeeded: the recovered DB-agent may also exit after non-timeout failures and recovery handling.

`CZone::LeaveUser` (`ps_game.exe` `004161b0`) removes zone and cell memberships, runs `CUser`'s leave-zone lifecycle (`0044aee0`), broadcasts `0202` plus character ID at the previous cell, and only then clears the user's zone and cell coordinates. `tools/zone_leave_probe.py` checks eight original cases with list operations, lifecycle and broadcast substituted as ports. `leave_zone_actor` now composes actual `ZoneUsers` removal and retained actor/admission state in this order; its lifecycle and view callbacks still require the full real implementations. The TCP disconnect fixture exercises this composition with those callbacks simulated. This does not yet implement the complete `0044aee0` effects or wire the production server.

`CZone::SendView` (`ps_game.exe` `004208c0`) allocates one shared buffer, visits the clipped 5×5 cell rectangle in row-major order, and releases its own reference; allocation failure skips delivery. `tools/zone_send_view_probe.py` checks 18 original geometry/allocation cases. `zone_send_cell_view.hpp` composes this traversal with the existing shared-buffer/cell transport. The disconnect unit test connects zone leave to the actual `ZoneUsers` recipient traversal, verifies a nearby actor receives `0202` after subject removal while a distant actor does not, and checks allocation failure. The test's enqueue backend is simulated; real connection mapping, user leave lifecycle, full server ownership and production wiring remain pending.

The `vertical_scene_unit` TCP case now sends the zone-leave `0202` packet to a real nearby `WorldChannel` using `zone_send_cell_view`, the canonical `ZoneUsers` traversal, shared packet-buffer ownership and channel framing/encryption. It checks the received packet and shared buffer release. The two actors and their connection mapping are test fixtures; user leave lifecycle and real server world ownership are still not connected. The test was linked against current zone, scene and guild sources because the pre-existing static archives were stale.

The trade portion of `CUser::LeaveZone` (`ps_game.exe` `0044af3c..0044b04c`) now operates on real `Actor::exchange()` state via `zone_leave_exchange.hpp`. Depending on the local/peer trade modes it clears one or both offers, resets the corresponding `+5830` timer and sends the original `0a03` or paired `0a04` notifications. `tools/zone_leave_exchange_probe.py` and the native unit check 40 combinations against the original, including missing peer and unknown nonzero modes. Its peer lease and timer storage remain caller-owned. This is only one branch of the larger leave-zone lifecycle; the preceding NPC/interaction and following vehicle, invitation, party and duel paths still need integration.

`zone_leave_interactions.hpp` composes the `0044aee0..0044b04c` prefix: releases the actual NPC use counter, looks up the visited shop owner and, if found, removes the departing actor ID from the departing user's embedded customer list by swapping its final active slot; then invokes the required full shop shutdown and the recovered trade cleanup. Notably the original calls `004784a0` with the departing user's shop object, not the looked-up shop owner. `tools/zone_leave_interactions_probe.py` and the native test check 16 combinations and ordering, with shop shutdown substituted. The real shop shutdown and later leave-zone branches remain unwired.

`zone_shop_shutdown.hpp` reconstructs `CUserShop::Ended` (`ps_game.exe` `00478520`) and its customer loop (`00478720`): modes 1/2 clear the mode, visit live customer IDs, clear each found customer's visited-owner field and send `230c`, clear 48 offer bytes and the active customer count, send the owner `2302`, and broadcast `0523` when still in a zone. Inactive customer slots are retained. The original probes cover 20 shutdown combinations with the customer loop substituted, plus five actual customer-loop lists with the map lookup substituted; the native unit covers 20 combinations and real actor sends with fixture lookups. Actual shop state ownership, zone connection mapping and production integration remain pending.

`zone_leave_bound.hpp` composes this NPC/shop/trade prefix with canonical `ZoneUsers::find_actor`, retained per-actor shop/timer providers and the existing shared-cell view transport. The integrated unit removes the subject, resolves a real customer in the zone, shuts the shop, cancels both actors' trade, and verifies direct notification ordering and `0523` followed by `0202` to that customer. The session state provider and enqueue backend are fixture-owned; later `CUser::LeaveZone` branches and production wiring remain incomplete.

`zone_leave_requests.hpp` reconstructs the vehicle invitation and three pending request branches of `CUser::LeaveZone` (`ps_game.exe` `0044b04c..0044b19c`), using the existing actor vehicle fields and `PartyRequestState`. It preserves mode-dependent timer/peer clearing and the `021e`, `0b03`, `0d06` and `2203` notifications. `tools/zone_leave_requests_probe.py` and native tests cover 32 original combinations; the integrated zone-leave fixture resolves peers through canonical `ZoneUsers` and checks notifications after shop/trade cleanup. Remaining party, vehicle and duel lifecycle branches and production session ownership are still required.

`zone_leave_individual_duel` reconstructs the individual-duel half of `ps_game.exe` `PvPChkLeaveByZone` (`0047b920..0047bafb`), including missing-peer behavior, real wager drops and `Actor::end_duel`. Unlike attack-triggered leave, wager states 3/4 send `2406` only to the peer, use result byte zero and produce two logs with outcome zero. `tools/zone_leave_duel_probe.py` and the native unit cover 14 state/peer cases; the original probe substitutes nested cleanup/drop/log functions and the native unit uses empty wagers. The guild-duel half and full leave-zone orchestration remain pending. The preceding `CUser+175c` cleanup is guild creation state, not ordinary party membership.

`zone_leave_guild_duel.hpp` models the guild half of `PvPChkLeaveByZone` (`ps_game.exe` `0047bafb..0047bcad`): it retains the captured guild lock, resolves the opposing leader through the same zone, resets both leaders and guild states in negotiation states 1/2 only when the departing actor is leader, and delegates active states 3/4 to the existing full `guild_duel_leave` provider. It preserves the original identity in `2502` notifications. `tools/zone_leave_guild_duel_probe.py` and the unit check 96 conditions; the probe substitutes nested locks/reset/leave bodies, and unit services are fixtures. Wiring those services to the live catalog/session and completing the entire `LeaveZone` lifecycle remain pending.

`zone_leave_guild_catalog.hpp` binds the recovered guild leave gate to `GuildDuelCatalogMovement`, its actual catalog-owned guild states and recursive locks, and the canonical zone-user lookup. The integrated scene unit loads two catalog guilds, verifies both-leader negotiation cleanup, and exercises active-leave participant decrement and member broadcasts through `GuildDuelCatalogWorld`. Actor/session retention and network broadcast providers are fixtures; this binding is not yet attached to the production world owner or the complete zone-leave lifecycle.

`zone_leave_guild_creation.hpp` reconstructs the `CUser+175c` creation cleanup in `ps_game.exe` `0044b19c..0044b1eb`. Depending on phase it broadcasts `0d03` or the 98-byte `0d01` with preserved scratch bytes, then clears leader and seven participant creation pointers and passes the departing user's *reread* pointer to the manager release, including null. `tools/zone_leave_guild_creation_probe.py` and the unit cover 18 cases. The probe substitutes the shared broadcast and manager release; actual creation ownership, allocation and network broadcast remain explicit services and are not yet wired to production.

`zone_leave_lifecycle.hpp` composes the recovered `CUser::LeaveZone` sequence: NPC/shop/exchange, pending invitations, guild-creation cleanup, tail scalar clearing, individual duel then guild duel. `tools/zone_leave_tail_probe.py` verifies the final scalar updates in ten original cases, and the native disconnect unit exercises the composed path with real zone membership and fixture-backed owners. All dependent per-session state, manager release, duel world and transports remain required external owners; this composition is not yet attached to `src/main.cpp` and is not evidence of a playable server.

The TCP disconnect fixture now executes `zone_leave_user_lifecycle` after the DB-agent acknowledgment instead of replacing the entire user leave lifecycle with a no-op. It checks the real retained NPC usage count, vehicle request and tail-state cleanup before the zone broadcast and final release. Shop, creation and duel states are inactive in this fixture, and SQL persistence, multi-user transport and the session owner remain fixture services; the production server remains unwired.

`vertical_save_core.hpp` implements the `SendDBAgentCharGetInfo` prefix (`ps_game.exe` `0046c980..0046cab9`): sends `0606` resources, then `0607` live position/map/direction, then `070d` for changed equipped-item qualities. Reads are phased around synchronous sends; the actor inventory mirror must be current. `tools/user_save_core_probe.py` and the native unit compare eight original combinations, including DWORD-to-word truncation, sparse slots and unchanged quality. The unit applies packets to the real `CharacterReplica`; the TCP disconnect fixture now applies the resource and position packets to its retained replica before posting DB exit and verifies current values. Later skills/effects/quests and actual SQL persistence remain unconnected in this fixture; production world entry and saving remain incomplete.

`vertical_save_core_skills.hpp` composes the real resource/position/equipment prefix with `user_skill_runtime_persist`, checks actor identity and returns the one captured tick for subsequent phases. The TCP disconnect test now holds a real `UserSkillRuntime`, loads a learned skill into both actor runtime and retained `CharacterReplica`, and verifies its current cooldown reaches the replica before DB exit. Skill definition/manager and DB save are fixture providers; applied effects and quest phases, SQL write and production session wiring remain incomplete.

`EffectCycle::save_database` reconstructs `SendDBAgentCharGetInfo` `0046cb63..0046ccd4`: eligible type 3/4 effects excluding detail 100 use the learned-skill phase's tick, offline or online timer units and definition duration cap; `0904` batches hold at most 255 records. The existing shared effect cursor is retained across full-batch callbacks. `tools/user_save_effects_probe.py` and the native unit compare 14 combinations, including 500 entries and DWORD limits, and apply the actual packets to `CharacterReplica`. The TCP disconnect fixture now verifies an active effect's remaining time reaches its retained replica. Quest saving, SQL writing and production wiring remain pending; actual callers must serialize the effect list and provide the original recursive-lock behavior during callbacks.

`quest_runtime_save.hpp` implements `SendDBAgentCharGetInfo` `0046ccd4..0046ce4a` on the retained `QuestRuntime`, using the already captured tick, original timed/untimed minute calculations, WORD delay and 201-entry `0a04` batches. Full batches are sent with the provided recursive quest lock held; the last partial batch is sent after unlock. `tools/user_save_quests_probe.py` and the native unit compare twelve cases, up to 450 quests and unsigned tick boundaries, applying packets to the actual `CharacterReplica`. The TCP disconnect test verifies live quest delay and counters in that replica. Nested mutation of the quest list during send is explicitly unsupported pending further recovery. Later save phases, real SQL and production session wiring remain incomplete.

`UserQuickSlots` retains the 128-slot game-side state for the session, implements client `010b` (`ps_game.exe` `00467058`) and final DB `0609` (`0046ce4a` / `0046c900`), and is owned by `VerticalSessionCharacter`. Rows whose incoming bag is 255 may be skipped based on the existing row's bar; destination position still advances, so the saved prefix can include stale rows. Zero retained entries send no DB update. `tools/quick_slots_probe.py` and the native unit compare twelve update/save combinations; the TCP fixture receives a real `010b`, updates the retained DB replica, then runs the final quick-slot save on disconnect. The session/DB owner and SQL write remain test fixtures; production wiring is incomplete.

Effect save now takes explicit recursive lock/unlock ports, matching `0046cb71` and `0046cca2`: the full 255-row send occurs under the lock and a final partial batch follows unlock. The native unit verifies the lock state for both kinds of delivery and the TCP disconnect fixture still passes using its test mutex. Production must bind the actual effect-list lock rather than the fixture mutex.

`CharacterSaveLifecycle` now combines synchronized replica packet delivery, internal disconnect routing, the existing DB user lifecycle and `CharacterSaveStore` save callbacks behind a caller-owned job queue. The store and delivery must share the same recursive cache mutex, callbacks must not destroy the owner, and queued work must drain before destruction. `character-save-lifecycle-unit` verifies foreign-account rejection, deferred exit retry and ordered `0302`/`0303` notifications with a fake store; the real store template is also compiled. This does not run SQL or connect the production server, executor, or session owner; those remain necessary integration work.

The TCP disconnect fixture now routes quick-slot and final save packets, internal disconnect and queued exit through `CharacterSaveLifecycle`, exercising its integration with the retained actor and real socket. Its store is simulated. The isolated SQL integration test also now exercises the same bridge with `CharacterSaveStore`, checks the persisted money and notification order; that SQL case was syntax-checked but not executed in this session. Production session ownership, executor and database connections are still unbound.

The save lifecycle bridge now rejects mismatched account IDs, missing social callbacks, and (for `CharacterSaveStore`) a different cache mutex at construction. The native unit verifies the account and callback guards; the disposable SQL test includes the real store mutex guard and was syntax-checked. SQL Server was not running, so its integration test remains unexecuted in this session.

Production entry assessment: `VerticalSessionCharacter` owns the selected snapshot, replica and actor pipeline, but `src/main.cpp` still constructs the roster-only `WorldServer`. `vertical_persistence_restore` deliberately rejects relocated or overflow inventory until original load-time SQL repair is recovered; lifting this gate would risk silently losing or misplacing items. The existing TCP handoff uses synthetic catalogs and service providers, so it cannot establish production readiness. A CMake target build was configured successfully but stopped at 9/230 objects to keep the gaming machine light; the full native build remains pending.

Correction from the existing Ghidra export of `ps_dbAgent.exe` `DBCharacter::LoadCharacter` (`00420610`, inventory duplicate placement around `004217b9`): duplicate rows with a free inventory/bank slot are retained and their pooled save marker changes from 0 to 2; SQL is updated by the later normal save rather than immediately. `vertical_persistence_restore` now accepts those relocated rows, preserves SQL row allocation order and the pooled nonzero marker. The subsequent update below covers overflow allocation and release. Native `vertical-persistence-unit`, `players-unit` and `inventory-replica-unit` pass after rebuilding `eternity_game`; the SQL integration target compiles, but isolated SQL execution is still required.

`DBCharacter::LoadCharacter` inventory duplicate cleanup (`ps_dbAgent.exe` around `004217b9`) allocates all valid rows before releasing excess duplicates in list order. Bank duplicate placement (`00421e10..00421e81`) examines only slots 0..5, even though direct bank rows can use 240 slots. The decoded snapshot now preserves this six-slot behavior; replica restoration allocates overflow in SQL row order and returns excess to the shared pool after the fetch. Native `players-unit` and `vertical-persistence-unit` pass, including the first-six limit and a retained overflow pool allocation. The subsequent update below covers failed allocations for snapshots retaining their raw SQL rows; SQL integration remains pending.

`tools/bank_duplicate_placement_probe.py` executes the original instruction range `00421df7..00421e81` in isolation with real list removal and a captured free callback. It verifies 512 cases spanning every occupancy mask in slots 0..5 and 1..8 pending duplicates; the native `players-unit` exercises the same case matrix and order. This probes the placement loop, not SQL fetching, pool exhaustion, or the entire login lifecycle.

In the item and bank row loads inside `ps_dbAgent.exe` `DBCharacter::LoadCharacter` (`00420610`), the original sets `m_nSaveType` for direct rows but does not clear `m_nSavedType`; duplicate rows retain both the nonzero save marker and saved marker. Craft text overwrites only its actual bytes, terminating NUL and final byte, leaving intervening pooled bytes intact. `InventoryReplica::load` now preserves these pooled residues; `vertical-persistence-unit` checks them and `inventory-replica-unit` passes its original packet oracle. The subsequent update below reconciles failed allocations for snapshots retaining raw SQL rows.

`vertical_persistence_restore` now processes retained raw `Items` and `Bank` rows in fetch order, skipping failed item-pool allocations before checking subsequent occupancy, then relocates or frees duplicate allocations and derives the actor-facing snapshot from the same replica. Synthetic snapshots without raw catalogs retain the previous conservative fallback. Native tests cover failed first rows and failed duplicates in inventory and bank; `vertical-persistence-unit`, `players-unit` and `inventory-replica-unit` pass, and the SQL integration target compiles. The full SQL test and production session integration remain pending.

`vertical-protocol-unit` now exercises raw DB item rows through allocation failure, replica restoration, `VerticalSessionCharacter`, actor inventory allocation and the existing entry delivery for inventory and bank. It verifies a later independent game-item allocation failure changes the actor's packets while the DB replica retains both surviving DB items. This is a native fixture, not live SQL, TCP entry or production-world wiring.

`deliver_vertical_entry_with_admission` binds the existing 0407 delivery to `WorldAdmission::wait` at the original wait-user step, retaining the session's real snapshot and actor and checking account ownership. The native vertical protocol unit verifies selected-character acknowledgment precedes world registration, which precedes guild initialization, and checks the user moves from selection to world-wait. The caller still owns input pausing, serialized world scheduling, the other lifecycle services, session retention, and zone entry; this is not wired into `src/main.cpp` and is not evidence of a playable server.

The TCP handoff fixture now runs `deliver_vertical_entry_with_admission` on its retained `VerticalSessionCharacter` rather than synthesizing selection confirmation and calling wait separately. The peer decrypts actual initial character data and system-time packets before the existing zone publication, admission and movement sequence; the disconnect fixture reuses the same path through timed save and cleanup. Both native TCP tests pass. Catalogs, lifecycle providers and persistence remain fixtures, and `src/main.cpp` still uses the roster-only application.

`SerializedWorldApplication` and `serialized_world_factory` provide a shared ASIO strand for construction, calls and destruction of multiple `WorldServer` applications. Connection workers synchronously await each call, preserving the existing close/drain contract; the adapter creates no threads. The world executor must stay active until all connection close calls return, and world callbacks must never block on that strand or its underlying io_context. A native test exercises four concurrent connection workers, callback exception propagation and destruction on the shared strand. Production still needs a nonblocking handoff state machine and a real application factory; this adapter is not wired into `src/main.cpp`.

`serialized-world-application-tcp-unit` now uses the shared strand adapter with two authenticated `WorldServer` TCP connections, decrypts their replies and stops the server while both remain connected. Both applications close and are destroyed on the world strand before `io_context` drains; the test passes. Application bodies remain fixtures; this does not validate production gameplay or nonblocking admission.

`VerticalAdmissionHandoff` provides a nonblocking socket-pause barrier: completion returns to the shared world strand, the owner performs retained entry/wait there, and `zone_ready` enables admission dispatch before resuming input only after location 3 and a zone are published. `cancel` suppresses late pause callbacks; exceptions invoke the failure port and disconnect. The unit exercises ordering, premature release, cancellation and failure. The two-connection TCP fixture now sends selection and `0201` back-to-back through the actual `WorldServer` and verifies the queued entry arrives only after zone publication, while the other connection remains live. Entry and zone publication in that fixture are synthetic; real owner integration, production scheduling and SQL remain pending.

`vertical-async-entry-tcp-unit` combines the shared world strand, nonblocking pause barrier, retained `VerticalSessionCharacter`, 0407 delivery, `WorldAdmission`, client 0201 and `enter_zone_actor` in one real TCP test. It sends selection and zone entry back-to-back, verifies initial character and clock packets and the five actual cell snapshots, and confirms the retained actor enters the zone. The test passes. Character data, zone services, external lifecycle and persistence are fixtures; the production world factory, SQL loading and game client validation remain incomplete.

The nonblocking shared-strand TCP entry fixture now transitions outgoing encryption to the world XOR mode after zone entry and dispatches client `0501` through the retained `VerticalMoveSession`. It verifies the moved actor's zone position, original movement broadcast and completion, while incoming traffic keeps its AES stream. The test passes with fixture-backed movement services; production integration and original-client validation remain pending.

`vertical-async-disconnect-tcp-unit` composes shared-strand nonblocking TCP entry, live zone admission, XOR movement, quick-slot update, socket closure, retained world owner, virtual logout deadline, DB-replica save/ack and zone cleanup. It verifies the moved X coordinate and changed resources reach the replica before exit and that the retained session is released. Native test passes; database writes, clock scheduling and several gameplay owners remain fixtures. The production application is not yet wired.

`VerticalSessionRegistry` is a serialized-world owner for live session objects, indexed by account and actor identity. It refuses duplicate account/character owners and requires matching session identity for removal so stale completion cannot release a replacement. The async disconnect TCP fixture now retains its real session through this registry rather than a one-off fixture pointer; it verifies account and actor lookup, disconnect save/ack, removal and final destruction. The fixture test passes. Production must bind this registry to its actual entry, failure, shutdown and DB-owner lifecycles; SQL remains simulated in the TCP test.

`vertical-session-registry-unit` checks duplicate account, actor and owner rejection, retained callback pins, and rejection of a stale identity-specific release after replacement. This does not make delayed `0302` safe: that original packet carries only account ID, so a production owner must drain or fence old acknowledgments and jobs before allowing the account's replacement; there is no generation in the recovered packet.

`VerticalSessionRegistry` can now count accepted DB operations through both job completion and world-side reply handling, refusing release while tracked operations remain. The registry unit tests nested work, foreign completion and underflow; the asynchronous TCP disconnect fixture tracks its save plus acknowledgment and verifies premature release is refused. These are native ownership guards, not a property of the generation-free original wire: production must register every producer before posting, stop producers before release, finish only after all corresponding replies, and fence unsolicited/untracked transport messages. Both relevant tests pass; production and SQL integration remain pending.

`VerticalTrackedDbJobs` binds the existing native `DbJobDispatcher` to the world-session fence: submission increments before enqueue, the worker returns its captured replies, and the world strand applies replies before decrementing. Worker or delivery failure and silently stranded legacy work keep the session fenced; rejected queue admission rolls back the count. A unit uses an actual one-worker DB dispatcher and a separate world executor, verifies the session cannot release during reply handling, and drains both threads. The work backend and reply are fixtures. The owner must retain the tracker, serialize SQL connections, stop producers and fence external/unsolicited transport traffic; production, retry integration and real SQL remain pending.

The async TCP disconnect fixture now routes its actual lifecycle job 9 through `VerticalTrackedDbJobs` and the native `DbJobDispatcher` worker. The tracked world callback processes the original-format 0302 acknowledgment and final zone leave before the fence drains; the test verifies the session cannot be released while the response is being handled. It passes and shuts down the worker. SQL save is still a fixture; timeout-driven requeue and producer/transport fencing are not integrated into this worker path.

`VerticalTrackedDbJobs` now captures retry requests made synchronously by a DB worker and registers each on the world strand before delivering replies and releasing the current job fence. A rejected retry leaves the current fence in place. The async TCP disconnect test injects two timeout failures into the reconstructed exit-save orchestration, confirms job 9 is requeued, observes two simultaneous tracked jobs during the handoff, and then processes the final 0302 and releases the session. Both tracked-job and TCP tests pass. The timeout/SQL backend is simulated; actual DB and production wiring remain unverified.

`VerticalSaveJobBridge` connects `CharacterSaveLifecycle` to tracked DB work without replacing callbacks at runtime: under the same recursive mutex, a worker dispatch captures its outbound packets and retry jobs for the world-strand completion. Ordinary synchronous calls still use the supplied transport. The async TCP disconnect fixture now uses this bridge with its simulated timeout/store and native DB worker; it passes. The bridge is instantiated against the real `CharacterSaveStore` in the lifecycle unit build, but SQL was not executed. Thread-safe social/DB leave ownership and production wiring remain the caller's responsibility.

The character-save SQL integration now includes `VerticalSaveJobBridge` with the real `CharacterSaveStore`, checking captured 0302/0303 replies and persisted money; the integration executable builds but SQL Server was not started, so this new case has not run. `character-save-lifecycle-unit` also passes a simulated save exception followed by a deferred retry and successful captured replies, verifying that the bridge clears its temporary collection context after the exception. Actual SQL failures and the complete production lifecycle remain unverified.

Tracked DB producer shutdown now rejects new external submissions while allowing retries generated by already accepted jobs. Previously `stop_producers` also rejected such retries and left the session permanently fenced. The native tracked-worker test stops producers immediately after submission, then verifies an internal retry is registered before the original fence drains and both replies finish. The async TCP disconnect regression also passes. Dispatcher shutdown must still wait for tracked world-side completions and queues, and actual SQL/production shutdown are not yet integrated.

`VerticalTrackedDbJobs::when_drained` now notifies on the world strand after producers stop and this tracker's accepted jobs, retries and delivered replies have all completed; stranded or failed work intentionally keeps it pending. The unit and asynchronous TCP disconnect test stop producers before a timeout retry and wait for this signal before release, both passing. It does not prove the global dispatcher, other trackers or external transport are drained; the production shutdown owner must coordinate those separately. The underlying original dispatch behavior can overlap jobs and is not changed to FIFO.

`VerticalDbDrainGroup` coordinates stopped producers and world-side reply completion across a supplied set of tracked sessions, retaining trackers until all notify. A two-worker unit holds one session's DB job while the other finishes and verifies the group cannot complete early; it passes and drains both workers. This barrier covers only the supplied trackers: production must first close admission and other producers and separately drain unrelated DB clients, transport and zones. It is not yet wired into server shutdown.

The DB drain group now exposes an owner-retained cancellable handle and weak completion callbacks. A failed job leaves the session fence intact, but cancelling the group releases its tracker references without falsely reporting a successful drain or creating a reference cycle. The two-session unit tests both successful completion and failed-job cancellation. Cancelling is not a successful shutdown and does not discard the fenced session or its pending work; production must choose and implement its own failure recovery.

`vertical_restore_entry_catalogs` now builds the game entry cache from the actual character replica by replaying its reconstructed save-point, purchase-history and stored-point packets. The native persistence test checks populated points, cash and items; async TCP entry/disconnect fixtures use the same binding and pass. The existing `players-sql-integration` now uses this binder and compiles, but has not been run with SQL in this session. Friend/block and guild owners remain separate and must not be fabricated; production entry remains unwired.

`VerticalSocialReplica` models the retained DB-agent friend/block slots populated by `DBFriend::LoadFriend` `00425f80` and `LoadBlock` `00426500`, including untouched family/grow bytes, unchanged trailing slots, zero-ID filtering and 25-row `SendCharacterFriend` `0040a7f0` batches into the real game-side `SocialCatalog`. The native unit passes retained-state and 20 boundary/sparse cases; `tools/social_db_send_probe.py` checks the same 20 cases against original instructions, ignoring original unspecified post-NUL stack bytes. SQL integration compiles with the new adapter but was not run. NULL names or numeric fields are currently rejected because original ODBC scratch-buffer residue is not represented; social save, production session ownership and real SQL validation remain pending.

`vertical_load_social_selection` binds a selected SQL snapshot to the retained account-owned social replica and constructs the game-side catalog, returning independent friend/block load results while retaining prior slots when a loader failed. The vertical protocol unit verifies friend online notification and friend/block client packets from loaded rows, plus retained slots after missing loader results. The disposable `players-sql-integration` now uses this adapter and builds, but SQL execution remains pending. The real account owner, social saves, NULL scratch-buffer cases and production entry wiring are still missing.

`VerticalSocialReplica::apply_db` models friend and block insertion, deletion and memo updates from `CUser::PacketUserFriend` `00406930`: inserted entries are marked new, persisted entries with edited memos are marked dirty, apostrophes in memos become spaces, and deletion of persisted entries is journaled. The native unit checks these transitions. These in-memory changes alone do not persist across disconnects.

`VerticalSocialSqlSave` implements the separately committed social save routines `DBFriend::SaveFriend` `004261e0` and `SaveBlock` `004266e0` from the Ghidra export and original instruction argument order. It executes deletions first, then scans active slots; create/delete failures roll back, while memo query errors do not abort the original transaction. Successful commits clear the corresponding journal and live markers. The native test checks ordering, arguments, rollback and memo failure using a simulated connection. Unlike the original, begin/commit errors are treated as failures; mutation must be serialized with the save. Production ownership and disconnect integration remain missing.

The existing `players-sql-integration` binary was subsequently run against the isolated SQL Server and a fresh disposable catalog with the original social procedures; selection, friend/block deletion and insertion passed. The binary predates later unrelated character-replica ownership changes, so it does not validate those changes or the newer asynchronous selection path. The fixture catalog was removed and the isolated server stopped after the run.

`VerticalSocialLifecycle` connects the account-owned social replica and its SQL save adapter to `CharacterSaveLifecycle` callbacks, using the same recursive mutex for social packet mutation and save. It rejects a mismatched account and reports save failures while retaining unsaved markers. A native disconnect test checks that a simulated store invokes the social callbacks before leave; the store and database in this test are fixtures. The production owner must retain the bridge and connection through queued work, route social packets to it, and arrange exclusive connection use; the real server and SQL disconnect path are still unverified.

`tools/social_db_mutation_probe.py` executes `ps_dbAgent.exe` `CUser::PacketUserFriend` `00406930` for ten friend/block cases: memo marker transitions and apostrophe replacement, deletion dispatch, insertion into the first empty slot and full-list behavior. The deletion helper is intercepted, so its allocation and journal behavior are not tested by this probe. The native social unit also checks full-list insertion. `ps_game.exe` `CUser::PacketComm` `00475870` is the game-side source of social mutations; the production application still exposes only the character roster/creation path, and neither that handler nor social save is connected there.

`SocialCatalog::apply_client` implements PacketComm's friend/block deletion and memo cases (`2205`, `2206`, `220a`, `220b`), emitting DB updates before client responses and notifying an online removed friend. Friend removal preserves the destination slot's family/grow bytes as in `CFriend::FriendRemove` `004b9890`; memo changes preserve apostrophes in the game cache while the DB replica sanitizes them. `tools/social_client_mutation_probe.py` executes eight original memo/delete cases, intercepting transport and online lookup; native tests cover corresponding packet boundaries and DB replication. This safely rejects truncated or unterminated input instead of relying on the original fixed receive buffer. Friend requests, acceptance, blocking additions, production packet dispatch, SQL execution and a real client remain outstanding.

The asynchronous TCP disconnect fixture receives an encrypted friend memo update, applies the game and DB-replica changes and verifies the retained session still owns the dirty memo after socket closure. Its tracked DB worker now invokes `VerticalSocialLifecycle` and `VerticalSocialSave` through the disconnect callbacks, verifying the character ID, friend ID, sanitized memo, two separate commits and cleared marker before acknowledgement. The database connection and character store are fixtures, so this does not prove SQL execution or production integration. Both asynchronous and synchronous disconnect regressions pass.

The same TCP fixture now loads social rows from its selected `PlayerSnapshot` through `vertical_load_social_selection`, delivers that retained catalog to the client during entry, updates its friend memo and sends the update through the simulated disconnect save. This checks continuity between the existing loader, entry serializer, client mutation and save adapter; the snapshot and SQL backend are fixtures and the real server still lacks this wiring.

The production `CharacterCommands` path handles deletion request `0103` and cancellation `010f`: `ps_game.exe` `PacketCharacter` `0046bf20` forwards these to DB agent, whose `DBCharacter::DeleteCharacterReq` `0041e5e0` stores the local packed time via `usp_Delete_Char_Request_E` and `RecoverCharacterReq` `0041e740` stores zero. Successful DB replies become seven-byte client acknowledgements. The native implementation additionally requires the character to belong to the account/server and its loaded roster, rather than trusting the original procedure's unscoped ID. The server and TCP SQL integration executable now build, and the existing character/creation units pass; these units do not exercise the new SQL-backed commands. The isolated SQL/TCP test has not been run.

`RosterWorldHandoff` provides a `WorldServer` application factory that owns a live roster and character commands, transfers the account, updated roster and channel to a selected world application on `0104`, then forwards subsequent packets and close reasons to that owner. It retains the selected owner if its open or initial receive fails so transport cleanup can close it. The native unit verifies roster delivery, updated roster transfer, packet forwarding, close reason and failed-open cleanup. The selected world factory, real SQL loading, admission, shutdown and production `main` remain unbound; this adapter alone does not enable world entry.

The asynchronous TCP entry fixture now wraps its selected application in `RosterWorldHandoff`: it receives the original roster packets, transfers the retained account and roster, checks the real `0104` selection latch and roster gate, pauses the transport, admits the actor to a zone and processes a movement packet after the XOR transition. The regression passes. The roster and selected character data, world services and selected factory remain fixtures; the real SQL and production world owners are still absent.

The disposable `vertical-persistence-sql-integration` now routes its real roster load and `vertical_entry_select_persisted` through `RosterWorldHandoff`, then exercises the existing save and reload checks. The selected application and channel are test fixtures; the SQL loader and restoration are real. The integration executable compiles, but this revised path has not been executed against SQL Server. Production world ownership and entry are still unbound.

`roster_world_serialized_factory` runs roster loading and character commands on WorldServer's per-connection worker, then constructs and invokes the selected application on the shared world strand. A native test holds the loader and confirms independent world work still runs, and the asynchronous TCP entry regression passes with this composition. The selected application must separately offload its SQL selection and retain its owners; production remains unwired and the disposable SQL integration has only been compiled.

`VerticalAsyncSelection` applies the existing `0104` latch and retained roster gate on the world strand, runs an injected persisted selection loader on a separate executor, then delivers its result or failure on the world strand. Cancellation suppresses late callbacks but does not interrupt a loader or undo SQL effects already performed. A native test blocks the loader while world work continues and verifies cancellation suppresses the late result. The real SQL loader, pool ownership, admission and global drain still need production binding and real SQL validation.

`VerticalSelectionSqlLoader` binds that loader to `vertical_persistence_select`, creating a separate SQL connection per worker call and retaining the supplied replica pools. The returned `CharacterReplica` now retains the loader itself until its pool-backed members have been destroyed, including if the replica moves independently of the selection result. A native test checks the pool's lifetime across that move and its release afterwards. The server and disposable SQL persistence integration build, but the SQL integration has not been executed. Global job drain, production session ownership and entry wiring remain pending.

`VerticalAsyncSelection::when_drained` lets the world owner wait until an accepted selection job has returned and its result has been delivered or discarded. Cancellation suppresses callbacks but does not declare the job drained early; a native test holds the worker across cancellation and verifies the notification follows completion. This covers only the individual selection and requires both executors to remain alive; global shutdown, reconnect fencing and SQL side effects are still the production owner's responsibility.

`VerticalSessionRegistry` can reserve an account before actor creation and bind the actor later. `VerticalAsyncSelection` can bind the registry's begin/finish DB fence around an accepted selection through result delivery or discard. A combined native test holds a cancelled loader, rejects account replacement while it is in flight, and admits a replacement only after drain; it also checks preselection actor binding and foreign-owner rejection. The production owner must actually reserve, bind the fence and actor, stop all other producers and drain unsolicited transports; SQL side effects and application integration remain unverified.

The asynchronous TCP entry fixture now reserves its account before selection, runs a fixture loader on a separate DB executor under the account fence, builds the retained character from the returned replica, binds its actor, and enters the zone before movement. The input is paused while loading; the test passes with simulated data and services. The related handoff and asynchronous disconnect TCP regressions also pass. This is not yet a production SQL or world implementation.
