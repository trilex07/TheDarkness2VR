// Read-only, bounded search for the renderer-to-skin-pack dispatch in the
// verified local EXE. Export only to a private path outside the repository.
// @category Darkness2VR
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import java.io.*;
import java.util.*;

public class RootCauseRefs extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1";
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) throw new IllegalArgumentException("private output path required");
        String sha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(sha256))
            throw new IllegalStateException("wrong EXE identity for root-cause addresses: " + sha256);
        try (PrintWriter out = new PrintWriter(args[0])) {
            out.println("Executable SHA256: " + sha256);
            FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(true);
            while (functions.hasNext() && !monitor.isCancelled()) {
                Function function = functions.next();
                boolean resource = false, method = false, packed = false;
                boolean pose = false, skin = false, dirty = false;
                ArrayList<String> hits = new ArrayList<>();
                InstructionIterator instructions = currentProgram.getListing().getInstructions(function.getBody(), true);
                while (instructions.hasNext()) {
                    Instruction instruction = instructions.next();
                    String repr = instruction.toString().toLowerCase(Locale.ROOT);
                    if (repr.contains("+ 0x5bc")) {
                        resource = true; hits.add(instruction.getAddress() + " " + repr);
                    }
                    if (repr.contains("+ 0xec")) {
                        method = true; hits.add(instruction.getAddress() + " " + repr);
                    }
                    if (repr.contains("+ 0x428")) {
                        packed = true; hits.add(instruction.getAddress() + " " + repr);
                    }
                    if (repr.contains("+ 0x424")) {
                        pose = true; hits.add(instruction.getAddress() + " " + repr);
                    }
                    if (repr.contains("+ 0x448")) {
                        skin = true; hits.add(instruction.getAddress() + " " + repr);
                    }
                    if (repr.contains("+ 0x580")) {
                        dirty = true; hits.add(instruction.getAddress() + " " + repr);
                    }
                }
                if ((resource && (method || packed)) || (pose && skin && dirty)) {
                    out.println("FUNCTION " + function.getEntryPoint() + " method=" + method +
                        " packed=" + packed + " pose=" + pose + " skin=" + skin + " dirty=" + dirty);
                    for (String hit : hits) out.println(hit);
                }
            }
            if (monitor.isCancelled()) throw new IllegalStateException("cancelled");
            out.println("EVIDENCE COMPLETE");
        }
    }
}
