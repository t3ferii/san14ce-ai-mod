// Read-only export of a saved program, grouped by function entry RVA.
// @category SAN14
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.framework.Application;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.mem.MemoryBlock;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.time.Instant;
import java.util.*;

public class San14FullExport extends GhidraScript {
    private Path out, parts; private Address base; private String hash, started;
    private final Map<String,Long> counts=new TreeMap<>();
    private String cell(String s) { return s==null?"":s.replace("\\","\\\\").replace("\t","\\t").replace("\r","\\r").replace("\n","\\n"); }
    private String rv(Address a) {return a==null?"":String.format("%08X",a.subtract(base));}
    private String q(String s){return "\""+cell(s).replace("\"","\\\"")+"\"";}
    private BufferedWriter writer(Path p) throws Exception {return Files.newBufferedWriter(p,StandardCharsets.UTF_8);}
    private void move(Path p,Path dest) throws Exception {Files.move(p,dest,StandardCopyOption.REPLACE_EXISTING,StandardCopyOption.ATOMIC_MOVE);}
    private void manifest(String status,Collection<String> done) throws Exception {
        StringBuilder b=new StringBuilder("{\n  \"status\": ").append(q(status)).append(",\n  \"sha256\": ").append(q(hash)).append(",\n  \"ghidra_version\": ").append(q(Application.getApplicationVersion())).append(",\n  \"started_utc\": ").append(q(started)).append(",\n  \"finished_utc\": ").append(status.equals("running")?"null":q(Instant.now().toString())).append(",\n  \"failure_file\": \"failures.tsv\",\n  \"completed_shards\": [");
        boolean first=true; for(String s:done){if(!first)b.append(", ");b.append(q(s));first=false;} b.append("],\n  \"counts\": {");first=true;
        for(Map.Entry<String,Long> e:counts.entrySet()){if(!first)b.append(", ");b.append(q(e.getKey())).append(": ").append(e.getValue());first=false;} b.append("}\n}\n");
        Path t=out.resolve("manifest.json.tmp");Files.writeString(t,b.toString(),StandardCharsets.UTF_8);move(t,out.resolve("manifest.json"));
    }
    private void combine(String name,String header,Collection<String> done) throws Exception {
        Path t=out.resolve(name+".tmp");long n=0;
        try(BufferedWriter w=writer(t)){w.write(header+"\n");for(String shard:done){Path p=parts.resolve(shard+"_"+name);if(Files.exists(p))try(BufferedReader r=Files.newBufferedReader(p,StandardCharsets.UTF_8)){String line;while((line=r.readLine())!=null){w.write(line);w.newLine();n++;}}}}
        move(t,out.resolve(name));counts.put(name,n);
    }
    @Override public void run() throws Exception {
        String[] a=getScriptArgs();if(a.length<2)throw new IllegalArgumentException("output expected-sha256 [shard ...]");
        hash=currentProgram.getExecutableSHA256();if(!a[1].equalsIgnoreCase(hash))throw new IllegalStateException("Wrong saved Program SHA-256: "+hash);
        base=currentProgram.getImageBase();if(base.getOffset()!=0x140000000L)throw new IllegalStateException("Unexpected image base");
        out=Paths.get(a[0]).toAbsolutePath();parts=out.resolve("parts");Files.createDirectories(parts);Files.createDirectories(out.resolve("c"));started=Instant.now().toString();
        SortedMap<String,List<Function>> groups=new TreeMap<>();FunctionIterator fi=currentProgram.getFunctionManager().getFunctions(true);
        while(fi.hasNext()){Function f=fi.next();String shard=String.format("%04X",f.getEntryPoint().subtract(base)>>>16);groups.computeIfAbsent(shard,k->new ArrayList<>()).add(f);}
        counts.put("total_shards",(long)groups.size());
        Set<String> requested=new TreeSet<>();if(a.length==2)requested.addAll(groups.keySet());else for(int i=2;i<a.length;i++)requested.add(a[i].toUpperCase(Locale.ROOT));
        SortedSet<String> done=new TreeSet<>();for(String s:groups.keySet())if(Files.exists(parts.resolve(s+".done")) && Files.exists(out.resolve("c/"+s+".c")))done.add(s);
        manifest("running",done);
        DecompInterface dc=new DecompInterface();if(!dc.openProgram(currentProgram))throw new IllegalStateException(dc.getLastMessage());
        try {for(String shard:requested){if(done.contains(shard))continue;List<Function> funcs=groups.get(shard);if(funcs==null)throw new IllegalArgumentException("Unknown shard "+shard);
            long tick=System.nanoTime();Path ct=out.resolve("c/"+shard+".c.tmp");String[] names={"functions.tsv","calls.tsv","datarefs.tsv","failures.tsv"};Map<String,BufferedWriter> ws=new HashMap<>();
            try(BufferedWriter c=writer(ct)) {for(String n:names)ws.put(n,writer(parts.resolve(shard+"_"+n+".tmp")));long line=1;
                for(Function f:funcs){monitor.checkCancelled();String start=rv(f.getEntryPoint()),end=rv(f.getBody().getMaxAddress());long size=f.getBody().getNumAddresses();
                    ws.get("functions.tsv").write(String.format("%s\t%s\t%d\t%s\tc/%s.c\t%d\t%d\t%s\t%s%n",start,end,size,cell(f.getName(true)),shard,line,f.getParameterCount(),cell(f.getSignature().getPrototypeString()),f.isThunk()));
                    String header=String.format("// ==== FUNC %s %s size=%d end=%s\n",start,cell(f.getName(true)),size,end);c.write(header);line++;
                    DecompileResults res=dc.decompileFunction(f,60,monitor);String body;
                    if(!res.decompileCompleted() || res.getDecompiledFunction()==null){body="// DECOMPILE_FAILED "+cell(res.getErrorMessage())+"\n";ws.get("failures.tsv").write(start+"\t"+cell(res.getErrorMessage())+"\n");}
                    else body=res.getDecompiledFunction().getC()+"\n";
                    c.write(body);line+=body.chars().filter(ch->ch=='\n').count();
                    InstructionIterator ins=currentProgram.getListing().getInstructions(f.getBody(),true);while(ins.hasNext()){Instruction in=ins.next();FlowType flow=in.getFlowType();boolean call=flow.isCall(),jump=flow.isJump();boolean emitted=false;
                        for(Reference ref:in.getReferencesFrom()){Address to=ref.getToAddress();RefType rt=ref.getReferenceType();if(rt.isFlow()){if(call||jump){String kind=call?(to.isMemoryAddress()?"direct":"indirect"):(currentProgram.getFunctionManager().getFunctionAt(to)!=null?"tailcall":"jump");ws.get("calls.tsv").write(start+"\t"+rv(in.getAddress())+"\t"+(to.isMemoryAddress()?rv(to):"")+"\t"+kind+"\n");emitted=true;}}else if(to.isMemoryAddress() && currentProgram.getMemory().contains(to) && currentProgram.getListing().getInstructionAt(to)==null){String type=rt.isWrite()?"WRITE":rt.isRead()?"READ":"DATA";ws.get("datarefs.tsv").write(rv(in.getAddress())+"\t"+start+"\t"+rv(to)+"\t"+type+"\n");}}
                        if(call&&!emitted)ws.get("calls.tsv").write(start+"\t"+rv(in.getAddress())+"\t\tindirect\n");
                    }
                }
            } finally {for(BufferedWriter w:ws.values())w.close();}
            move(ct,out.resolve("c/"+shard+".c"));for(String n:names)move(parts.resolve(shard+"_"+n+".tmp"),parts.resolve(shard+"_"+n));Files.writeString(parts.resolve(shard+".done"),"complete\n");done.add(shard);
            String log=String.format("%s shard=%s functions=%d seconds=%.1f%n",Instant.now(),shard,funcs.size(),(System.nanoTime()-tick)/1e9);println(log);manifest("running",done);
        }}finally{dc.dispose();}
        combine("functions.tsv","rva\tend\tsize\tname\tshard\tline\tparameters\tsignature\tthunk",done);combine("calls.tsv","caller_func_rva\tcallsite_rva\tcallee_rva\tkind",done);combine("datarefs.tsv","from_rva\tfrom_func_rva\tto_rva\tref_type",done);combine("failures.tsv","rva\treason",done);
        exportGlobal();manifest(done.containsAll(groups.keySet())?"complete":"partial",done);println("SAN14_FULL_EXPORT_FINISHED");
    }
    private List<Reference> codeRefs(Address addr) { List<Reference> result=new ArrayList<>(); for(Reference r:currentProgram.getReferenceManager().getReferencesTo(addr)) if(currentProgram.getListing().getInstructionContaining(r.getFromAddress())!=null) result.add(r); return result; }
    private String firstRefFunc(Address addr) { for(Reference r:codeRefs(addr)){Function f=currentProgram.getFunctionManager().getFunctionContaining(r.getFromAddress());if(f!=null)return rv(f.getEntryPoint());}return ""; }
    private void exportGlobal() throws Exception {
        long n=0;Path t=out.resolve("strings.tsv.tmp");try(BufferedWriter w=writer(t)){w.write("rva\tencoding\ttext\tref_functions\n");DataIterator it=currentProgram.getListing().getDefinedData(true);while(it.hasNext()){Data d=it.next();Object v=d.getValue();if(!(v instanceof String))continue;TreeSet<String> refs=new TreeSet<>();for(Reference r:currentProgram.getReferenceManager().getReferencesTo(d.getAddress())){Function f=currentProgram.getFunctionManager().getFunctionContaining(r.getFromAddress());if(f!=null)refs.add(rv(f.getEntryPoint()));}w.write(rv(d.getAddress())+"\t"+cell(d.getDataType().getName())+"\t"+cell((String)v)+"\t"+String.join(",",refs)+"\n");n++;}}move(t,out.resolve("strings.tsv"));counts.put("strings.tsv",n);
        t=out.resolve("symbols.tsv.tmp");Path vt=out.resolve("vtables.tsv.tmp"),im=out.resolve("imports.tsv.tmp"),ir=out.resolve("import_refs.tsv.tmp");long ns=0,nv=0,ni=0,nir=0; Set<String> importSlots=new HashSet<>(), importRefs=new HashSet<>();
        try(BufferedWriter s=writer(t);BufferedWriter v=writer(vt);BufferedWriter i=writer(im);BufferedWriter refs=writer(ir)){refs.write("from_rva\tfrom_func_rva\tdll\tfunction\n");s.write("rva\tname\tkind\n");v.write("vftable_rva\tclass\tslot\ttarget_rva\tcode_refs\tfirst_ref_func\n");i.write("dll\tfunction\tiat_rva\n");SymbolIterator it=currentProgram.getSymbolTable().getAllSymbols(true);while(it.hasNext()){Symbol sym=it.next();Address addr=sym.getAddress();String name=sym.getName(true);if(addr.isMemoryAddress() && currentProgram.getMemory().contains(addr)){s.write(rv(addr)+"\t"+cell(name)+"\t"+sym.getSymbolType()+"\n");ns++;if(name.toLowerCase(Locale.ROOT).contains("vftable") && codeRefs(addr).size()>0){for(int slot=0;slot<512;slot++){Address at=addr.add(slot*8L);long ptr;try{ptr=currentProgram.getMemory().getLong(at);}catch(Exception ex){break;}Address target=toAddr(ptr);Function f=currentProgram.getFunctionManager().getFunctionAt(target);if(f==null)break;v.write(rv(addr)+"\t"+cell(name)+"\t"+slot+"\t"+rv(target)+"\t"+codeRefs(addr).size()+"\t"+firstRefFunc(addr)+"\n");nv++;}}}if(sym.isExternal()){for(Reference ref:currentProgram.getReferenceManager().getReferencesTo(addr)){Address from=ref.getFromAddress();if(from.isMemoryAddress()&&currentProgram.getMemory().contains(from)){String dll=cell(sym.getParentNamespace().getName(true)), fn=cell(sym.getName()); Instruction ins=currentProgram.getListing().getInstructionContaining(from); if(ins==null){String key=dll+"\t"+fn+"\t"+rv(from);if(importSlots.add(key)){i.write(key+"\n");ni++;} for(Reference use:currentProgram.getReferenceManager().getReferencesTo(from)){Function f=currentProgram.getFunctionManager().getFunctionContaining(use.getFromAddress());if(f!=null && currentProgram.getListing().getInstructionContaining(use.getFromAddress())!=null){String key2=rv(use.getFromAddress())+"\t"+rv(f.getEntryPoint())+"\t"+dll+"\t"+fn;if(importRefs.add(key2)){refs.write(key2+"\n");nir++;}}}}else{Function f=currentProgram.getFunctionManager().getFunctionContaining(from);if(f!=null){String key2=rv(from)+"\t"+rv(f.getEntryPoint())+"\t"+dll+"\t"+fn;if(importRefs.add(key2)){refs.write(key2+"\n");nir++;}}}}}}}
            // Anonymous candidates: at least three consecutive aligned pointers to function entries.
            for(MemoryBlock block:currentProgram.getMemory().getBlocks()) {if(!block.isInitialized() || block.isExecute())continue;
                long lo=block.getStart().getOffset(),hi=block.getEnd().getOffset();long pos=(lo+7)&~7L;
                while(pos<=hi-23){long first=pos;ArrayList<Address> targets=new ArrayList<>();
                    while(pos<=hi-7){long ptr=currentProgram.getMemory().getLong(toAddr(pos));Address target=toAddr(ptr);if(currentProgram.getFunctionManager().getFunctionAt(target)==null)break;targets.add(target);pos+=8;}
                    if(targets.size()>=3 && codeRefs(toAddr(first)).size()>0){for(int slot=0;slot<targets.size();slot++){v.write(rv(toAddr(first))+"\t?\t"+slot+"\t"+rv(targets.get(slot))+"\t"+codeRefs(toAddr(first)).size()+"\t"+firstRefFunc(toAddr(first))+"\n");nv++;}}
                    if(pos==first)pos+=8;
                }
            }
        }
        move(t,out.resolve("symbols.tsv"));move(vt,out.resolve("vtables.tsv"));move(im,out.resolve("imports.tsv"));move(ir,out.resolve("import_refs.tsv"));counts.put("import_refs.tsv",nir);counts.put("symbols.tsv",ns);counts.put("vtables.tsv",nv);counts.put("imports.tsv",ni);
    }
}
