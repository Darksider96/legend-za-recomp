import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.io.*;
public class BootPointers extends GhidraScript {
 public void run() throws Exception {
  String[] args=getScriptArgs();PrintWriter out=new PrintWriter(new File(args[0]),"UTF-8");
  for(int j=1;j<args.length;j++){
   String[] p=args[j].split(":");Address a=toAddr(p[0]);int count=p.length>1?Integer.parseInt(p[1]):1;
   for(int i=0;i<count;i++){
    Address slot=a.add(i*8);long v=getLong(slot);Address target=toAddr(v);Symbol s=currentProgram.getSymbolTable().getPrimarySymbol(target);
    out.printf("%s -> %016x %s\n",slot,v,s==null?"":s.getName(true));
    if(p.length==1 && currentProgram.getMemory().contains(target)){
     for(int k=0;k<36;k++){try{Address t=target.add(k*8);long z=getLong(t);Symbol zs=currentProgram.getSymbolTable().getPrimarySymbol(toAddr(z));out.printf("  %s +%03x -> %016x %s\n",t,k*8,z,zs==null?"":zs.getName(true));}catch(Exception e){break;}}
    }
   }
  }out.close();
 }
}
