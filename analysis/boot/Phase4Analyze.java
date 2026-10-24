// Static, headless evidence collector for the TRPFS/TRPFD and HID investigation.
// Usage: -postScript Phase4Analyze.java OUTPUT_DIR VA:HEX_SIZE [VA:HEX_SIZE ...]
// Use a copied project with -readOnly -noanalysis. Does not run the target.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.app.util.PseudoDisassembler;
import ghidra.app.util.PseudoInstruction;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class Phase4Analyze extends GhidraScript {
    boolean relevant(String n) {
        return n.contains("Npad") || n.contains("2fs8OpenFile") ||
               n.contains("2fs8ReadFile") || n.contains("2fs11GetFileSize");
    }
    public void run() throws Exception {
        String[] args=getScriptArgs();
        if(args.length<2) throw new IllegalArgumentException("OUTPUT_DIR VA:HEX_SIZE [...]");
        File dir=new File(args[0]); dir.mkdirs();
        int firstSelector=1;
        if(args.length>1 && args[1].startsWith("sha256:")) {
            String expected=args[1].substring(7);
            if(!expected.equalsIgnoreCase(currentProgram.getExecutableSHA256()))
                throw new IllegalArgumentException("Program SHA256 mismatch: "+currentProgram.getExecutableSHA256());
            firstSelector=2;
        }
        File completed=new File(dir,"COMPLETE.txt");
        if(completed.exists()) throw new IOException("Output already completed; select a new output directory");
        Map<Long,String> imports=new TreeMap<>();
        try(PrintWriter out=new PrintWriter(new File(dir,"identity_and_imports.txt"),"UTF-8")) {
            out.println("STATIC ONLY; no target execution. Program: "+currentProgram.getName());
            out.println("Input: "+currentProgram.getExecutablePath());
            out.println("SHA256 metadata: "+currentProgram.getExecutableSHA256());
            out.println("Base: "+currentProgram.getImageBase()+"; language: "+currentProgram.getLanguageID());
            SymbolIterator syms=currentProgram.getSymbolTable().getAllSymbols(true);
            while(syms.hasNext()) {
                Symbol s=syms.next(); String n=s.getName(true);
                if(relevant(n)) {
                    out.println(s.getAddress()+" "+n);
                    if(s.getAddress().isMemoryAddress()) imports.put(s.getAddress().getOffset(),n);
                }
            }
        }
        // Raw B/BL scan also finds callers in regions not yet analyzed as code.
        try(PrintWriter out=new PrintWriter(new File(dir,"import_direct_calls.txt"),"UTF-8")) {
            for(MemoryBlock b:currentProgram.getMemory().getBlocks()) {
                if(!b.isInitialized() || !b.isExecute()) continue;
                if(b.getSize()>Integer.MAX_VALUE) throw new IOException("Executable block too large");
                byte[] bytes=new byte[(int)b.getSize()];
                int count=currentProgram.getMemory().getBytes(b.getStart(),bytes);
                out.println("BLOCK "+b.getName()+" read="+count+" expected="+bytes.length);
                for(int p=0;p+4<=count;p+=4) {
                    if((p&0xffff)==0) monitor.checkCancelled();
                    int w=(bytes[p]&255)|((bytes[p+1]&255)<<8)|((bytes[p+2]&255)<<16)|((bytes[p+3]&255)<<24);
                    if((w&0x7c000000)!=0x14000000) continue;
                    long pc=b.getStart().getOffset()+p;
                    long dest=pc+(((long)(w<<6)>>6)<<2);
                    if(imports.containsKey(dest)) out.printf("%012x %s -> %012x %s%n",pc,(w&0x80000000)!=0?"BL":"B",dest,imports.get(dest));
                }
            }
        }
        DecompInterface decompiler=new DecompInterface();
        decompiler.openProgram(currentProgram);
        PseudoDisassembler pd=new PseudoDisassembler(currentProgram);
        try {
            for(int k=firstSelector;k<args.length;k++) {
                monitor.checkCancelled();
                String[] spec=args[k].split(":");
                if(spec.length!=2) throw new IllegalArgumentException("Expected VA:HEX_SIZE: "+args[k]);
                Address start=toAddr(spec[0]); long size=Long.parseLong(spec[1],16);
                if(size<=0 || size>0x20000 || size%4!=0) throw new IllegalArgumentException("Bad bound: "+args[k]);
                AddressSet body=new AddressSet(start,start.add(size-1));
                try(PrintWriter out=new PrintWriter(new File(dir,spec[0].toLowerCase()+".txt"),"UTF-8")) {
                    out.println("FUNCTION "+start+" explicit size=0x"+Long.toHexString(size));
                    out.println("Caller must supply verified bounds; pseudocode is advisory. Raw instruction range below is authoritative.");
                    Function f=getFunctionAt(start);
                    if(f==null) { disassemble(start); f=createFunction(start,null); }
                    if(f!=null) {
                        try { f.setBody(body); }
                        catch(Exception e) { out.println("BODY WARNING "+e); }
                        DecompileResults result=decompiler.decompileFunction(f,15,monitor);
                        if(result.decompileCompleted()) out.println(result.getDecompiledFunction().getC());
                        else out.println("DECOMPILER UNAVAILABLE "+result.getErrorMessage());
                    }
                    out.println("RAW INSTRUCTIONS (explicit bounds)");
                    for(long offset=0;offset<size;offset+=4) {
                        Address a=start.add(offset);
                        try {
                            PseudoInstruction ins=pd.disassemble(a);
                            String line=a+" "+(ins==null?"UNKNOWN":ins.toString());
                            if(ins!=null) for(Address dest:ins.getFlows()) {
                                Symbol s=currentProgram.getSymbolTable().getPrimarySymbol(dest);
                                if(s!=null) line+=" ; "+s.getName(true);
                            }
                            out.println(line);
                        } catch(Exception e) { out.println(a+" DISASSEMBLY ERROR "+e); }
                    }
                }
            }
        } finally { decompiler.dispose(); }
        try(PrintWriter out=new PrintWriter(completed,"UTF-8")) {
            out.println("Evidence collection completed. Program SHA256="+currentProgram.getExecutableSHA256());
            out.println("Explicit function ranges="+(args.length-firstSelector)+"; static evidence only.");
        }
    }
}
