// Read a saved Program and export selected functions; never edits the Program.
// @category SAN14
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.io.BufferedWriter;
import java.time.Instant;
import java.util.Arrays;

public class San14ExportFunctions extends GhidraScript {
    @Override public void run() throws Exception {
        String[] a=getScriptArgs();
        if(a.length<3)throw new IllegalArgumentException("output-directory expected-sha256 RVA-or-address [...]");
        String hash=currentProgram.getExecutableSHA256();
        if(!a[1].equalsIgnoreCase(hash))throw new IllegalStateException("Wrong saved Program SHA-256: "+hash);
        if(currentProgram.getImageBase().getOffset()!=0x140000000L)throw new IllegalStateException("Unexpected image base");
        byte[] expected={(byte)0xE8,0x51,(byte)0xCF,0x33,0},observed=new byte[5];
        currentProgram.getMemory().getBytes(toAddr(0x1405F14DAL),observed);
        if(!Arrays.equals(expected,observed))throw new IllegalStateException("Native CALL bytes differ at RVA5F14DA");
        if(currentProgram.getMemory().getLong(toAddr(0x14141BD18L))!=0x1405F12B0L)
            throw new IllegalStateException("Native vtable entry differs at RVA141BD18");
        println("SAN14_CODE_BYTES_VERIFIED RVA5F14DA and RVA141BD18");
        Path out=Paths.get(a[0]);Files.createDirectories(out);
        DecompInterface dc=new DecompInterface();
        try {
            if(!dc.openProgram(currentProgram))throw new IllegalStateException(dc.getLastMessage());
            for(int i=2;i<a.length;i++) {
                monitor.checkCancelled();
                long value=Long.parseUnsignedLong(a[i].replaceFirst("(?i)^0x",""),16);
                Address at=value>=currentProgram.getImageBase().getOffset()?toAddr(value):currentProgram.getImageBase().add(value);
                Function f=currentProgram.getFunctionManager().getFunctionContaining(at);
                if(f==null)throw new IllegalArgumentException("No function contains "+at);
                long rva=f.getEntryPoint().subtract(currentProgram.getImageBase());
                String stem=String.format("%08X",rva);
                Path c=out.resolve(stem+"_c.txt"),asm=out.resolve(stem+"_asm.txt");
                if(Files.exists(c) && Files.exists(asm)) {println("SAN14_CACHE_PRESENT "+stem);continue;}
                DecompileResults result=dc.decompileFunction(f,90,monitor);
                if(!result.decompileCompleted() || result.getDecompiledFunction()==null)
                    throw new IllegalStateException("Decompile failed "+stem+": "+result.getErrorMessage());
                String header="# rva="+stem+" kind=%s digest="+hash+"\n# source=persistent-ghidra:"+currentProgram.getName()+
                    "\n# saved="+Instant.now()+"\n";
                if(!Files.exists(c))Files.writeString(c,String.format(header,"c")+result.getDecompiledFunction().getC(),StandardCharsets.UTF_8,StandardOpenOption.CREATE_NEW);
                if(!Files.exists(asm))try(BufferedWriter w=Files.newBufferedWriter(asm,StandardCharsets.UTF_8,StandardOpenOption.CREATE_NEW)) {
                    w.write(String.format(header,"asm"));
                    InstructionIterator instructions=currentProgram.getListing().getInstructions(f.getBody(),true);
                    while(instructions.hasNext()) {Instruction ins=instructions.next();w.write(ins.getAddress()+" "+ins+"\n");}
                }
                println("SAN14_FUNCTION_EXPORTED "+stem+" "+f.getName());
            }
        } finally {dc.dispose();}
    }
}
