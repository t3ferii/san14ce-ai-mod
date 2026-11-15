// Export of a freshly analysed reference-mod DLL (San14FullExport variant: no fixed image base,
// parallel decompile, exports.tsv, game_rvas.tsv). Args: outDir expectedSha256 gameFunctionsTsv source
// @category SAN14
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.framework.Application;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.scalar.Scalar;
import java.io.*;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.time.Instant;
import java.util.*;
import java.util.concurrent.*;

public class San14RefExport extends GhidraScript {
    private Path out; private Address base; private String hash, started, source;
    private final Map<String,Long> counts=new TreeMap<>();
    private String cell(String s) { return s==null?"":s.replace("\\","\\\\").replace("\t","\\t").replace("\r","\\r").replace("\n","\\n"); }
    private String rv(Address a) {return a==null?"":String.format("%08X",a.subtract(base));}
    private String q(String s){return "\""+cell(s).replace("\"","\\\"")+"\"";}
    private BufferedWriter writer(Path p) throws Exception {return Files.newBufferedWriter(p,StandardCharsets.UTF_8);}
    private void move(Path p,Path dest) throws Exception {Files.move(p,dest,StandardCopyOption.REPLACE_EXISTING);}
    private void manifest(String status) throws Exception {
        StringBuilder b=new StringBuilder("{\n  \"status\": ").append(q(status)).append(",\n  \"sha256\": ").append(q(hash)).append(",\n  \"source\": ").append(q(source)).append(",\n  \"image_base\": ").append(q(base.toString())).append(",\n  \"ghidra_version\": ").append(q(Application.getApplicationVersion())).append(",\n  \"started_utc\": ").append(q(started)).append(",\n  \"finished_utc\": ").append(status.equals("running")?"null":q(Instant.now().toString())).append(",\n  \"failure_file\": \"failures.tsv\",\n  \"counts\": {");
        boolean first=true;
        for(Map.Entry<String,Long> e:counts.entrySet()){if(!first)b.append(", ");b.append(q(e.getKey())).append(": ").append(e.getValue());first=false;} b.append("}\n}\n");
        Path t=out.resolve("manifest.json.tmp");Files.writeString(t,b.toString(),StandardCharsets.UTF_8);move(t,out.resolve("manifest.json"));
    }
    private final Set<Long> gameStarts=new HashSet<>();
    private BufferedWriter gw;
    private long gameRows=0;
    private void gameHit(long v,String form,Address loc,String func) throws Exception {
        long rva;
        if(v>=0x140001000L && v<0x142B00000L) rva=v-0x140000000L;
        else if(v>=0x2000L && v<0x2B00000L) rva=v;
        else return;
        if(!gameStarts.contains(rva)) return;
        gw.write(String.format("%08X\t%s\t%s\t%s\t0x%X%n",rva,form,rv(loc),func,v));gameRows++;
    }
    @Override public void run() throws Exception {
        String[] a=getScriptArgs();if(a.length<4)throw new IllegalArgumentException("output expected-sha256 gameFunctions source");
        hash=currentProgram.getExecutableSHA256();if(!a[1].equalsIgnoreCase(hash))throw new IllegalStateException("Wrong Program SHA-256: "+hash);
        source=a[3];
        base=currentProgram.getImageBase();
        if(currentProgram.getDefaultPointerSize()!=8)throw new IllegalStateException("Not a 64-bit program");
        try(BufferedReader r=Files.newBufferedReader(Paths.get(a[2]),StandardCharsets.UTF_8)){String line=r.readLine();while((line=r.readLine())!=null){int t=line.indexOf('\t');if(t>0)gameStarts.add(Long.parseLong(line.substring(0,t),16));}}
        out=Paths.get(a[0]).toAbsolutePath();Files.createDirectories(out.resolve("c"));started=Instant.now().toString();
        manifest("running");
        List<Function> funcs=new ArrayList<>();FunctionIterator fi=currentProgram.getFunctionManager().getFunctions(true);while(fi.hasNext())funcs.add(fi.next());
        SortedMap<String,List<Function>> groups=new TreeMap<>();
        for(Function f:funcs){String shard=String.format("%04X",f.getEntryPoint().subtract(base)>>>16);groups.computeIfAbsent(shard,k->new ArrayList<>()).add(f);}
        counts.put("total_shards",(long)groups.size());
        int nThreads=Math.max(1,Math.min(6,Runtime.getRuntime().availableProcessors()/2));
        List<DecompInterface> all=Collections.synchronizedList(new ArrayList<>());
        ThreadLocal<DecompInterface> tl=ThreadLocal.withInitial(()->{DecompInterface d=new DecompInterface();d.openProgram(currentProgram);all.add(d);return d;});
        ExecutorService ex=Executors.newFixedThreadPool(nThreads);
        BufferedWriter fw=writer(out.resolve("functions.tsv.tmp")),cw=writer(out.resolve("calls.tsv.tmp")),dw=writer(out.resolve("datarefs.tsv.tmp")),xw=writer(out.resolve("failures.tsv.tmp"));
        gw=writer(out.resolve("game_rvas.tsv.tmp"));
        fw.write("rva\tend\tsize\tname\tshard\tline\tparameters\tsignature\tthunk\n");cw.write("caller_func_rva\tcallsite_rva\tcallee_rva\tkind\n");dw.write("from_rva\tfrom_func_rva\tto_rva\tref_type\n");xw.write("rva\treason\n");
        gw.write("game_rva\tform\tloc_rva\tfunc_rva\tvalue\n");
        long nf=0,nc=0,nd=0,nx=0;
        try {
            for(Map.Entry<String,List<Function>> g:groups.entrySet()){
                String shard=g.getKey();List<Function> fl=g.getValue();
                List<Future<String[]>> futs=new ArrayList<>();
                for(Function f:fl){futs.add(ex.submit(()->{DecompInterface d=tl.get();DecompileResults res=d.decompileFunction(f,30,monitor);
                    if(!res.decompileCompleted()||res.getDecompiledFunction()==null)return new String[]{"// DECOMPILE_FAILED "+cell(res.getErrorMessage())+"\n","1"};
                    return new String[]{res.getDecompiledFunction().getC()+"\n","0"};}));}
                try(BufferedWriter c=writer(out.resolve("c/"+shard+".c"))){long line=1;
                    for(int idx=0;idx<fl.size();idx++){Function f=fl.get(idx);monitor.checkCancelled();
                        String start=rv(f.getEntryPoint()),end=rv(f.getBody().getMaxAddress());long size=f.getBody().getNumAddresses();
                        fw.write(String.format("%s\t%s\t%d\t%s\tc/%s.c\t%d\t%d\t%s\t%s%n",start,end,size,cell(f.getName(true)),shard,line,f.getParameterCount(),cell(f.getSignature().getPrototypeString()),f.isThunk()));nf++;
                        c.write(String.format("// ==== FUNC %s %s size=%d end=%s\n",start,cell(f.getName(true)),size,end));line++;
                        String[] r;try{r=futs.get(idx).get();}catch(Exception e){r=new String[]{"// DECOMPILE_FAILED "+cell(String.valueOf(e))+"\n","1"};}
                        String body=r[0];
                        if(r[1].equals("1")){xw.write(start+"\t"+cell(body.replace("// DECOMPILE_FAILED ","").trim())+"\n");nx++;}
                        c.write(body);line+=body.chars().filter(ch->ch=='\n').count();
                        InstructionIterator ins=currentProgram.getListing().getInstructions(f.getBody(),true);
                        while(ins.hasNext()){Instruction in=ins.next();FlowType flow=in.getFlowType();boolean call=flow.isCall(),jump=flow.isJump();boolean emitted=false;
                            for(Reference ref:in.getReferencesFrom()){Address to=ref.getToAddress();RefType rt=ref.getReferenceType();if(rt.isFlow()){if(call||jump){String kind=call?(to.isMemoryAddress()?"direct":"indirect"):(currentProgram.getFunctionManager().getFunctionAt(to)!=null?"tailcall":"jump");cw.write(start+"\t"+rv(in.getAddress())+"\t"+(to.isMemoryAddress()?rv(to):"")+"\t"+kind+"\n");nc++;emitted=true;}}else if(to.isMemoryAddress() && currentProgram.getMemory().contains(to) && currentProgram.getListing().getInstructionAt(to)==null){String type=rt.isWrite()?"WRITE":rt.isRead()?"READ":"DATA";dw.write(rv(in.getAddress())+"\t"+start+"\t"+rv(to)+"\t"+type+"\n");nd++;}}
                            if(call&&!emitted){cw.write(start+"\t"+rv(in.getAddress())+"\t\tindirect\n");nc++;}
                            for(int op=0;op<in.getNumOperands();op++){for(Object o:in.getOpObjects(op)){if(o instanceof Scalar){Scalar s=(Scalar)o;long v=s.getUnsignedValue();gameHit(v,v>=0x140000000L?"imm_abs":"imm_rva",in.getAddress(),start);}}}
                        }
                    }
                }
                println(String.format("%s shard=%s functions=%d",Instant.now(),shard,fl.size()));
            }
        } finally {ex.shutdownNow();for(DecompInterface d:all)d.dispose();fw.close();cw.close();dw.close();xw.close();}
        // data pointers and RVA tables in non-executable blocks
        for(MemoryBlock block:currentProgram.getMemory().getBlocks()){
            if(!block.isInitialized()||block.isExecute()||block.getName().equals("Headers")||block.getSize()>200_000_000L)continue;
            byte[] buf=new byte[(int)block.getSize()];try{block.getBytes(block.getStart(),buf);}catch(Exception e){continue;}
            ByteBuffer bb=ByteBuffer.wrap(buf).order(ByteOrder.LITTLE_ENDIAN);long lo=block.getStart().getOffset();
            for(int off=(int)((4-(lo&3))&3);off+4<=buf.length;off+=4){long v=bb.getInt(off)&0xFFFFFFFFL;if(v>=0x2000L&&v<0x2B00000L&&gameStarts.contains(v))gameHit(v,"data_dword",block.getStart().add(off),"");}
            for(int off=(int)((8-(lo&7))&7);off+8<=buf.length;off+=8){long v=bb.getLong(off);if(v>=0x140001000L&&v<0x142B00000L&&gameStarts.contains(v-0x140000000L))gameHit(v,"data_qword",block.getStart().add(off),"");}
        }
        gw.close();
        move(out.resolve("functions.tsv.tmp"),out.resolve("functions.tsv"));move(out.resolve("calls.tsv.tmp"),out.resolve("calls.tsv"));move(out.resolve("datarefs.tsv.tmp"),out.resolve("datarefs.tsv"));move(out.resolve("failures.tsv.tmp"),out.resolve("failures.tsv"));move(out.resolve("game_rvas.tsv.tmp"),out.resolve("game_rvas.tsv"));
        counts.put("functions.tsv",nf);counts.put("calls.tsv",nc);counts.put("datarefs.tsv",nd);counts.put("failures.tsv",nx);counts.put("game_rvas.tsv",gameRows);
        exportGlobal();
        long ne=0;try(BufferedWriter e=writer(out.resolve("exports.tsv"))){e.write("name\trva\n");AddressIterator it=currentProgram.getSymbolTable().getExternalEntryPointIterator();while(it.hasNext()){Address ad=it.next();Symbol s=currentProgram.getSymbolTable().getPrimarySymbol(ad);e.write(cell(s==null?"":s.getName())+"\t"+rv(ad)+"\n");ne++;}}
        counts.put("exports.tsv",ne);
        manifest("complete");println("SAN14_REF_EXPORT_FINISHED");
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
