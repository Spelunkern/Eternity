# Worker 02: mob drops

Status: reconstructed loot algorithms, catalog construction/limit initialization
and explicit world projections; production
world/item adapters remain required. This does not make the server playable.

## Evidence

Original `originals/ps_game/ps_game.exe`, SHA256
`0fb7c9f13b5bcf186118f8bfe73ab214226b93e8aa502b8e6fded4a8312bc547`.
All twelve EXE/PDB entries in `SHA256SUMS` verified successfully.
Ghidra exports regenerated with locked `tools/reverse.py --process`, filter for
loot functions, output `work/exports/02-mob-drops/ps_game.exe` (ignored).

- `CGameData::LoadConstMobItem` 004c61f0: four positional columns of
  `usp_Read_MobItems_R`, ten slots at definition +bc/+c0, stride eight; invalid
  mob/order rows log and continue, duplicate rows overwrite.
- `CMob::ItemDrop` 004a5ea0: quest eligibility, nine regular slots, ranking-zone
  branch, sequence selection, money chance, ownership and party money grants.
  Slot zero is loaded but ignored. Slots 1–6 use CRT `%100`; 7–9 use the mob's
  real `SRandom` with bound 1,000,000. Quest rolls stop after the first eligible
  quest/member even on failure. Money chance is 30%; party divisor includes
  ineligible members, share truncates and clamps to one, and bonuses retain
  signed behavior. Ranking mode suppresses sequence/money.
- `CMob::ItemDrop` 004a65a0: allocation failure, owner overrides, type-27 bypass,
  raid leader handling, random/turn/ground assignment, exact cursor updates,
  log/grant/ground ordering, sequence-log callback, appearance argument two.
- `CItemDrop::GetItem` 004bdd10: one catalog draw, clock before lock, shared
  item count/absolute unsigned tick gate, no retry, unsigned delay wraparound.
- `CItemDrop::GetSequenceItem` 004be490: first matching divisor wins, including
  an empty grade; level in 1..99, level/10 grade bucket, object zero excluded.
- `CMob::GetDropPosRand` 00490c70: two CRT draws, rounded float offsets and y=0.

## Validation

- Original execution fixtures generated under `work/fixtures/` before copying
  into tracked files; executable hash checked by every oracle loader.
- 512 original slot loops, 256 real catalog-limit calls, 128 sequence calls,
  128 original definition assignments and 128 exact position results.
- 1,024 complete original loot coordinator traces and 1,024 complete original
  item-delivery traces. Actual selection/distribution, cursor and ownership code
  execute; world services and explicit CRT streams are observed ports. The mob
  generator executes unchanged. Oracle source headers identify all port limits.
- `python3 tools/dev.py build`: exit 0; final incremental build completed all
  133 actions with no compiler warnings (`work/mob-drops-final-build.log`).
- `python3 tools/dev.py test`: exit 0; all 122 tests passed in 35.48 seconds,
  including the three new oracle comparisons (`work/mob-drops-test.log`).
- Python oracle sources pass `py_compile`; `git diff --check` passes.
- Read-only isolated SQL test, holding the same `parallel_lock.resource_lock`
  SQL lock used by `tools/dev.py`: exit 0; 24,723 original MobItems rows,
  2,747 mob definitions, zero rejected rows. Compared all 27,470 slots against
  the restored stored-procedure results without writes or lifecycle operations.
  `mob-drops-sql-unit` is built but deliberately not registered in ordinary
  credential-free CTest. Full shared integration belongs to the coordinator.

## Integration dependencies and canonical interfaces

No shared headers or SQL fixtures were changed. Bind `MobDeathWorld::drop_items`
to `mob_loot`, retaining its existing party lock and the mob's `drop_random`.
Use the same CRT `RandomSource` across catalog selection, delivery and position
callbacks; do not reseed or draw independently at these boundaries.
The independent `MobLootContext/User/Party/Item` projections avoid changing other
owners. They must refer to live state for the duration of callbacks.

Required canonical additions/adapters:

- Mob definition money base/range at +28/+2a and quest type/type-ID +2c/+2d;
  use the original `MobMaxID_R + 1` count and the separate loaded slot catalog.
  Level +22, object ID +5c and mob definition ID +220 feed sequence/drop metadata.
- Item definition `wDex` +38 (distinct from `ConstDex`), grade membership and
  shared `wDrop` +7e, count +8c, delay +90, random delay +94, next tick +98.
  Existing quest-start, type/type-ID, real type and quality fields can project
  into `MobLootItem`. Catalog population and initial limit reset are now supplied
  by `mob_drops_catalog`; original SequenceItemCreate configuration and runtime
  catalog ownership still need adapters.
