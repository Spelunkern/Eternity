# Worker 02: mob drops

Status: loot algorithms, catalogs, creation and eligibility are reconstructed.
The new delivery adapter connects actual pool/Actor/ZoneItems owners. Production
death/world routing and required services remain open; the server is not playable.
Only owned files and private CMake registrations were changed.

## Evidence and prior blocks

Original `ps_game.exe` SHA256:
`0fb7c9f13b5bcf186118f8bfe73ab214226b93e8aa502b8e6fded4a8312bc547`.
All twelve EXE/PDB hashes verified again. Focused Ghidra exports use the locked
`--process --output work/exports/02-mob-drops` workflow; generated evidence stays
ignored. No original binary or canonical/shared source was changed.

| Implementation commit | Original source / completed behavior |
| --- | --- |
| ad3247b | CMob::ItemDrop 004a5ea0/004a65a0: quest/regular/sequence/money selection, party distribution; LoadConstMobItem 004c61f0 |
| 3b1e120 | CItemDrop::Init/InitLimit 004bde50/004be3e0: ordered grade population and original limit initialization |
| 52ab268 | InitSequenceItemCreate/ReSequenceItemCreate 004be520/004be6a0: retained backing groups, partial writes and 32-bit grades |
| 058df95 | CMob::ItemDrop creation 004a662a..004a66cc / money 004a645b..004a64cc; actual pool/init/craft and live position |
| b2f12a9 | EnableApplyRangeItem/ZoneItem 004a64f0/004a6560, Distance2 0041b1f0, money gate 004a6311..004a6433; unordered NaN acceptance |

Current milestone implementation commit: `050179d9ee5ff51953c70215558d758031ac8d3b`.

## Milestone 6: concrete pool / inventory / zone delivery adapter

`mob_drops_delivery.hpp/cpp` implements `deliver_mob_drop` using the recovered
policy, creation, craft and eligibility functions. It resolves a retained canonical
ItemDefinition, validates its identity, creates through the actual GroundItemPool,
and either invokes actual `Actor::receive_item` or `ZoneItems::enter(...,2)`.
Source: complete CMob::ItemDrop 004a65a0, CUser::ItemGet 0045df40 and CZone::EnterItem
00417320. Existing inventory/pool/zone implementations are reused, not duplicated.
`place_mob_drop_money` likewise performs real creation and zone publication.

Per-invocation handles permit nested delivery without overwriting the outer drop.
Policy owner/party changes synchronize into the actual GroundItem before transfer;
required log callbacks refresh the projection from live storage. Sequence delivery
sets make='V' before its required log callback. Randomness is one shared executor
stream across position, craft and subsequent party selection.

Results explicitly distinguish missing definition, allocation failure, inventory
admission, ground publication and retained unlinked allocation. Inventory results
include the real stored/merged/returned receipt and expose no caller-owned item
pointer: Actor/world may own it or the pool may already have recycled it. An entry
rejected by geometry returns the unlinked item instead of fabricating success or
silently releasing it. Once delegated to Actor, its inventory/return owner retains
responsibility; callers must not free via an earlier drop handle.

Required `MobDropDeliveryServices` supplies creation time, shared raw UID,
canonical definition/recipient resolution and sequence/both-mob log sinks. Recipient
Actor identity is checked. InventoryWorld supplies its existing mandatory transport,
party and return owners. These dependencies have no default no-op implementations.

Validation:

- 512 complete original ItemDrop executions with actual CItem reset/init, craft,
  position, eligibility and party distribution. Every log/transfer snapshots 101
  payload bytes. Native composition matches 41 real Actor admissions and 431
  real ground publications, complete event order, cursor and final CRT state.
- Original ports supply allocated storage, time/raw UID, explicit CRT/TLS/cookie,
  logs, ItemGet and EnterItem transfer observers. Native tests continue through
  real Actor admission/zone membership beyond those observed original boundaries;
  they do not claim original DB/client transport was executed by those observers.
- Real inventory DB/client/loot-log packets are emitted. Additional composition
  checks cover recursive delivery, stored then merged/recycled receipts, the
  original 0707 stack count update, full-inventory return with appearance one,
  money publication, strict expiry/pool reuse and explicit invalid-geometry outcome.
- Focused asset/isolated SQL check: exit 0. New bridge comparison passes; 16,047
  restored definitions still match original craft hash `14191292995060048443`
  (9,430 eligible; 133,142 CRT draws). Shared SQL lock held, no database writes
  or lifecycle changes. Log: `work/mob-drops-bridge-integration-final.log`.
- Native builds: exit 0, no warnings. Logs: `work/mob-drops-bridge-build.log`,
  `work/mob-drops-bridge-reviewed-build.log`,
  `work/mob-drops-bridge-corrected-build.log`.
- Final `python3 tools/dev.py test`: exit 0, 128/128 passed in 47.55 seconds.
  Log: `work/mob-drops-bridge-final-test.log`.
- Initial focused test failed because its merged-stack assertion expected 0702;
  the existing original-backed inventory contract emits 0707. Only that assertion
  was corrected and the explicit 0707 count check added; delivery code unchanged.
  The failed initial focused run is not counted as passed.
- New Python oracle passes `py_compile`; `git diff --check` passes.

Prior fixtures remain: full loot/delivery, catalog/limits, sequence reloads,
creation/craft and zone/range/money edge comparisons. Restored SQL also matches
24,723 MobItems rows / 2,747 mobs / 27,470 slots and 15,505 grade members.
Sequence configuration matches the original seven-group INI. Earlier milestone
reports retain their detailed validation and corrected harness-attempt records.

## Required production integration and remaining work

- Bind `MobDeathWorld::drop_items` to `mob_loot`; its delivery callback can now use
  `deliver_mob_drop`, and ground money can use `place_mob_drop_money`. Preserve the
  outer party lock and the same executor CRT source plus mob-owned ServerRandom.
- Resolve actual retained Actor/ItemDefinition owners, not detached copies. Keep
  all participating storage alive through nested callbacks. InventoryWorld must
  capture/revalidate the same live party owner; recursive acquisition must remain
  compatible with the death coordinator's outer lock. Actual transports and log
  sinks must be supplied. Exceptions do not imply speculative ownership rollback.
- A single shared raw UID owner is mandatory; server ID is added once. An ItemIds
  configured with server zero can provide raw values; do not create per-drop IDs.
- Mob templates still need money base/range +28/+2a and quest type/type-ID +2c/+2d.
  Use MobMaxID+1, existing slot loader, level +22, object ID +5c and mob type +220.
  User/party projections need mode +104, leader +0c, cursor +108, type +114,
  members +18, zone +e0, instance +164, state +5758, dead +142c, X/Z +d0/+d8 and
  money mode/percentage +58b0/+58b4. No shared canonical header was edited.
- `MobLootUser.map` / context zone identity mean live CZone identity, not numeric
  map ID. Supply consistent stable identities; never truncate native host pointers.
- Retain catalog entry storage and all eight sequence backing groups. Publish
  after every reload attempt, including failures. The INI reader only claims the
  recovered decimal/unquoted/unique-key syntax; unsupported Windows profile edge
  cases and undefined negative original grade indexing are not guessed.
- Real quest links/ownership, guild ranking/revalidation, party money grants and
  top-level world/death dispatch remain required. Both-mob logs (events 7/8) and
  127-byte sequence log 0a06 stay with the coordinator's logging owner; the new
  adapter passes actual GroundItem data and already marks sequence make='V'.
