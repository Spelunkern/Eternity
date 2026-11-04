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
