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
regenerates 84 committed regression vectors; regular CTests need no emulator.
Equipment, blessing, passives, effects, damage resolution and world integration
are separate pending stages. Ghidra's `EternityOffsets.java` gives anonymous
PDB padding fields unique offset names without inferring their meaning.
