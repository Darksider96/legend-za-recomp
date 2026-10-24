import ghidra.app.script.GhidraScript;
import java.io.*;
public class BootExport extends GhidraScript {
 public void run() throws Exception {
  String dir=getScriptArgs()[0];
  long[][] ranges={{0x7100000000L,0x2fc7250L},{0x7102fc8000L,0xa19000L},{0x71039e1000L,0x39e1c0L}};
  String[] names={"text.bin","rodata.bin","data.bin"};
  for(int i=0;i<3;i++){byte[] b=new byte[(int)ranges[i][1]];int got=currentProgram.getMemory().getBytes(toAddr(ranges[i][0]),b);if(got!=b.length)throw new IOException("Incomplete "+got);try(FileOutputStream fs=new FileOutputStream(new File(dir,names[i]))){fs.write(b);}}
 }
}
