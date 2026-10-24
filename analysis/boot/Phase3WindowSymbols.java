import ghidra.app.script.GhidraScript;
import ghidra.program.model.symbol.*;
import java.io.*;
public class Phase3WindowSymbols extends GhidraScript {
 public void run() throws Exception {
  PrintWriter out=new PrintWriter(new File(getScriptArgs()[0]),"UTF-8");
  SymbolIterator it=currentProgram.getSymbolTable().getAllSymbols(true);
  while(it.hasNext()){Symbol s=it.next();String n=s.getName(true);if(n.contains("_ZN2nn2vi")||n.contains("Window")||n.contains("Display")||n.contains("Layer"))out.println(s.getAddress()+" "+s.getSymbolType()+" "+n);}
  out.close();
 }
}
