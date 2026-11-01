#!/usr/bin/env python3
"""Import exact original EXE/PDB pairs into a local Ghidra project and export analysis."""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SERVICES = ("ps_session", "ps_login", "ps_dbAgent", "ps_game", "ps_gameLog", "ps_userLog")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("services", nargs="+", choices=(*SERVICES, "all"))
    parser.add_argument("--ghidra", type=Path, default=os.environ.get("GHIDRA_HOME"))
    parser.add_argument("--filter", default="", help="Optional Java regex selecting functions to decompile")
    parser.add_argument("--process", action="store_true", help="Re-export an already imported program")
    args = parser.parse_args()
    if not args.ghidra:
        parser.error("Set GHIDRA_HOME or pass --ghidra")
    launcher = args.ghidra / "support" / "analyzeHeadless"
    if not launcher.is_file():
        parser.error("Invalid Ghidra installation")
    project = ROOT / "work" / "ghidra"
    project.mkdir(parents=True, exist_ok=True)
    output = ROOT / "work" / "reverse"
    names = SERVICES if "all" in args.services else args.services
    for name in names:
        original = ROOT / "originals" / name
        command = [str(launcher), str(project), "Eternity", "-scriptPath", str(ROOT / "tools" / "ghidra"),
                   "-max-cpu", "4", "-analysisTimeoutPerFile", "600"]
        if args.process:
            command += ["-process", name + ".exe", "-noanalysis"]
        else:
            command += ["-import", str(original / (name + ".exe")), "-preScript", "EternityPrepare.java",
                        str(original / (name + ".pdb"))]
        command += ["-postScript", "EternityExport.java", str(output), args.filter]
        log = project / (name + ".log")
        print(f"Ghidra: {name} (log: {log})", flush=True)
        with log.open("w") as stream:
            subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, check=True)
        report = log.read_text(errors="replace")
        if "Eternity export: functions=" not in report:
            raise RuntimeError(f"Ghidra did not complete its export; inspect {log}")
        print(f"Ghidra: {name} complete", flush=True)


if __name__ == "__main__":
    main()
