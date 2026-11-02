// Inspect a targeted memory range and recover code referencing it.
// @category Eternity
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.mem.MemoryAccessException;

public class EternityMemory extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args=getScriptArgs();
        if(args.length<3 || args.length>4 || (args.length==4 && !args[3].equals("writes")))
            throw new IllegalArgumentException("Expected output directory, hexadecimal address, byte length, optional writes filter");
        boolean writesOnly=args.length==4;
        File output=new File(args[0]); output.mkdirs();
        Address start=toAddr(args[1]);
        int length=Integer.parseInt(args[2]);
        if(length<1 || length>65536) throw new IllegalArgumentException("Range must be in 1..65536 bytes");
        Set<Function> functions=new TreeSet<>(Comparator.comparing(f->f.getEntryPoint().toString()));
        try(PrintWriter references=writer(new File(output,"references.tsv"));
            PrintWriter symbols=writer(new File(output,"symbols.tsv"))) {
            references.println("source\ttarget\ttype\tfunction");
            for(int offset=0;offset<length && !monitor.isCancelled();offset++) {
                Address address=start.add(offset);
                for(Reference reference:getReferencesTo(address)) {
                    Function function=getFunctionContaining(reference.getFromAddress());
                    references.println(reference.getFromAddress()+"\t"+address+"\t"+reference.getReferenceType()+"\t"+
                        (function==null?"":function.getName(true)));
                    if(function!=null && (!writesOnly || reference.getReferenceType().isWrite())) functions.add(function);
                }
            }
            SymbolIterator iterator=currentProgram.getSymbolTable().getAllSymbols(true);
            while(iterator.hasNext()) {
                Symbol symbol=iterator.next();
                String name=symbol.getName(true);
                if(name.toLowerCase().contains("xor")) symbols.println(symbol.getAddress()+"\t"+name);
            }
        }
        try(FileOutputStream bytes=new FileOutputStream(new File(output,"memory.bin"))) {
            byte[] data=new byte[length];
            try {
                currentProgram.getMemory().getBytes(start,data);
                bytes.write(data);
            } catch(MemoryAccessException error) {
                println("Target includes uninitialized memory; runtime contents are not present in the file");
            }
        }
        DecompInterface decompiler=new DecompInterface();decompiler.openProgram(currentProgram);
        try(PrintWriter code=writer(new File(output,"decompiled.c"));
            PrintWriter instructions=writer(new File(output,"disassembly.txt"))) {
            for(Function function:functions) {
                code.println("\n/* "+function.getEntryPoint()+" "+function.getName(true)+" */");
                DecompileResults result=decompiler.decompileFunction(function,60,monitor);
                if(result.decompileCompleted() && result.getDecompiledFunction()!=null)
                    code.println(result.getDecompiledFunction().getC());
                instructions.println("\n"+function.getEntryPoint()+" "+function.getName(true));
                InstructionIterator iterator=currentProgram.getListing().getInstructions(function.getBody(),true);
                while(iterator.hasNext()) {
                    Instruction instruction=iterator.next();
                    instructions.println(instruction.getAddress()+" "+instruction.toString());
                }
            }
        } finally {decompiler.dispose();}
        println("Eternity memory: referencing functions="+functions.size());
    }
    private PrintWriter writer(File file) throws IOException {
        return new PrintWriter(new OutputStreamWriter(new FileOutputStream(file),StandardCharsets.UTF_8));
    }
}
