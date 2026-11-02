# Recovered transport

Source: `originals/ps_login/ps_login.exe`, matching PDB, Ghidra 12.1.4.

- `SWorker::IORecv` at `00416d60` reads a little-endian uint16 total frame length,
  rejects lengths below 2 or above 2048, and splits/coalesces TCP input.
- `SPacketBufferSend::AllocBuffer` at `0041eca0` prefixes payload length + 2.
- `CUser::OnRecv` at `004042f0` dispatches payload opcodes `A101` (key exchange),
  `A102` (authentication), and `A202` (server selection), subject to state.
- `CUserCrypto::Enc` / `Dec` at `00405010` / `00405030` exclude the length prefix.
- `SAes::CtrInc` at `0040db10` increments the 128-bit counter in little-endian
  32-bit limbs. Standard big-endian AES-CTR is not interchangeable.
- `CUserCrypto::KeyPublicSend` at `00404de0` sends the packed 197-byte `TP_KEY`:
  opcode, mode, exponent length, modulus length, exponent[64], modulus[128].
  Integer fields use little-endian GMP limbs.
- `CUserCrypto::KeyInit` at `00404ea0` decrypts raw RSA into a zero-padded 128-byte
  little-endian secret. HMAC-SHA256 uses that secret as key and the public modulus
  as message; its halves become the AES-128 key and counter IV.

Native framing, cryptography and asynchronous TCP login are implemented and
tested with a generated protocol peer and SQL Server. Realm selection validates
availability, capacity and version, and reserves a single-use session handoff.
`--serve` runs both TCP listeners with a shared session registry; `--serve-login`
advertises its world offline. Actual client work is deferred.

`CUser::OnDispatchDB` (`00404a10`) hashes the submitted password and calls
`GetUserMD5` (`00406e50`), which expects `usp_try_gamelogin_korea` and three
result columns. The backups lack that procedure. Column 3 becomes `nJoinDiff`
in `TP_LUSER_CONNECT` (offset 99), sent by `ChkPasswdMD5` (`00404700`). Its SQL
semantics are not yet established. In `ps_game.exe`, `CWorld::LoginUser`
(`0040c510`) stores it at `CUser + 5814h`; `EnterWorld` and `LeaveWorld` compare
it against 3 (`0044aa23`, `0044ad01`). This does not establish the SQL time unit.
The native account repository currently tests
the available plaintext `usp_Try_GameLogin3_R` path, not this missing MD5 path.

`CUser::OnRecv` terminates username and password at payload offsets `0Eh` and
`2Eh`; their data starts at 2 and `22h`. `TP_LOGIN` is 24 bytes; realm entries
are 38 bytes and sent in batches of 48. `SAgent::PacketSystem` (`004127b0`)
closes opcode 2 and consumes opcode 3. Failed login closes on attempt 10.
`CUserCrypto::GetKeyGen` (`00404d50`) derives the 32-byte world key as
`AES-key || SHA256(IV)[0:16]`. In `ps_session.exe`, `CUserMgr::AddUser`
(`00409700`) replaces an account's previous owner; native removal also checks
the session ID so the old socket cannot erase its replacement.

World handoff: `ps_game.exe` `CUser::PacketProcessing` (`00466da0`) accepts
`TPC_GAME_CONNECT` (`A301`, 22 bytes: user ID and 16-byte session token) before
encryption. `CWorld::LoginUser` (`0040c510`) initializes the derived AES state and
sends `TP_GAME_CONNECT` (`A301`, 20 bytes: result, encryption mode and 16-byte XOR
seed). `CUserCrypto::EnterCharSelect` / `LeaveCharSelect` (`00458060` / `00458070`)
switch outgoing traffic between AES and a separate world XOR transform; incoming
traffic retains AES. `CryptoInitKey` (`00457eb0`) constructs the 4096-byte table:
the first 32 bytes are SHA256(seed); each subsequent block hashes the previous
block's final 16 bytes. `CryptoXor` (`00457f70`) XORs payload byte `i` with table
byte `payload_length + i`. Returning to selection resumes the outgoing AES
counter without advancing it during XOR traffic. Native cipher and game protocol
tests cover these transitions, encrypted greeting, replay and expired tokens.
`ps_session.exe` `AddUserSessionLost` (`0040a2f0`) gives a handoff 30 seconds.
`stUserInfo::EnableConnectGame` (`00402fa0`) checks realm and token; its IP
argument is unused. Closing login after selection retains the reservation;
closing its world owner removes it without removing a replacement login.

Character selection contracts: `ps_dbAgent.exe` `DBCharacter::LoadCountry`
(`0041fdc0`) calls `usp_Read_User_MaxGrow_R`; no row means country 2 and max grow
2, otherwise max grow is clamped to at least 2. `LoadCharacterList` (`0041d930`)
reads `usp_Read_Chars_R` and equipment through `usp_Read_Char_Items_Simple_R`
with `(CharID, 8, 0)`; equipped slot 7 supplies the six cloak bytes. It removes
ASCII spaces from names. The creation time is a seconds offset from the world's
base calendar, not a Unix timestamp. `CWorld::SetWorldDay` (`0040f1c0`) decodes
the original packed calendar (year-2000, month, day, hour, minute, second).
`ps_game.exe` `CUser::PacketUserDBChar` (`0046cf60`, case `403`) sends country
(`0109`, 4 bytes), empty slots (`0101`, 7 bytes), then populated slots
(`0101`, 83 bytes). There are five slots. The internal 86-byte database records
are transformed, including map checks, into the client's 80-byte `stCharacterList`.
At `0046d318` / `0046d325`, assembly overwrites the last two bytes of `szName[21]`
with rename and remaining-time flags (wire offsets 76 and 75 respectively).
Native roster loading/serialization is implemented and tested against the original
procedures, including a rolled-back populated fixture. Native TCP tests cover
login-to-world transfer, encrypted roster, ordered asynchronous dispatch, replay
and account replacement while connected to the world. Map normalization, legacy
wire name encoding, timed deletions, billing-item delivery, remaining character commands
and gameplay remain pending. `CWorld::DBWorldDayLoad` / `DBWorldDaySave`
(`ps_dbAgent.exe` `0040ef40` / `0040f070`) retain a nonzero packed base from
`usp_Read_WorldInfo_E`; a missing/zero value is initialized through
`usp_Save_WorldInfo_E`. Native startup implements this using the SQL Server local
calendar, matching the timezone of character RegDate. SQL tests cover initialization,
stable reload, fixed persisted dates and rollback without identity changes.

Creation research: `ChkCharName` (`ps_game.exe` `0046bf00`) only checks at least
two nonzero bytes; `TrimString` (`0046c250`) terminates the bounded buffer and
removes ASCII spaces. `CUser::ProcessCharacterCreate` (`00480e90`) applies class
stats, mode multipliers (1, 1.1, 1.5), racial bonuses, configured random start
positions and base equipment. `DBCharacter::CreateCharacter` (`ps_dbAgent.exe`
`00420270`) wraps `usp_Create_Char_R` and every `usp_Save_Char_Item_Add_E` call
in an outer transaction. The available create procedure overrides SkillPoint to
5. Native request validation, class/race resources, positions and transactional
creation persistence are implemented. `CMap::LoadGameMapConfig` (`004270a0`)
reads the four racial start arrays from `[BASE]` in `data/map.ini`, with original
fallback coordinates. `CGameData::LoadBaseItem` (`004bf800`) ignores the base
definition's Slot/Quality: it takes quality from Items, equips the first type
1..19 in slot 5 and puts other items into consecutive inventory slots. SQL
creation uses a transaction-scoped catalog lock because the recovered procedure
returns `IDENT_CURRENT`, shared across accounts. Country save retains the original
`usp_Save_Char_Country_E` contract. Native TCP now dispatches creation (`0102`),
country (`0109`) and close (`0107`) sequentially using per-connection roster state.
Creation replies with the original three-byte result, followed by the refreshed
five-slot list on success. Country save only updates internal state; game
`PacketUserDBChar` case `402` emits no client reply. Country change runs pending
deletions first (`ps_dbAgent.exe` `CUser::ProcessDelReqNoTimeCheck`, `004043e0`)
through `usp_Delete_Char_E`, retaining SQL guild/deleted-character side effects.
The current session's maximum grow mode stays cached across creation; reconnecting
reloads it from the available procedure. Legacy wire name decoding remains pending.

Item IDs follow `SUniqueIDGen::GenID` (`ps_game.exe` `004cbe60`) and the creation
caller's server-ID addition (`00481750`): packed local calendar in the high 32 bits,
counter in bits 16..31 and server ID in the low bits. The constructor (`00402550`)
sets the counter/start to zero. Calendar/counter refresh every 2000 milliseconds;
item MakeTime is sampled independently. Native IDs serialize concurrent callers
and refuse counter wrap within an interval. `tools/data.py` stages the original
server Data files locally; standalone creation reads their racial start positions.

Creation integration copies the original tables, defaults, database collation
and procedures into a disposable catalog. It verifies valid creation,
occupied/duplicate names, concurrent names and rollback after failure on the
second starter item, plus encrypted TCP faction/create, roster refresh, mode caching,
relogin and pending deletion, then drops that catalog. Existing account/character data
and identity counters are preserved. The original starter craft label at
`ps_dbAgent.exe` `00467e70` is CP949 text. Native UTF-8 parameters bind as Unicode;
the restored database's Chinese varchar procedure collation converts that label
to five question marks. The original Windows ANSI path's collation behavior is
not yet verified; no forced collation conversion is added to the procedure.

Player loading follows `ps_dbAgent.exe` `DBCharacter::LoadCharacter` (`00420610`):
53 core columns and 17 supporting catalogs, including inventory, bank, skills,
effects, quests, quick slots, cash, social records and gifts. Native selection
validates ownership and selectable flags before the detail procedure updates
JoinDate. Duplicate items relocate to available bag/bank slots; skills renumber
in procedure order, with the original 255-skill limit. Raw rows retain ancillary
data for later gameplay implementation. `DBCharacter::LoadUserGiftNotify`
(`0041f870`) consumes notifications through the original read/delete procedure.
Disposable SQL tests cover populated loading, rejected selections, negative cash,
gift consumption and unchanged source accounts. Only the scratch cash procedure
redirects its Users_Master reference to a local fixture table.

`ps_game.exe` `CGameData::LoadExp` (`004bfe00`) accumulates the four mode curves.
`CUser::SendCharacterData` (`00483140`) sends opcode `0105`, 32-bit kill counters
and a terminated guild name, totaling 59–83 payload bytes. Native serializers
cover these layouts and preserve single-precision coordinates. This component
is not connected to selection yet: maximum resources must first be calculated
from definitions, equipment, honor, passives and effects, rather than copied from
persisted current HP/MP/SP. World entry and gameplay remain pending.

Native base status initialization follows `CUser::InitEquipment` (`00454560`),
before equipment and effects: persisted attributes, per-job/level StatusDefs,
mode scaling, racial resources and stat contributions. Out-of-range job/level
uses job 0, level 60. `CUser::SetAttack` (`00454280`) now calculates the three
attack categories, retaining x87 float spills, truncation and 32-bit wrap.
The critical formula uses the original float constant `0x3e428f5c` (0.19).
`tools/attack_oracle.py` executes that original function with Unicorn and
regenerates 244 committed regression vectors; regular CTests need no emulator.
Blessing, active effects, damage resolution and world integration remain
pending. Ghidra's `EternityOffsets.java` gives anonymous
PDB padding fields unique offset names without inferring their meaning.

Equipment initialization now applies all 13 equipped slots in original order,
before blessing/passives/effects. `CGameData::LoadConstItem` (`004c6420`) derives
quality thresholds at 50%/10%, half effects and zero effects at the lowest tier.
`CUser::ItemEquipmentAdd` (`00454c70`) treats weapons, armor and accessories
separately; `GemEquipmentAdd` (`004560c0`) handles recovery gems, state blocks,
stat/resource bonuses and element conflicts. `CItem::CopyCraftExpassion`
(`004ba670`) decodes the nine two-digit craft bonuses and weapon/defense enchant
values from server `ItemEnchant.ini`; ineligible items retain empty craft state
when freshly initialized. Native initialization is tested with restored SQL
definitions and loaded inventory. `tools/equipment_oracle.py` executes the original
craft, equipment, gem and element routines to regenerate 138 committed vectors;
only Windows critical-section calls are replaced with no-ops around a supplied
local skill list. Equipment-change broadcasts, reroll/removal, wear updates and
combat execution are pending.

Passive initialization follows `CUser::InitPassive` (`004899a0`) and
`AddSkillPassive` (`00489a80`), including byte-sized weapon/shield mastery tables,
bag/movement limits and low-HP ability state. Repeated masteries add the updated
table total to equipped attack/speed, rather than only their latest increment.
`SetSkillAbility` (`00485e00`) implements all 46 stat/benefit modifiers; effect 3
does not change resource maxima, including stat-derived HP/MP/SP. Native billing
flags follow `BillingApply` (`00457a70`). `LoadConstSkill` (`004bffe0`) retains
three runtime skill levels, weapon-type caps, nonzero ability-value defaults and
the effect overrides for detail 93/122. `GetSkillInfo` (`00402770`) requires a
level-one definition. Original SQL definitions and loaded player skills pass
integration tests; `tools/skill_oracle.py` regenerates 1,152 ability and 144 passive
vectors by executing original code with state-initialization broadcasts disabled.
Low-HP activation, billing-service lookup and world integration
remain pending.

Active effect dispatch now follows `AddApplySkillBuff` (`00484cf0`) and
`AddApplySkillDebuff` (`004855b0`): modifier signs, mirror/block categories,
movement restrictions, appearance state, targeting flags and elemental overrides.
Detail 91 negates only its first ability; debuff details 62/67 negate all three.
Speed getters (`0048a460` / `0048a4d0`) preserve clamps and the 255 disabled value.
`EffectWorld` requires implementations for broadcasts, taunts, hide detection,
attack rebuilding and conflicting-element removal. These operations are not
implemented by the live world yet, so selection remains disconnected from this
stage. `tools/effect_oracle.py` verifies 296 original state/call-order vectors;
world operations are intercepted and recorded, including one supplied element
replacement. The vectors do not verify the delegated operations themselves.
Live expiry and periodic combat operations remain pending.

Persisted effect timers follow `CUser::AddApplySkill` (`00487ce0`). Ordinary
effects use millisecond deadlines; FixRange=1 effects use seconds and consume
disconnected time. Restore adds ten seconds, capped by KeepTime, with the
original unsigned deadline comparison and wrap behavior. FixRange=1 converts
definition KeepTime hours to seconds. Signed SQL KeepTime values retain their
DWORD bits; the restored kit contains two negative values. Already-spent offline
effects request deletion from the DB-agent replica; missing definitions and map
restrictions only skip loading. `EnableApplySkillByMap` (`004845f0`) retains
map 18's skill/level exceptions. `ChkEndTimeSkill` (`004865b0`) supplies the
expiry predicate. `tools/effect_timer_oracle.py` verifies 178 restoration vectors
and expiry branches, stopping before removal/notification operations. Player SQL
snapshots now retain typed persisted effects. Live actor allocation and the
world's tick loop still need integration.

Effect removal now follows `RemApplySkillBuff` (`00485130`) and
`RemApplySkillDebuff` (`00485720`): inverse modifier signs, original counter
behavior, mirror/block cleanup, equipped-element restoration and transformation
rebuild calls. `EffectRemovalWorld` requires periodic combat/heal completion;
those world operations are still pending. The removal oracle verifies 288
state/call-order cases, excluding periodic damage/reflection/death. Final periodic
healing amounts are checked separately against 200 original executions before
world health clamping and broadcasts. `_setdefaultprecision` (`0051136e`)
configures 53-bit arithmetic; native attack/heal calculations now use that
precision while retaining original float spills and low-DWORD truncation.
`tools/heal_oracle.py` executes the original CRT setup before checking healing.

`EffectCycle` now connects restoration, state initialization and expiry in
insertion order. Expiry sends `050e` before reversal, then releases its instance;
group/raid `0c05`/`0c10` notifications follow, capped at 48 ordinary effects.
`tools/effect_cycle_oracle.py` executes the complete original expiry routine in
11 list scenarios, including group changes, reentrant insertion, low-HP passive
expiry and separately sampled healing time. `ps_dbAgent.exe`
`CUser::PacketUserSkillApply` (`00406250`, case `0902`) removes only the first
matching replica record. SQL changes wait for `DBCharacter::SaveCharacter`
(`00423190`), which deletes all saved effects and re-adds nonzero timers from the ordered replica
through `usp_Save_Char_ApplySkill_Add_E2`. Native effect snapshot replacement
checks ownership and rolls back the whole replacement on failure. Disposable SQL
tests verify those contracts without changing existing characters. Live actor
scheduling, replica-message delivery, group broadcasting and disconnect cleanup
remain pending.

`ActorResources` follows `ps_game.exe` `CUser::Heal` (`004505a0`),
`SendRecoverAdd` (`00481f20`), `SendRecoverAddSet` (`00482060`) and
`SendMaxHP/SP/MP` (`00481990`/`00481aa0`/`00481ba0`). Arithmetic retains
DWORD wrapping, signed upper clamps and the recovery routes' WORD-sized
previous values. Heal resets damage history before party energy; recovery
resets it after view/party messages. View `0505` and self `0521` order HP/MP/SP;
party `0c03` and raid `0c0c` order HP/SP/MP. Max-resource `050b` uses HP=0,
SP=1, MP=2; HP goes to the view, SP/MP to self, followed by group `0c02`/`0c0b`.
Initialization suppresses both clamping and max-resource messages. Required
`ResourceSink` operations leave routing and damage-history ownership to the
future actor runtime. `tools/resources_oracle.py` checks 1,008 original executions
including packet bytes, call order, wrapping, negative recovery, group kinds and
initialization. Live actor integration remains pending.

Faction blessing initialization follows `ps_game.exe` `CGameData::LoadKCStatus`
(`004bf4a0`), `CUser::InitKCStatus` (`0048be90`) and `SendKCStatus` (`00481920`).
`KillStatus.SData` holds 25-byte records with six type/value pairs. Each country
starts with a zero level; later levels retain each modifier's prior maximum,
while duplicate types within one record keep the last value. Initialization
preserves signed DWORD product wrapping, float spills and the original order:
resource percentages, recovery/defense percentages, then direct stat additions.
Dexterity's blessing bonus adds MP in this executable (`0048c268`..`0048c277`).
The `0211` notification retains signed wrapped deadline subtraction. The oracle
executes three full original data loads (including the staged distribution),
240 actor initializations and 14 notifications. Runtime blessing changes,
world kill-count progression and integration with actor creation remain pending.

Initial character packets follow `ps_game.exe` `SendCharacterItem` (`004832b0`),
`Bank` (`00483410`), `Skill` (`00483540`), `SetSkill` (`00483890`), `Quick`
(`00483800`), `Quest` (`004835f0`), `QuestFinish` (`00483730`) and `SendSpeedMe`
(`00482730`). Inventory traverses visible bags/slots and batches 50 records;
bank batches 50 with the low DWORD of stored money. Empty inventory/bank,
effects and quests send nothing; learned skills and quick slots send empty
headers. Effects batch 128; quests and finished quests batch 255. Cooldowns use
signed wrapped subtraction and truncating division; negative effect seconds
become one, while zero stays zero. `EffectCycle::initial_packets` serializes
live instances in insertion order using their current definition's countdown
mode. The oracle verifies 321 full original serialization scenarios. Original
item `strcpy` leaves unused bytes after the craft NUL unspecified; native packets
zero those bytes and oracle comparison normalizes only that padding. Typed quest
runtime restoration, complete initialization sequencing, remaining entry packets
and live character selection integration are still pending.

`PacketUserDBSetInfo` (`0046d780`, case `0505`) also filters persisted effects
before `AddApplySkill`: details 74/75/76 and detail 90 with ChangeType exactly
100/101/102 request replica deletion and never allocate a runtime skill. This
filter precedes effect-category and map checks; a missing definition is ignored.
`EffectCycle::restore` now preserves that outer loading step.
`tools/effect_load_filter_oracle.py` verifies 84 original branch executions,
including exact WORD ChangeType comparisons and non-buff/debuff definitions.

Quest definitions follow `CQuestData::Load` (`004ca5d0`) in the server's
`NpcQuest.SData`. `DBInitializeConstNpcQuest` (`004caf80`) first reads NPC lists
and 256x256 item/quest links. Native prefix traversal locates the quest block;
it does not implement NPC behavior. All pointer-free definition fields are
retained without relying on host structure alignment. `GetQuest` (`0046c2c0`)
indexes by row position rather than the embedded ID. Name truncation follows the
original forced space/NUL. The oracle executes the complete original prefix and
quest loaders; the staged distribution contains 1,720 records at file offset
684,633. Integration compares every recovered definition byte with the original
loader's aggregate fingerprint. Three synthetic records verify individual fields,
padding, discarded text and long names. Post-load `CalcQuestItem`, quest condition
checks and rewards remain pending.

