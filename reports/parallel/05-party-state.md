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

Block 2 evidence commit: recorded in the following report update.
