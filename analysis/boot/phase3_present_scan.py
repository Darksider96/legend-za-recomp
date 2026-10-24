from image_helpers import *
import json
slots=json.loads((ROOT/'phase2-nvn-slots.json').read_text())
wanted={int(s['slot'],16)-BASE:s['name'] for s in slots if any(n in s['name'] for n in ['nvnQueuePresentTexture','nvnQueueSubmitCommands','nvnQueueFlush','nvnQueueFinish'])}
got={}
for off in range(0,len(DATA)-7,8):
 v=struct.unpack_from('<Q',DATA,off)[0]-BASE
 if v in wanted:got[0x39e1000+off]=wanted[v]
pages={a&~4095 for a in got};refs=[]
for i,(w,) in enumerate(struct.iter_unpack('<I',TEXT)):
 pc=i*4
 if w&0x9f000000!=0x90000000:continue
 imm=((w>>5)&0x7ffff)<<2|((w>>29)&3)
 if imm&(1<<20):imm-=1<<21
 page=(pc&~4095)+(imm<<12);reg=w&31
 if page not in pages:continue
 for q in range(pc+4,min(pc+48,len(TEXT)),4):
  x=struct.unpack_from('<I',TEXT,q)[0]
  if x&0xffc00000==0xf9400000 and (x>>5)&31==reg:
   a=page+(((x>>10)&4095)*8)
   if a in got:
    s,size,valid=function(q);refs.append({'name':got[a],'site':hex(BASE+q),'got':hex(BASE+a),'function':hex(BASE+s),'size':hex(size),'fde_valid':valid})
  if x&31==reg and x&0xffc00000==0xf9400000:break
result={'slots':{hex(BASE+k):v for k,v in wanted.items()},'got':{hex(BASE+k):v for k,v in got.items()},'refs':refs}
(ROOT/'phase3-present-refs.json').write_text(json.dumps(result,indent=2))
print(json.dumps(result,indent=2))
