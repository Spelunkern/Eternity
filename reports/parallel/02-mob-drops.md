# Worker 02: mob drops

Status: loot selection, catalog/sequence configuration and pooled item/money
creation are reconstructed. Production death/world adapters remain required;
this is not a playable server. Only owned files/private CMake were changed.

## Source evidence and completed behavior

Executable: `originals/ps_game/ps_game.exe`, SHA256
`0fb7c9f13b5bcf186118f8bfe73ab214226b93e8aa502b8e6fded4a8312bc547`.
All twelve original EXE/PDB hashes verified again. Focused Ghidra exports use
locked `tools/reverse.py --process --output work/exports/02-mob-drops` (ignored).

| Original symbol/address | Recovered behavior |
| --- | --- |
| LoadConstMobItem 004c61f0 | Four positional SQL columns, ten slots, bounds/last-row overwrite |
| CMob::ItemDrop 004a5ea0 | Quest eligibility, slots 1–9, ranking, sequence, money and party grants |
| CMob::ItemDrop 004a65a0 | Owner overrides, type-27 bypass, leader/random/turn/ground distribution |
| CItemDrop::GetItem 004bdd10 | Catalog draw, clock/lock order, shared count/tick gates and delay wrap |
| CItemDrop::Init / InitLimit 004bde50 / 004be3e0 | Stable unrestricted/restricted grade partition and original interval initialization |
| GetSequenceItem 004be490 | First divisor match, level/10 grade bucket, exact 32-bit grades |
| InitSequenceItemCreate / ReSequenceItemCreate 004be520 / 004be6a0 | Eight retained groups, signed count, clamp and nontransactional failures |
| CMob::GetDropPosRand 00490c70 | Two CRT draws, rounded offsets, y=0 |
| CItem::CreateCraftExpansion / IsAccessory 004ba2e0 / 004b9ee0 | Six/nine-slot craft selection and minimum-of-three value draws |
| CMob::ItemDrop 004a662a..004a66cc / 004a645b..004a64cc | Actual pooled item and money initialization |

Prior implementation commits: `ad3247b` (loot), `3b1e120` (catalog),
`52ab268` (sequence configuration). Their reports were committed separately.
Current milestone implementation commit: `058df95db2ddec96c969660d6afcaf2fd70f61b7`.

## Milestone 4: pooled creation and generated craft stats

`mob_drops_creation.hpp/cpp` composes the existing real `GroundItemPool` and
`initialize_ground_item` with recovered mob metadata, position and craft logic.
Items receive count=1, make='D', time, raw UID plus server ID, credited ownership,
party ID, drop-type=1 and mob definition ID. Money receives type=26, count=1,
quality=0, make='D', drop-type=1, party=0 and amount. Money does not request a
make time or UID; allocation reset retains its previous UID and craft bytes after
the first NUL. Allocation failure performs no later clock/UID/random work.

Craft expansion gates on a definition, nonzero Server and ReqWis in 1..99.
Real types 1..15 and accessories 22/23/25 use six possible stats; others use nine.
Server selects stats without replacement, with dedicated all/all-but-one cases.
Each selected value is the minimum of three CRT draws modulo ReqWis+1.
Unselected addition WORDs and both enchant WORDs remain unchanged, even though
the generated text writes zero pairs for unselected stats and enchant text.
Creation preserves callback order: quality after make-time, server ID after UID,
and mob coordinates/type after both position draws. Returned items are unlinked.

Validation:

- 768 complete original craft calls, 512 original item creation paths and 128
  money creation paths; 29 allocation failures. Compared 101-byte payloads,
  exact CRT consumption, retained pool state, UID wrap and callback mutations.
  Real CItem reset/init, drop position, craft and accessory bodies execute;
  ports supply allocation storage, time, raw UID, explicit CRT stream and cookie.
- Real pool -> creation -> ZoneItems appearance -> strict expiry -> pool reuse
  composition passes. Expiry invokes the actual pool release provider.
