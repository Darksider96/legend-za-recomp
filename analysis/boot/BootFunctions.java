import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.*;

public class BootFunctions extends GhidraScript {
  public void run() throws Exception {
    String[] args=getScriptArgs();
    PrintWriter out=new PrintWriter(new File(args[0]),"UTF-8");
    DecompInterface di=new DecompInterface(); di.openProgram(currentProgram);
    for(int k=1;k<args.length;k++) {
      String[] selector=args[k].split(":");
      Address a=toAddr(selector[0]); disassemble(a); Function f=getFunctionAt(a);
      if(f==null) { disassemble(a); f=createFunction(a,null); }
      if(f==null) { out.println("MISSING "+a); continue; }
      CreateFunctionCmd.fixupFunctionBody(currentProgram,f,monitor);
      if(selector.length>1) f.setBody(new AddressSet(a,a.add(Long.parseLong(selector[1],16)-1)));
      out.println("\nFUNCTION "+f.getEntryPoint()+" "+f.getName(true)+" BODY "+f.getBody());
      ReferenceIterator incoming=currentProgram.getReferenceManager().getReferencesTo(a);
      while(incoming.hasNext()) {Reference r=incoming.next();out.println("XREF "+r.getFromAddress()+" "+r.getReferenceType()+" "+getFunctionContaining(r.getFromAddress()));}
      DecompileResults dr=di.decompileFunction(f,40,monitor);
      if(dr.decompileCompleted()) out.println(dr.getDecompiledFunction().getC()); else out.println("DECOMP ERROR "+dr.getErrorMessage());
      InstructionIterator ii=currentProgram.getListing().getInstructions(f.getBody(),true);
      while(ii.hasNext()) {
        Instruction ins=ii.next(); StringBuilder b=new StringBuilder(ins.getAddress()+" "+ins);
        for(Reference r:ins.getReferencesFrom()) {
          Symbol s=currentProgram.getSymbolTable().getPrimarySymbol(r.getToAddress());
          b.append(" ; "+r.getReferenceType()+" "+r.getToAddress()+" "+(s==null?"":s.getName(true)));
          Data d=getDataAt(r.getToAddress()); if(d!=null && d.hasStringValue())b.append(" STRING "+d.getDefaultValueRepresentation());
        }
        out.println(b);
      }
      out.flush();
    }
    di.dispose();out.close();
  }
}