`Players::load` now retains typed quest, finished-quest and quick-slot state in
addition to its raw SQL catalogs (`ps_dbAgent.exe` `DBCharacter::LoadCharacter`,
`00420610`). `PacketUserDBSetInfo` (`ps_game.exe` `0046d780`, cases `0506/0507`)
restores minute-based start/end/minimum-success times with DWORD wrapping,
initializes active success to zero and counts mob/player kill quests. Unknown
active quests produce error intents; unknown finished quests are ignored.
Case `0504` reconstructs learned cooldown deadlines and stops on an unknown skill
definition, requiring disconnection. The oracles verify 432 original quest
restorations and 40 learned-skill load scenarios. SQL integration verifies the
typed fields and quick-slot packet, including its WORD item/skill number.
Live world delivery remains pending.

`Actor` now connects effect application/removal, resource changes, the effect
replica and instance cleanup. `SendSpeed` (`004827b0`) uses proximity routing;
`SendShape` (`00482590`) and vehicle partner `021c` (`00482660`) use the zone grid.
`SendShapeMe` (`00482510`) always uses `051d`, while the view uses `0524` for shape
102. Admin visibility and vehicle shape arithmetic retain their original rules.
`SendRunMode` (`004826c0`) sends a command without changing movement state;
`StatusAttackStop` (`00489ed0`) preserves modes zero/one and otherwise clears its
original attack fields. Final periodic healing sends actual clamped HP/SP/MP in
`050f`, after damage-history/party operations; `PSendViewCombat` (`004bd8d0`)
supplies the same proximity route. The oracle checks 896 complete original
operations. Effect apply/removal vectors also compare all calculated attack
fields, preserving `SetSkillAbility` (`00485e00`) recalculation points.

`load_actor` connects learned/quest/effect restoration, equipment, blessing,
passives, active effects, billing and the final silent resource clamp in original
loading order (`PacketUserDBChar`, `0046cf60`, case `0407`). Active-effect
initialization permits notifications and intermediate resource clamping;
passive initialization suppresses them. A combined test retains the resulting
partially filled MP after a later MP-max buff. Destruction follows
`ResetCharacter` (`0044a050`): free active instances without reversal packets or
replica deletion, including failed preparation. Required `ActorScene` services
still own the current user-zone binding, combat and party/guild lookups. The live world does not implement
those services yet; TCP character selection still does not enter the world.

`SendUserShape` (`ps_game.exe`, `00482ad0`) now serializes native `0303` grid-view
packets. Normal appearance includes death/motion, country/family, hair/face/size,
job/sex, party role, grow mode, kills, eight item type/id/enchantment triples and
the 20-byte character name plus NUL. Enchantment requires an exactly 20-byte craft
string with decimal digits at 18..19; it is not clamped to the combat enchant range.
Cloak presence adds its six gem bytes before the NUL-terminated guild name,
giving `67 + guild_length` or `73 + guild_length` bytes. Social lookups still run
outside a zone, but no packet is sent. Shape 100 with a copied-user snapshot
uses its cached appearance/equipment/name/extra fields and length, forces the
character name's final byte to NUL and skips current social lookups. A missing
snapshot falls back to normal appearance. The actor owns serialization and
receives current party/guild state through a required scene port. Loading copies
the persisted identity; the owner supplies account country. Snapshot capture and
live social/zone ownership remain pending. The oracle checks 256 original calls,
including arbitrary legacy name bytes, craft edge cases, cloak and copied fields.

`RemApplySkillHide` (`00489290`) is now native: a nonzero shape permits an ordered
search for the first detail-88 node, regardless of effect category. Only that node
is unlinked. Replica deletion and self `050e` precede clearing shape, grid-view
`051d`, self run-mode command `0210/1` and instance release; a group notification
follows, using the current group type. Transformation template/form fields remain
unchanged. Shape zero or no matching node leaves the actor untouched. The oracle
checks 256 full original calls, including multiple hide nodes and group changes.

`ZoneUsers` implements ordered cell membership and `ChkFindHide` (`00486e00`).
The query visits the clipped 3x3 cells in row order and users in insertion order
(`CCell::EnterUser`, `00423560`); unlinking the current user preserves iteration
(`CCell::LeaveUser`, `004235a0`). Dead users (life mode 1), allies and administrators
with visibility/attackability disabled are skipped. Distance ignores Y and includes
the range boundary, retaining original float spills around double square/sum/sqrt.
`IsTeam(CUser*)` (`0044bdf0`) checks self, the candidate's active duel opponent,
reciprocal guild-duel IDs/readiness, then country on map war type zero; other map
war types treat remaining users as allies. These are regular gameplay rules,
independent of the excluded `ps_gamewar` service. The actor's detail-93 effect now
queries its required scene zone and reveals actual actors, updating their replicas.
An oracle checks 128 original queries, including the real CRT sqrt (only Windows
thread-local storage is replaced), boundary/nonfinite coordinates, cell clipping,
guild duels and query order. Native tests also exercise cell moves and live unlink.
The membership API accepts resolved cell coordinates or original map geometry;
actors must outlive the grid. World admission, movement authorization, zone
broadcasts, mob/NPC membership and live TCP ownership remain pending.

`MapDefinition` loads complete `.svmap` files using `CMap::LoadMap` (`00425780`):
square extent, packed tile bits (`CBitArray::Create`, `00425250`), doors, ladders,
mob areas/spawn counts, NPC positions/angles, portals, bind boxes and other areas.
Mob-area deltas truncate the double-precision endpoint difference and clamp to
one; NPC radians use the original float degree factor, spill to float, then add
360 once if negative. Portal country values below 1001 produce ID -1; the country
is retained in both base/current fields. Runtime-only pointers are omitted;
mob/NPC definition binding and spawning remain separate. Only the initialized
byte/byte/WORD fields of other-area attributes are retained. Native loading
rejects truncated/invalid counts. The oracle runs original constructors and field
calculations, replacing allocation/file I/O, and checks four synthetic files and
all 77 staged maps through canonical field fingerprints. Full distribution checks
are part of integration, including sanitizer validation.

`MapGeometry` reconstructs `CZone::Create` (`00415c40`), `CMap::IsSafePos`
(`00415360`), `CCell::IsInnerCellView` (`004153c0`) and the coordinate phases of
`EnterUser` (`00415fa8`) / `MoveUserLocal` (`00416370`). Grid dimensions are
`size / 40 + 1`; view bounds for cell n are `40*n - 10` through `40*n + 50`,
inclusive. Entry uses an exclusive map-size bound, ignores Y, divides X/Z by 40
at double precision and truncates. Movement keeps its previous cell while inside
that cell's overlapping view. Original x87 comparison tests allow NaN through the
map/view helpers; entry subsequently rejects its integer-indefinite cell, while
movement can retain the existing cell. These helper quirks are tested explicitly.
`ZoneUsers` now applies these rules to positional attachment/movement, retaining
the original position-write order. Movement authorization and connection closure
remain the future world owner's responsibility. The oracle verifies 4,440 grid,
bounds, view and movement cases; a combined test checks the effect on detection
across overlapping cell windows and rejected-position preservation.

`MapConfiguration` follows `LoadGameMapConfig` (`004270a0`), `CMap::LoadConfig`
(`00426940`), `ParseMapPos` (`004264c0`), `ParseMapPosArray` (`00426660`) and
`ParseMapOpenTimeArray` (`00426820`). `BASE.MapCount` controls positional map IDs;
creation now uses the same start-position parser. Profile values are limited to
254 bytes, tokenized using the original delimiter set, with partial start tuples
excluded. Rebirth retains partial fields, and a common value of at least six bytes
is copied to both factions; otherwise each faction falls back separately. Only
MapType, weather and rebirth inherit `DEFAULT_SET_ZONE`. MapName defaults to
`<id>.svmap`; WarType P maps to 1. CreateType N/S/F/P/G/R/H maps to 0..6.
Weather state is restricted to 0..2, while other numeric fields retain original
DWORD wrapping and minute-to-millisecond arithmetic. Expiry adds 30 seconds;
seven opening-time triples preserve WORD conversion and partial writes. Unused
schedule hours are initialized to zero natively. Market mode is enabled for ID 42.

`ServerMaps` loads configuration and exactly its referenced files, including
aliases, in original ID order (`CWorld::InitMaps`, `00414710`, configuration/file
phases). `--check-original-loaders` now checks this path alongside SQL definitions.
The oracle executes original constructors, parsers, numeric conversion and string
formatting; Windows profile/TLS and single-thread CRT lock ports are isolated.
It checks 19 synthetic configurations and the staged INI's complete field
fingerprint. Integration loads its 68 configured maps and verifies shared creation
coordinates. Zone selection, access schedules, weather transitions and initial NPC
populations are reconstructed below. Obelisk binding, mob spawns and the running
world executor remain pending; loading the map registry does not enter the world.

`ZoneRegistry` reconstructs `CWorld::InitZones` (`00414870`), `ChkZoneID`
(`004146f0`) and `GetZoneByMapID` (`00413df0`). Startup prepares one zone per map;
mode 0 keeps it permanently, while every other mode releases its objects and
caches the allocation. Selection returns a permanent zone immediately. Mode 1
creates a new active instance each time; mode 2 reuses the first active instance.
Modes 3/4 key instances by party/guild. Modes 5/6 use guild ranking/house rules.
Permission bytes <=10 with a nonzero instance-owner override bypass ordinary
party/ranking/house checks, but mode 4 still requires a guild. Existing keyed
instances are returned after access checks. New dynamic instances reuse the
initial cache or allocate another zone; only ID-zero instances repeat map
preparation. Object initialization, expiry clearing, owner assignment, unsigned
statistics and insertion precede the server wait-queue call. Startup checks
initialization failures; dynamic selection preserves the original ignored return
values. Serial/counter DWORD wrapping and zero-valued owner IDs are retained.

`IsGuildRankMapOpenTime` (`00428d20`) checks only the first two configured schedule
slots and stops at the first day-FFFF sentinel. It handles overnight intervals
with WORD arithmetic and packs the entry stamp from scheduled hour/weekday and
current local day/month, excluding year. Ranking access rejects an unchanged
entry stamp. `CGuild::EnableGuildHouse` (`0042f0c0`) requires ranking index <31,
acquired byte exactly 1 and enabled byte nonzero. These predicates use local civil
time, separate from the persisted game-world calendar.

The oracle checks 1,053 original startup/selection sequences, 2,240 ranking windows
and 36 house predicates. Original list/access/counter code executes with isolated
allocator, object preparation, ownership-index and queue ports. Tests include
pool reuse, owner changes, zero IDs, failure paths and counter wrap, plus multiple
maps in one registry. `ZoneServices` remains a required dependency for actual
allocation, map/object initialization, release and queue ownership; the native
registry is not wired to TCP. Object runtime initialization, zone expiry/removal
and admission remain pending.

`ZoneEnvironment` reconstructs the environment portion of `CZone::MapInit`
(`0041c8a0`): weather state copy, door creation and type-1 region descriptors.
Door/region IDs compact after invalid records are skipped. Objects validate
truncated X/Z cell indices rather than world bounds, retaining the original
acceptance of small negative coordinates and positions in the last grid cell
beyond the nominal map edge. Region centers use double addition/division with a
final float spill; boxes and attribute values are retained. Door close times
convert seconds to milliseconds with DWORD wrapping. NPC, mob, linked-boss and
obelisk preparation are separate; this component does not replace full MapInit.

`CDoor::Use` (`004b9590`) toggles a door only after its unsigned use deadline,
sets a 2,000-ms cooldown and updates the close deadline when opening an automatic
door. Manual close retains the previous close deadline. `UpdateAutoClose`
(`004b95f0`) and `CZone::UpdateDoor` (`0041d530`) close eligible doors in order,
without resetting the use cooldown. Changes send `0403`, low-byte door ID and
open byte through the door's grid cell; a detached door changes state silently.

`stMapWeather::Update` (`004290d0`) consumes random only at its unsigned deadline.
`rand()%100 < rate` activates/continues weather with the active delay; failure
after active byte exactly 1 clears weather with the none delay. Other inactive
failures use the active delay. `UpdateWeather` (`0041d4d0`) broadcasts changes,
while `SendWeather` (`004201b0`) also supports direct synchronization. Both send
the five-byte `0451`: mode, low-byte type and power, with zeros when inactive.
`MicrosoftRandom` matches the per-thread MSVCRT generator (`00504697`), retaining
its wrapping state separately from the original mob drop generator.

The oracle checks environment fields for all 77 staged maps and four synthetic
maps, 384 door operations, 512 weather operations and five complete random
sequences. Original MapInit calculations/constructors run with NPC, obelisk and
door-cell insertion ports isolated. Native combined tests check ordered door
closing, cooldown and repeated weather activation. Integration and the original
loader CLI now prepare environment templates too. `RuntimeZone` can own this
environment state, but actual world transport/object services and TCP admission
remain pending.

Attack-only rebuilding now follows `InitEquipment(bool)` (`00454ba0`),
`ItemEquipmentAttackAdd` (`00456450`) and `GemEquipmentAttackAdd` (`004565b0`).
It resets only item attack, variance, physical defense and magic defense;
equipment uses cached quality tiers, mastery, shield, craft attack and gems in
slots 0..12. Range, speeds, attributes, maxima and unrelated modifiers remain
intact. Transformation uses the original mob-index lookup and first attack's
damage/variance plus defense/magic. `MobAttackTemplates` projects those fields
from `LoadConstMob` (`004c1720`), retaining WORD/DWORD bits and zero-HP-to-one
normalization; this does not implement mob AI/combat loading. `Actor` owns its
inventory and performs both rebuild paths without a scene callback. The oracle
checks 192 original rebuilds and all resulting attack values; actor tests cover
transformation reversal while the shape flag is still set. SQL integration checks
the projection against the restored original definitions.

Element replacement now executes inside `EffectCycle`/`Actor`, following
`AddApplySkillBuff` (`00484cf0`, detail 94). Conflicting detail-94 nodes are removed
regardless of effect category or countdown mode. Replica deletion and self
`050e` precede reversal/freeing; group notices follow, capped at 48. The original
shared list cursor (`CUser+0xa98`) matters: nested replacement finishes at the
sentinel, so `InitApplySkill` (`00487e50`) can revisit surviving earlier buffs.
Native initialization preserves this behavior, including repeated stat additions.
The oracle verifies 15 list scenarios with exact state/notification ordering,
70-node batches, group changes, reentrant insertion and nested initialization;
the actor test also checks its resulting persistence replica.

NPC definitions: `ps_game.exe` `CNpcData::Load` (`004c9410`) reads thirteen
positional lists from `NpcQuest.SData`: shops, gatekeepers and eleven ordinary
NPC lists. The native decoder preserves the common 276-byte prefix, item pairs,
start/end quest links and all three gate destinations. Country 1/2 maps to 0/1;
other values map to 2. Name bytes 29/30 become a space/NUL. Gate destination Y
adds the original single-precision 0.9 constant before spilling to float.
`GetNpc` (`0041d640`) accepts types 1..13 and one-based row positions, ignoring
embedded type/ID fields. The quest decoder now uses this NPC decoder to locate
the item-link/quest blocks. NPC movement and combat remain pending. The oracle
executes the original loader and 85 positional lookups per
input: six synthetic files plus the complete staged distribution. Integration
compares every normalized NPC field, shop item, gate and quest link.

`ZoneNpcs` reconstructs `CMap::InitNPC` (`00429130`), `CZone::EnterNpc`
(`00416d70`) and `LeaveNpc` (`00416e90`). Initialization skips type 5, unresolved
positional definitions, missing paths and positions outside the map. It uses only
the first position/angle, truncating angles to signed DWORDs with the x87 integer
indefinite on overflow/nonfinite input. A blocked type-8 guard logs the invalid
position, still allocates/enters, and sets its disable deadline to `FFFFFFFF`.
Unblocked NPCs preserve the allocator's existing deadline. Existing nonzero object
IDs survive reuse; only zero IDs consume the shared counter, including DWORD wrap.

Entry uses truncated X/Z divided by 40 and the zone's cell extent, allowing its
last extra cell beyond the nominal map boundary. Ordered cell membership, zone
membership and ID lookup update before the packed 23-byte `0E01` announcement.
Removal unlinks both registries before the six-byte `0E02` announcement; the zone
pointer clears before sending, and cell coordinates clear afterward. The native
pool follows `SSyncHeap<CNpc>::Alloc` (`0040acc0`) / `Free` (`0040ad30`): LIFO reuse
without implicit state reset. Pool/ID/log/transport services are required;
connection to the world executor, path movement and AI are not implemented here.

The NPC runtime oracle runs original lookup, geometry, tile tests and intrusive
cell insertion/removal for all 77 staged maps plus four synthetic populations.
Synthetic cases cover excluded/unknown NPC types, positional ID failure, empty
paths, boundaries, blocked guards, allocator failure, retained IDs/deadlines,
ID overflow, multiple path positions and angle overflow. Native integration
compares every initialized NPC and all ordered arrival/removal packets, with
sanitizers covering the same complete populations.

NPC movement: `ps_game.exe` `CZone::MoveNpc` (`00416f30`) validates destination
X/Z with `IsSafePos`, retaining the original unordered-comparison behavior. When
the destination is outside the current cell's inclusive expanded view, a possible
replacement cell is calculated from the NPC's **stored** position. A valid change
updates the intrusive cell lists, sends transition notices, then assigns the cell
coordinates; the destination position is assigned afterward. Zone membership and
ID indexing stay intact. This produces delayed cell changes during ordinary paths.
Invalid destinations return false without mutations or broadcasts. Angle remains
unchanged. The final 19-byte `0E03` packet contains the mode byte and destination
XYZ; a null NPC zone pointer suppresses this final broadcast.

`SendMoveNpc` (`0041faa0`) clips old/new observer areas to +/-2 cells. Its original
exclusion predicate requires being outside **both axes** of the other area, so
horizontal/vertical shifts send no appearance/removal notices. The native version
preserves this predicate and row-major order: `0E02` to eligible old cells, then
`0E01` using the still-stored position to eligible new cells. Each broadcast has
an independent nullable buffer allocation. `PSendView` (`00421120`) sends the final
movement across the current clipped 5x5 observer area through `PSendAll`: priority
6, forced immediate recipient IDs 0/0, timeout 65000. Actual transport buffers and
priority queues remain required world services.

The movement oracle executes original geometry, list changes, visibility routing
and prioritized-view routing. Native comparisons cover 1,536 independent cases
and 144 consecutive steps across twelve paths, including view boundaries,
nonfinite positions, stale stored positions/cells, null ownership, byte/WORD wire
truncation, and every combination of three independent buffer allocation failures.
Every per-cell packet includes a snapshot of position/cell state at delivery,
verifying notification order as well as packet bytes and recipients.

NPC waypoint patrols: `ps_game.exe` `CNpc::UpdateMove` (`004bbeb0`) waits while
users are interacting, until its unsigned next-move deadline, or when its source
path is absent/contains at most one point. Direction starts forward and the next
position starts at index zero. The current X/Z coordinates truncate to signed
integers before subtraction from the target point; nonfinite/overflow conversion
uses the x87 integer indefinite. Distance uses the original float spill before
CRT sqrt (`SVector::Length`/`Length2`, `0041b2e0`/`004a7c30`). Normalized steps also
retain float division/multiplication/addition spills.

At distance <= `nMoveDist`, a successful move to the target advances the index,
reversing at either end without repeating the terminal point. A failed move keeps
the index/direction. Both outcomes consume one signed `rand()%10000` and schedule
`tick + nMoveSpeed + 10000 + random_remainder`, with DWORD wrapping. Longer steps
multiply the normalized vector by `nMoveDist`, retaining current Y; they ignore
the movement result and schedule `tick + nMoveSpeed` without RNG. All moves use
mode zero and route through the native `ZoneNpcs::move` implementation. The idle
and guard dispatchers must still enforce disable deadlines and combat states;
this patrol function does not replace the pending guard AI/world tick.

The patrol oracle executes original `UpdateMove`, vector math and CRT sqrt with
isolated movement/RNG ports. It compares 1,536 independent states and twelve
48-tick routes. The native route tests call real zone movement and visibility
broadcasts, covering endpoint reversal, fractional/nonfinite coordinates, range
and timer overflow, interacting users, missing/short paths and failed moves.

Map collision/visibility prerequisites: `MapDefinition::blocked` reconstructs
`ps_game.exe` `CMap::IsBlockSafe` (`0041b420`), treating positions outside the map
as blocked and reading packed tile bits least-significant-first. Guard preparation
now shares this native collision query.

Both `CMap::EnableLine` overloads (`0042ab80`, `0042ade0`) validate the two integer
endpoints, then sample a DDA line along the longer axis. The minor-axis accumulator
and slope spill to float on each iteration; sampled coordinates truncate toward
zero. The start tile is excluded and the destination is included; a coincident
start/destination returns true even on a blocked tile. The output overload leaves
its output untouched for invalid endpoints, initializes it to the start for valid
ones, and only changes it on collision. A collision returns the preceding major
coordinate plus truncation of the current accumulator minus the slope. On success
the output remains the start. It is not the last point traversed. Both overloads
produce the same boolean result. Native A* search and guard navigation are
described below; combat and the live world executor remain pending.

The line oracle executes both original overloads, safe/unchecked collision and bit
queries without replacing any function. Native validation covers 4,096 queries
across sixteen synthetic collision maps and 128 queries on each of all 77 staged
maps, including invalid endpoints, zero-length rays, blocked starts, cardinal and
diagonal rays, reversals, single-tile maps and float accumulation near obstacles.

