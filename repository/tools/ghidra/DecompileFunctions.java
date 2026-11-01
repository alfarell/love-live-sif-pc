// Ghidra headless post-script: decompiles the functions containing the given addresses into one .c file each.
// Usage: analyzeHeadless <project dir> <project> -process eboot.elf -noanalysis
//          -scriptPath repository/tools/ghidra -postScript DecompileFunctions.java <out dir> <0xaddr> [<0xaddr> ...]
// @category SIF
import java.io.File;
import java.io.PrintWriter;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class DecompileFunctions extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            printerr("usage: DecompileFunctions.java <out dir> <0xaddr> ...");
            return;
        }
        File out = new File(args[0]);
        out.mkdirs();
        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);
        for (int i = 1; i < args.length; i++) {
            Address addr = toAddr(args[i]);
            Function fn = getFunctionContaining(addr);
            if (fn == null) fn = createFunction(addr, null);  // addresses taken from the disassembly may start a function
            if (fn == null) {
                printerr("no function at " + args[i]);
                continue;
            }
            DecompileResults res = decomp.decompileFunction(fn, 120, monitor);
            String name = fn.getEntryPoint().toString() + "_" + fn.getName();
            try (PrintWriter w = new PrintWriter(new File(out, name + ".c"), "UTF-8")) {
                w.println("// " + fn.getName() + " at " + fn.getEntryPoint() + " (requested " + args[i] + ")");
                w.println(res.decompileCompleted() ? res.getDecompiledFunction().getC() : "// failed: " + res.getErrorMessage());
            }
            println("wrote " + name + ".c");
        }
        decomp.dispose();
    }
}
