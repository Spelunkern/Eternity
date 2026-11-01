// Select the exact local PDB before Ghidra's native PDB Universal analysis.
// @category Eternity
import java.io.File;
import ghidra.app.script.GhidraScript;
import ghidra.app.plugin.core.analysis.PdbUniversalAnalyzer;

public class EternityPrepare extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) throw new IllegalArgumentException("Expected exact PDB path");
        File pdb = new File(args[0]);
        if (!pdb.isFile()) throw new IllegalArgumentException("PDB does not exist: " + pdb);
        PdbUniversalAnalyzer.setPdbFileOption(currentProgram, pdb);
        println("Eternity: selected " + pdb.getCanonicalPath());
    }
}