Native A* reconstruction follows `ps_game.exe` `AStar::FindPath` (`0048eda0`),
`GetSuccessorList` (`0048ecf0`), priority-tree operations (`0048e810`/`0048e850`)
and `PathToLine` (`0048f4a0`). It preserves eight neighbors in NW/N/NE/E/SE/S/SW/W
order and permits diagonal corner crossing. Step costs are squared displacement
(1 cardinal, 2 diagonal); the heuristic is squared distance to the goal, with the
original float spills. Equal total costs insert on the right of the priority tree.
Reopening a node reinserts its left/right subtrees before recycling it. Candidate
nodes are allocated as a group before processing, using original LIFO recycling.

The scan rectangle extends each endpoint range by half that axis's difference,
capped at 20 and clipped to the map. The rectangle controls expansion of the
current node, while generated neighbors only check map bounds and tile collision.
The start tile may be blocked. Success links a path to the separate goal node,
removes intermediate nodes of straight runs and starts the cursor after the start.
A coincident start/goal immediately returns true while preserving previous path,
cursor, found flag and scan bounds. `ResetPath` (`0048ec70`) clears path pointers
without clearing the found flag; `Reset` (`0048ec60`) also clears it. Advancing past
the final node clears both. `SetPath` (`004a6b20`) installs a single current node.
The native owner must serialize these operations; combat and the live world
executor remain pending. The oracle executes original tree operations, node recycling,
successors, collision queries and compression, isolating allocation and locks. It
compares 1,536 searches, 416 operations across persistent paths (including cursor,
reset and single-point replacement), plus eight local searches on each of all 77
staged maps. Both complete compressed paths and cursor/flag/bounds are compared.

Guard navigation reconstructs `ps_game.exe` `CNpc::Chase` (`004bca00`) and
`ReturnHome` (`004bcf30`) using native collision/line queries, A* and `ZoneNpcs`
movement. Each NPC owns its path, reverse breadcrumb trail, status, target and
navigation deadlines. Chase waits on its unsigned deadline, stops when its current
tile is blocked, and reports attack proximity only at distance strictly below
`float(range)+1`. Targets beyond 30 units from either the guard or its current
route point trigger return mode. Obstructed unblocked targets use A*; blocked
targets distinguish no progress from a partial clear line. Status 2 guards status
writes, while target field clearing follows each individual original branch.
A failed A* search separately samples the clock and sets the disable deadline to
that value plus 3000 ms.

Chase movement subtracts one from the waypoint distance before choosing a step.
A near step broadcasts `position + integer_delta - normalized_direction*0.3f`.
After a successful move, the NPC's stored position becomes the full integer-delta
endpoint. This preserves the original difference between announced and internal
positions. Near steps scale the truncated `nMoveSpeed/4` interval by remaining
waypoint distance; far steps use the whole quarter interval. The timer conversion
preserves float spills and signed x87 DWORD integer-indefinite behavior, including
large unsigned ticks. Line coordinates instead use the low DWORD of `_ftol2`
(`0051c424`), whose QWORD conversion differs for nonfinite/overflow inputs.

`ChaseTrail` follows `ChasePathList::AddPath` (`0048f660`): when a line to the node
after the head is clear, replace the head position; otherwise suppress duplicate
heads or push a new head. Return navigation can pop these breadcrumbs into a
single-point A* cursor when its direct line is blocked. It stops within strictly
1.5 units of home, clears its path/trail on arrival or blocked/unreachable home,
and uses mode-1 native zone movement for both short and full steps. Guard
attacks and the live idle/guard dispatcher remain pending. Target queries are
described below.

The navigation oracle runs original chase/return, breadcrumb, A*, collision,
vector and CRT routines with allocation/movement/clock/lock/TLS ports isolated.
It compares 1,544 states and three persistent routes totaling 108 updates through
real native zone movement. An address-based coverage check verifies 21 navigation
branches, including both blocked-target outcomes, A* failure, breadcrumbs, near
and far steps, proximity, leash and deadline checks. Native and sanitizer tests
compare complete NPC/path/trail/deadline state, clock calls and movement arguments.

Guard target queries reconstruct `ps_game.exe` `CNpc::FindTarget` (`004bc180`)
and both `SetTarget` overloads (`004bc750`, `004bc790`). The query rejects an
invalid/blocked guard tile, then scans its clipped 3x3 cell neighborhood in row
order. Each cell visits players first, then mobs, preserving insertion order. It
returns the first eligible candidate; it does not select the nearest globally.
`ZoneUsers::visit_cell` supplies actual native actors through the shared live cell
cursor. Actors own their original runtime guard deadline (`CUser+5850`). The mob
runtime must provide ordered cell traversal, live FSM status and its equivalent
shared deadline (`CMob+2b0`); these are required interfaces with no default stubs.

Players must have a different country, mode other than 1, zero shape, mob targeting
enabled, and both visibility/attackability flags. Mobs require a
different country unless the NPC country is 2, which treats every mob faction as
an enemy; derived mob status must differ from 2. Both categories use inclusive
20-unit distance from the guard and 30-unit distance from its current route point,
ignoring Y. Float differences, double squares/sum/sqrt and final float spill match
the original; NaN distances fail. No target-tile or line-of-sight check is added.

The clock is sampled after distance checks, before reading the candidate's unsigned
guard deadline. A ready candidate can make `FindTarget` return true while an
already-attacking NPC (status 1) preserves its existing target and leaves the
candidate deadline unchanged. Otherwise setters recheck death/live mob state,
preserve NPC status 2, assign target kind/ID, sample the clock again and write
`sample + 5000` with DWORD wrapping. Direct setters intentionally do not repeat
search range, country, visibility or cooldown filters. State changes between the
two clock samples therefore matter and are preserved natively.

The oracle executes original cell lists, player filters, inline/live mob FSM
lookups and CRT distance math, with clock/lock/TLS ports isolated. Native tests
compare 1,024 queries, 256 direct setters and 128 rounds across shared deadlines,
using real actors and the native user grid. Cases include boundary/NaN distances,
neutral countries, dead/hidden/transformed/admin-filtered players, scalar/empty/
active mob FSMs, clock wrap and changes to state or deadlines during sampling.
Full mob objects, guard attacks and the live world dispatcher remain pending.

Scalar HP damage reconstructs `ps_game.exe` `CUser::Damage(int,DWORD,int)`
(`0044c120`). Dead mode 1 or zero HP returns false without side effects. Source
kind 3 with nonzero protection (`CUser+12fc`) and signed damage at least half
signed maximum HP subtracts one damage with DWORD wrapping, then sends `0525`
with the protection's low WORD. Defense wear precedes damage attribution. The
signed HP/damage comparison determines lethality: lethal damage zeros HP, sends
group HP and returns true; otherwise subtraction wraps, positive damage sends
group HP, and the low-HP ability check runs even for zero/negative damage.
This routine does not itself change the actor's life mode or execute death.

`SendEnergyHPToParty` (`004824b0`) sends nine bytes: `0c01` for party or `0c0a`
for raid, character DWORD, zero resource byte and current HP WORD. The actor is
excluded. There is no zone requirement, proximity broadcast or reset of damage
attribution in this path. The native actor supplies equipment wear, attribution
and low-HP activation in the original order.

`CDamage::Add` (`004b8b60`), `Reset` (`004b8aa0`) and `GetMax` (`004b8c40`)
are implemented as an owned ledger. Records merge by stored BYTE kind compared
against the full input int and source DWORD; amounts wrap as DWORDs. Input kinds
outside 0..255 therefore create another truncated-kind entry instead of merging.
Traversal preserves first insertion order; maximum uses signed comparisons and
ties keep the oldest entry, including all-negative records. Reset empties the
list. The native vector retains capacity; original pooled allocation identity is
not exposed. The instruction oracle compares 1,000 scalar damage cases with
ordered callbacks/packets and 2,048 persistent list operations, plus empty reset.

Equipment wear reconstructs `ps_game.exe` `CUser::QualityDec` (`0045b350`),
`QualityByDefense` (`0045b290`) and `ItemEquipmentAttackRemQ/AddQ`
(`004566d0`/`00456620`). Null/zero-quality items return false. Other items use
wrapping DWORD subtraction, clamp on the resulting sign bit, then store a WORD;
negative decrements can increase quality. Thresholds select cached tiers 2/1/0.
A changed tier replaces only quality-dependent attack/variance/defense/magic
defense accumulators and recalculates final attack. Accessories use full effects
at every tier; their cache can still change. Craft attack cancels during removal
and addition, and gems, attributes, resource maxima and other modifiers remain.
There is no wear packet in these routines.

Defense wear skips nonzero no-durability-loss benefits or shape, then scans the
remaining suffix of slot order `6,0,1,3,2,4`. Every attempt advances the saved
cursor, including absent/broken items; the first eligible item stops the scan.
The cursor wraps for the next invocation, without revisiting earlier slots in
the current scan. Values >=6 do nothing; negative cursors are invalid runtime
state. `CUser::CUser` (`004499d0`, write at `00449bb1`) initializes this cursor to
zero; `ResetCharacter` (`0044a050`) preserves it, so future connection reuse must
retain it. The native actor owns an accessible cursor and recalculates final
defenses after a tier change. The oracle compares 1,136 direct decrements and
1,536 defense scans, with a concrete actor integration check and suffix/wrap tests.

Low-HP passives reconstruct `ps_game.exe` `CUser::ChkHPAbilityApply`
(`00487020`). A zero selected skill or active flag skips even the clock read.
Otherwise the unsigned wrapped `previous + 180000 <= tick` gate precedes lookup.
The HP threshold multiplies maximum HP by the definition's `LimitHP` BYTE with
DWORD wrapping, interprets the product as signed, rounds it to float, divides by
100 with the original 53-bit arithmetic and truncates to int. Current HP uses a
signed inclusive comparison. Missing definitions or failed allocation do nothing.

`SendSkillApply` (`00482860`) first sends the DB-agent application with the
definition's `KeepTime`, then self `050d` (instance DWORD, skill WORD, level BYTE),
then group `0c04`/raid `0c0e` excluding self (character DWORD, skill WORD, level
BYTE). Only after these notices are the active flag and previous tick updated;
all three abilities run through `SetSkillAbility` (`00485e00`), then the effect
is appended. Its runtime deadline is always the sampled tick plus 30000, even
when persisted `KeepTime` differs. Existing expiry reverses the abilities, clears
the active flag and removes the replica entry; the three-minute cooldown remains.

`ps_dbAgent.exe` `CUser::PacketUserSkillApply` (`00406250`, case `0901`) updates
the first matching skill ID/level in place or appends an absent pair. The owned
replica implements this alongside the existing first-match erase. `Actor::damage`
now connects scalar HP mutation, concrete equipment wear, owned attribution and
passive activation. Full healing resets owned attribution. Lethal damage reports
true to the caller without itself running death or changing life mode.

The oracle compares 1,354 original activation attempts, executing actual ability
mutations and list insertion while isolating allocation, clock, transport and
final-stat notification ports. Coverage includes float/overflow thresholds,
inclusive cooldowns and wrap, missing/invalid selections, active state, allocation
failure, ordered persistence/self/group notices and flag timing. Concrete actor
tests cover damage-triggered activation, first-match persistence, fixed expiry,
cooldown reactivation, protection leaving one HP, lethal damage and full healing.
Guard attack/death integration and TCP gameplay remain pending.

Death effect cleanup reconstructs `ps_game.exe` `CUser::ClearApplySkillByDeath`
(`00488000`). It preserves definitions with nonzero `FixRange` and, when the
death penalty is -1, skills above 200 except level 100. `FixRange==1` still
selects offline countdown/hour conversion; other nonzero values preserve on
death without selecting that clock. Removed effects reverse abilities only for
details 6/62/67/82/91: 62/67 add all three values, 6/82 subtract all, and 91 adds
the first then subtracts the others. Detail 6 clears its active flag before
reversal. Detail 90 rebuilds equipment attack, clears transformation and emits
user appearance only for change type 100. Each removal then erases its DB-agent
replica and releases the instance; ordinary self/group expiry notices are absent.

The final reset clears movement/action/form/casting blocks, the shape template,
SP/MP absorption, chase, attack-element flags and targeting/death flags even if
effects remain. Defense block modes and mirror skill IDs clear while their
values/levels remain. Equipped armor/weapon elements are restored when present,
even over a retained element effect; absent equipment leaves its prior element.
A speed notification finishes the routine. Runtime low-HP selection/cooldown and
the active flag of a retained passive survive. The cached copied-user profile at
`CUser+135c` survives; clearing the adjacent template at `+1358` does not free it.
The actor retains the derived presence of surviving defense-element skills.
The oracle compares 728 single/multiple-effect cases against original linked-list,
ability and element code. Actor checks cover persistent effects, passive removal,
resource clamps, replica order and absence of ordinary expiry notices.

Corpse handling reconstructs `ps_game.exe` `CUser::EnterDead` (`004584d0`),
`CZone::FindDead/FindDeadWorld/EnterDead/LeaveDead`
(`00417640`/`004176c0`/`00417720`/`00417820`), cell list operations
(`00423910`/`00423960`/`004239c0`), `CWorld::LeaveDead` (`0040d520`) and the
LIFO `CDead` heap (`0040aeb0`/`0040a6a0`). Capture stores character ID, position,
direction, seven appearance bytes, occupied equipment slots 0..7 and the name
through its NUL. Reuse preserves absent equipment and bytes beyond the new name.
Original fresh appearance memory is unspecified; native fresh snapshots start zeroed.

Entry validates truncated X/Z cells against the extra final cell extent, then
checks a world-wide ID index. Duplicate IDs log and return true without replacing
or attaching the new allocation. Entry emits no packet. Zone lookup logs/rejects
cross-zone bodies; world lookup has no such filter. Removal erases the global ID,
unlinks the cell, clears the zone, broadcasts `0406` (ID DWORD), clears coordinates
and returns the body to the heap. Missing IDs log and return false. The native
view rejects invalid negative/nonfinite cell indices that would access invalid
original memory; fractional negative positions truncating to cell zero remain valid.

`CCell::SendCellInfoEnter` (`00423aa0`, corpse block `00423ccb..00423e9e`)
sends `0405`: ID DWORD, XYZ floats, direction WORD, country/family/hair/face/size/
job/sex BYTEs, eight equipment types, eight equipment IDs and a NUL-terminated
name (44+name-length bytes). `SendCellInfoLeave` (`00424130`) sends `0406`
per body without removing it. Insertion order and a live shared cell cursor govern
both streams and searches. Runtime zones now have a corpse registry attachment;
concrete zone ownership, release scheduling and full death dispatch remain pending.
The oracle compares 396 persistent operations across two zones, including real
snapshot/list/packet/heap code, with global-map, transport and allocation ports.
Every allocation's snapshot/binding, global ownership and ordered notifications
are compared. Native checks also cover send-time unlink/reuse, shared-cursor
mutation, allocation failure, invalid cells and duplicate heap release.

Attack effect cancellation reconstructs `ps_game.exe` `RemApplySkillToMOB`
(`00489560`) and `RemApplySkillSleep` (`004893a0`). Shape cancellation requires
nonzero shape and inspects the first detail-90/88 candidate. Definition 127 with
level other than 100 and change type other than 101 stops the search without
removing it or trying later hide effects. Other candidates unlink, erase the
replica and notify self before sampling group membership. Transform removal
rebuilds equipment attack, clears shape/template and notifies shape; change type
100 also notifies user appearance, while 101 clears fixed form and sends speed.
Hide removal clears shape, notifies shape and enables running without clearing
the template. Instance release precedes the optional group removal notice, which
uses live membership/type. Cached copied-user data survives both paths.

Sleep removal requires nonzero `CUser+1344` and removes every detail-66 node
regardless of category. Each removal erases persistence, notifies self, records
at most 48 group notices if currently grouped, and releases its instance. It then
zeros the sleep block, sends speed even when no matching node existed, and sends
deferred notices using live group membership/type. Neither route uses generic
effect reversal or a clock. Shared active-list cursors remain live.
The oracle compares 2,304 scenarios with original lists, frees and packet builders,
including 70 simultaneous sleep effects, protected transforms, empty/missing
matches, categories, and group changes during release/speed. Actor checks exercise
the concrete attack rebuild, shape/speed/run packets, replica deletion and cached
profile retention. The full actor cancellation sequence is described below.

Actor action cancellation reconstructs `ps_game.exe` `CancleActionVehicle`
(`0044b2c0`), `CancleMove` (`0048a010`) and `StatusCastingStop` (`00489f10`).
Vehicle cancellation applies only to mode 2: resolve a nonzero partner, clear
the peer's partner/mode and send its `021c` vehicle message, then clear the
caller's partner. Resolve a nonzero invitation next, clear the peer invitation,
then the caller invitation/expiry. Standalone cancellation does not clear the
caller mode or other vehicle fields. Self, missing and zone-filtered peers retain
the same operation order.

Casting-stop handles mode 1 by clearing its request and mode; mode 2 uses the
vehicle routine, clears mode and sends ordinary shape. Mode 3 clears mode, sends
the caller's vehicle message, resolves its partner even for ID zero, clears the
peer partner/mode and sends peer shape, then clears the caller partner. Next it
cancels pending movement, attack-sensitive shapes and sleep in that order.
If attack mode remains 3, a self-user target restores the saved target pair.
A zero previous attack tick follows `StatusAttackStop` (`00489ed0`); otherwise
mode becomes 2, the next field becomes 1, and the clock sample writes `+1450`.
The stop path preserves that field. Saved target fields remain intact.

Pending move zero does nothing. Type zero clears only the pending field; type
1 or negative preserves it. Type >=2 clears pending, zeros a slot only for an
unsigned index <=10, then sends seven-byte `0222` (character DWORD, zero BYTE)
through ordinary radius-60 `SendView` (`004209a0`) when bound to a zone. This
route is separate from prioritized radius-60 speed/resource messages. Required
`ActorActionWorld` operations provide requester-aware user lookup/diagnostics
and this ordinary route; no default no-op implementation is supplied.

The oracle compares 1,536 two-actor operations, executing the original nested
shape/vehicle/speed/effect/attack-stop code with lookup, transport and clock ports
isolated. It checks both actors' 38-field snapshots and ordered notices, including
self/absent peers, vehicle modes, invalid slot indices, pending-move types,
target restoration, recovery clamps and clock wrap. A concrete active-effect
case verifies the complete vehicle/move/transform/sleep/resume sequence with
replica deletion. Live world adapters and guard combat dispatch remain pending.

Duel cleanup reconstructs `ps_game.exe` `CancleApplySkillDebuff` (`00487ed0`),
`PvPClearDebuff` (`0047c610`) and `PvPModeEnd` (`0047b200`). It removes every
category-3 node in insertion order: unlink, persisted-effect erase, cancellation,
then instance release. Outstanding periodic damage is discarded; no clock,
self removal packet or group removal packet is used. Other categories survive.
Cancellation restores abilities, decrements individual blocks with DWORD wrap,
restores equipped armor/gem element and clears chase as indicated by detail.
Transform cancellation tests the current shape: 101 clears fixed form and sends
speed; other shapes rebuild equipment attack. Both clear shape/template and send
shape, retaining the copied-user cache and omitting user-appearance notification.

`Actor::end_duel` performs that cleanup even for duel state zero, resets the
damage ledger, then sets duel state to zero, duel type to -1 and target kind to
zero. Opponent and target IDs survive. The oracle compares 868 direct-helper,
multi-effect-list and duel-end cases using original ability/element/list/ledger
code, including counter wrap and repeated skill identities. An actor integration
case combines negative effects, a surviving buff, replica erasure and damage
reset. Duel cleanup does not itself dispatch results or return wagers.

Attack-triggered duel exit reconstructs `PvPChkLeaveByAttack` (`0047c750`). State
zero skips lookup; other states resolve the opponent in the requester's zone,
and missing opponents preserve local state. Unknown states still perform lookup.
States 1/2 end the peer then caller and send both `2402`, reason 3, naming the
peer/caller respectively. States 3/4 return each player's own wager, peer first,
end both, send both `2406` naming the caller, then emit two `0e03` game logs,
peer first. State 5 ends both and sends caller `2408` reason 2, peer reason 3.
Self-opponents follow the same ordered calls, including two damage resets.

`ItemDropPvP` (`0047c990`) allocates a type-26 money item for a nonzero stake,
sets count 1, quality 0, make type `U`, null definition and drop metadata, then
enters the owner's zone with notification. Allocation failure skips only money.
Each of eight nonnull item pointers gets recipient user ID, party zero, drop
type 3 and owner drop ID. The original draws two positions per existing item,
overwriting the first; money draws once. All wager fields clear even if admission
fails. Other existing item fields survive. `GetDropPosRand` (`0048a270`) draws
X then Z offsets `(rand % 1000)/1000 - 0.5`, rounds each to float before adding
the owner's coordinates, and preserves Y. Required `DuelWorld` services supply
the shared random stream, pool allocation, zone admission, lookup and log transport;
no successful no-op world implementation is provided.

