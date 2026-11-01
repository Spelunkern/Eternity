// Export Ghidra's function inventory, applied data types and decompiled code.
// @category Eternity
import java.io.*;
import java.nio.charset.StandardCharsets;
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.data.*;

public class EternityExport extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) throw new IllegalArgumentException("Expected output directory and optional name filter");
        File folder = new File(args[0], currentProgram.getName());
        folder.mkdirs();
        String filter = args.length > 1 ? args[1] : "";
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        int functions = 0, succeeded = 0;
        try (PrintWriter inventory = writer(new File(folder, "functions.tsv"));
             PrintWriter types = writer(new File(folder, "types.txt"));
             PrintWriter strings = writer(new File(folder, "strings.tsv"));
             PrintWriter instructions = writer(new File(folder, "disassembly.txt"));
             PrintWriter code = writer(new File(folder, "decompiled.c"))) {
            inventory.println("address\tname\tsignature\tbody_bytes\tdecompiled");
            FunctionIterator iterator = currentProgram.getFunctionManager().getFunctions(true);
            while (iterator.hasNext() && !monitor.isCancelled()) {
                Function function = iterator.next();
                functions++;
                String name = function.getName(true);
                boolean selected = !function.isExternal() && (filter.isEmpty() || name.matches(filter));
                String status = "not_requested";
                if (selected) {
                    instructions.println("\n" + function.getEntryPoint() + " " + name);
                    InstructionIterator listing = currentProgram.getListing().getInstructions(function.getBody(), true);
                    while (listing.hasNext()) {
                        Instruction instruction = listing.next();
                        instructions.println(instruction.getAddress() + " " + instruction.toString());
                    }
                    DecompileResults result = decompiler.decompileFunction(function, 30, monitor);
                    if (result.decompileCompleted() && result.getDecompiledFunction() != null) {
                        code.println("\n/* " + function.getEntryPoint() + " " + name + " */");
                        code.println(result.getDecompiledFunction().getC());
                        succeeded++;
                        status = "ok";
                    } else {
                        status = "failed";
                        code.println("\n/* FAILED: " + function.getEntryPoint() + " " + name + " */");
                    }
                }
                inventory.println(function.getEntryPoint() + "\t" + name + "\t" +
                    function.getSignature() + "\t" + function.getBody().getNumAddresses() + "\t" + status);
            }
            java.util.Iterator<DataType> iteratorTypes = currentProgram.getDataTypeManager().getAllDataTypes();
            while (iteratorTypes.hasNext() && !monitor.isCancelled()) {
                DataType type = iteratorTypes.next();
                types.println(type.getPathName() + "\t" + type.getLength() + "\t" + type.toString());
            }
            strings.println("address\tvalue");
            DataIterator data = currentProgram.getListing().getDefinedData(true);
            while (data.hasNext() && !monitor.isCancelled()) {
                Data item = data.next();
                if (!StringDataInstance.isString(item)) continue;
                StringDataInstance value = StringDataInstance.getStringDataInstance(item);
                String text = value.getStringValue();
                if (text != null) strings.println(item.getAddress() + "\t" +
                    text.replace("\\", "\\\\").replace("\t", "\\t").replace("\r", "\\r").replace("\n", "\\n"));
            }
        } finally {
            decompiler.dispose();
        }
        println("Eternity export: functions=" + functions + ", decompiled=" + succeeded + ", output=" + folder);
    }

    private PrintWriter writer(File file) throws IOException {
        return new PrintWriter(new OutputStreamWriter(new FileOutputStream(file), StandardCharsets.UTF_8));
    }
}
