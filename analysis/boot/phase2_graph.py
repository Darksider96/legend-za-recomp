from image_helpers import *
from collections import defaultdict,deque
import json
graph=defaultdict(list);incoming=defaultdict(list)
for site,target,kind in edges():
    start,size,valid=function(site)
    if valid:
        graph[start].append((site,target,kind));incoming[target].append((start,site,kind))

def path(root,target):
    seen={root};q=deque([root]);parents={}
    while q:
        node=q.popleft()
        for site,t,kind in graph[node]:
            if t in seen:continue
            seen.add(t);parents[t]=(node,site,kind)
            if t==target:
                result=[];v=t
                while v!=root:
                    parent,s,k=parents[v];result.append({'caller':hex(BASE+parent),'site':hex(BASE+s),'kind':k,'target':hex(BASE+v)});v=parent
                return result[::-1]
            if t<0x2fc3900:q.append(t)
    return None

if __name__=='__main__':
    results={}
    for target in [0x175bd84,0x176b8e0,0x1779834,0x1775db0,0x2fc4fb0,0x2fc4fe0]:
        results[f'incoming_{BASE+target:x}']=[(hex(BASE+s),hex(BASE+a),k) for s,a,k in incoming[target]]
        for root in [0xae62b0,0x19dd280,0x5aeb60]:results[f'{BASE+root:x}->{BASE+target:x}']=path(root,target)
    (ROOT/'phase2-initial-graphics-paths.json').write_text(json.dumps(results,indent=2))
    print(json.dumps(results,indent=2))
