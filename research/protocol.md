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
tested with a generated protocol peer and SQL Server. World selection returns
the original offline result until the world endpoint is implemented. Actual
client work is deferred.

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
sends `TP_GAME_CONNECT` (`A301`, 20 bytes: result, encryption mode and 16-byte game
key). `CUserCrypto::EnterCharSelect` / `LeaveCharSelect` (`00458060` / `00458070`)
switch outgoing traffic between AES and a separate world XOR transform; incoming
traffic retains AES. The XOR table initialization still needs recovery.
`ps_session.exe` `AddUserSessionLost` (`0040a2f0`) gives a handoff 30 seconds.