`SetGameLogMain` (`0045b0d0`) fills the 74-byte header with account ID/name,
character ID/name, level, experience, map, XYZ and packed local calendar.
`GetSystemTime` (`004cb8e0`) actually calls `GetLocalTime`; the native writer
preserves its DWORD year wrap. Name writes stop at NUL, retaining buffer tails.
Duel logs add the other character ID/name at 74/78 and reason 3 at 99. They reuse
one 100-byte buffer, preserving earlier name tails on the second write; native
fresh buffers start zeroed where original stack bytes are unspecified.
The oracle compares 768 header/drop/attack-exit cases with original nested
cleanup, float positioning, header/calendar and packet code. It covers every
duel state, absent/self peers, money allocation/admission failure, all eight
slots, repeated pointers, signed random inputs and calendar wrap. Full duel
request/accept/tick/death handlers and live item ownership remain pending.

Ground items reconstruct `ps_game.exe` `CZone::AddItem/RemoveItem/FindItem`
(`004170b0`/`00417140`/`004171b0`), both `EnterItem` overloads
(`00417210`/`00417320`), `LeaveItem` (`00417440`), `UpdateItem` (`00421e20`),
and cell enter/leave/find (`004237f0`/`00423840`/`004238a0`). `GroundItem` extends
the wager allocation with a runtime ID, expiry, zone and cell; the runtime ID is
separate from the definition's ItemID. `RuntimeZone` can own a `ZoneItems` view.

Entry truncates X/Z divided by 40 against the extra final cell extent, samples
the clock, writes expiry as DWORD tick+lifetime (default 60,000 ms), links the
cell, links the zone list, inserts the zone-local ID and broadcasts. The 22-byte
`0401` contains runtime ID, caller's appearance byte, type/type-ID/count and XYZ.
Appearance zero still broadcasts. Duplicate IDs attach both allocations but
retain the first map entry. Removing either allocation erases that ID, even if
the other allocation owned the map key. This differs from corpse registration.

Ordinary removal erases the ID and zone/cell links, clears the zone, broadcasts
six-byte `0402` (runtime ID), then clears cell coordinates without freeing the
allocation. Expiry scans insertion order with a live shared zone cursor; only
unsigned `expires_at < tick` expires, then follows removal with pool release.
There is no signed wrap-safe comparison. Cell entry/leave streams preserve
insertion order and use live shared cursors, emitting `0401` appearance zero
or `0402` without removing membership (`00424018..004240a7` and
`004242be..0042431c`). Negative/nonfinite cell indices that would dereference
invalid original memory throw; fractional negatives truncating to zero remain
valid. Allocation and pool semantics are described below.

The oracle compares 640 persistent two-zone operations using original intrusive
lists, admission, expiry and packet builders, with tree/clock/transport ports
isolated. It checks all allocations, per-zone indexes, ordered messages and
release timing, including duplicate keys, edge cells, explicit/default lifetime,
equality and DWORD wrap. Native checks cover removal/insertion during streaming,
nested shared-cursor lookup and release-time removal of the next item. A concrete
wager integration places money and two existing items in the actual zone view,
checks metadata/notices and expires all three through that view. Pickup permission
is described below; the running world's tick/transport adapter remains pending.

The item pool reconstructs `CObjectMgr::CreateItem/FreeItem`
(`0040a510`/`0040a590`), `SSyncHeap<CItem>::Alloc` (`0040adb0`), the item
constructor (`004b9f00`) and both `CItem::Init` overloads
(`004b9f30`/`004b9f80`). The item-specific counter at `CObjectMgr+4` starts at
1 (`0040a0e0`); a fresh item gets 2. Reuse is LIFO, preserving the runtime ID
unless it is zero. Increment uses DWORD wrap, including a zero ID at wrap that
is assigned again on its next allocation. This counter is independent of NPC,
mob, persisted UID and definition ID counters. Failed fresh allocation increments
the fresh-attempt count but consumes no ID. Null release is a no-op.

Every allocation resets type, type-ID, count, quality, six gems, the first craft
byte, creation time/type and eleven craft additions. Other fields survive,
including definition, persisted UID, world position/expiry and drop metadata.
Craft storage retains bytes beyond its first NUL; the native craft setter keeps
that storage and the logical string consistent. Parameterized initialization
uses the already-bound definition's full quality and supplied type/count/time;
it does not look up a new definition. Fresh original memory outside initialized
fields is unspecified; fresh native storage starts zeroed. Pool ownership and
its ID counter are protected by one mutex; zone/actor executors still serialize
individual item operations. Invalid attached, duplicate and foreign releases
throw instead of corrupting the original free list.

The oracle compares 640 persistent allocation/free/mutation/initialization cases
with original pool and reset code, including allocation failure, ID wrap and
retained craft tails. Native integration returns expired zone items to the actual
pool and checks their subsequent reuse; four concurrent batches hold 256 distinct
live IDs before release. Inventory transfers, pickup dispatch and runtime transport/tick
ownership remain pending.

Item/quest links reconstruct `CItemQuestData::Load/GetQuestLink`
(`004c9d90`/`004152f0`): all 256x256 slots contain ordered start/end WORD lists,
including duplicates and zero IDs. Lookup accepts only type/type-ID 1..255 but
returns a nonnull empty record for valid empty slots. Native loading retains the
complete matrix and consumed offset, rejects truncation and reads it after the
NPC definitions in `NpcQuest.SData`. `CGameData::CalcQuestItem` (`004bf400`)
clears each definition's quest-start flag, then sets it for a nonzero start count.
The loader check now loads all 505 linked slots and initializes native item flags
against the SQL item catalog.

Pickup reconstructs `CItem::PickEnable` (`004ba240`) and
`CZone::GetPickItem` (`004174a0`). No owner, matching user, or a matching nonzero
party with checking enabled grants ownership without a clock read. Otherwise,
unsigned `expires_at - 30000 < tick` grants access; equality remains protected,
including the original DWORD subtraction wrap. Full zone pickup first resolves
the item, then unconditionally resolves the caller's party when found. Missing
items set error 3; ownership rejection sets 0. With checking enabled, horizontal
distance greater than 10 sets 4. Float differences round before double square/sum,
sqrt and final float rounding; Y is ignored and unordered comparison preserves
the original behavior. Disabling checking also skips distance/quest checks and
disables shared party access, while ownership protection still applies.

After distance, runtime type 26 bypasses the quest gate. Other items with a
definition of real type 27 and no quest-start flag require a linked end quest
active on the caller. Links are queried in order until the first match, with no
deduplication; invalid/empty links or no match set error 2. Success uses ordinary
zone removal and returns the allocation without freeing it or changing the
caller's existing error word. `ItemPickWorld` requires live party, position,
active-quest and clock services; inventory admission remains a separate operation.

The oracle compares 1,536 permission/full-zone cases using original party lookup,
quest-list search, distance/CRT math and removal code. It also compares a complete
synthetic link matrix, flag initialization and the supplied distribution's link
hash/consumed offset. Cases include missing items, owner/group combinations,
deadline equality/wrap, exact/near distance boundaries, duplicate links and all
failure codes. SQL integration now runs the complete supplied link distribution
check. Inventory transfers and live TCP pickup dispatch remain pending.

Money reconstruction covers `ps_game.exe` `ChkAddableMoney` (`00438460`),
`IsvaildMoney` (`0044a040`), `ChkAddMoneyGet` (`0045b230`), `ChkAddMoney`
(`00478360`), `SendDBMoney` (`0046c700`), `ItemGetMoney` (`0045ec70`) and the
money branch of `ItemGet(CItem*)` (`0045df40..0045e085`). Bounds are unsigned
DWORDs: sufficient money accepts amount <= balance, addability accepts amount
<= UINT32_MAX-balance, and credit saturates at UINT32_MAX. `ChkAddMoneyGet`
also changes the caller's amount to the accepted portion; `ChkAddMoney` does not.
Arithmetic-only helpers send nothing. Actor loading now retains account UID and
money separately from character/runtime ID.

Credit always queues ten-byte `0603` (account UID, total balance), sends the
16-byte `0205` receipt, then emits an 82-byte `0801` game log with the common
74-byte header, accepted amount and current total. Zero credit still follows
that sequence. The receipt has bag 1/slot 0/type 26, with the accepted DWORD
at offsets 7..10. Direct credit uses type-ID 0/count 1; money-item credit copies
those two bytes from the item, then releases the detached allocation after the
log. Drop amount and item metadata remain unchanged, including when some money
cannot fit. Receipt amount is captured before transport; the log samples the
current balance after the common header, matching the original order.

`MoneyReplica` reconstructs the cached DWORD assigned by `ps_dbAgent.exe`
`CUser::PacketUserSetStatus` (`00405b00`, case `0603`) with owned account binding.
This status update does not itself write SQL. Character saving through
`DBCharacter::SaveCharacter` (`00423190`) persists this replica as described below.
Party loot distribution and live pickup command dispatch remain pending;
detached inventory admission is described below.
`MoneyWorld` requires ordered DB-status and log transport/calendar services.

The oracle compares 1,792 original bounds/saturation/receipt/log/free operations,
including zero, exact capacity, full DWORD values and calendar wrap. Native
checks apply each status packet to the owned money replica, reject other-account
or truncated updates, and join actual zone pickup, capped actor credit and pool
release. Actor-load checks retain distinct account identity and unsigned balance.

Detached item admission reconstructs the complete `ps_game.exe`
`CUser::ItemGet(CItem*)` (`0045df40`). Money delegates to its reconstructed path.
Other items capture the current party lock, fill matching partial stacks in bag
1..max-bags/slot 0..23 order, then seek the first empty slot in the same order.
Bag zero is never a destination. Matching uses type/type-ID only, ignoring UID,
quality, gems and craft. Each stack change sends `0707` (account UID, bag, slot,
count) before the 16-byte `0205` self receipt. Full merge leaves the input's final
chunk count intact; after group/log notices, the allocation returns to the pool.

Empty-slot admission retains the input allocation and projects its payload into
the actor's inventory, sends fixed 57-byte `0701`, then `0205`. Nonstackable
receipts retain quality/gems and the NUL craft string (16+strlen bytes); stackable
receipts zero those fields. The remaining count is stored whole in the empty
slot, even above the stack maximum. Definition loading now retains original
`Count` as `stack_count`. Later changes to a newly retained stack update both its
inventory row and pooled allocation. Bound pools must outlive the actor; teardown
returns newly retained allocations. Full loaded-inventory runtime ownership and
explicit equip/remove/disconnect orchestration still require integration.

Successful admission sends current group `0b08`/raid `0c14` with actor ID and
item type/type-ID through the original radius-100 party route. Membership is
sampled live; the initially captured lock is released at the end even if notices
use a changed group. Drop types 1/2 emit 151-byte `0401` loot logs; type 3 emits
174-byte `0408` with drop ID/name. Both carry common header, UID, definition ID,
item name, gems, creation time and craft. Type 1/2 logs use the input's current
remaining count, rather than the sum transferred across earlier stacks. Other
drop types omit the log. The type-3 success path uses raw zone lookup; partial
fallback uses `FindUser`, preserving that distinction in the required adapter.

When no empty slot exists, changed stacks stay changed. A reduced remainder gets
group/log notices, then `EnterItem` appearance 1 with the default lifetime; an
unchanged input is simply returned to the ground. Failed readmission is ignored
without freeing it, matching the original. No inventory-full self packet is
invented for this overload. `InventoryWorld` requires actual party lock/transport,
DB status, original name bytes, lookup and ground-admission services.

The oracle compares 640 complete original operations, including bag 9, zero/
negative active-bag counts, dense/empty inventories, oversized input, craft/gem
payloads, group changes during DB sends, all drop types, lookup variants, failed
ground return and actual default expiry/visibility. Native consecutive admissions
verify retained identity, later stack synchronization, pool reuse and teardown.
The ID-based pickup overload is described below; durable inventory saving and
live TCP commands remain pending.

ID pickup reconstructs the complete `CUser::ItemGet(DWORD)` (`0045d2e0`).
`Actor::pickup_item` first uses actual zone pickup permission/removal, with its
error word initialized to zero. Rejection sends four-byte `0205`, zero BYTE,
reason BYTE. Ordinary admission shares the recovered stacking/storage/log path;
if inventory is full, it sends reason 1 after any partial-transfer notices and
before ground readmission. This preserves the difference from detached-item
admission, which emits no inventory-full packet.

Money pickups use an all-or-nothing capacity check. For drop type 1, the signed
bonus rate starts at `SkillBenefits::mob_gold` (`CUser+58b4`), adding 20 for
amulet mode 2 or 50 for mode 3 (`+58b0`), with DWORD wrap. A positive rate spills
to float, divides by 100 in double precision, multiplies the unsigned amount
rounded to float and truncates to signed DWORD through x87 semantics. Conversion
overflow gives INT32_MIN. A nonpositive rate is added raw instead of converted;
other drop types receive no bonus. Amount plus bonus wraps as a DWORD.

If the resulting amount cannot fit, pickup returns the unchanged money item to
the ground first, then sends reason 5, without credit/status/log/free. Otherwise
it credits the complete amount, sends DB status, the receipt with the item's
type-ID/count, and `0801`, then frees the item. Original drop money remains
unchanged even when its credited amount includes a bonus. This is separate from
the saturating programmatic money-credit overload.

The oracle compares 2,048 complete original ID pickups, executing original
permission, party lookup, distance, admission, money arithmetic and fallback
code. It includes 392 forced successful money cases spanning all seven amulet
states, seven signed rates and eight boundary amounts, so incorrect arithmetic
cannot hide behind a capacity rejection. Other cases cover absence, owner/group
protection, quest rejection, failed readmission, stack payloads, group changes and
error/message ordering. The previous 640 detached-admission vectors regenerate
unchanged. Live packet dispatch, full inventory ownership during loading and
durable snapshot saving remain pending.


Equipment removal (`ps_game.exe`, `CUser::ItemEquipmentRem` `004553c0`,
`GemEquipmentRem` `00456280`) now runs through native actors. It uses cached
quality tiers, suppresses attack/defense changes while transformed, reverses
craft/gem stats and recovery, and clears the slot's cached tier. Craft reduction
underflow logs then clamps before gem reduction; later gem subtraction remains
unclamped. Weapon removal cancels all detail-94 effects in list order, updates
the replica and self, frees each node, sends at most 48 deferred group notices,
and finally clears the attack element. Armor removal clears its element without
removing detail-95 effects. Speed, HP/SP/MP maximum updates and recovery follow
in original order, except while initializing. The helper leaves inventory rows
intact for its caller. Two 1,024-case original-code fixtures cover arithmetic,
notification order and complete actor packets/resources. Element-list fixtures
also cover group changes, callbacks inserting effects and the 48-notice limit.
Inventory movement and TCP dispatch remain pending.


Trade/duel inventory references (`ps_game.exe`, `ExchangeItemRemove` `0046f310`,
`ExchangeCancleReady` `0046f450`, `ExchangeCancleReadyPvP` `0047d0a0`) now run
through actors. Trade mode 3 uses a captured peer pointer and the first matching
bag/slot, irrespective of its count. Nonzero readiness on either actor clears
both flags before the two `0a05 03 01` messages. Removal clears only the offer's
bag byte, preserving slot/count and later duplicates, then sends `0a07` with
sides 1/2. Duel state 5 plus offer mode 3 performs zone lookup even without a
matching slot, then follows the same sequence using `2409`/`240b`. Live duel
state is read after ordinary trade callbacks; self peers receive both messages.
Two thousand forty-eight original-code cases compare three actors, exact packets,
pre-send state and callback mutations, including captured peers, missing users,
duplicates, zero counts and readiness bytes other than 1. This reconstructs
reference invalidation; complete trade/duel exchange handlers remain pending.


Inventory removal (`ps_game.exe`, `ItemRemove` `0045f260`, `ItemRemoveFree`
`0045f470`, `ItemDelete` `0045f650`) now runs through actors with pooled items.
Equipment first sends dirty WORD quality via `070c`, clears that DB-quality
cache, reverses modifiers, sends `SendEquipment` `004839e0`, and recalculates
attack. The original still exposes the equipped pointer during `0507`; removal
does not substitute an empty-equipment payload. Offer cleanup follows, then the
inventory pointer clears before DB `0702` (9 bytes), log `0402` (152 bytes),
optional self `0206` (7 bytes), and pool release. The silent variant omits only
`0206`. The log ends with bag/slot/count after its 21-byte craft field. `ItemDelete`
scans bags 0 through the live maximum and slots 0..23, returning true even if
nothing matches. `0507` is 10 bytes, or 16 for a present cloak with its six gem
bytes; an absent cloak uses 10 bytes. Only visible slots 0..7 broadcast, using
the cell view without initialization suppression.

Resolved SQL inventory can now be loaded into the item pool during actor
preparation (`PacketUserDBSetInfo` `0046d780`, case `0502`, allocation at
`0046ddd7`; `CopyCraftExpassion` `004ba670`). Each successful allocation retains
its runtime ID, definition, UID and normalized craft cache. Allocation failure
skips that row before equipment/passive initialization. Existing SQL row repair
still runs before this phase. `InitEquipment` `00454560` snapshots the separate
13 WORD DB-quality cache; ordinary wear changes the item's quality and cached
tier without changing that snapshot. Actors release remaining allocations at
teardown. Projection-only actor fixtures remain supported; live removal requires
pooled ownership. Live equipment removal accepts slots 0..12; other bag-0
storage loads without applying equipment modifiers. The runtime owner must keep
those pools alive.

A 1,408-case original-code fixture covers complete normal/silent/bulk removal,
visibility, exact packets/logs, ordered frees, current resource clamps, offer
references, qualities and final inventories. Attack arithmetic is isolated in
this fixture and retains its separate original-code tests. Native preparation
also checks pooled SQL-row loading (including unequipped bag-0 storage beyond
slot 12), allocation failure and teardown. Inventory movement handlers and TCP
world dispatch remain pending; DB-agent inventory persistence is documented below.


Ground drops (`ps_game.exe`, complete `CUser::ItemDrop` `0045cd10`) now connect
pooled inventory, equipment removal, offer invalidation and zone admission.
Missing items and real type 27 return without messages. Equipment removal runs
before quantity validation. A nonstackable singleton drops whole regardless of
the requested amount; other oversized requests return after equipment changes.
Whole drops retain their allocation, runtime ID, UID, quality, gems and craft,
clear the inventory pointer, copy current XYZ, and set owner/drop ID to the
character, party 0 and drop type 2. Offer cleanup precedes DB `0702`, log `0402`,
`EnterItem` appearance 1, and self `0206`.

Partial drops allocate first. Failure preserves the stack/offers and still
sends the current `0206` slot. Success decrements the BYTE count, sends DB
`0702`, clears offer references, and creates a new UID through
`SUniqueIDGenByServer::GenID` `00457a50`. The new object has zero quality/gems,
`InitCraftExpansion` `004ba280`, current packed make time and make type `U`.
The log refers to the original UID and its remaining count, followed by new-item
admission and the remaining slot's `0206`. Zero/negative helper arguments retain
original low-BYTE arithmetic; packet-level positive-count validation is a separate
caller. Admission failure is ignored and does not free either allocation.
`ItemDropResult::whole`/`partial` identify the ownership operation, independently
of admission success; the world owner retains allocations that fail admission.

A 1,536-case original-code fixture compares complete whole/split drops, exact
packets/logs, new-item payloads, source resources/qualities/offers, allocation
failure and ignored admission results. Every tested bag covers both whole and
partial admission, including equipped oversized requests. Native zone integration
checks `0401` visibility, lifetime/expiry, recycling, new split identity and
retained whole-drop identity without double release at actor teardown. UID
arithmetic and zone internals retain their separate original-code fixtures.
Inventory movement and live TCP dispatch remain pending; the DB-agent inventory
SQL phase is documented below.


DB-agent inventory/bank replica (`ps_dbAgent.exe`, `CUser::PacketUserItem`
`00408480`) now implements owned commands `0701`..`070f`, `0711`, `0715`..`0717`.
Its inventory is 6 x 24 pointers at `CUser+e0`; the 240-slot bank starts at `+320`.
`CItem+8` holds save type: 0 unchanged, 1 insert, 2 update. Ordinary mutation only
changes 0 to 2. `0701` allocates a new insert, or adds the BYTE count with wrap
when type/typeID match an existing slot; mismatches report the original
`Disconnect` outcome for the owner to act on. UID and other incoming metadata
are ignored when updating an existing slot. Allocation failure leaves the slot
empty. `CopyString` `00408450` copies all 21 craft bytes and forces byte 20 to NUL;
the replica preserves the complete buffer as well as its string prefix.

Inventory/bank moves to empty destinations retain the original count. Occupied
destinations receive the two supplied counts before pointer swapping. Same-cell
moves preserve the second count write. Inventory/inventory and bank/bank swaps
remove the new source when its count becomes zero. Cross-bank moves journal old
rows before moving and force insert type 1 at the new locations. `0702` removes
when the requested count reaches/exceeds the current count; `0707` stores zero
without removing. Gem, craft and repair commands only replace money when their
item exists, while bulk repair `070b` replaces it even with no repair entries.
`0603` status uses that same owned money field.

`AddDeleteItem` `00405200` and `AddDeleteItemBank` `00405270` append FIFO UID
journals only when save type is not 1. UID zero and duplicate journal entries are
retained. The source `070e`/`070f` reset loops clear their ranges without recycling
allocations (`070f` calls `FreeItem` with a null pointer); native pools preserve
those allocations outside the reusable list. Both original loops dereference
all traversed slots without null checks; native application requires occupied
ranges. Other commands validate complete envelopes, owner and index domains
before mutation. Billing receipt `0710` and guild transfers `0712`/`0713` remain
separate pending owners and return unsupported here.

