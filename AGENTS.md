# Eternity

- Completion means an original EP4 client can connect and play with original server behavior.
- Client work is deferred at the user's request. Continue server reconstruction and protocol tests independently.
- Reconstruct native Linux code in C++20. Use Ghidra for reverse engineering.
- Keep SQL Server and the recovered stored-procedure contracts for now. SQLite migration is deferred.
- Exclude `ps_gamewar`. Preserve regular gameplay unless evidence shows it belongs exclusively to that service.
- Keep original binaries unchanged. Cite executable, symbol and address for recovered behavior.
- Generated Ghidra projects, database exports, backups, credentials and runtime data stay under ignored local directories.
- Use only Eternity's isolated database container. Never reset existing databases implicitly.
- Documentation and code are in English; keep documentation brief and current.
- Do not describe protocol stubs or a successful database connection as a playable server.
- Validate changes with the native build, relevant unit tests and isolated SQL integration tests.
- After integrating work, regenerate `PROGRESS.md`, the README summary and `research/coverage/` with `python3 tools/coverage.py`. Citations are not proof of equivalence.