- Read-only isolated SQL/original comparison: 16,047 definitions, 9,430 eligible,
  133,142 CRT draws; hash `14191292995060048443` matches complete original craft
  execution. Hash covers 21 craft bytes, eleven addition WORDs, final RNG state
  and draw count per definition. Shared SQL lock held; no database writes or
  lifecycle operations. Log: `work/mob-drops-creation-sql-final.log`.
- Native build: exit 0, no warnings (136 initial actions, four new-target actions,
  two diagnostic-test actions). Logs: `work/mob-drops-creation-build.log`,
  `work/mob-drops-creation-final-build.log`,
  `work/mob-drops-creation-diagnostic-build.log`.
- Final `python3 tools/dev.py test`: exit 0, 125/125 passed in 29.57 seconds.
  Log: `work/mob-drops-creation-final-test.log`.
- Initial fixture run failed at item case 1: the allocator trampoline omitted
  restoring EAX after real CItem::Init advanced it by 0x84. The fixture was
  corrected and regenerated under work before copying; final checks above use
  that corrected fixture. The first run is not counted as a pass.
- Oracle Python sources pass `py_compile`; `git diff --check` passes.

Prior evidence retained: 512 slot loops, 256 catalog-limit calls, 128 sequence
lookups, 128 MobItems assignments, 128 positions, 1,024 complete loot traces,
1,024 delivery traces, 32 catalog initializations, 256 sequence init/reloads and
six wide-grade lookups. Original SQL catalogs match 24,723 MobItems rows / 2,747
mobs / 27,470 slots and 16,047 Items / 15,505 grade members. Sequence INI matches
the original seven groups. Fixtures identify all observed provider boundaries.

## Required integration and remaining work

- Bind `MobDeathWorld::drop_items` to `mob_loot`, retaining the existing party
  lock, live projections and mob-owned `drop_random`. Use the same executor CRT
  source across catalog, delivery, position and craft; do not reseed per call.
- Mob templates need money base/range +28/+2a and quest type/type-ID +2c/+2d;
  use MobMaxID+1 and loaded slots. Level +22, object ID +5c and mob type +220
  supply sequence and drop metadata. No shared header was modified.
- Catalog ownership must retain owned entry definitions and sequence backing
  state. Publish after every reload attempt, including failure. Runtime sequence
  grades are 32-bit. Undefined negative original array indexing is not emulated.
  The INI reader supports the original decimal/unquoted/unique-key syntax;
  unsupported Windows profile edge cases fail explicitly, not by guessed values.
- `MobLootDeliveryWorld::create` can now call `create_mob_drop_item` using actual
  ItemDefinition and pool owners. Its wrapper must retain the GroundItem and
  synchronize policy owner/party changes before real grant or ZoneItems entry.
  `MobLootWorld::ground_money` can use `create_mob_drop_money`, then actual
  `ZoneItems::enter(...,2)`. Returned allocations require a real transfer/release
  owner; these helpers do not provide production transport or inventory grants.
- Creation time and raw UID remain required providers. Use the one shared UID
  owner; an ItemIds configured with server zero supplies raw IDs. Do not create
  per-drop generators or add server ID twice. Existing ItemDefinition fields
  already supply all craft inputs, so this block needs no canonical field change.
- User/party projections still require mode +104, leader +0c, cursor +108,
  type +114, ordered members +18, zone +e0, instance +164, state +5758, dead +142c,
  position +d0/+d8 and money mode/percentage +58b0/+58b4.
- Real quest ownership/links, guild lock/revalidation/ranking, money grants,
  range/zone helpers 004a64f0/004a6560 and inventory delivery remain required.
  Both-mob logs (events 7/8) and 127-byte sequence log 0a06 belong to the
  coordinator's logging owner. Sequence callback must set make='V'. No required
  provider has a default no-op implementation. ItemCreate.ini crafting
  distributions remain outside these mob-drop blocks.