A 2,048-operation original-code fixture spans 32 persistent caches, comparing
all item fields, raw craft buffers, flags, ordered journals, money and allocation/
recycle/stranded counts after each command. It executes the original nested
journal/list, allocation, copy and free code. A native actor-to-replica flow also
checks money credit, stacking, split drop, real-zone pickup, persisted UID
removal and transient insert/remove suppression. Billing/guild cache owners,
complete character saving and TCP world integration remain pending.


Inventory/bank persistence reconstructs the corresponding phase of
`ps_dbAgent.exe`, `DBCharacter::SaveCharacter` (`00423190`,
`0042366d..00423f55`), insert recovery `SaveCharacterItemUnit` (`00424ac0`)
and `SaveCharacterBankItemUnit` (`00424bc0`). Each FIFO deletion journal is
written before its slot-ordered rows. The first delete call contains up to 40
UIDs; the original counter never resets, so later UIDs each produce one call
(`00423733`, `00423bc9`). Save type 1 inserts, 2 modifies; other values skip.
Failed inserts delete that UID with `ChkID=0` (global UID scope), then retry the
same insert once. Modifications and journal deletions have no recovery path.
UIDs preserve signed BIGINT bit patterns; item IDs preserve signed DWORDs;
`ConvTimeToString` (`0042ad20`) formats raw packed-date fields even when invalid.
The original procedures normalize inventory insert quality >=5000 to zero,
while bank inserts and both modifications retain SQL SMALLINT limits.

`InventoryStore::save_in_transaction` requires the enclosing character
transaction and never commits, rolls back or acknowledges it. Its owner must
retain exclusive cache access through the entire save, matching original
`SaveLock`/`SaveUnlock`; only after commit may it invoke the inventory/bank
portion of `CUser::SaveSuccess` (`00409560`) to clear journals and item flags.
All SQL values are bound parameters, including craft strings. A 128-scenario
original-code fixture compares 4,925 generated calls, including journal
boundaries, slot ordering, dirty flags, numeric extremes and packed dates.
Disposable original-procedure SQL checks cover inserts, modifications, bank
transfers, global-UID recovery, rollback across other character writes and
retry without losing pending changes. The remaining character save phases,
file fallback, save scheduling and runtime transaction owner are pending.


Character core persistence reconstructs `ps_dbAgent.exe`,
`CUser::PacketUserSetStatus` (`00405b00`), owned core commands `0601`..`0608`,
`060a`..`0610`, and body `0615`. Level-up, stat allocation, hunger/vigor,
resources, map/position, bank balance, kills, grow and separate point/stat
writes retain their original widths. Unknown extended-stat selectors succeed
without changes. Kill index 0..3 writes counters; 4..5 writes complete DWORD
kill-used fields, now retained in `PlayerCore` rather than truncated to BYTEs.
Native envelopes reject other identities, incomplete packets and kill indices
that would overwrite unrelated original memory. Quick slots, reset dispatch,
rename, inventory-status and market-list commands still have separate pending
owners. `CharacterReplica` owns the inventory cache and projects its money
into the core view, including item-command money updates.

`CharacterCoreStore` reconstructs the first Info/optional BodyChg portion of
`DBCharacter::SaveCharacter` (`0042328e..00423483`). Level is zero-extended;
points, stats, resources, map and direction use signed WORDs. Hunger stays
unsigned, vigor signed, experience/money/kills preserve signed DWORD bits.
Kill-used fields are read as signed low WORDs and then printed unsigned, even
though SQL accepts TINYINTs. Positions format the promoted float through the original CRT
`_fltout2` (`00450877`, 17 significant digits), `_fptostr` (`00450746`,
round decimal ties upward in magnitude), and `_cftof` (`0044b6c7`, six decimal
places). Exact negative zero formats positive; negative values rounded to zero
keep a minus sign. This differs from Linux printf, including `0.0078125`
becoming `0.007813`. Bound strings pass through DECIMAL(38,6) locals before REAL conversion
to match original numeric literals, including positive zero for `-0.000000`
and rejection beyond SQL's literal precision. Original procedures update
LeaveDate; BodyChg executes only while dirty. Its flag clears only on the
explicit core acknowledgment corresponding to `SaveSuccess` `004097b8`.

A 2,048-operation original-code fixture compares complete core bytes, 2,798
generated SQL calls and 160 body acknowledgments. It executes the original
`sprintf` (`0044447f`) and floating-point conversion code after the original
`_cfltcvt_init` (`00445779`), including decimal ties and large magnitudes. Disposable SQL checks compose
core/body and inventory phases in one caller-owned transaction, verifying signed
values, coordinate rounding, shared money, dirty retries, rollback after later
item failures, missing characters and SQL conversion errors. Connection rollback
now also recovers when an early error occurs before SQL Server opens a transaction
or the server already ended it; autocommit is restored only after rollback
completes. Billing, skills, quest/quick-slot/save-point phases, max-grow saving,
file fallback, scheduling and the complete runtime transaction owner remain pending.


Learned/applied skill caches reconstruct `ps_dbAgent.exe`,
`CUser::PacketUserSkill` (`00406100`, `0801`..`0804`) and
`PacketUserSkillApply` (`00406250`, `0901`..`0905`). Learned level 1 appends
a new allocation even for duplicate IDs; other levels update all matching IDs
and clear their delays. Skill points update even if allocation fails or no
matching skill exists. `0802` is a no-op. Individual delays can target any of
256 pointers; bulk delays skip indices beyond the live count. Loaded skill
numbers are reassigned sequentially, ignoring persisted numbers. The original
loader aborts after allocating its 256th row; native failed loads retain their
partial cache for disposal. Learning into the final pointer remains valid;
native envelopes reject further writes that would corrupt unrelated memory.

Applied add/set/remove affect the first ID/level match; add alone appends when
missing. `0904` uses a single forward list scan for its incoming records, never
rewinds and can update the same current node repeatedly for duplicate records.
Zero timers remain cached. `0902` unlinks without recycling; native pools retain
those allocations outside the available list. `0905` recycles the remaining
list in order. The shared LIFO heap (`0040e4c0`) preserves payloads across reuse,
including the unused applied-skill number byte. Fresh original payloads are
unspecified; native fresh allocations initialize them to zero.

The successful non-event reset job `SaveResetSkill` (`00404c40`) recycles learned
rows, applies the staged skill-point total, clears the live count, marks reset
and returns account-bound `050a`. Event-reset SQL, billing/request dispatch and
transport delivery remain pending. `SaveSuccess` (`004097a6..004097b8`) alone
advances saved count and clears reset after complete character commit.

`SkillStore` implements `SaveCharacter` `00423f55..004242c2`: modify the saved
learned prefix, or delete all on reset; insert remaining learned rows; delete
all applied rows and reinsert only nonzero timers in order. IDs are zero-extended
WORDs, learned delays are DWORDs supplied by WORD packets, and applied timers
preserve signed DWORD bits in SQL. Thus out-of-range positive IDs/delays fail
SQL SMALLINT conversion. The standalone `EffectStore` now preserves those same
ID and zero-timer rules. `CharacterReplica` shares core skill points with this
cache; each SQL phase leaves commit/rollback and acknowledgment to the owner.
An original-code fixture compares 2,048 persistent operations, 7,392 SQL calls,
96 resets, 128 acknowledgments and complete pool/cache state. Disposable SQL
checks compose core, inventory and skills, proving reset/new-row boundaries,
stored numbers, timer filtering, conversion failures and rollback/retry when
a later applied insert fails. Quest/quick-slot/save-point/billing phases,
max-grow saving, file fallback, scheduling and the runtime save owner are pending.


Quest persistence reconstructs `ps_dbAgent.exe`, `PacketUserQuest` (`00406590`,
`0a01`..`0a05`), `AddDeleteQuest` (`00405380`), and its shared LIFO quest heap
(`0040e3f0`). Active SQL rows load with save type 2; completed rows with 0.
Allocation failure skips a loaded row. Starting allocates first, appends an
active type-1 row with zero counters, then consumes the first same-ID completion,
even when the new allocation failed. Old success/delay/counter fields not written
by a command retain pooled payloads; fresh original payloads are unspecified,
while native fresh allocations start zeroed. Duplicate IDs remain ordered.

Ending with result 2 cancels the first active match, journaling unless it is
new type 1, then recycles it. Other results move that node to the completed
list, preserving its save type. Direct progress/counter updates affect the
first match without changing save type. Bulk progress makes one forward pass
and advances after each match, including duplicate records; it never rewinds.
Native validation checks ownership, full envelopes and three counter indices.

`QuestStore` reconstructs `SaveCharacter` `004242c2..00424617`: replay FIFO
deletions, write active type-1/type-2 inserts/updates, then write completed
type-1 inserts or type-2 updates. Loaded completed type 0 is skipped. IDs and
delays are zero-extended WORDs; positive values beyond SQL SMALLINT fail rather
than wrap. Each phase requires the complete caller-owned character transaction.

The full original `SaveSuccess` (`00409560`) leaves the quest deletion journal
intact: EBP becomes zero at `00409756`, so the branch at `004097d1` bypasses
its cleanup. It still sets active save types to 2 and completed types to 0
(`00409879..004098fe`). Consequently, a restarted quest can be inserted after
a queued delete on one save, deleted again on the next save, then receive a
successful zero-row update while remaining active in memory. Native persistence
retains this observed original behavior. A 2,048-operation fixture executes the
original pool/list/journal, complete SaveSuccess and 26,748 SQL calls, including
160 acknowledgments. Disposable SQL verifies progress, cancellation, finished
insert/update, allocation failure during restart, the retained-journal effect,
conversion failures and rollback/retry across core, inventory, skills and quests.
Quest gameplay, quick-slot/save-point/billing phases, max-grow saving, file
fallback, scheduling and the complete runtime save owner remain pending.


Interface state reconstructs `ps_dbAgent.exe`, quick-slot status `0609` in
`PacketUserSetStatus` (`00405b00`), `PacketUserSavePoint` (`00406c20`, `0d02`),
`SendCharacterQuick` (`0040a770`, `0508`) and `SendCharacterSavePoint`
(`0040aa10`, `0d01`). Quick entries carry three BYTEs and a WORD. Their actual
storage uses 128 six-byte entries at `CUser+3814`, ending at the quest list
`+3b14`; native validation rejects writes beyond that physical extent despite
stale PDB array counts. Updates replace the logical count and preserve unused
entries. Save points have four slots: absent map `ffff` or existing insert flag
produces flag 1; an unchanged occupied point becomes flag 2. Coordinates retain
raw float bits until SQL. Output packets omit absent maps and preserve slot order.
Typed SQL point rows now load into `PlayerSnapshot` and the interface cache,
ignoring slots >=4 and letting later duplicate rows win.

`InterfaceStore` reconstructs `SaveCharacter` `00424617..004247e2`. The explicit
first boolean argument controls quick replacement; both periodic-save attempts
pass false. Quick delete or insert errors log immediately, skip remaining quick
inserts, and continue to points. Thus a successful overall save can retain a
partial quick list. Point insert/update errors instead fail the character save
and require rollback of all phases. These original EXEC calls check query
status, without binding procedure return values. The native diagnostic port
reports a quick error even if a later point error propagates. SQL values remain
bound parameters; points share the recovered CRT coordinate formatter with core
saving, including decimal ties and SQL literal conversion.

Map `ffff` points generate no SQL, including no deletion of an older stored row.
`SaveSuccess` (`004098fe..00409916`) clears all four point flags after commit,
without changing coordinates or the quick list. A 1,024-state original-code
fixture compares complete logical state, packets, 5,111 planned SQL calls and
4,333 attempted calls, including 190 nonfatal quick errors and 303 fatal point
errors. It executes original CRT formatting, complete SaveSuccess and save-phase
branches. Disposable SQL verifies periodic/explicit quick saves, partial quick
commit, immediate diagnostics before a later point failure, rollback/retry
across core/inventory/skills/quests/interface, coordinate rounding and sentinel
behavior. Gameplay use and TCP delivery, billing, file fallback, scheduling
and the complete runtime save owner remain pending.


Market watch state reconstructs `ps_dbAgent.exe`, status `061a` in
`PacketUserSetStatus` (`00405b00`) and `SendCharacterConcernMarket` (`0040ae20`).
The cache holds ten DWORD IDs at `+867c`, a BYTE count at `+8678`, and state at
`+8674`: incoming nonempty lists set state 1, empty lists state 3. Loaded lists
start at 0. Order, duplicates and signed DWORD bit patterns remain intact.
`0510` emits no packet for an empty list. The game sender `SendConcernMarket`
(`ps_game.exe`, `00484010`) sends `061a` with variable length `7+4*count`.
Typed market rows decode after their SQL query, preserving original load order.

`MarketStore` reconstructs the last `SaveCharacter` phase
(`004247e2..00424a81`). States 1/3 first delete the stored list; state 1 then
adds IDs in order. Query errors log, stop that phase and remain nonfatal, so
a partial list can commit while the cache remains pending. Only complete SQL
success clears state, before the enclosing commit. SaveSuccess does not clear
it or restore it after rollback. Explicit rollback after successful market SQL
therefore leaves an unmarked cache whose list differs from the database.
A 1,024-state original-code fixture compares 4,920 planned calls, 3,888 attempts,
packets, error paths and complete SaveSuccess behavior.

`GrowthReplica`/`GrowthStore` reconstruct `DBCharacter::SaveMaxGrow`
(`004200c0`). If current grow exceeds cached maximum and is <=3, the cache
catches up. At equal grow <3, it advances once; grows 1/2 require level >=30.
The cache changes before DB allocation and is never restored on failure. Later
calls evaluate the changed maximum, so a failed 2-to-3 unlock is not retried
for that same grow. This write occurs before the character transaction on its
own DB allocation. Null user returns 1; unavailable allocation returns 3;
negative procedure return gives -1. API failures log but return 0, as does
success. A zero-row SQL update is accepted by the original procedure. Native
ports retain those distinct outcomes; the SQL store requires a connection
outside the character transaction. Its initial maximum is supplied by the
account roster, whose existing loader clamps it to at least 2.

An original-code growth fixture compares 1,660 sequential attempts in 830
cases, including threshold bounds, catch-up, null users, allocation/bind/query/
result failures, negative procedure returns and repeat attempts after mutation.
Disposable SQL composes market with core/items/skills/quests/interface and
verifies partial commit, retry, pre-commit marker timing, signed IDs and owner
isolation. It also verifies unlocks surviving character rollback and failed
unlocks retaining cache changes. Billing, file fallback, scheduling and the
complete runtime transaction owner remain pending; market gameplay and TCP
delivery are still separate work.


Inventory status messages `0617..0619` now route through the same inventory/bank
cache as item commands. `ps_dbAgent.exe`, `CUser::PacketUserSetStatus`
(`00405f4e..00405fea`) replaces the count for `0617` (bag/slot) or `0618`
(bank slot), changing save state 0 to 2 while preserving other states. Quantity
zero retains the item. `0619` journals a bank UID unless its state is 1, clears
the slot and returns the item to the shared pool. Empty slots return success
without mutation. Native envelopes validate owner, length and indices before
these writes. A separate 2,048-operation original-code fixture mixes the three
statuses with allocation, bank transfers and item updates, comparing both caches,
FIFO journals and pool reuse after every message. SQL verifies dirty counts,
zero-count retention, deletion rollback and retry. Character dispatch uses these
same caches; live gameplay dispatch remains pending.


`PointItemReplica` reconstructs product and stored-point receipts in
`ps_dbAgent.exe`: `LoadBillingItem` (`0041ead0`), stored-point loading in
`LoadCharacter` (`0042293a..00422a97`), `PacketUserItem` `0710`
(`00408f55..00409081`), and `PacketUserPoint` `0e05`
(`004070d5..00407254`). Both caches have 240 slots. Product loading fills slots
with zero count; stored loading fills null pointers, so a zero-count stored row
still owns its slot. IDs are divided unsigned by 1000 and truncated into BYTE
type/type-ID fields. Invalid slots are skipped; stored allocation failures skip
the row. Pooled payload not overwritten by loading is retained.

Receipts allocate a normal inventory item with state 0 and copy its SQL metadata
into the source cache, marking that source 3 unless it was 1. Product availability
count becomes zero; stored count becomes the received count. Occupied inventory
targets disconnect. Native envelopes additionally reject invalid owners, lengths,
indices and missing stored pointers before mutation. Full inventory gems and
craft data are retained in memory, but receipt metadata has no such fields.
`040c` emits products with nonzero count; `0e04` emits every stored pointer,
including count zero or pending receipts. Empty lists emit no packet.

`PointItemStore` reconstructs the receipt phases of `SaveCharacter`
(`00423483..0042366d`), after core/body and before inventory. It saves products
then stored points in slot order through the recovered procedures, with zero
gems and empty craft, signed UID/ID bits, unsigned quality/count and the original
packed timestamp. Failures and negative returns are fatal, with no item-insert
fallback. Both original procedures retain `BEGIN DISTRIBUTED TRANSACTION`;
outer rollback also restores source rows, product metadata logs and inventory.
`SaveSuccess` clears all product flags (`00409620..00409638`) and recycles only
state-3 stored pointers (`00409916..0040996f`), retaining other payload.

An original-code fixture compares 1,024 states, 2,912 planned calls, 2,350
attempts, 495 fatal boundaries and 64 complete acknowledgments. It executes
original loads, transfer handlers, CRT formatting, packets and SaveSuccess,
including duplicate/invalid slots, zero counts, allocation failure and pool reuse.
Disposable SQL redirects only the recovered billing linked-table dependency to
a cloned fixture table, preserving procedure transaction logic. It verifies
late-phase rollback/retry, product logs, receipt defaults, subsequent inventory
updates, commit acknowledgments, owner isolation and zero-source deletion
rollback. Original zero/quote MakeType SQL errors remain failures with bound
native parameters. Gifts, the complete transaction/retry/file owner, scheduling and live TCP
delivery remain pending.


`PurchaseReplica` reconstructs `ps_dbAgent.exe` point purchases, `0e02` in
`CUser::PacketUserPoint` (`00406cd9..00407075`). The envelope is `37+46*N`:
21-byte product code, DWORD amount/date, make byte and BYTE item count. The first
of ten history slots whose state is not 1 is reserved before item allocation.
Each item allocates a normal inventory object (state 0) and a frozen SQL copy
whose state packs bag/slot as two WORDs. Both use `strncpy(...,21)`, preserving
21 non-NUL bytes rather than the ordinary item's forced 20-byte termination.
Successful parsing enqueues the copies and requests DB job 18. Occupied targets,
allocation failures or full history preserve earlier inventory writes and the
reserved flag; temporary SQL copies are stranded. Failure returns false and
requests job 9 for an active game user or ExitUser otherwise. Native envelopes
reject invalid ownership, sizes and indices before any mutation.

`SendCharacterPointLog` (`0040ab50`) emits `0e01` even with empty history:
account, unchanged cached cash balance, count and 29-byte code/date/amount rows.
History loading (`0042280e..00422908`) restores rows then clears ten state flags.
Complete character SaveSuccess leaves purchase history and queued copies alone.

`execute_purchase_save` reconstructs complete `DBCharacter::SaveBuyPointItem`
(`0041ed10`). Each queued purchase acquires an item connection and begins a
transaction, writes frozen copies in order without binding procedure returns or
retrying inserts, then acquires a second connection for the point-charge/log
procedure. That procedure commits independently before the item commit. Item
begin/commit/rollback API errors log; a commit error still acknowledges and frees
the queued purchase, as in the original. FreeDB calls End, which attempts rollback
of an unfinished item transaction. Normal failures roll back items, free queued
copies and leave the history flag at 1; iteration continues and the final return
is false. HYT00 in either connection retains that entry, advances to later ones,
and does not itself make the legacy return false. Missing first connection stops
iteration with false. Missing second connection follows the ordinary error path.
Successful SQL does not clear the original connection's previous SQLSTATE, so
stale HYT00 classification also remains observable. Ports retain that state;
`PurchaseStore` supplies two dedicated SQL connections outside character saving.

A 1,536-state original-code fixture executes complete handlers, original C++
lists/pools, SaveBuyPointItem, SaveSuccess and the history sender, comparing 192
saves, 809 SQL attempts and 4,443 ordered DB/log events across twenty fault modes.
It covers partial allocations, duplicate targets, full history, frozen metadata,
stranded copies, SQL return/error distinctions, timeouts and stale SQLSTATE.
Disposable SQL preserves original purchase/account procedure logic while
redirecting the account procedure dependency into the scratch catalog. It proves
independent commits, later character rollback/retry, insufficient-balance rollback,
late item errors, mixed-queue continuation, signed DWORD costs and history reload.
The complete user/DB-job owner, gifts, save/retry orchestration, scheduling and
TCP purchase gameplay remain pending.


`capture_character_recovery` reconstructs complete `ps_dbAgent.exe`
`DBCharacter::SaveToFile` (`00424ce0..00425e6f`) and its max-grow helper
(`00425ee0`). The emergency path has distinct SQL and acknowledgment behavior.
It advances cached maximum grow again using the original eligibility rules,
then writes an older 29-argument core envelope. WORD stats/resources/map/direction
are zero-extended, kills are unsigned DWORDs, and kill/dead-level arguments are
absent. Stored bank money uses a separate procedure and its low DWORD. Body
changes clear their flag when captured.

