import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class BootCallSites extends GhidraScript {
 public void run() throws Exception {
  PrintWriter out=new PrintWriter(new File(getScriptArgs()[0]),"UTF-8");
  Memory mem=currentProgram.getMemory();MemoryBlock text=mem.getBlock(".text");
  byte[] bytes=new byte[(int)text.getSize()];mem.getBytes(text.getStart(),bytes);
  long base=text.getStart().getOffset();
  for(int i=0;i+3<bytes.length;i+=4) {
   int word=(bytes[i]&255)|((bytes[i+1]&255)<<8)|((bytes[i+2]&255)<<16)|(bytes[i+3]<<24);
   if((word&0xfc000000)!=0x94000000 && (word&0xfc000000)!=0x14000000)continue;
   long target=base+i+(((long)(word<<6)>>6)<<2);
   if(target<0x7102fc3900L || target>=0x7102fc7250L)continue;
   Symbol sym=currentProgram.getSymbolTable().getPrimarySymbol(toAddr(target));if(sym==null)continue;
   String n=sym.getName(true);
   if(n.contains("_ZN2nn2fs") || n.contains("_ZN2nn3hid") || n.contains("QueryMemoryInfo") || n.contains("SetMemoryAllocatorForThreadLocal"))
     out.println(String.format("%010x %s %010x %s",base+i,(word&0x80000000)!=0?"BL":"B",target,n));
  }
  FunctionIterator fs=currentProgram.getFunctionManager().getFunctions(true);
  while(fs.hasNext()){Function f=fs.next();if(f.getEntryPoint().getOffset()<0x7102fc3900L && !f.isExternal())out.println("INTERNAL "+f.getEntryPoint()+" "+f.getName(true));}
  out.close();
 }
}
