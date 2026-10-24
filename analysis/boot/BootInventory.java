import ghidra.app.script.GhidraScript;
import ghidra.program.model.symbol.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import java.io.*;

public class BootInventory extends GhidraScript {
  public void run() throws Exception {
    PrintWriter out = new PrintWriter(new File(getScriptArgs()[0]), "UTF-8");
    out.println("PROGRAM " + currentProgram.getName());
    out.println("PATH " + currentProgram.getExecutablePath());
    out.println("SHA256 " + currentProgram.getExecutableSHA256());
    out.println("FORMAT " + currentProgram.getExecutableFormat());
    out.println("LANG " + currentProgram.getLanguageID());
    out.println("BASE " + currentProgram.getImageBase());
    out.println("FUNCTIONS " + currentProgram.getFunctionManager().getFunctionCount());
    for (MemoryBlock b: currentProgram.getMemory().getBlocks()) out.println("BLOCK " + b.getName()+" "+b.getStart()+" "+b.getEnd());
    SymbolIterator it = currentProgram.getSymbolTable().getAllSymbols(true);
    while(it.hasNext()) {
      Symbol s=it.next(); String n=s.getName(true);
      if(n.contains("nnMain") || n.contains("nn::os::") || n.contains("nn::fs::") || n.contains("nn::hid::") || n.contains("_ZN2nn2os") || n.contains("_ZN2nn2fs") || n.contains("_ZN2nn3hid") || n.contains("nninit") || n.equals("entry"))
        out.println("SYMBOL " + s.getAddress()+" "+s.getSymbolType()+" "+n);
    }
    out.close();
  }
}