Pending products produce the original 14-argument envelope, omitting six gems
and craft, then clear their state-3 flag. Stored-point receipts retain the full
SQL envelope and remain state 3 without being freed. The recovered live core and
product procedures reject those older file envelopes. Native output preserves
those differences; replay requires handling the recovered legacy signatures.

Inventory/bank deletion journals are consumed in FIFO order, with the first
40 UIDs batched and later UIDs emitted singly. Inventory inserts unconditionally
emit a global UID delete (ChkID=0) before the insert; bank inserts do not. Every
occupied item/bank slot clears its flag, including states that emit no statement.
Learned skill saved-count/reset markers advance. Applied skills are fully
replaced, including remaining-time zero entries that live SQL saving skips.
Quest deletion journals are consumed here, unlike the dead cleanup branch of
normal SaveSuccess; active flags become 2 and finished flags 0. Quick slots are
always emitted. Save-point flags remain pending. Market state, purchase history
and queued purchase copies are untouched.

`CharacterRecoveryFile` appends records under its file mutex, flushing each line
before proceeding and retaining original begin/end metadata and CRLF boundaries. `SLog::PrintFileText`
(`00432f30`) flushes each line and returns false for an unavailable file; the
original SaveToFile ignores that result and still returns 1 after changing its
cache markers. Native results expose both legacy success and actual persistence,
including an unavailable-file case. File recovery does not imply a SQL commit.

A 32-case fixture executes the complete original function twice per case,
comparing 2,172 statements and complete affected marker/journal state under
successful and failed log output. It covers signed-width differences, invalid
packed timestamps, item UID batches, duplicate/zero UIDs, resets, zero effects,
quest transitions, receipts, quick/save points and repeat captures. Filesystem
checks verify failed opens, repeated appends and concurrent noninterleaved
records. Disposable SQL confirms legacy envelope failures and replays compatible
statements with rollback/commit, including global UID replacement, consumed
quests, zero-time effects, truncated stored money, stored-point delivery and UI.
User/DB-job lifecycle, social persistence, connection pool/reconnect ownership
and live periodic dispatch still require integration.


`execute_character_save` reconstructs `ps_dbAgent.exe`
`DBCharacter::SaveCharacter` (`00423190..00424abe`) around the recovered SQL
phases. Maximum-grow saving precedes allocation and the character lock. Its
nonzero legacy result logs but does not stop saving. Missing allocation returns
false, with a locked file capture only when recovery is requested. BeginTrans
status is ignored. On fatal phase error the owner logs, records HYT00 only while
the shared timeout flag is still zero, rolls back, and conditionally captures
recovery before unlocking/freeing the connection. The first observed timeout
suppresses that attempt's file capture. A subsequent failure with an already-set
flag writes the file when recovery is requested, regardless of its current state.
Commit status is also ignored: original SaveSuccess still runs, then unlock/free.
FreeDB/End attempts rollback of an unfinished transaction.

`execute_user_character_save` reconstructs explicit `CUser::SaveCharacter`
(`00404f30`) and periodic `SaveCharacterTick` (`00405000`). Pending point purchases
save first. A purchase failure requests job 9 for an active game user, otherwise
ExitUser, then still proceeds with character saving. The first character attempt
has recovery disabled; the second attempt enables it and shares the timeout
flag. Explicit saves preserve the quick-slot argument. Periodic saves force it
false. A successful attempt invokes friend saving then block saving. Two failed
explicit attempts return false and request job 9 only for an active user with a
timeout; other explicit failures exit. Two failed periodic attempts with a timeout
return true without social saving or a new exit request. Other periodic failures
exit and return false.

`CharacterSaveStore` composes core/body, product/stored-point receipts,
inventory/bank, learned/applied skills, quests, quick/save points and market state
in their original order, then performs the full cache acknowledgment. Growth and
purchase charges use the separate auxiliary connection outside that transaction.
Point failures emit the additional original diagnostic before the general error;
quick/market failures retain their nonfatal behavior. A private legacy write
context lets phases execute when BeginTrans fails, matching the original ignored
status; ordinary phase callers still require an active transaction. The scope
cannot escape success or exception cleanup. The dedicated-connection adapter uses
the caller's recursive cache mutex and required recovery writer. SQLSTATE history
belongs to each SQL connection, surviving successful calls and shared use by
purchases, growth and character saving. User callbacks
for social saving and deferred exit/job dispatch are explicit integration ports.

A 5,120-case fixture executes original DB/user control branches with SQL/cache/recovery/
purchase/social bodies substituted as ports: 3,072
standalone and 2,048 explicit/periodic cases. It compares return values, sticky
timeout flags, attempt counts and complete owner effect order across null users,
allocation/growth/phase/transaction failures, stale SQLSTATE, purchases and exits.
Disposable SQL composes all phases, fails the first core update with a scratch
sequence/trigger to prove retry, and then exercises a permanent late save-point
failure, rollback and one file capture. Subsequent saves preserve the original
file-cleared item/skill/quest marker consequences, resume pending market/points,
and distinguish periodic versus explicit quick slots. Purchase failure still
queues exit before successful character/social saving. Shared-connection checks
retain a purchase after injected stale HYT00 plus a real negative charge return,
then discard it after a real later SQL error replaces that state. Live session/DB-job
lifecycle, social cache persistence, connection pool/reconnect ownership and
live periodic dispatch remain pending.

`execute_user_character_exit` reconstructs `ps_dbAgent.exe`
`CUser::SaveExitCharacter` (`004050b0`) and `SaveExitCharacterShutdown`
(`00405140`). Both ignore a pending purchase save's result, request quick slots,
retry once with file recovery, and save friends/blocks only after a successful
character attempt. Normal exit starts with timeout flag zero; two failed attempts
with that flag set enqueue job 9 and skip leave/exit. Shutdown starts with the flag
set and always proceeds to `DBCharacter::SaveCharLeave` (`004230b0`) and ExitUser,
even after two failures. Leave is a separate autocommit call; its return never
blocks exit. The recovered `usp_Save_Char_LeaveDate_R` contains a commented UPDATE
and therefore changes no date. `usp_Save_Char_Info_E` still sets LeaveDate during
the preceding core save. `CharacterSaveStore::save_exit` composes these paths.

`UserLifecycle` reconstructs `CUser::ExitUser` (`00402f70`), game disconnect
(`00402fe0`), selection logout (`00403010`), and save jobs 8/9/10/22 of
`OnDispatchDB` (`00403dd0`). Active game disconnect posts job 9; selection logout
posts job 10. Successful selection saving sends DB-agent `0303` with the account
UID, resets character state and reloads the roster with country. Exit sends `0302`
then, for status 2, `0303`, and removes the owner; absent connections suppress only
sends. These opcodes belong to the DB-agent link, not the public client protocol.
`UpdateDBSave` (`00403d90`, also inlined at `0041d252`) compares unsigned absolute
DWORD ticks, advances the per-user deadline by 900,000 ms, updates the shared last
deadline only if it is due, and posts job 8. It deliberately retains rollover
behavior. Successful `LoadCharacter` (`00404b51..00404b81`) staggers the first save
at shared last deadline plus `(save_time_id % 900) * 1000` and enters status 2.

Original execution fixtures cover 2,048 additional exit/shutdown paths, 5,024 save
ticks, 560 lifecycle/dispatch cases and 54 initial clocks. Native SQL integration
joins periodic, selection and disconnect jobs to actual composed saving, checking
exit packet account ownership/order, normal/shutdown failures, ignored purchases
and the standalone leave no-op. Saving, full ResetData, roster reload, registry
removal and transport are ports in the lifecycle instruction fixture; this does
not prove their live service ownership. Live DB-job ownership, manager sweep,
social persistence, connection-pool ownership and live world-session wiring still
require reconstruction/integration.

`post_db_job` / `dispatch_db_jobs` reconstruct `ps_dbAgent.exe`
`SDBDispatchClient::PostDispatchDB` (`00431a60`) and `DispatchDB` (`00431ac0`).
Detached clients do nothing. Uncounted mode pushes a FIFO status and posts one
completion; each dispatch pops at most one nonzero status. Synchronous mode
pushes before incrementing the DWORD job counter, waking only when the new count
is one. Dispatch snapshots that count as its batch limit, stops on a zero pop,
and subtracts completed calls only when both their count and the current counter
are signed-positive. A signed-positive *old* subtraction result posts another
completion, even if the new count is zero. Zero statuses are consumed without
calling the handler or reducing the job counter. Node allocation failure drops
the pushed status but retains subsequent counter/wake behavior. Reset (`00403c30`)
clears the FIFO, then exchanges the counter to zero.

The native `DbJobDispatcher` replaces IOCP with completion tokens, condition
variables and worker threads. It retains batch/wake/count rules and permits
concurrent dispatches for a client; callers serialize owned state separately.
Default worker selection follows `SDBDispatcher::Start` (`00431990`): four times
processor count, capped at 256. The original DB-agent enables synchronous mode
in `CServer::Start` (`0040c6e0`). Native shutdown closes admission, posts one null
token per worker and joins, matching original sentinel ordering (`004319c0`);
work requeued behind sentinels has no drain guarantee. Worker exceptions are
recorded at the native boundary; source callbacks normally do not throw.

A 2,304-sequence fixture executes 20,736 original post/dispatch/reset operations
across detached/sync modes, zero/status FIFO entries, allocation failures, DWORD
counter limits and injected producer/reset effects. A separate cooperative
original instruction schedule pauses a producer after Push and overlaps two
DispatchDB calls. Both statuses run, but one completion observes zero and skips
its subtraction; the delayed producer increment leaves count one with an empty
FIFO. Native control reproduces the same trace. Thus completion-idle does not
prove all counted work drained. Concurrent native checks verify status
conservation/at-most-once execution; uncounted mode also drains every posted
status. Single-worker checks cover ordered lifecycle jobs, zero-counter stalls,
reset, detached clients and shutdown admission. Disposable SQL now sends an
actual deferred disconnect through a native worker, composed character saving,
leave and ordered `0302`/`0303` notifications. Live server ownership, periodic
manager sweep, social persistence and connection-pool integration remain pending.

`request_character_selection` reconstructs `ps_game.exe` `CUser::PacketCharacter`
(`0046bf20`, `0104` at `0046c0f0`). An already-cleared selection latch ignores the
request. Country byte 2 closes with reason 9 without consuming an enabled latch.
Otherwise it clears the latch and forwards DB-agent `0407`, account UID and the
raw requested character DWORD; trailing request bytes are ignored. This gate does
not invent SQL ownership decisions or send a premature client success.

`complete_character_selection` reconstructs successful `PacketUserDBChar`
(`0046cf60`, `0407` branch `0046d4c1..0046d717`). It copies character ID to runtime
ID, notifies the manager with `0803`, initializes equipment/passives/effects/billing,
then performs signed HP/MP/SP upper clamps. Client `0104` success and world-day
`0404` precede attack stop, world wait membership and guild/friend initialization.
The original catalog-send order is preserved through stored point items. Automatic
characters receive `0112` (runtime ID and level) and finish with `0113` instead of
quick slots. A captured guild lock covers optional bank/NPC sends and the final
clock/restriction/PvP/run-mode/quick/flush section. The original `CGuildLock`
(`0042b800`) releases and discards a captured lock if the user's guild disappears
during acquisition. System time `0114` and raw `021b` flag transforms retain DWORD
and BYTE wrap. Motion byte 13 requests run-mode sending with argument zero. The
initial-data marker is set before flush, and a captured lock releases afterward.
This is preparation of a waiting user, not zone admission.

`ActorLoadPipeline` separates DB restoration from the four initialization phases,
allowing this owner to preserve manager/initialization/ACK order. It rejects
repeated/out-of-order phases and premature actor release. The existing `load_actor`
wrapper retains its previous behavior. A composed native check binds the four
phases and attack stop to the real actor, proving effect-driven MP notification
and current-resource clamping precede the selection ACK. Catalog/social/zone sends
remain explicit test ports there. The original control fixture covers 144 gate
cases and 864 complete 0407 sequences: normal/automatic, signed resource extremes,
raw flags, motion, no guild, captured guild and guild disappearance. Initialization,
catalog serialization, time and transport are ports; original clamps, envelopes,
CGuildLock and final branches execute unchanged. TCP session ownership, complete
catalog delivery and actual world/map admission remain pending.

`enter_zone` reconstructs `ps_game.exe` `CZone::EnterUser` (`00415f80..0041619e`).
Original safe-position and 40-unit cell rules reject invalid X/Z, including
nonfinite values and mismatched grid dimensions, before changing admission state.
Y is transmitted without a finite-value check. A map dungeon DWORD of exactly 1
clears vehicle mode 2. Map/cell/zone fields precede consecutive cell/user membership,
then the 29-byte `0201` grid-view packet: character ID, permission class (2 for
permission <=10, otherwise 1), direction WORD, raw XYZ, guild-user level DWORD and
vehicle partner only for mode 3. Nonzero GetShape triggers shape delivery; speed,
cell contents, weather mode zero, obelisks and EnterZone follow in that order.
Invalid coordinates log and close with reason 9 without an admission packet.
`enter_zone_actor` binds actual `ZoneUsers` membership, actor shape/speed and native
weather; cell contents, obelisks, transport and EnterZone lifecycle remain required
services. It uses the actor's authoritative vehicle state and initialized zone
resources. A 1,584-case instruction fixture verifies admission fields, full 0201
bytes, signed/raw metadata, coordinates, dungeon values and notification order.
Membership/scene bodies are ports; original map/cell arithmetic and packet code run.

`WorldAdmission` implements connected/selection/world-wait/zone-wait lists with the
shared cursor used by `SSyncList<CUser>` (`AddBack` `0040f170`, Remove `0040f200`,
RemoveFront `0040f260`). Removing the current user during packet processing moves
the cursor back, retaining the next user. `CWorld::WaitUser` (`0040ce70`) requires
location 1, clears logout type/tick, moves to location 2 and registers the world
user. Wrong location closes before logging. `UpdatePackets` (`0040bf90`) processes
connected and selecting users in list order, then consumes world-wait FIFO entries.
Zone selection precedes the user transition; success binds the zone, appends its
wait list and sets location 3. Failure logs, appends the selection list, sets
location 1 and closes. No extra location recheck is inserted after selection.

Client `CUser::ZoneEnterUserMove` (`0044b8a0`) first requires an assigned zone.
Party/rank/house maps (create types 3/5/6) clear a nonzero administrator instance
owner for permission <=10. Rank maps check their opening window, record its stamp
and increment the live guild's DWORD entry counter under a captured guild lock.
These effects precede the location-3 check, including on rejected requests.
A valid request removes the zone-wait node and invokes CZone::EnterUser; success
sets location 4, failure appends it again. World-directed entry (`0040d000`) instead
reselects the zone first and skips those client rank/override effects. Source
ownership requires the user to belong to that selected zone's queue; the native
adapter rejects invalid list ownership instead of dereferencing stale links.

A 1,024-sequence fixture executes 7,168 original wait/update/client/world operations,
including original list bodies and guild locking. A packet-body trampoline invokes
original WaitUser during iteration. Selection, full zone entry, rank clock, world
indexes/logging, OS and transport are explicit ports. Native composition joins a
loaded actor, waiting lists, failed-position requeue, successful location 4, actual
cell membership and view/shape/speed/weather delivery. It does not supply live TCP
ownership, entity snapshots, obelisks, world indexes/logging or EnterZone services;
those remain required integration/reconstruction work before a playable entry.

`SocialCatalog` reconstructs the game-side `CFriend` append/find behavior
(`ps_game.exe`, `004b97f0`, `004b9bb0`, `004b96f0`) and DB friend/block decoding
(`0046e770`, `0c01`/`0c05`). Each list retains insertion order and duplicate IDs,
caps at 100, and appends rather than replacing on another load packet. Original
Add returns false even after appending; the native append API has no success
return. `InitFriend` (`004756d0`) notifies every present nonzero friend with `2207`,
including one-way relationships. `SendCharacterFriend` (`00483ac0`) queries every
friend, including ID zero, and sets `2201` online only if the peer's friend list
contains this character. Friends and blocks (`2208`) send batches of 25, no empty
packets, fixed NUL-terminated name fields and variable memo bytes. The memo length
excludes the copied NUL; that NUL is outside the transmitted record. Live streaming
preserves lookup/send interleaving at each batch boundary. Wire bytes after a
name's first NUL are unspecified in the original and zeroed natively.

`EntryCatalogs` reconstructs game DB catalog updates and initial client output.
`PacketUserDBSavePoint` (`0046e810`, `0d01`) overwrites valid slots 0..3 without
clearing others. `SendSavePoint` (`00483d60`) checks DWORD map sentinel 65535,
then sends `0225` slots 1..4 with WORD map IDs and raw XYZ, omitting an empty set.
`PacketUserDBPoint` (`0046e880`) loads `0e01` cash/history and `0e04` fixed stored
items. Empty/partial history retains old trailing entries; `strncpy(21)` pads
codes after NUL and does not force a terminator for 21-byte codes. `SendPointLog`
(`00483ed0`) always sends `2601`, cash and nonempty-code rows, including an empty
history header. Stored updates preserve untouched slots and ignore >=240;
`SendCharacterStoredPointItem` (`00483f80`) sends `2606` in slot order, skipping
zero counts. `SendCharacterHonor` (`00483ea0`) sends CUser+`0x168` as `0230`;
this is the HG WORD loaded from DB `0501` offset `0x47` / DB-agent CUser+`0xa8`,
represented by `PlayerCore::hunger`. `SendKCStatus` (`00481920`) sends `0211`, raw
country/points and signed-positive wrapped expiry-minus-tick milliseconds, or zero.

`guild_listings` reconstructs `CGuildMgr::SendGuildList` (`00432000`): `0d0d`
begin, country-DWORD-filtered records in map key order, 14 fixed 120-byte rows per
`0d2f`, then `0d0e` end even for an empty result. Name/master/notice limits are
24/20/64 wire bytes; original fixed-string padding is normalized after NUL only.
The fixture executes 240 social decode/notification cases, 96 stateful four-round
catalog update sequences, 180 kill-count clocks, five honor values and 36 guild
lists. Social append/find, DB decoders, packet code and guild-tree iteration run
unchanged; world lookups, transport, clock, OS and CRT boundaries are ports.

`EntryDelivery` connects the restored actor, skills/effects/quests, social cache,
UI/history/points and existing serializers to the 0407 owner's actual delivery
steps, including InitFriend, speed and quick slots. Actor money is authoritative
for `0105`; unsupported lifecycle/guild steps remain explicit services. A native
composition verifies packet order and ownership. Disposable SQL now follows real
UI/history/stored-point replica packets into game caches and client packets, plus
ASCII friend/block rows into original layouts. This does not provide the complete
live TCP owner, global guild loader, social persistence or legacy wire text
encoding; those remain required for the original-client service goal.

The native `WorldServer` now creates one `WorldApplication` after authenticated
handoff, retaining its immutable account/session/connection identity. A worker
strand serializes factory/open/receive/close and destruction. Disconnect cancels
queued input and releases the registry immediately; application cleanup waits for
in-flight work. A work guard keeps graceful IO shutdown alive until cleanup ends.
The existing SQL roster/creation callbacks use this owner through an adapter.
`WorldChannel` provides weak asynchronous plaintext delivery, disconnect and
ordered outgoing AES/XOR transitions; input remains AES. Retained ports become
inert after close. Application exceptions, including cleanup failures, increment
an observable counter without escaping worker threads.

Ghidra identifies the player disconnect boundary as `ps_game.exe`
`CServer::OnDisconnect` (`00406430`), which calls `CWorld::LeaveUser`
(`0040ca20`). The new native ownership boundary is infrastructure for that
lifecycle, not a reconstruction of the complete game leave/save path.
`CServer::Start` (`00405a30`) calls `SAgentParam::SetParamSender` (`004d87a0`)
with `0xc0000` for accepted sockets; the native bounded framed-output queue uses
this 768 KiB default instead of its previous arbitrary 16 KiB cap. This records
the original configured threshold, without asserting identical internal buffer
accounting. Outgoing modes follow `CUserCrypto::Enc` (`00458080`). Native TCP
tests verify independent state, early commands, external delivery, mode ordering,
remote close/relogin/shutdown during blocked work, failure cleanup and catalog
bursts above 16 KiB. They use a synthetic application and do not enable playable
0104 selection. Global world ownership, account save/load exclusion on relogin,
full leave/save integration and live gameplay routing remain required.

`enter_zone_lifecycle` reconstructs `ps_game.exe` `CUser::EnterZone`
(`0044ae30`): clear motion/current-target-kind, sample the clock for both
movement anchors, set the wrapped five-second check deadline and zero accumulated
distance/speed. A separate clock sample feeds `ResetKeepTick` (`00486230`), which
updates active detail-61/81 previous ticks regardless of effect type. Map checks
and NProtect follow in that order. `enter_zone_actor` invokes this lifecycle
directly; `Actor` binds it to actual combat state, effects and equipped items.

