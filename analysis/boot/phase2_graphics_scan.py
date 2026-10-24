from image_helpers import *
import re,json

names={}
for m in re.finditer(rb'nvn[A-Za-z0-9_]+\0',RO):
    names[0x2fc8000+m.start()]=m.group()[:-1].decode()
wanted={a:n for a,n in names.items() if n in {
 'nvnDeviceInitialize','nvnQueueInitialize','nvnDeviceGetProcAddress',
 'nvnDeviceBuilderSetDefaults','nvnDeviceBuilderSetFlags','nvnQueueBuilderSetDefaults',
 'nvnQueueBuilderSetDevice','nvnQueueBuilderSetFlags','nvnQueueBuilderSetCommandMemorySize',
 'nvnQueueBuilderSetComputeMemorySize','nvnQueueBuilderSetControlMemorySize',
 'nvnQueueBuilderSetQueueMemory','nvnQueueBuilderSetQueuePriority',
 'nvnQueueBuilderSetCommandFlushThreshold'}}
records=[]
for i,(w,) in enumerate(struct.iter_unpack('<I',TEXT)):
    pc=i*4
    if w&0x9f000000==0x90000000:
        imm=((w>>5)&0x7ffff)<<2|((w>>29)&3)
        if imm&(1<<20):imm-=1<<21
        page=(pc&~4095)+(imm<<12);reg=w&31
        if not any(a&~4095==page for a in wanted):continue
        for q in range(pc+4,min(pc+84,len(TEXT)),4):
            x=struct.unpack_from('<I',TEXT,q)[0]
            if x&0xffc00000==0x91000000 and (x>>5)&31==reg:
                addr=page+((x>>10)&4095)
                if addr in wanted:
                    s,size,valid=function(q)
                    records.append({'site':hex(BASE+q),'adrp':hex(BASE+pc),'function':hex(BASE+s),'size':hex(size),'fde_valid':valid,'string':hex(BASE+addr),'name':wanted[addr]})
            if (x&31)==reg and x&0xffc00000==0x91000000:break
(ROOT/'phase2-nvn-string-refs.json').write_text(json.dumps(records,indent=2))
print(json.dumps(records,indent=2))
