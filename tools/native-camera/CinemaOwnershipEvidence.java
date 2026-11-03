// Read-only search of the pinned native movie owner. Output stays private.
// @category Darkness2VR
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.*;

public class CinemaOwnershipEvidence extends GhidraScript {
    public void run() throws Exception {
        String[] args=getScriptArgs();
        if(args.length!=1) throw new IllegalArgumentException("Private output path required");
        if(!"d46bb6cb762732b72ab9d34428237d2afa89f5792bdbed957d78793ee8134ec1".equalsIgnoreCase(
                currentProgram.getExecutableSHA256())) throw new IllegalStateException("Wrong EXE");
        try(PrintWriter out=new PrintWriter(args[0])) {
            out.println("EXE "+currentProgram.getExecutableSHA256());
            for(long address:new long[]{0x477fe0,0x81d230,0xfc7d00,0x8cf3f0}) {
                ReferenceIterator refs=currentProgram.getReferenceManager().getReferencesTo(toAddr(address));
                while(refs.hasNext()) {
                    Reference ref=refs.next();
                    Function f=getFunctionContaining(ref.getFromAddress());
                    out.println("XREF "+ref+" FUNCTION "+(f==null?"unknown":f.getEntryPoint()));
                }
            }
            InstructionIterator instructions=currentProgram.getListing().getInstructions(true);
            while(instructions.hasNext() && !monitor.isCancelled()) {
                Instruction ins=instructions.next();
                String s=ins.toString().toLowerCase(java.util.Locale.ROOT);
                if(!s.contains("+ 0x2e8]") && !s.contains("+ 0x2dc]")) continue;
                Function f=getFunctionContaining(ins.getAddress());
                out.println("FIELD "+ins.getAddress()+" "+ins+" FUNCTION "+(f==null?"unknown":f.getEntryPoint()));
            }
            if(monitor.isCancelled()) throw new IllegalStateException("Cancelled");
            out.println("EVIDENCE COMPLETE");
        }
    }
}
