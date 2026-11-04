# Worker 02: mob drops

Status: loot algorithms, catalogs, selective reload, creation and eligibility are
reconstructed. Delivery connects actual pool/Actor/ZoneItems owners. Production
death/world routing and required services remain open; the server is not playable.
Only owned files and private CMake registrations were changed.

## Evidence and prior blocks

Original `ps_game.exe` SHA256:
`0fb7c9f13b5bcf186118f8bfe73ab214226b93e8aa502b8e6fded4a8312bc547`.
All twelve EXE/PDB hashes verified again. Focused Ghidra exports use locked
`--process --output work/exports/02-mob-drops`; generated evidence stays ignored.

| Implementation | Original source / completed behavior |
| --- | --- |
| ad3247b | ItemDrop 004a5ea0/004a65a0 selection/distribution; LoadConstMobItem 004c61f0 |
| 3b1e120 | CItemDrop::Init/InitLimit 004bde50/004be3e0, ordered grade population/limits |
| 52ab268 | Sequence init/reload 004be520/004be6a0, retained groups and 32-bit grades |
| 058df95 | Creation 004a662a..004a66cc / money 004a645b..004a64cc, actual pool/init/craft |
| b2f12a9 | Range/zone 004a64f0/004a6560, Distance2 0041b1f0, distinct money gate and NaN acceptance |
| 050179d | Concrete delivery through real Actor::receive_item 0045df40 and ZoneItems::enter 00417320 |

Current milestone implementation commit: `940a1f986fa4e92de56e9f0f169bbbf5e0fa4a04`.

## Milestone 7: selective MobItems reload and live slot binding

Evidence: `CGameData::ReLoadConstMobItem` 004c2e70, write block
004c3043..004c3105, original `usp_Read_MobItems_R2` query. Focused Ghidra export
and immutable EXE/PDB hash verification completed again.

`mob_drops_reload.hpp/cpp` updates existing MobDropDefinitions in place. All four
columns must be read before a row is published. Missing rows do not clear slots;
invalid mob/order rows report diagnostics and continue. Grade then rate each
increment the caller's cumulative change counter only when changed, preserving
DWORD signed wrap. Empty/idempotent results succeed without resetting that
counter. Earlier writes survive later read/conversion failure. Storage is never
resized and initial-load rejection metadata is retained.

`reload_mob_drop_sql` uses the supplied native SqlServer owner and original
four-column selective procedure. It performs no database/pool lifecycle operations,
reconnect/reset, transaction changes or schema writes. Diagnostics are mandatory.
A failed query reports failure with unchanged state; result rows convert lazily,
so conversion failure preserves earlier rows. NULL/unsupported conversions are
explicitly rejected instead of guessing original uninitialized-buffer behavior.

`MobLootContext::live_slots` borrows the retained original table record. The loot
coordinator rereads that storage between callbacks; retaining a copied slot array
would hide successful reloads. The old value projection remains available for
explicit standalone fixtures. Production should bind the borrowed record.

Validation:

- 512 complete original reload executions: exact row comparisons/writes, cumulative
  counter, diagnostics and final table. Includes empty/duplicate/invalid rows,
  all four GetData failure positions, query failures, signed boundaries and 383
  partial/query failures. Profile/credential/DB-pool/query/GetData/logger/cookie
  responses are observed ports; actual reload control/write bodies execute.
- Original full ItemDrop callback installs a known two-field reload effect;
  native callback invokes the actual reload engine. Later slot reads select
  grade nine and suppress slot three, with matching CRT consumption and stable
  borrowed storage. This is composition evidence, not a claim that the original
  callback executed a nested database query.
- Read-only isolated SQL/original/native comparison: five selective requests,
  45 rows, 2,747 retained definitions / 27,470 slots. Cumulative counter 17->107;
  all five full-table/counter hashes match original row-write execution. A second
  identical reload per request leaves the counter unchanged. Shared SQL lock held;
  all snapshots stay ignored under work/. Only hash references are tracked.
  Log: `work/mob-drops-reload-sql-final.log`.
- Final native build: exit 0, no warnings; `work/mob-drops-reload-final-build.log`.
  Initial build rejected a mixed-type auto declaration in the SQL test; split
  declarations corrected it before validation. Initial failure is not a pass.
- `python3 tools/dev.py test`: exit 0, 129/129 passed in 32.77 seconds.
  Log: `work/mob-drops-reload-test.log`.
- New Python oracles pass `py_compile`; `git diff --check` passes.

Important native SQL boundary: SqlServer::query buffers its entire result, so a
transport/fetch error occurs before any row publication. `MobDropReloadCursor`
already supports the original streaming failure semantics, but exact production
mid-fetch publication requires a canonical statement/cursor interface delivering
Row/End/Error after four typed column reads while retaining statement lifetime.
That shared storage interface was not edited. Normal restored procedure results
and lazy conversion failures are verified; no streaming error equivalence claimed.

## Retained delivery/creation evidence

Concrete delivery uses one executor CRT source across position/craft/party draw,
a real shared GroundItemPool, canonical ItemDefinition and actual Actor admission
or ZoneItems publication. Sequence mark='V' precedes required logging. Per-call
handles survive recursive delivery. Results distinguish none/allocation failure,
inventory, ground and retained unlinked allocation; inventory receipts include
stored/merged/returned and expose no caller-owned item pointer after admission.

Prior comparison: 512 complete original drops, 41 real inventory admissions,
431 ground publications; real DB/client/log packet effects, nested delivery,
merged/recycled storage, 0707 count update, full-inventory return, money placement,
expiry/reuse and rejected geometry. Restored SQL craft comparison retains 16,047
definitions / 9,430 eligible / 133,142 draws, hash `14191292995060048443`.
Earlier commit reports retain detailed fixture limits and corrected harness runs.

## Required production integration and remaining work

- Bind MobDeathWorld::drop_items to mob_loot, then deliver_mob_drop / ground-money
  placement. Keep the outer party lock, mob-owned ServerRandom and shared CRT.
- Bind `live_slots` to the retained `definitions.mobs[mob_id]` record; serialize
  reload/read access under the definition owner and never resize borrowed storage.
  Use MobMaxID+1. Other live mob-template fields still require their owner bindings.
- Mob templates need money +28/+2a and quest type/type-ID +2c/+2d; level +22,
  object ID +5c and mob type +220 feed sequence/drop metadata. User/party views
  need mode +104, leader +0c, cursor +108, type +114, members +18, zone +e0,
  instance +164, state +5758, dead +142c, X/Z +d0/+d8, money +58b0/+58b4.
- Resolve retained Actor/ItemDefinition owners. InventoryWorld must capture and
  revalidate the same live party with compatible recursive locking. Actual
  transports/return owners/log sinks remain required; exceptions imply no rollback.
- Shared original UID ownership is still a coordinator dependency. Current
  canonical ItemIds has known clock/deadline/zero/wrap differences; configuring
  server zero alone does not establish original GenID compatibility. The owned
  creation header now states this explicitly. Add server ID only once.
- Zone identity means live CZone identity, not map number; never truncate host
  pointers. Retain catalog entries and all eight sequence backing groups; publish
  after every sequence reload attempt, including failure. Unsupported INI syntax,
  original negative-grade indexing and SQL NULL buffers are not guessed.
- Real quest links/ownership, guild ranking/revalidation, party money grants,
  top-level dispatch and logging remain required. Both-mob events 7/8 and sequence
  0a06 stay with the coordinator's logging owner. No required service defaults to
  a no-op. No shared canonical header, SQL fixture or main checkout was modified.
