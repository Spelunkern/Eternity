// Give unnamed PDB padding fields distinct offset labels; infer no field semantics.
// @category Eternity
import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.*;

public class EternityOffsets extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) throw new IllegalArgumentException("Expected a data-type name regex");
        int changed = 0;
        java.util.Iterator<DataType> types = currentProgram.getDataTypeManager().getAllDataTypes();
        while (types.hasNext() && !monitor.isCancelled()) {
            DataType type = types.next();
            if (!(type instanceof Structure) || !type.getName().matches(args[0])) continue;
            for (DataTypeComponent component : ((Structure)type).getDefinedComponents()) {
                String name = component.getFieldName();
                if (name == null || !name.startsWith("_padding_")) continue;
                component.setFieldName("field_0x" + Integer.toHexString(component.getOffset()));
                changed++;
            }
        }
        println("Eternity offset labels: " + changed);
    }
}
