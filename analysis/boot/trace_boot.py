from image_helpers import *
from collections import defaultdict,deque
import json

graph=defaultdict(list)
for site,target,kind in edges():
    start,size,valid=function(site)
    if valid:graph[start].append((site,target,kind))

# Resolve two indirect dispatches from constructor assignments and loaded tables.
# b10da0 writes *(this) = *(3cc99e0)+0x10; run() calls vptr[0x20/8].
vtable=struct.unpack('<Q',mem(0x3cc99e0,8))[0]-BASE+0x10
virtual_init=struct.unpack('<Q',mem(vtable+0x20,8))[0]-BASE
graph[0x19e31b0].append((0x19e31d0,virtual_init,'BLR resolved from constructor/vtable'))
# 19dd110 iterates 16-byte member-function records [3a38e18,3a39038).
for entry in range(0x3a38e18,0x3a39038,16):
    target,adjustment=struct.unpack('<QQ',mem(entry,16))
    if adjustment&1==0:
        graph[0x19dd110].append((0x19dd140,target-BASE,f'BLR callback table {BASE+entry:x}'))

def path(root,target):
    seen={root}; q=deque([root]);parents={}
    while q:
        node=q.popleft()
        for site,t,kind in graph[node]:
            if t in seen:continue
            seen.add(t);parents[t]=(node,site,kind)
            if t==target:
                result=[];v=t
                while v!=root:
                    parent,s,k=parents[v];result.append((hex(BASE+parent),hex(BASE+s),k,hex(BASE+v)));v=parent
                return result[::-1]
            if t<0x2fc3900:q.append(t)
    return None

result={}
for root in [0x19e8950,0x19e8720]:
    for target in [0x2fc4910,0x2fc5600,0x2fc5610,0x2fc4050,0x2fc4770,0x2fc3970]:
        result[f'{BASE+root:x}->{BASE+target:x}']=path(root,target)
(ROOT/'boot-paths.json').write_text(json.dumps(result,indent=2))
print(json.dumps(result,indent=2))
