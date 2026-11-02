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
(`00423190`), which deletes all saved effects and re-adds the ordered replica
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
