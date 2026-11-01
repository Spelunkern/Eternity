// Locate login protocol constants in a candidate original client.
// @category Eternity
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.scalar.Scalar;

public class EternityClient extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) throw new IllegalArgumentException("Expected output directory");
        File folder = new File(args[0]);
        folder.mkdirs();
        try (PrintWriter metadata = writer(new File(folder, "program.tsv"))) {
            metadata.println("executable\t" + currentProgram.getExecutablePath());
            metadata.println("image_base\t" + currentProgram.getImageBase());
            metadata.println("functions\t" + currentProgram.getFunctionManager().getFunctionCount());
            for (ghidra.program.model.mem.MemoryBlock block : currentProgram.getMemory().getBlocks())
                metadata.println("block\t" + block.getName() + "\t" + block.getStart() + "\t" + block.getSize());
        }
        Set<Long> targets = Set.of(0xa101L, 0xa102L, 0xa201L, 0xa202L, 0x1001L, 30800L, 30810L);
        Map<String, Function> functions = new TreeMap<>();
        try (PrintWriter anchors = writer(new File(folder, "login-anchors.tsv"))) {
            anchors.println("instruction\tconstant\tfunction\tassembly");
            InstructionIterator instructions = currentProgram.getListing().getInstructions(true);
            while (instructions.hasNext() && !monitor.isCancelled()) {
                Instruction instruction = instructions.next();
                for (int operand = 0; operand < instruction.getNumOperands(); operand++) {
                    for (Object object : instruction.getOpObjects(operand)) {
                        if (!(object instanceof Scalar)) continue;
                        long value = ((Scalar)object).getUnsignedValue();
                        if (!targets.contains(value)) continue;
                        Function function = currentProgram.getFunctionManager().getFunctionContaining(instruction.getAddress());
                        anchors.println(instruction.getAddress() + "\t" + Long.toHexString(value) + "\t" +
                            (function == null ? "" : function.getEntryPoint()) + "\t" + instruction.toString());
                        if (function != null) functions.put(function.getEntryPoint().toString(), function);
                    }
                }
            }
        }
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try (PrintWriter code = writer(new File(folder, "login-candidates.c"));
             PrintWriter assembly = writer(new File(folder, "login-candidates.asm"))) {
            for (Function function : functions.values()) {
                DecompileResults result = decompiler.decompileFunction(function, 45, monitor);
                code.println("\n/* " + function.getEntryPoint() + " " + function.getName(true) + " */");
                if (result.decompileCompleted() && result.getDecompiledFunction() != null)
                    code.println(result.getDecompiledFunction().getC());
                InstructionIterator instructions = currentProgram.getListing().getInstructions(function.getBody(), true);
                assembly.println("\n" + function.getEntryPoint() + " " + function.getName(true));
                while (instructions.hasNext()) {
                    Instruction instruction = instructions.next();
                    assembly.println(instruction.getAddress() + " " + instruction.toString());
                }
            }
        } finally { decompiler.dispose(); }
        println("Eternity client: login candidate functions=" + functions.size());
    }

    private PrintWriter writer(File file) throws IOException {
        return new PrintWriter(new OutputStreamWriter(new FileOutputStream(file), StandardCharsets.UTF_8));
    }
}
