"""Bounded, read-only parser for supplied TRPFD and TRPFS footer artifacts.

No game execution; reads the 7 MiB directory and bounded header/footer/member prefixes.
Field names are descriptive working names, not recovered source identifiers.
"""
from pathlib import Path
import struct, hashlib, collections, json

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).parent

class FB:
    def __init__(self, b): self.b = b
    def u16(self,p): return struct.unpack_from('<H',self.b,p)[0]
    def u32(self,p): return struct.unpack_from('<I',self.b,p)[0]
    def i32(self,p): return struct.unpack_from('<i',self.b,p)[0]
    def u64(self,p): return struct.unpack_from('<Q',self.b,p)[0]
    def ref(self,p): return p+self.u32(p)
    def fields(self,t):
        v=t-self.i32(t); n=self.u16(v)
        assert 4 <= n <= 64 and n % 2 == 0
        return {'vtable':v,'object_size':self.u16(v+2),'offsets':[self.u16(j) for j in range(v+4,v+n,2)]}
    def field(self,t,i):
        fs=self.fields(t)['offsets']
        return t+fs[i] if i<len(fs) and fs[i] else None
    def vector(self,t,i):
        p=self.ref(self.field(t,i)); return p,self.u32(p)
    def elems(self,v):
        p,n=v; return [self.ref(p+4+i*4) for i in range(n)]
    def string(self,t):
        n=self.u32(t); assert t+4+n<=len(self.b)
        return self.b[t+4:t+4+n].decode('utf8')
    def layout(self,t):
        f=self.fields(t)
        return dict(address=hex(t),vtable=hex(f['vtable']),object_size=f['object_size'],offsets=f['offsets'],raw=self.b[t:t+f['object_size']].hex())

def fnv1a(s):
    # Seed recovered in executable 0x7101948F10; differs from canonical FNV-1a.
    h=0xcbf29ce484222645
    for c in s: h=((h^c)*0x100000001b3)&0xffffffffffffffff
    return h

directory=ROOT/'romfs/arc/data.trpfd'
pack=ROOT/'romfs/arc/data.trpfs'
db=directory.read_bytes(); d=FB(db); dr=d.u32(0)
vectors=[d.vector(dr,i) for i in range(4)]
hashes=[d.u64(vectors[0][0]+4+i*8) for i in range(vectors[0][1])]
names=[d.string(t) for t in d.elems(vectors[1])]
file_tables=d.elems(vectors[2]); pack_tables=d.elems(vectors[3])
with pack.open('rb') as f:
    header=f.read(16); footer_start=struct.unpack_from('<Q',header,8)[0]
    f.seek(footer_start); footer=f.read(pack.stat().st_size-footer_start)
    p=FB(footer); pr=p.u32(0); pvs=[p.vector(pr,i) for i in range(2)]
    pack_hashes=[p.u64(pvs[0][0]+4+i*8) for i in range(pvs[0][1])]
    pack_offsets=[p.u64(pvs[1][0]+4+i*8) for i in range(pvs[1][1])]
    prefixes=[]
    for i in [0,1,2,len(pack_offsets)-1]:
        f.seek(pack_offsets[i]); prefix=f.read(96)
        prefixes.append(dict(index=i,offset=hex(pack_offsets[i]),length=(pack_offsets[i+1] if i+1<len(pack_offsets) else footer_start)-pack_offsets[i],prefix_hex=prefix.hex()))

def summarize_tables(fb,tables):
    layouts=collections.Counter()
    for t in tables:
        l=fb.fields(t); layouts[(l['object_size'],tuple(l['offsets']))]+=1
    return {'layouts':[{'object_size':k[0],'offsets':k[1],'count':n} for k,n in layouts.items()], 'samples':[fb.layout(tables[i]) for i in sorted({0,1,2,len(tables)-1})]}

lookup={h:i for i,h in enumerate(pack_hashes)}
name_matches=[]
for i,n in enumerate(names):
    h=fnv1a(n.encode())
    if h in lookup: name_matches.append((i,lookup[h]))

