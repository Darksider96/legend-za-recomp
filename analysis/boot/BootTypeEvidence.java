import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.*;
import ghidra.program.model.data.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class BootTypeEvidence extends GhidraScript {
 public void run() throws Exception {
  PrintWriter out=new PrintWriter(new File(getScriptArgs()[0]),"UTF-8");
  out.println("PROGRAM "+currentProgram.getName()+" SHA256 "+currentProgram.getExecutableSHA256());
  Iterator<DataType> types=currentProgram.getDataTypeManager().getAllDataTypes();
  while(types.hasNext()) {DataType t=types.next();String n=t.getName();
   if(n.contains("LightEvent")||n.contains("EventClearMode")||n.contains("MutexType")||n.contains("ReaderWriterLock")||n.equals("Result"))out.println("DATATYPE "+t.getPathName()+" size="+t.getLength()+" "+t);
  }
  String[] needles={"InitializeLightEvent","FinalizeLightEvent","WaitLightEvent","SignalLightEvent","ClearLightEvent","InitializeMutex","FinalizeMutex","LockMutex","UnlockMutex","InitializeReaderWriterLock","QueryMountRomCacheSize","MountRom"};
  if(getScriptArgs().length>1) needles=Arrays.copyOfRange(getScriptArgs(),1,getScriptArgs().length);
  DecompInterface di=new DecompInterface();di.openProgram(currentProgram);
  SymbolIterator syms=currentProgram.getSymbolTable().getAllSymbols(true);
  ArrayList<Symbol> targets=new ArrayList<>();
  while(syms.hasNext()){Symbol s=syms.next();if(s.getAddress().isExternalAddress()||s.getAddress().getOffset()>=0x7104000000L)continue;
   String n=s.getName();for(String needle:needles)if(n.contains(needle) && n.startsWith("_ZN2nn")){targets.add(s);break;}
  }
  for(Symbol s:targets){
   Address a=s.getAddress();out.println("\nSYMBOL "+a+" "+s.getName(true));out.flush();
   new DisassembleCommand(a,null,true).applyTo(currentProgram,monitor);
   Function f=getFunctionAt(a);if(f==null)f=createFunction(a,null);
   if(f==null){out.println("NO FUNCTION");continue;}
   ghidra.app.cmd.function.CreateFunctionCmd.fixupFunctionBody(currentProgram,f,monitor);
   out.println("BODY "+f.getBody());
   DecompileResults dr=di.decompileFunction(f,30,monitor);
   if(dr.decompileCompleted())out.println(dr.getDecompiledFunction().getC());else out.println("DECOMPILE "+dr.getErrorMessage());
   InstructionIterator ii=currentProgram.getListing().getInstructions(f.getBody(),true);int max=1800;
   while(ii.hasNext()&&max-->0){Instruction ins=ii.next();StringBuilder b=new StringBuilder(ins.getAddress()+" "+ins);
    for(Reference r:ins.getReferencesFrom())if(r.getReferenceType().isCall()||r.getReferenceType().isJump()){Symbol dst=currentProgram.getSymbolTable().getPrimarySymbol(r.getToAddress());b.append(" ; "+r.getToAddress()+" "+(dst==null?"":dst.getName(true)));}
    out.println(b);
   }out.flush();
  }di.dispose();out.close();
 }
}
