from image_helpers import *
import json,sys
targets={int(x,16)-BASE for x in sys.argv[1:]};pages={t&~4095 for t in targets};refs=[]
for i,(w,) in enumerate(struct.iter_unpack('<I',TEXT)):
 pc=i*4
 if w&0x9f000000==0x10000000:
  imm=((w>>5)&0x7ffff)<<2|((w>>29)&3)
  if imm&(1<<20):imm-=1<<21
  dest=pc+imm
  if dest in targets:
   f,size,v=function(pc);refs.append({'site':hex(BASE+pc),'target':hex(BASE+dest),'kind':'ADR','function':hex(BASE+f),'size':hex(size),'fde_valid':v})
 if w&0x9f000000!=0x90000000:continue
 imm=((w>>5)&0x7ffff)<<2|((w>>29)&3)
 if imm&(1<<20):imm-=1<<21
 page=(pc&~4095)+(imm<<12);reg=w&31
 if page not in pages:continue
 for q in range(pc+4,min(pc+52,len(TEXT)),4):
  x=struct.unpack_from('<I',TEXT,q)[0]
  if x&0xffc00000==0x91000000 and (x>>5)&31==reg:
   a=page+((x>>10)&4095)
   if a in targets:
    f,size,v=function(q);refs.append({'site':hex(BASE+q),'target':hex(BASE+a),'kind':'ADRP+ADD','function':hex(BASE+f),'size':hex(size),'fde_valid':v})
  if x&31==reg and x&0xffc00000 in (0x91000000,0xf9400000):break
print(json.dumps(refs,indent=2))
