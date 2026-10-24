import ghidra.app.script.GhidraScript;
import ghidra.app.util.PseudoDisassembler;
import ghidra.app.util.PseudoInstruction;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.io.*;
public class BootRanges extends GhidraScript {
 public void run() throws Exception {
  String[] args=getScriptArgs();PrintWriter out=new PrintWriter(new File(args[0]),"UTF-8");
  PseudoDisassembler pd=new PseudoDisassembler(currentProgram);
  for(int k=1;k<args.length;k++){
   String[] p=args[k].split(":");Address a=toAddr(p[0]);int count=Integer.parseInt(p[1]);out.println("RANGE "+a);
   for(int i=0;i<count;i++,a=a.add(4)){
    try {PseudoInstruction in=pd.disassemble(a);if(in==null){out.println(a+" UNKNOWN");continue;}String line=a+" "+in;
     for(Address t:in.getFlows()){Symbol s=currentProgram.getSymbolTable().getPrimarySymbol(t);if(s!=null)line+=" ; "+s.getName(true);}
     out.println(line);
    }catch(Exception e){out.println(a+" "+e);}
   }
  }out.close();
 }
}
