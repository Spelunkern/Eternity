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
This status update does not itself write SQL. Full character saving through
`DBCharacter::SaveCharacter` (`00423190`) remains pending, as do party loot
distribution and live pickup command dispatch; detached inventory admission is
described below.
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
