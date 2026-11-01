# Eternity

Reverse engineering of the Shaiya EP4.5 server, working toward native Linux executables.

`originals/` contains unmodified Windows executables and matching PDB symbols for
`ps_game`, `ps_dbAgent`, `ps_login`, `ps_session`, `ps_gameLog`, and `ps_userLog`.
All pairs have matching CodeView GUID and age. `ps_gamewar` is out of scope.

Source: the EP4.5 `PSM_Server/ShaiyaServerPatch/Server` distribution, including
its matching `ps_login.exe`. Database backups and deployment configuration are
not included. No Linux implementation exists yet.

Verify original files with `sha256sum -c SHA256SUMS`.