- Party mode +104, leader +0c, cursor +108, type +114 and ordered members +18;
  credited/member user zone +e0, instance +164, state +5758, dead +142c,
  positions +d0/+d8, money mode +58b0 and percentage +58b4.
- `MobLootWorld` must supply item/quest links and actual quest ownership,
  guild locking/revalidation/ranking, money grant/ground allocation and logging.
  `MobLootDeliveryWorld::create` must perform actual CItem allocation/init,
  unique ID plus server ID, position, mob/drop metadata and craft expansion;
  connect item grants and `ZoneItems::enter(...,2)`. Ground money requires
  type=26, type-ID=0, count=1, quality=0, make='D', drop-type=1, mob-ID,
  party-ID=0 and the selected amount. Allocation failure remains a valid result.
- Zone/range providers must implement original helpers 004a6560/004a64f0;
  range requires credited user in mob zone and recipient alive/in state four,
  same zone/instance and radius 100. Zone-only ownership does not check death.
- Sequence log remains a required callback: set make='V' and construct original
  opcode 0a06, 127 bytes. Both-mob item/money logging uses the coordinator's
  logging owner, event seven/eight. These interfaces have no default no-ops.

Implementation commit: `ad3247ba0543d475e9d722363c05dd2d2c2893aa`.
This report is committed separately after that implementation checkpoint.


## Milestone 2: catalog construction and initial limits

New evidence: `ps_game.exe CItemDrop::Init` 004bde50 and `InitLimit` 004be3e0,
regenerated through locked Ghidra `--process` into the assigned ignored export.
All original binary/PDB hashes verified again.

`initialize_mob_drop_catalog` builds all 1,025 grades in original type/type-ID
scan order, with unrestricted (Server=0 and Drop=0) entries before restricted
entries and the original unrestricted-prefix counts. It excludes table ID zero,
invalid definition type/type-ID, grades above 1,024 and real type 27. Duplicate
input coordinates use the last binding. The separate limit pass starts at table
type one and ignores grade/server/real-type restrictions. Nonzero selectors reset
count/next tick and split intervals 90% fixed / 10% random. Exact intervals are
1,440,000, 14,400,000, 28,800,000, 43,200,000, 86,400,000 and 259,200,000 ms.
Selectors above six access undefined original stack data; native code rejects
those explicitly. Unlimited definitions, zero type/type-ID fields and table type zero retain
their previous limit state.

`MobDropCatalogDefinitions::load_sql` owns stable entry projections from the
original Items procedure, reuses the recovered real-type mapping and server
normalization, and leaves quest-start initialization to the existing quest/item
owner. Catalog initialization does not replace unrelated crafting/sequence config.

Validation:

- 32 complete original catalog initializations, including actual limit loops;
  only allocator and unrelated ItemCreate/sequence configuration calls are ports.
  Fixtures cover stale state, grade edges, type-zero table differences, validity,
  partition order and all seven limit selectors. Native repeated initialization,
  duplicate coordinates and undefined selector rejection are checked separately.
- Original execution over all 16,047 restored SQL item definitions yields 15,505
  catalog members and hash `15810976553663983297`. The hash includes all 1,025
  grade member orders/unrestricted counts and every projected limit state. SQL
  includes five nonzero Drop selectors and 9,430 nonzero server flags.
- Focused isolated SQL comparison: exit 0, exact full original catalog hash
  matched; previous 24,723-row MobItems check also passed. Both SQL reads use
  the existing shared SQL resource lock; no schema/data/lifecycle changes.
  Log: `work/mob-drops-catalog-sql-final.log`.
- `python3 tools/dev.py build`: exit 0, no warnings in either the initial
  139-action build or final two-action incremental build.
  Logs: `work/mob-drops-catalog-build.log`, `work/mob-drops-catalog-final-build.log`.
- `python3 tools/dev.py test`: exit 0; all 123 native tests passed in
  42.92 seconds, including the new original initialization comparison.
  Log: `work/mob-drops-catalog-test.log`.
- Python oracle `py_compile` and `git diff --check`: exit 0.

Remaining: connect this owned catalog to the production loot provider; implement
original SequenceItemCreate configuration loading/reloading. ItemCreate.ini is
an unrelated crafting distribution and was observed as an explicit external port.
Production allocation/grant/log/range and death-owner adapters listed above remain
required. No shared canonical files, SQL fixtures or main checkout were changed.

Milestone 2 implementation commit: `3b1e120b6e104e2d569be7d5a97a64b2f7100e77`.
