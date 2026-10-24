from image_helpers import *
import re,json

regs={};pending=None;slots=[]
lines=(ROOT/'phase3-window-loader.txt').read_text(encoding='utf-8')
for line in lines.splitlines():
    m=re.match(r'([0-9a-f]+) (.+)',line)
    if not m:continue
    pc=int(m[1],16);ins=m[2]
    m=re.fullmatch(r'adrp x(\d+),0x([0-9a-f]+)',ins)
    if m:regs[int(m[1])]=int(m[2],16);continue
    m=re.fullmatch(r'add x(\d+),x(\d+),#0x([0-9a-f]+)',ins)
    if m and int(m[2]) in regs:regs[int(m[1])]=regs[int(m[2])]+int(m[3],16);continue
    m=re.fullmatch(r'ldr x(\d+),\[x(\d+)(?:, #0x([0-9a-f]+))?\]',ins)
    if m and int(m[2]) in regs:
        addr=regs[int(m[2])]+int(m[3] or '0',16)
        try:regs[int(m[1])]=struct.unpack('<Q',mem(addr-BASE,8))[0]
        except ValueError:regs.pop(int(m[1]),None)
        continue
    if ins=='blr x19' and 1 in regs:
        try:name=mem(regs[1]-BASE,100).split(b'\0')[0].decode();pending=(name,pc)
        except Exception:pending=None
    if ins=='str x0,[x8]' and pending and 8 in regs:
        name,call=pending;slots.append({'name':name,'resolver_call':hex(call),'store':hex(pc),'slot':hex(regs[8])});pending=None

(ROOT/'phase3-window-nvn-slots.json').write_text(json.dumps(slots,indent=2))
wanted={int(s['slot'],16)-BASE:s['name'] for s in slots if any(n in s['name'] for n in ['nvnWindow','nvnQueuePresentTexture'])}
got={}
for off in range(0,len(DATA)-7,8):
    value=struct.unpack_from('<Q',DATA,off)[0]-BASE
    if value in wanted:got[0x39e1000+off]=wanted[value]
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
            addr=page+(((x>>10)&4095)*8)
            if addr in got:
                s,size,valid=function(q);refs.append({'name':got[addr],'site':hex(BASE+q),'got':hex(BASE+addr),'function':hex(BASE+s),'size':hex(size),'fde_valid':valid})
        if x&31==reg and x&0xffc00000==0xf9400000:break
(ROOT/'phase3-window-nvn-slot-refs.json').write_text(json.dumps(refs,indent=2))
print(json.dumps({'wanted_slots':{hex(BASE+k):v for k,v in wanted.items()},'refs':refs},indent=2))
