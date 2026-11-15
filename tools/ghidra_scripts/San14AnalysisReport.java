// Export a bounded index after native auto-analysis, without changing the Program.
// @category SAN14
import ghidra.app.util.headless.HeadlessScript;
import ghidra.framework.Application;
import ghidra.program.model.listing.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.io.BufferedWriter;
import java.time.Instant;

public class San14AnalysisReport extends HeadlessScript {
    private String quoted(String s) {
        return "\""+s.replace("\\","\\\\").replace("\"","\\\"").replace("\n","\\n").replace("\r","\\r").replace("\t","\\t")+"\"";
    }
    private String cell(String s) {return s.replace("\\","\\\\").replace("\t","\\t").replace("\r","\\r").replace("\n","\\n");}
    @Override public void run() throws Exception {
        String[] a=getScriptArgs();
        if(a.length!=2)throw new IllegalArgumentException("output-directory expected-sha256");
        if(analysisTimeoutOccurred())throw new IllegalStateException("Auto-analysis timed out; database is not certified complete");
        String hash=currentProgram.getExecutableSHA256();
        if(!a[1].equalsIgnoreCase(hash))throw new IllegalStateException("Wrong Program SHA-256: "+hash);
        Path out=Paths.get(a[0]);Files.createDirectories(out);
        long count=0,strings=0;
        Path ftmp=out.resolve("functions.tsv.tmp"),stmp=out.resolve("strings.tsv.tmp");
        try(BufferedWriter w=Files.newBufferedWriter(ftmp,StandardCharsets.UTF_8)) {
            w.write("rva\tentry\tend\tbytes\tparameters\tthunk\tname\n");
            FunctionIterator it=currentProgram.getFunctionManager().getFunctions(true);
            while(it.hasNext()) {
                monitor.checkCancelled();Function f=it.next();
                long rva=f.getEntryPoint().subtract(currentProgram.getImageBase());
                w.write(String.format("%08X\t%s\t%s\t%d\t%d\t%s\t%s%n",rva,f.getEntryPoint(),f.getBody().getMaxAddress(),
                    f.getBody().getNumAddresses(),f.getParameterCount(),f.isThunk(),cell(f.getName(true))));
                count++;
            }
        }
        try(BufferedWriter w=Files.newBufferedWriter(stmp,StandardCharsets.UTF_8)) {
            w.write("rva\taddress\tbytes\ttext\n");
            DataIterator it=currentProgram.getListing().getDefinedData(true);
            while(it.hasNext()) {
                monitor.checkCancelled();Data d=it.next();Object value=d.getValue();
                if(!(value instanceof String))continue;
                w.write(String.format("%08X\t%s\t%d\t%s%n",d.getAddress().subtract(currentProgram.getImageBase()),
                    d.getAddress(),d.getLength(),cell((String)value)));strings++;
            }
        }
        Files.move(ftmp,out.resolve("functions.tsv"),StandardCopyOption.REPLACE_EXISTING);
        Files.move(stmp,out.resolve("strings.tsv"),StandardCopyOption.REPLACE_EXISTING);
        String json="{\n  \"status\":\"post_analysis_index_exported\",\n  \"created_utc\":"+quoted(Instant.now().toString())+
            ",\n  \"sha256\":"+quoted(hash)+",\n  \"program\":"+quoted(currentProgram.getName())+
            ",\n  \"image_base\":"+quoted(currentProgram.getImageBase().toString())+
            ",\n  \"ghidra_version\":"+quoted(Application.getApplicationVersion())+
            ",\n  \"language\":"+quoted(currentProgram.getLanguageID().toString())+
            ",\n  \"compiler\":"+quoted(currentProgram.getCompilerSpec().getCompilerSpecID().toString())+
            ",\n  \"functions\":"+count+",\n  \"instructions\":"+currentProgram.getListing().getNumInstructions()+
            ",\n  \"strings\":"+strings+",\n  \"analysis_timeout\":false\n}\n";
        Files.writeString(out.resolve("analysis_report.json"),json,StandardCharsets.UTF_8);
        println("SAN14_INDEX_EXPORTED functions="+count+" strings="+strings+" sha256="+hash);
    }
}
