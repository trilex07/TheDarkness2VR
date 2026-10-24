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
                // Optional VA:HEX explicitly asserts an independently verified entry.
                // Plain VAs remain read-only queries and never invent function starts.
                String[] query = args[i].split(":", 2);
                Address address = toAddr(Long.parseLong(query[0], 16));
                if (query.length == 2) {
                    String hex = query[1];
                    if (hex.length() < 12 || hex.length() % 2 != 0)
                        throw new IllegalArgumentException("Entry assertion needs >=6 bytes");
                    for (int j = 0; j < hex.length(); j += 2) {
                        int expected = Integer.parseInt(hex.substring(j, j + 2), 16);
                        int observed = currentProgram.getMemory().getByte(address.add(j / 2)) & 255;
                        if (expected != observed)
                            throw new IllegalArgumentException("Entry bytes differ at " + address);
                    }
                    if (!currentProgram.getMemory().getBlock(address).isExecute())
                        throw new IllegalArgumentException("Entry is not executable: " + address);
                    Function existing = getFunctionContaining(address);
                    if (existing != null && !existing.getEntryPoint().equals(address))
                        throw new IllegalArgumentException("Entry lies inside existing function: " + address);
                    if (existing == null) {
                        if (!disassemble(address) || createFunction(address, null) == null)
                            throw new IllegalStateException("Cannot analyze asserted entry: " + address);
                    }
                    output.println("ASSERTED ENTRY BYTES " + address + " " + hex);
                }
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
            // Headless Ghidra may exit 0 even after a script exception.
            // Only a complete export contains this final marker.
            output.println("EVIDENCE COMPLETE");
        } finally {
            decompiler.dispose();
        }
    }
}