file_pack_indices=[d.u64(d.field(t,0)) if d.field(t,0) else 0 for t in file_tables]
file_extras=[d.ref(d.field(t,1)) for t in file_tables]
pack_lengths=[d.u64(d.field(t,0)) for t in pack_tables]
pack_counts=[d.u64(d.field(t,1)) for t in pack_tables]
index_counts=collections.Counter(file_pack_indices)
padding=collections.Counter()
expected=collections.defaultdict(list)
for h,i in zip(hashes,file_pack_indices): expected[i].append(h)
inner_checks=[]
with pack.open('rb') as f:
    for i,j in name_matches:
        start=pack_offsets[j]
        span=(pack_offsets[j+1] if j+1<len(pack_offsets) else footer_start)-start
        padding[span-pack_lengths[i]]+=1
        # Each member is FlatBuffers. Read only a prefix and the hash vector,
        # never compressed asset payloads or the complete 3.3 GB carrier.
        f.seek(start); prefix=f.read(min(pack_lengths[i],256)); m=FB(prefix)
        mr=m.u32(0); hv=m.ref(m.field(mr,0))
        assert 0 <= hv <= pack_lengths[i]-4
        f.seek(start+hv); n=struct.unpack('<I',f.read(4))[0]
        assert hv+4+n*8<=pack_lengths[i]
        inner=list(struct.unpack('<'+str(n)+'Q',f.read(n*8)))
        inner_checks.append(dict(name_index=i,footer_index=j,hashes=n,
            matches_trpfd=inner==expected[i],count_matches_metadata=n==pack_counts[i]))

result={
 'trpfd':{'path':str(directory),'size':len(db),'sha256':hashlib.sha256(db).hexdigest(),'root':hex(dr),'root_layout':d.layout(dr),'vectors':[{'field':i,'offset':hex(v),'count':n} for i,(v,n) in enumerate(vectors)],'hashes_sorted_unsigned':all(a<b for a,b in zip(hashes,hashes[1:])),'file_tables':summarize_tables(d,file_tables),'pack_tables':summarize_tables(d,pack_tables),'names_samples': [{'index':i,'name':names[i],'fnv1a64':hex(fnv1a(names[i].encode()))} for i in [0,1,2,len(names)-1]]},
 'trpfs':{'path':str(pack),'size':pack.stat().st_size,'header_hex':header.hex(),'footer_offset':hex(footer_start),'footer_size':len(footer),'footer_sha256':hashlib.sha256(footer).hexdigest(),'root_layout':p.layout(pr),'vectors':[{'field':i,'offset_in_footer':hex(v),'count':n} for i,(v,n) in enumerate(pvs)],'offsets_strictly_increasing':all(a<b for a,b in zip(pack_offsets,pack_offsets[1:])),'hashes_sorted_unsigned':all(a<b for a,b in zip(pack_hashes,pack_hashes[1:])),'member_prefixes':prefixes},
 'name_fnv_correlations':{'seed':'0xcbf29ce484222645','prime':'0x100000001b3','names':len(names),'matches':len(name_matches),'same_index_matches':sum(i==j for i,j in name_matches),'samples':name_matches[:10]},
 'trpfd_semantic_checks':{'file_pack_index_range':[min(file_pack_indices),max(file_pack_indices)],'file_entry_field1_targets':summarize_tables(d,file_extras),'pack_file_count_sum':sum(pack_counts),'pack_file_counts_match_index_references':sum(index_counts[i]==pack_counts[i] for i in range(len(names))),'pack_length_range':[min(pack_lengths),max(pack_lengths)],'trpfs_span_minus_trpfd_length_histogram':dict(padding)},
 'inner_trpak_hash_checks':{'members_checked':len(inner_checks),'hashes_checked':sum(x['hashes'] for x in inner_checks),'matching_trpfd_members':sum(x['matches_trpfd'] for x in inner_checks),'matching_trpfd_counts':sum(x['count_matches_metadata'] for x in inner_checks),'samples':inner_checks[:3]+inner_checks[-1:]}
}
(OUT/'phase4-archive-data.json').write_text(json.dumps(result,indent=2),encoding='utf8')
if __name__ == '__main__': print(json.dumps(result,indent=2))