`EffectCycle::check_map` reconstructs `ChkApplySkillByMap` (`00484680`) using
the existing map-18 restrictions: unlink each rejected instance, erase its DB
replica, send `050e`, reverse its effects and release its ID. Deferred party
notifications include offline effects and cap at 48; traversal uses the shared
list cursor. `send_nprotect_query` reconstructs
`CUser::NProtectCSASend` (`0044b700`): disabled/missing/not-ready sessions do
nothing; provider error logs and closes reason 111 without changing receive
state. Success clears readiness, samples a new clock, sets the wrapped 900000-ms
deadline and sends `a302` with four query DWORDs. CCSAuth2 query generation remains
an external required provider; its algorithm is not reconstructed.

A 320-case fixture executes the original entry/reset/map-check/NProtect bodies,
including real intrusive removal and pool release, raw XYZ bits, clock wrapping,
queries/errors and more than 48 restricted effects. OS/auth/unapply/party/transport
are ports. Native composition additionally reverses a periodic heal after its
keep-tick reset and runs the lifecycle on the actual loaded actor after zone
membership/weather delivery. This still leaves entity snapshots, obelisks, full
live world ownership and TCP gameplay entry unfinished.

`WorldUsers` reconstructs `ps_game.exe` `CWorld::EnterUserWorld` (`0040d450`)
and `LeaveUserWorld` (`0040d4d0`) with independent ID and name indexes. Both
insertions happen before the required EnterWorld callback; both erasures happen
before LeaveWorld. Duplicate keys preserve the first pointer independently in
each index, while callbacks still run. Erasure is by key, even if another user
owns that key. Native session orchestration must prevent invalid duplicate owners
rather than silently changing these source semantics. Actor/admission wrappers
must remain alive while indexed; calls belong to one serialized world executor.

`FindUser` (`0040d640`/`0040d690`) has no location filter. `FindUserZone`
(`0040d700`/`0040d760`) requires location 4; `FindUserZoneMove`
(`0040d7e0`/`0040d840`) additionally requires CUser+`0x57dc` (logout tick) zero,
not the logout type at `0x57d8`. Name keys stop at the first NUL and compare
unsigned bytes with case preserved, matching `stString::operator<` (`00412c50`).
Native code rejects names beyond the original 31-byte stString capacity rather
than reproducing a stack overflow. The runtime actor ID equals the indexed
character ID after the existing successful-selection initialization.

The 192-sequence fixture executes actual original insert/find/erase and balancing,
covering 4,608 entry/leave/state operations with duplicate IDs/names, case variants,
high bytes, embedded NUL, zero IDs and all location/logout filters. Allocation,
OS locks and CUser EnterWorld/LeaveWorld bodies are ports. Native composition
indexes the actual loaded actor during WaitUser and observes selection/wait/zone
transitions and pending logout before removal. Full EnterWorld/LeaveWorld game
services, guild/social ownership, logging and live TCP routing remain unfinished.

`enter_world`/`leave_world` reconstruct `ps_game.exe` `CUser::EnterWorld`
(`0044a820`) and `LeaveWorld` (`0044aad0`). Permission <=10 requires successful
administrator registration or closes reason 9 before guild-list/log delivery.
Entry sends the guild list, a 113-byte `0306` log and, for auto-character zero,
consumes offline money. An unsigned increase of at least 10000 emits `030d` and
manager `080e` severity 1. Signed CUser+`5814` <=3 emits `030f` and a new-user
notice. Required services preserve the source's gameplay/admin operations.

Leave orders normal/raid departure, guild release, friend release and dead-index
removal, followed by administrator end/not-answered/leave and vehicle cancellation
(mode 2 versus other nonzero modes). It sends a 108-byte `0307` log. Reconnect flag
exactly 1 skips offline-money storage and subsequent notices. Otherwise, storage
precedes the check for an unsigned increase of at least 1000000000 over initial
money (`030e`/severity 0), then signed CUser+`5814` <=3 emits `0310`/new-user notice.
Initial money is the final DWORD of `0307`, after its level WORD; bank money is
the source DWORD at CUser+`148`. Both diagnostic logs are 82 bytes.

The existing `SetGameLogMain` serializer writes real headers and samples local
calendar separately for each full log. New-user logs reuse the first header,
including its calendar, rather than resampling. Manager packets have original
259-byte framing, format literals and severity; bytes after their NUL are
undefined in the original and normalized to zero. `OfflineMoney` overwrites by
character ID and consumes an entry on take, following `GetOffCharMoney`
(`0040d8c0`) / `AddOffCharMoney` (`0040d920`). `SocialCatalog::notify_offline`
implements `ReleaseFriend` (`004757a0`): skip zero IDs, notify every present peer
with `2207` status 0, preserving duplicate/one-way friends and the cached list.

The 1,024-sequence fixture executes three original owners per case, real common
log-header code and all envelope/condition branches. Gameplay/admin, calendar,
offline-money hash storage, CRT formatting and transport are ports; only undefined
string tails are normalized. Native composition joins actual loaded-actor world
indexing, guild-list serialization, lifecycle logs, offline money and social exit
after index removal. Full party/guild/admin ownership, production log transport
and live TCP world entry remain unfinished.

`GuildMembership` reconstructs `ps_game.exe` `CGuild::ConnectUser` (`0042d1c0`)
and `DisconnectUser` (`0042d300`). An application row takes priority and only
sets the user's guild-request ID. Otherwise connection consumes the offline row,
copies guild/rank/ranking-entry state, binds its live user and fills the first
empty one of seven officer slots for signed rank <3. `0d0c` mode `0x68` broadcasts
before insertion into the online index. Disconnect consumes the online row,
clears only the first matching officer slot when the user's live rank <3,
retains the current ranking-entry stamp, clears the record's live user, inserts
offline and broadcasts mode `0x69`. It deliberately leaves the user's guild fields
for later character reset. Record-ID insertion remains independent of lookup key.

`CGuild::Send` (`0042c050`) allocates one shared packet even for no recipients;
allocation failure drops that broadcast without failing membership. It sends in
online-map key order, skips absent live users and stops on a null record. The
native transport exposes packet allocation/release and peer-delivery boundaries.
`SendGuildUserList` (`0042c980`) streams `0d12` online, `0d13` offline with zero
billing ID and `0d19` offline with nonzero billing ID. Records contain ID/rank/
level/job/name in 29 bytes. `SendGuildJoinList` (`0042ce10`) sends 28-byte `0d16`
application records without rank. Both batch 40, omit empty packets, preserve key
order, truncate numeric wire widths and normalize only undefined name padding.
The billing field remains part of normal guild views despite excluding gamewar.

`GuildDirectory` reconstructs first-insert character-to-guild registration
(`00432ab0`) and character-ID routing (`00431730`). InitGuild (`004762c0`)
clears request ID, guild ID, rank and guild pointer before connecting; successful
connection invokes SendCharacterGuild (`00476390`), sending member lists and,
for signed rank <3, applications. ReleaseGuild (`00476300`) disconnects the
captured guild. The native owner serializes these operations, so concurrent
guild destruction/lock-time disappearance must be resolved by the future global
owner rather than imitated with unowned raw pointers.

The 72-sequence fixture executes 648 original membership/list owners with real
tree insertion/removal, all seven officer slots, full officer arrays, packet-pool
failures, null rows and 39/40/41-record boundaries. OS/allocation/transport are
ports; original broadcast and roster iteration run. Actor composition now joins
selection InitGuild, world entry guild listings and world-leave ReleaseGuild,
preserving ranking state and officer cleanup. Global SQL guild loading, ownership,
applications/rank-management gameplay, bank/NPC delivery and live TCP integration
remain required before a playable service.

`GuildCatalog` reconstructs game `PacketGuild` (`00408e00`) for `0b01` maximum
ID, `0b02` 135-byte guild records and `0b03` 29-byte member records.
`InitAddGuild` (`00431870`) retains independent first-key ID/name indexes and
updates the maximum only after successful allocation; a later `0b01` overwrites
it directly. `InitAddUser` (`0042cf70`) corrects a member's stored rank to 1 when
its name equals the current master. The original input rank still determines
application rank 0 versus accepted membership, master-name replacement at rank 1,
and the rank-1/2 officer counter. Duplicate insertions still perform these side
effects. Member billing/ranking state starts zero and character-to-guild routing
uses first insertion. Unknown-guild member packets are ignored. A 96-sequence
fixture executes 768 original packet owners, real ID/member trees and master/rank
logic; guild/member pool allocation, reset, name-index insertion and OS are ports.

`GuildSql` connects actual original procedures to these game packets:
`DBGuild::LoadMaxID` (`ps_dbAgent.exe`, `00426a00`) uses
`usp_Read_MaxGuildID_R`; `LoadGuildNUserList` (`004271a0`) uses
`usp_Read_Guild2_R`, then `LoadUserList` (`004275d0`) calls
`usp_Read_GuildChar_R` on a separate connection. Each guild packet precedes its
member batches of 60, omitting empty sets. Member conversion failure discards
only the unfinished batch; the outer guild loader ignores that failure and
continues. Metadata conversion failure stops the outer loader. Original signed
LONG/SHORT and unsigned BYTE ranges and string-buffer truncation are retained.
SQL NULL leaves prior conversion storage unchanged; initially undefined storage
is zeroed natively. Wire text remains tested with ASCII; legacy encoding is pending.

`GuildMgrSave::AddFromDB` (`0040fc10`) keeps an existing replica unchanged.
Reloaded SQL guild name/master/country remain current while points, rank, etin,
remark, house flags and return count come from the retained replica. New replicas
load bank/NPC owners, in that order, before sending metadata. The combined loader never
writes record DWORD `0x7f` (KeepEtin), unlike the standalone guild-list loader;
the native packet normalizes these undefined bytes to zero while retaining the
actual SQL KeepEtin in the DB replica. Return count at `0x83` is defined.

The restored `usp_Read_Guild2_R` fails with SQL error 207: it orders by `G.Rank`
although restored `dbo.Guilds` has no Rank column. Integration records that real
failure without modifying original databases, then clones tables/procedures into
a disposable catalog with the missing column supplied only there. It verifies
SQL-to-game-to-client membership, missing-detail defaults, reload retention,
new-replica bank/NPC loading and conversion-failure batches. The fixture
does not settle the intended ordering field for a production compatibility fix.
Production bootstrap retry semantics, SQL cursor-error
streaming, the restored schema incompatibility and live TCP startup remain open.

`GuildNpcReplica` reconstructs the eight three-WORD DB slots written by
`DBGuild::LoadNpcLvList` (`ps_dbAgent.exe`, `00427f40`). The actual SQL owner
executes `usp_Read_GuildNpcLv_R`: signed SHORT type, unsigned BYTE level and
unsigned BYTE slot. Slots >=8 are ignored; valid rows overwrite only that slot,
zero its third WORD and leave untouched slots intact. Duplicate rows overwrite
in fetch order. Query/conversion failure retains successful previous rows and is
ignored by the enclosing new-guild load. Existing guild replicas skip NPC reload.
The old NPC callback is replaced by this native SQL owner.

`GuildSql::send_npcs` follows `GuildMgrSave::SendGuildNpcLvList` (`00410cb0`)
in unsigned guild-ID order. `GuildInfo::SendGuildNpcLv` (`0040f660`) emits `0b0e`
only for nonzero types, retaining slot order and the low byte of each level.
Game `PacketGuild` (`00408e00`, `0b0e`) resolves the guild and overwrites the
corresponding two-WORD slots without clearing absent entries. Unknown guilds are
ignored. Native game input rejects out-of-range slots instead of reproducing the
source's out-of-bounds writes. `CGuild::SendNpcLvList` (`ps_game.exe`, `0042f0e0`)
uses the same records in client `0d24`, omitting empty lists.

The 512-sequence fixture executes the original DB SQL owner, DB serialization,
game update and client serialization in both original binaries. ODBC, pool, OS,
guild lookup and transport are ports. It covers retained slots, duplicate rows,
NULL conversion buffers, partial/query failures and WORD-to-BYTE truncation.
Disposable SQL now loads the actual original NPC procedure and table through the
whole DB/game/client path, preserves cached levels after SQL changes and verifies
partial NPC conversion failure without aborting metadata/member loading. NPC
upgrade gameplay, native world spawning and production TCP entry remain required.


Guild banks now follow `DBGuild::LoadStoredItemList` (`ps_dbAgent.exe`,
`00427800`) through the original `usp_Read_Guild_StoredItems_Detail_R`. Its
240 slots retain directly assigned rows on failure; temporary duplicate rows are
deleted. Successful loading relocates duplicates into the first free slots and
returns overflow to the shared inventory replica pool. Reused save markers follow
the source's conditional zero-to-two update. `TrimString` (`0041e9f0`) removes
all ASCII spaces from craft text. SQL NULL retains conversion buffers, date parts
pack into the original DWORD, and existing guild replicas skip bank reload.

`GuildMgrSave::SendGuildBankList` (`00410c20`) visits guild IDs in order;
`GuildInfo::SendGuildBank` (`0040f7d0`) emits `0b0d`, up to 40 rows of 46 bytes.
Game `PacketGuild` (`ps_game.exe`, `00408e00`) resolves definitions, allocates
items, applies `CopyCraftExpassion` (`004ba670`) and assigns bank slots. Only
real equipment types 1..25 clamp excessive stack counts, emitting `0b14`.
Overwritten items remain owned by the shared item pool but are not recycled,
matching the source. Missing definitions invoke a fatal diagnostic port; the
source deliberately crashes, which native code does not reproduce.
`CGuild::SendBankItemList` (`0042f250`) emits `0d29`, 50 rows of 33 bytes.

The 96-sequence oracle executes both original binaries across full banks,
duplicates, pooled flags, allocation failures, partial/query errors and repeated
game reception. String padding after NUL is normalized to zero; it is undefined
source stack data. Native input rejects negative/out-of-range slot writes.
Disposable SQL verifies the actual procedure through game/client output,
NULL text reuse, duplicate relocation, date/quality conversion, count correction,
cached reload and nested failure retention. Character/guild transfers, production world initialization and playable TCP
entry remain pending.


DB bank mutation now follows `CConnection::PacketGuild` (`ps_dbAgent.exe`,
`0040ba00`, `0b11/0b14/0b15`) and `GuildInfo::AddDeleteItemBank` (`0040f550`).
Moving into an empty slot retains count; occupied moves write both supplied
counts, swap pointers, mark zero save states dirty and remove a zero-count source.
Self-swaps preserve the original final target-count write. Count updates retain
zero-count objects. Explicit deletion and swap removal journal UIDs unless save
state is 1, then recycle the object. Other nonzero save states remain unchanged.
The original oracle now also runs 1,536 mutations, comparing every changed slot,
deletion journal and pool count. SQL integration routes game equipment corrections
back into the DB replica and checks move/delete journal order and unknown-guild
no-op diagnostics. SQL saving follows these retained cache markers and journals.


`save_guild_bank` follows `GuildMgrSave::DBSaveItem` (`ps_dbAgent.exe`,
`004105b0`). Deletions precede slot-ordered inserts/updates. Its deletion counter
is never reset: the first 40 UIDs share a call, then each remaining UID gets its
own call. Failure retains the entire deletion journal, even after earlier calls
committed, and still attempts dirty items. Successful deletion clears the journal.
Each successful item write clears its marker immediately; unrecognized markers
also become zero without SQL. An item failure stops the slot scan. Only insert
Query failure appends a cleanup UID; bind, MoreResults and negative procedure
results do not. Lease failure returns one error without further work. Leases are
released after each pass; caller retry/scheduling remains a separate owner.

`GuildBankStore` executes the recovered Add/Mod/Del procedures with independent
autocommit operations. `GuildSql::save_bank` exposes one pass for a retained guild.
It preserves signed DWORD/UID formatting and unsigned quality arguments, including
real SMALLINT conversion failures for quality >=32768. A 144-case, two-pass oracle
executes the original save owner with lease/ODBC failures and compares SQL
arguments, marker changes, journal retention, prepare/release order and error
counts. Disposable SQL proves insertion survives a later failing update, subsequent
delete/update clears acknowledged state, and invalid packed-date insertion
schedules cleanup before retry. Production guild save scheduling and character/guild inventory transfers remain
pending.


NPC mutations follow `GuildMgrSave::UpdateNpcUpgrade` (`ps_dbAgent.exe`,
`00410010`) and `ResetNpcLevel` (`004100f0`). Upgrade fills an empty type and
marks it for insertion; an existing type remains unchanged. Level is a BYTE,
stored as WORD. Existing insertion markers remain 1; other occupied markers
become 2. Reset zeroes occupied levels and preserves pending inserts, leaving
empty slots untouched. Upgrade writes etin/keep-etin; reset writes keep-etin.
Both mark guild metadata dirty. DB `PacketGuild` (`0040ba00`) routes `0b0f/0b17`;
unknown guilds are silent no-ops, while native code rejects out-of-range slots.

`save_guild_npcs` follows `GuildMgrSave::DBSaveNpcLv` (`004103e0`): nonzero types
with marker 1/2 issue Add/Mod in slot order, acquiring a database lease only when
needed. Successful Query clears that marker. Failure retains it, increments the
error count and continues other slots. Procedure returns are not read. Other
markers and empty types remain untouched; SQL arguments retain full WORD values,
including SMALLINT/TINYINT conversion failures. `GuildNpcStore` executes recovered
procedures; `GuildSql::save_npcs` exposes one pass. The original caller's retry is reconstructed by the save coordinator; live
scheduling remains separate work.

The 256-sequence oracle executes original tree lookup, upgrade/reset, metadata
writes and 512 save passes, including lease failures, isolated and persistent
query errors. Disposable SQL verifies cached packet routing, retained existing
types, insertion markers, signed-type failures with later-slot success, reset
persistence and ignored negative procedure returns. Defaults and procedures are
copied only into the disposable test catalog. Production save scheduling and NPC upgrade gameplay remain pending.


`save_guilds` reconstructs `GuildMgrSave::DBSave` (`ps_dbAgent.exe`, `00410190`).
A primary database lease is acquired even for an empty registry. Guilds follow
unsigned ID order. Metadata marker 2 invokes `usp_Save_GuildInfo_E2` with signed
DWORD fields, remark, house flags, return count, keep-etin and rank. Successful
Query immediately clears that marker; its procedure return is not read.
`HYT00` retries up to three attempts for that guild, then aborts the pass. Other
errors retain dirty metadata and continue to children; native error 105 terminates
the remark at byte 64. Each bank/NPC owner is called in that order and retried once
when its first result is positive. Persistent child failures do not make the
outer result false. Live periodic scheduling remains separate.

`GuildSql::save` connects these owners through separate metadata/child SQL
connections with independent commits. `SqlError` retains the first ODBC diagnostic
state/native code, matching `SDatabase::GetErrorMsg` (`0043ea40`); formatted error
text still includes all records. Successful queries/transaction cleanup retain
connection diagnostics. The 160-sequence oracle executes the real original ID
tree and coordinator with verified child owners as ports, covering empty registries,
lease errors, two-successful-retry/three-failure timeout cases, HYT01, other SQL
failures, unsigned IDs, child ordering/retries and dirty-state retention.
Disposable SQL verifies metadata constraint failure still permits bank deletion,
NPC retries preserve errors, later metadata success uses original argument order,
and a negative metadata procedure return is ignored. The storage integration also
checks the native error code and its retention across successful operations.

Guild remark updates now follow `CConnection::PacketGuild` (`0b0a`, `0040ba00`)
and `GuildMgrSave::UpdateGuildRemark` (`0040fdf0`): force the final field byte to
NUL, copy at most 64 bytes through the first NUL and mark metadata dirty even
when the text is unchanged. Unknown guilds are ignored. The SQL owner now uses
the original formatted statement, including quote bytes, signed integer text and
trailing space. It does not reinterpret the remark as a bound SQL parameter.

A 128-case oracle executes original packet dispatch, lookup, forced termination,
copying and save formatting. SQL text matches byte for byte for embedded quotes,
doubled quotes, control characters, full fields and NUL tails. Disposable SQL
verifies ordinary single quotes preserve error 102/dirty state, trailing quotes
preserve error 105, doubled quotes are decoded by SQL Server, the recovered
procedure trims stored text without changing the cache, and valid text recovers
after syntax errors. Live save scheduling and client guild-management gameplay
remain pending.


Mob runtime preparation now follows `CMob::Init` (`ps_game.exe`, `00490040`),
`InitAttack` (`00490290`) and `SetAttack` (`004902f0`). Initialization binds the
respawn record, copies definition resources/stats, resets combat/control fields
and four multipliers, samples random then two clocks, and preserves wrapping
movement/targeting deadlines. Native path/trail owners reset; only the first byte
of each previous-attack text buffer clears. Position, identity, retained Lua
flags/timers and other unassigned fields survive reuse. Attack reset clears all
three attack/addition tables, current attack and attack clocks and restores speeds.

Attack calculations preserve source float spills, integer-indefinite conversion
and DWORD wrapping. Melee evasion uses half the definition level; ranged evasion
uses the full level and ranged defense includes `dwDefenseAddSH`. Magic attack
and critical chance use live wisdom/luck. The runtime definition projection
follows `LoadConstMob` (`004c1720`), including zero HP ->1, invalid attribute ->0,
NULL-initialized fields and the copy of `Attacktime3` into shield defense.
It does not replace complete mob-skill/loot loading.

