from pathlib import Path
import struct, bisect

ROOT=Path(__file__).parent
BASE=0x7100000000
TEXT=(ROOT/'text.bin').read_bytes()
RO=(ROOT/'rodata.bin').read_bytes()
DATA=(ROOT/'data.bin').read_bytes()
HDR=0x323222c
H=HDR-0x2fc8000
COUNT=struct.unpack_from('<I',RO,H+8)[0]
ROWS=[(HDR+a,HDR+b) for a,b in struct.iter_unpack('<ii',RO[H+12:H+12+COUNT*8])]
STARTS=[a for a,b in ROWS]

def mem(rva,size):
    for start,data in [(0,TEXT),(0x2fc8000,RO),(0x39e1000,DATA)]:
        if start<=rva and rva+size<=start+len(data): return data[rva-start:rva-start+size]
    raise ValueError(hex(rva))

def function(rva):
    idx=bisect.bisect_right(STARTS,rva)-1
    start,fde=ROWS[idx]
    raw=mem(fde,20)
    pcdisp,size=struct.unpack_from('<iI',raw,8)
    pc=fde+8+pcdisp
    return start,size,pc==start and start<=rva<start+size

def edges():
    for i,(w,) in enumerate(struct.iter_unpack('<I',TEXT)):
        if w&0xfc000000 in (0x94000000,0x14000000):
            disp=w&0x3ffffff
            if disp&(1<<25):disp-=1<<26
            yield i*4,i*4+disp*4,'BL' if w&0x80000000 else 'B'

if __name__=='__main__':
    targets=[0x19dca1c,0x193a670,0x118310c,0x050cbc4,0xb10dd8,0x19dd2e8]
    for t in targets:print('FUNCTION',hex(BASE+t),tuple(hex(x) if isinstance(x,int) and not isinstance(x,bool) else x for x in function(t)))
    wanted={function(t)[0] for t in targets}
    for a,t,k in edges():
        if t in wanted: print(hex(BASE+a),k,'->',hex(BASE+t),'CALLER',hex(BASE+function(a)[0]),function(a)[2])
