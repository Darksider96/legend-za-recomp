import sys
sys.path.insert(0,'analysis/boot')
from image_helpers import *
import json
slots=json.loads((ROOT/'phase3-window-nvn-slots.json').read_text())
wanted={int(s['slot'],16)-BASE:s['name'] for s in slots if any(n in s['name'] for n in ['nvnWindow','nvnQueuePresentTexture'])}
got={}
for off in range(0,len(DATA)-7,8):
 v=struct.unpack_from('<Q',DATA,off)[0]-BASE
 if v in wanted:got[0x39e1000+off]=wanted[v]
targets={**wanted,**got};pages={a&~4095 for a in targets};refs=[]
for i,(w,) in enumerate(struct.iter_unpack('<I',TEXT)):
 pc=i*4
 if w&0x9f000000!=0x90000000:continue
 imm=((w>>5)&0x7ffff)<<2|((w>>29)&3)
 if imm&(1<<20):imm-=1<<21
 page=(pc&~4095)+(imm<<12);reg=w&31
 if page not in pages:continue
 for q in range(pc+4,min(pc+48,len(TEXT)),4):
  x=struct.unpack_from('<I',TEXT,q)[0];addr=None
  if x&0xffc00000==0xf9400000 and (x>>5)&31==reg:addr=page+(((x>>10)&4095)*8)
  if x&0xffc00000==0x91000000 and (x>>5)&31==reg:addr=page+((x>>10)&4095)
  if addr in targets:
   s,size,valid=function(q);refs.append({'name':targets[addr],'site':hex(BASE+q),'refaddr':hex(BASE+addr),'function':hex(BASE+s),'size':hex(size),'fde_valid':valid})
  if x&31==reg and x&0xffc00000==0xf9400000:break
(ROOT/'phase3-window-all-refs.json').write_text(json.dumps(refs,indent=2))
for r in refs:
 if r['function']!='0x7101775db0':print(r)