AI types 10..99 call the required compiled-AI provider; success sets status 100
and PAIS. Types >=100 call the required Lua initializer after lock/unbeatable
reset but before the final vision-user reset. Provider side effects retain their
original order. The factory/Lua implementations remain separate required work.

The 384-case fixture executes the full original SQL mob loader and all three
runtime methods, comparing 1,152 state/attack snapshots, retained bytes,
AI/random/clock calls, overflow/NaN behavior and respawn/zone bindings. SQL,
allocation, verified path reset implementations, AI providers and OS are ports.
Native tests also clear actual allocated path/trail state. SQL integration loads
the recovered distribution and verifies the first mob's runtime projection.
Production population/pool integration, linked bosses, obelisks and the complete
world application are still required before production TCP world entry.


Mob zone/cell ownership follows `CZone::AddMob` (`ps_game.exe`, `00416700`),
`RemoveMob` (`00416790`), `FindMob` (`00416800`), `EnterMob` (`00416860`),
`LeaveMob` (`00416990`) and `CCell::FindMob` (`00423680`). Global and cell lists
retain FIFO ordering independently of the first-key ID index. Removing a duplicate
ID erases the index key even while another object with that ID remains linked.
Entry copies respawn position, binds both lists/indexes, sends `0601` appearance 1,
then invokes the required Lua `Init` callback when enabled. Leave unlinks silently
without recycling. Unsafe negative/out-of-grid cell indexes are rejected natively.

Cell snapshots follow `CCell::SendCellInfoEnter` (`00423aa0`) and
`SendCellInfoLeave` (`00424130`). Entry sends `0601` appearance 0 and skips effective
FSM status 2; an active empty stack yields status 0. Leave sends every `0602`,
including dead mobs. Shared cell cursors preserve reentrant lookup effects.
Packets contain X/Z, without Y. The 128-registry/2,048-operation fixture executes
original tree insertion/erasure/lookup, intrusive lists and whole-cell snapshots;
only allocation, locks, Lua and transport are ports.

Movement follows `CZone::MoveMob` (`004169d0`), `SendMoveMob` (`0041f860`) and
`PSendView` (`00421120`). Transition sends observe the old public cell/position
after membership moves to the destination cell. The original view difference
requires exclusion on both axes, so purely horizontal transitions announce no
appearance/removal cells. Position/direction and the sampled clock update before
prioritized `0603` delivery. Float spills differ between normalization and the
separate planar range calculation. The 1,536 independent movements and 144 steps
across 12 sustained paths compare byte-for-byte deliveries, binding order,
allocation failures, detached zone pointers, boundaries and NaN behavior against
original execution. The source cell operations, geometry and math execute; the
zone ID index is a port in this movement fixture and verified separately above.
The registry is owned by `RuntimeZone`; actual population/pool/AI initialization
and production world transport remain required integration work.


The mob pool follows `CObjectMgr::CreateMob` (`ps_game.exe`, `0040a5e0`),
`FreeMob` (`0040a630`) and `SSyncHeap<CMob>::Alloc` (`0040ab80`). Free objects
are reused LIFO without runtime reset. Every successful allocation, including
reuse, increments the shared mob counter with DWORD wrap, overwrites the ID,
then seeds drop randomness from Unix time XOR that ID. Failed fresh allocation
increments the construction-attempt counter without consuming an ID or time.
Release invokes the required compiled-FSM cleanup, active stack-FSM cleanup,
conditional Lua flag reset and Lua close, then makes the object reusable. Stack
release disables the stack FSM. Position, status, target and other unassigned
fields survive. Actual list membership must be removed before recycling; bulk
zone release retains the raw zone/cell values. Actual AI/state
handlers, Lua ownership/initialization and final world teardown remain required
services; this pool does not substitute no-op gameplay providers.

`ServerRandom` follows `SRandom` construction (`004a8840`), `Random` (`004a8860`)
and `SetRandomSeed` (`004a8980`): lazy first seeding, the original 69069 recurrence,
624-word twist/tempering and unsigned remainder. Bound zero returns without
advancing or seeding. The source uses an earlier seeding algorithm than
`std::mt19937`. The random fixture compares 166,400 original draws and all final
state words across 128 streams, including reseeding and multiple twist cycles.
The pool fixture executes the real original constructor, heap/list operations,
empty compiled/stack FSM cleanup and random seeding across 128 pools and 1,920
operations. Allocator failure, locking, clock, atomic increment and Lua close
are ports. Native providers supply constructor/AI resources; tests verify retained
runtime state, cleanup order, failed allocations, wrap, new/reused IDs and seeded
random words. Live world ownership remains pending.


Regular zone populations follow `CZone::MapInitObject` (`ps_game.exe`, `0041c5f0`).
`MobPopulation` owns per-zone respawn copies, binding map types to valid SQL mob
projections as in `CMap::LoadMap` (`00425780`), then skipping empty areas as
`stMapMob::Copy` (`0041c280`) does. Missing definitions preserve their existing
current/deadline fields. Valid definitions set deadline zero and current to the
configured count before placement; blocked/out-of-range tiles and failed
allocations do not decrement it. Each attempt consumes X then Z random values;
successful initialization consumes its separate movement delay and clocks before
position assignment and zone entry. Position uses float spills, truncated tile
coordinates and the area's minimum Y, including subzero fractional X/Z that
truncate to tile zero.

The native path connects actual `MobPool`, `initialize_mob`, drop randomness,
`ZoneMobs` and entry packets. Required linked-group initialization follows every
regular pass; its return value is ignored, as in the source. Linked-group
ticks and obelisks remain required work. `RuntimeZone` owns the population
so active respawn pointers remain valid. The 128-population/256-call fixture
executes original regular spawning, real construction/pool allocation, runtime and
attack initialization, random seeding, ID tree/cell insertion and entry formatting.
AI/Lua providers, linked groups, allocation failure, clocks/random and transport
are ports. It compares interleaved events, configured/current/deadline state,
retained memberships and bindings across two calls per population, including
memory failure, negative/zero counts, unknown definitions, blocked tiles,
boundaries/NaN and wrapped IDs/ticks. Isolated SQL also drives a restored mob
through preparation, allocation, runtime initialization, cell entry and recycling,
verifying original resources, respawn binding and deferred linked initialization.
Production scheduling, AI and TCP world entry remain pending.


Bulk population release now follows `CZone::MapReleaseObject` (`ps_game.exe`,
`0041c3b0`). It clears every mob cell list and the complete ID index first, then
removes global-list members FIFO before invoking compiled/stack FSM and Lua
cleanup and returning each allocation to the pool. Cleanup observers see the
remaining global members while both lookup/cell indexes are already empty.
It sends no departure packets. Raw zone/cell, position, respawn binding, status,
target and unassigned state survive recycling. Native ownership bookkeeping now
tracks actual membership separately from these retained public fields, allowing
original cross-zone reuse without clearing information that AI initialization
may observe. Active duplicate membership and duplicate pool release remain
invalid. Cleanup callbacks may inspect registries but must not mutate membership.

After all mobs are recycled, the source zeroes all ten `stInsZonePortal` records
(`CZone+0218`, 80 bytes). `RuntimeZone::release_mob_population` resets the native
portal ID/country pairs at the same point. Other zone entities/environment and
population definitions remain intact. The 128 two-zone/3,072-operation fixture
executes original allocation, construction, index/cell insertion, full bulk
release, empty FSM cleanup and portal zeroing, then reuses objects across zones.
Heap/locks/time/atomic increment, Lua close and transport are ports. It compares
cleanup-phase observations, duplicate-ID indexes, FIFO release/LIFO reuse,
retained raw bindings, Lua flag/handle ordering, silent empty/repeated release,
wrapped IDs and the complete portal arrays. SQL integration now releases and
recreates the restored mob population using the same allocations and then checks
ordinary per-mob leave/recycle. Complete world teardown and live AI resources
remain part of production integration.


Instance portal ownership follows `CZone::GetInsZonePortalCountry` (`ps_game.exe`,
`004179e0`) and `UpdateInsZonePortalCountry` (`00417a10`). Lookup scans all ten
slots, returns the first matching signed ID (including zero) and defaults to 3.
Updates require ID >1000 and a nonzero map CreateType; they replace the first
matching or empty slot, stopping at an earlier empty slot even when a duplicate
exists later. A full table drops unknown IDs. Arbitrary country DWORDs survive
without normalization. The 8,192 original update/query fixture compares all
80 table bytes with holes, duplicate IDs, signed boundaries and full tables.

Linked-group initialization follows `CZone::MapInitBothMob` (`0041b470`) and
`stMapBothMob::Reset/AddMob/RemoveMob` (`0041b3d0`, `0041b3f0`, `0041d730`). The
native population copies prepared group definitions and repairs respawn self
pointers as `MapInit` (`0041c8a0`) does. Every group resets its current stage to 0;
a missing first leader definition preserves the remaining group state. Immediate
leaders set current=1/deadline=0, allocate and initialize before position/member
assignment and entry. Allocation failure skips all companions. Leader placement
does not check tiles; a failed entry still permits companion processing.
Companions use the first definition, set configured current counts, clear only
the active member prefix, consume separate X/Z random draws, and skip blocked or
out-of-map tiles without decrementing counts. Float spills and radius subtraction
retain the source precision. Member addition permits duplicates and removal
clears only the first matching pointer; inactive tail slots survive reset.

Randomized leaders defer all spawning, retaining member slots and scheduling
current=0/deadline from the tick. The minimum delay is minutes*60000; the original
maximum field is an additional minutes*60 random window, sampled in seconds and
multiplied by 1000. The temporary `SRandom` samples Unix time even with no groups;
window sampling reseeds it with time XOR definition ID, independently of movement
and drop randomness. DWORD timing wraps. Group transitions,
death/respawn ticks and portal propagation remain required integration work.

The 128-group-set fixture executes 256 original initialization passes through
actual allocation, construction, attack/runtime initialization, SRandom timing,
cell/ID insertion and entry formatting, followed by 1,280 original member helper
operations. AI/Lua, allocation failure, clock/random and transport are ports;
linked initialization itself executes. It compares event order, timer/current
state, all member slots, respawn bindings and allocated/entered objects, including
missing definitions, partial failure, wrap, radius precision and guarded
nonfinite companion positions. Leader positions avoid the original unchecked
negative cell-index domain; native cell memory guards still apply. SQL integration
also constructs immediate leader/companion and deferred groups from the restored
mob definition, checks reused allocations/resources/placement and bulk release.
`RuntimeZone` owns these records, while live AI and world scheduling remain
pending.


Linked-group configuration now follows the linked portion of the complete
`CMap::LoadObelisk` parser (`ps_game.exe`, `00427300`). `LinkedMobConfiguration`
reads binary `Obelisk.ini`, preserving section context, case-insensitive labels,
Windows text-mode CRLF/Ctrl-Z and the 8,191-byte fgets limit. Incomplete headers,
companions and random-center tuples retain their partial writes to the same
uncommitted slot. Only complete records advance counts. Repeated map sections
append to their existing group bucket; unknown sections preserve parser context.
Leader change counts cap at 15, companion counts at 16, and further companion
records are ignored after 32. Integer parsing wraps like MSVCRT atoi; real
parsing preserves its decimal D/d exponent support and maps hex/inf/nan to zero.

Prepared groups retain input order, convert group respawn seconds to wrapped
milliseconds and resolve the first matching destination portal only for IDs
>1000. Leader candidates use change-count IDs; companions use one additional
candidate. Invalid SQL mob definitions clear both the definition pointer and
stored type ID, retaining their array positions. Leader spawn count remains zero
while its respawn count is 1, type is the group index and the linked flag is set.
Companions keep the source's separate flags/respawn fields and template self
pointers; zone copies repair these addresses before initialization. Negative
map references and capacity-overflow writes retain native memory guards.
Obelisk and portal-link sections are not materialized by this owner.

The 44-case fixture executes the complete original parser, CRT numeric/token
conversion, copy/binding and constructors. File I/O, zeroed allocation, TLS/CRT
locks, cookie and exact temporary-array zeroing are ports. Canonical bytes cover
all active group fields, candidate IDs/pointers, respawn state, random centers
and portal/self bindings, including malformed/repeated input, overflow, comments,
section changes, buffer boundaries and partial writes. The full distribution also
matches its canonical length/fingerprint: 25 groups across maps 14, 17, 19, 20,
48, 49, 50, 64 and 67. The integration runner now checks this asset fixture.
SQL integration loads the actual 68 configured maps and recovered mob catalog,
resolves the distribution, and drives a parsed immediate/deferred configuration
through allocation, entry and recycling. `--check-original-loaders` also loads
this configuration and reports its group count; it does not start object runtime.
Linked transitions and replenishment are covered below. Death accounting,
obelisks, live AI and complete world integration remain pending.


Mob runtime definitions now include the source respawn delay from
`CGameData::LoadConstMob` (`ps_game.exe`, `004c1720`). `Attackspecial3` (column 68)
is a BYTE selector, with NULL ->0 and values >15 normalized to 0. The 16 delays
in milliseconds are 5000, 30000, 60000, 180000, 300000, 600000, 900000, 1800000,
3600000, 14400000, 43200000, 86400000, 259200000, 7200000, 0 and 604800000.
The complete-loader/runtime fixture now compares this 14th definition field,
covering every selector and invalid/NULL normalization. The source switch's
15000 default is unreachable after its BYTE clamp. The restored SQL mob 1 uses
selector 3 and therefore a 180000-ms delay.

Regular periodic replenishment follows `CZone::UpdateMobRespawn` (`0041d240`).
Each eligible record attempts one placement per call when its signed current
count is below configured count and its unsigned deadline is <= the supplied
tick. Definition/position/collision/allocation failure preserves both fields,
allowing a later retry. Success initializes the native mob and position, then
sets deadline=tick+the live definition's respawn delay and increments current
before entry/broadcast. DWORD deadlines wrap and retain the source's ordinary
unsigned comparison, rather than substituting a different elapsed-time rule.
`RuntimeZone::update_mob_population` invokes regular records first, required
obelisk updates for CreateType=0, then required linked-group updates using the
same supplied tick. These callbacks still need their actual runtime owners.

The 256-population/2,304-tick fixture executes the complete original updater,
real allocation/constructor/runtime initialization, seeding, geometry, zone/cell
insertion and entry formatting. Allocation failure, OS clocks/random, AI/Lua,
obelisk/linked update boundaries and transport are ports. It compares sustained
count/deadline state, caller tick versus initialization clocks, callback and
packet order, missing definitions, full/negative counts, collision/NaN/boundary
failure and ID/timer wrap. Entry observers verify mutations occur before packet
delivery. SQL integration replenishes a record using the restored 180000-ms
interval, checks pre-deadline rejection and due-time admission, callback order,
reused allocation and release. Death-triggered accounting, obelisk ticking, world scheduling and gameplay
dispatch remain pending.


Linked population transitions follow `CZone::BothMobDestroy` (`ps_game.exe`,
`0041bad0`), `BothMobRespawn` (`0041be50`) and `UpdateBothMobRespawn` (`0041b870`).
Destroy clears the principal slot, removes companion prefixes in order and
advances the phase. Completing the final phase resets it to zero, writes portal
country 0 and spawns companions using candidate slot change_count. Respawn at
phase zero writes country 3 and removes companions before checking the leader
candidate. It sets current=1/deadline=0 before allocation, preserving these
values on allocation failure. Random center selection occurs after initialization.
Companion bursts preserve old member slots, unlike initial map population.

The updater gates each group on its current principal candidate. A missing
principal gets a due respawn attempt and skips companion replenishment that
call. Otherwise each due, underfilled companion record gets one placement
attempt, with successful count/deadline changes before entry and the live
candidate's respawn interval. Required world providers perform removal and
write the shared portal before its instance table; principal death accounting
remains caller-owned.

The 128-scenario fixture executes 2,048 sustained operations against the three
original methods, actual allocation/constructor/initialization, zone insertion
and instance portal updater. It compares phase changes, every member slot,
counts/deadlines, all ten instance portal entries, callbacks, packets and actor
state across 0/1/2/15 phase limits, missing candidates, allocation/placement
failure and timer/ID wrap. OS/AI/transport boundaries remain ports. ID removal is
an observer-only port in this comparison: it does not establish the original
MobRemoveByID implementation. SQL integration separately composes actual
zone removal/recycling with restored definitions and parsed configuration,
terminal companion replacement, portal state and scheduled principal respawn.
Death-triggered accounting, full removal, obelisk ticking and the production
world owner remain necessary before these controllers run as gameplay.


Removal dependency audit (`ps_game.exe`, Ghidra decompilation checked against
instructions): `CZone::MobRemoveByID` (`0041e910`) first finds the ID; a miss
returns false without other work. It removes the global index/list through
`RemoveMob` (`00416790`), then the cell list (`00424ac0`). It clears the raw zone
pointer before sending 0602 to the old cell, retaining old cell coordinates
until after delivery. Consequently composing the existing silent `leave` with a
later packet send would expose different state during delivery.

For a bound respawn, forced removal sets next=GetTickCount()+live definition
delay only when current equals configured count, then decrements current with
DWORD wrapping. Dispatch precedence is obelisk flag, obelisk member source,
linked principal flag, linked member source. Principal dispatch calls
BothMobDestroy after that decrement; member removal clears the first matching
prefix slot (`0041d700` / `0041d730`). Recycling (`0040a630`) is last. Natural
corpse cleanup (`UpdateMobDeath`, `00421a80`) has a different timer path, including
strict death-time comparison and long-delay jitter; it cannot reuse forced
removal accounting unchanged. These findings define the next native removal
implementation and differential fixture; they do not constitute implemented
world death handling.


`ZoneMobs::remove_by_id` now implements the forced-removal coordinator with
required `MobRemovalOwners` dispatch and actual zone index/cell unlinking and
pool recycling. During 0602 delivery the actor has no zone/membership but still
retains its old cell; accounting and owner callbacks follow delivery. The native
ordering/precedence tests also cover missing IDs and unbound respawns. A separate
360-case fixture executes the original accounting instruction block and compares
native count/deadline/clock calls, including signed limits and DWORD wrap. This
fixture establishes accounting only, not complete original-method equivalence.
SQL integration now uses this removal path for companion replacement with
parsed linked configuration and restored definitions, including tolerated stale
IDs after bulk recycling and four actual disappearance sends.
An additional 24-case fixture executes the complete original MobRemoveByID
coordinator with FindMob, global/cell unlink, SendView, owner callbacks, FreeMob
and clock ported. It compares return values, observable send/clock/owner/recycle
order and final accounting for missing IDs, unbound respawns, every dispatch
branch, full/underfilled records and deadline wrap. Packet bytes and raw binding
state are checked at source delivery; native observers check actual registry
unlinking and retained cell coordinates. Source container/callback bodies are
not exercised by this fixture. Production obelisk/group owners and natural
death/corpse processing remain pending.


Regular/member corpse accounting from `CZone::UpdateMobDeath` (`ps_game.exe`,
`00421d02` through `00421d62`) now has a native implementation. Only a full
record receives a new deadline; all regular/member records decrement current.
Zero delay schedules exactly now without randomness. Nonzero delays use the
normalized Attackspecial3 selector retained by the definition loader: selectors
>=9 add `(rand()%120-60)*60000` with DWORD wrap; lower selectors use the fixed
delay. Linked principals and obelisks require separate accounting paths and are
rejected by this helper. The 540-case original-instruction fixture covers full
and underfilled records, signed count boundaries, zero/fixed/long delays,
selector thresholds, random values and deadline wrap, including clock/random
consumption. It does not execute traversal, corpse eligibility or unlinking.
SQL integration also checks the restored mob 1 selector 3 and fixed 180000-ms
delay without consuming random. The 384-scenario full original loader fixture
also compares the retained normalized selector directly, alongside its computed
duration and the existing 1,152 initialization/attack snapshots. Zone corpse
traversal remains pending.


Linked principal corpse timers follow `UpdateMobDeath` (`ps_game.exe`,
`00421c16` through `00421d62`). A deadline changes only when strictly below the
supplied tick or equal to FFFFFFFF; equality otherwise preserves it. The default
is now+live definition delay. Phase zero replaces this with the group's delay
plus MS rand minute jitter, or the randomized leader's minimum minutes plus a
window sampled from the dying mob's own SRandom. No count decrement occurs in
this branch. Invalid group indices retain native bounds guards.

The 256-case fixture executes these original instructions and actual SRandom
seed/draw with only MS rand ported. It compares deadlines, unchanged counts,
rand call counts and SRandom index/first word across phases, fixed/randomized
leaders, zero windows, wrapped minima and deadlines. SQL integration drives
the same helper with a restored and initialized principal and parsed group.
This does not yet implement zone corpse traversal or principal death handling.


Obelisk corpse deadlines follow `UpdateMobDeath` (`ps_game.exe`, `00421bdd`
through `00421d62`): preserve next when >=now except FFFFFFFF; otherwise use
now+live definition delay, with DWORD wrap and no count change or random draw.
The 60-case instruction fixture uses no dependency ports and covers equality,
sentinel, unsigned limits and zero/nonzero durations. It excludes traversal,
status/death-time eligibility, unlinking and object recycling. The native helper
requires the obelisk flag and does not substitute obelisk death/ownership logic.
