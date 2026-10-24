"""Extract and inventory .trpokecfg assets with the existing project tool (no game execution)."""
from pathlib import Path
import collections, hashlib, json, math, struct, subprocess

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'analysis'/'boot'/'phase5-params-assets'
names=[s.strip() for s in (ROOT/'analysis/recovered_dictionary.txt').read_text(encoding='utf-8-sig').splitlines() if s.strip().endswith('.trpokecfg')]
log=[]
for start in range(0,len(names),20):
    args=[str(ROOT/'za_port/trpfs_tool.exe'),'--romfs',str(ROOT/'romfs'),'-o',str(OUT),'extract']+names[start:start+20]
    result=subprocess.run(args,cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    log.append(result.stdout.decode('utf-8',errors='replace'))
    if result.returncode: raise RuntimeError(f'extractor returned {result.returncode}')
(ROOT/'analysis/boot/phase5-params-extract-all.log').write_text('\n'.join(log),encoding='utf-8')
rows=[]
for name in names:
    p=OUT/name
    b=p.read_bytes()
    root=struct.unpack_from('<I',b,0)[0]
    vt=root-struct.unpack_from('<i',b,root)[0]
    vlen,objlen=struct.unpack_from('<HH',b,vt)
    assert vlen%2==0 and vlen>=4 and vt+vlen<=len(b) and root+objlen<=len(b)
    fields=[]
    for i in range((vlen-4)//2):
        off=struct.unpack_from('<H',b,vt+4+i*2)[0]
        candidate=struct.unpack_from('<f',b,root+off)[0] if off and root+off+4<=len(b) else None
        fields.append({'index':i,'table_offset':off,'hex':b[root+off:root+off+min(12,objlen-off)].hex() if off else None,
                       'f32_candidate':candidate if candidate is None or math.isfinite(candidate) else None})
    rows.append({'path':name,'size':len(b),'sha256':hashlib.sha256(b).hexdigest(),'root':root,'vtable':vt,'vtable_length':vlen,'object_length':objlen,'fields':fields})
summary={'count':len(rows),'sizes':dict(collections.Counter(r['size'] for r in rows)),
         'vtable_lengths':dict(collections.Counter(r['vtable_length'] for r in rows)),
         'objects':rows,'limits':'Field f32 candidates are byte interpretations, not established semantic types.'}
(ROOT/'analysis/boot/phase5-params-assets.json').write_text(json.dumps(summary,indent=2,allow_nan=False),encoding='utf-8')
print(json.dumps({k:v for k,v in summary.items() if k!='objects'},indent=2))
