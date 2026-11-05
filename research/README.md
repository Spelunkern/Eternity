# Baseline

The baseline used Ghidra 12.1.4 with the exact EXE/PDB pairs; later exports may use
another version, and addresses come from the same binaries. Reproduce with
`tools/reverse.py`; use `--process` to export an already imported program.
`tools/database.py export` extracts the restored SQL catalog, and
`tools/contracts.py` compares it with SQL call candidates found by Ghidra.
Generated exports are local; `baseline.json` records the initial findings.

The candidate Spanish client `shaiya-es-ps0085/game.exe` has SHA-256
`2c3513584747d1e6bf0260b4b8f0606dade01f61918d3f27953c849ab5df6fa9`.
Its patch number is 85. Ghidra identifies four functions in the protected static
image; EP4.5 compatibility is not confirmed. Client work is deferred; assets stay local.
