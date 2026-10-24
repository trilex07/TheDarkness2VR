// Export only to a private directory OUTSIDE the repository.
// @category Darkness2VR
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.io.*;

public class CameraEvidence extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            throw new IllegalArgumentException("Private output path, then explicit hex VAs required");
        }
        DecompInterface decompiler = new DecompInterface();
        boolean haveDecompiler = decompiler.openProgram(currentProgram);
        try (PrintWriter output = new PrintWriter(args[0])) {
            output.println("Executable SHA256: " + currentProgram.getExecutableSHA256());
            for (int i = 1; i < args.length; i++) {
                Address address = toAddr(Long.parseLong(args[i], 16));
                output.println("ADDRESS " + address);
                ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(address);
                while (refs.hasNext()) {
                    Reference ref = refs.next();
                    Function caller = getFunctionContaining(ref.getFromAddress());
                    output.println("XREF " + ref + " FUNCTION " +
                        (caller == null ? "unknown" : caller.getEntryPoint()));
                }
                Function function = getFunctionContaining(address);
                if (function == null) {
                    // A call site inside an unknown function is not a function entry.
                    // Do not invent a function at the requested instruction.
                    output.println("NO ANALYZED FUNCTION; analyze caller boundaries first");
                    continue;
                }
                output.println("FUNCTION " + function.getEntryPoint() + " " + function.getSignature());
                InstructionIterator instructions = currentProgram.getListing().getInstructions(
                    function.getBody(), true);
                while (instructions.hasNext()) {
                    Instruction instruction = instructions.next();
                    output.println(instruction.getAddress() + " " + instruction);
                }
                if (haveDecompiler) {
                    DecompileResults result = decompiler.decompileFunction(function, 60, monitor);
                    output.println(result.decompileCompleted() ?
                        result.getDecompiledFunction().getC() : result.getErrorMessage());
                } else {
                    output.println("DECOMPILER UNAVAILABLE; disassembly only");
                }
            }
        } finally {
            decompiler.dispose();
        }
    }
}
