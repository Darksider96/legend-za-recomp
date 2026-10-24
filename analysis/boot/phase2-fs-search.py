from image_helpers import *
from collections import defaultdict,deque
graph=defaultdict(list)
for a,t,k in edges():
 f,s,v=function(a)
 if v:graph[f].append((a,t,k))
FS={0x2fc4580:'OpenFile',0x2fc4630:'OpenDirectory',0x2fc3ed0:'GetEntryType'}
def search(root):
 q=deque([(root,[])]);seen={root};res=[]
 while q:
  n,p=q.popleft()
  for a,t,k in graph[n]:
   np=p+[(hex(BASE+n),hex(BASE+a),hex(BASE+t))]
   if t in FS:res.append((FS[t],np));continue
   if t<0x2fc3900 and t not in seen:seen.add(t);q.append((t,np))
 return res
for n in [0x19dd280,0x01edbec,0x30196c,0x56c5a0,0x19deeec,0x19deef0,0x19deef4,0x19def44,0x19def50,0x5aeb60]:
 print('ROOT',hex(BASE+n));print(*search(n),sep='\n')
for a in [0x3cc99e0,0x309b31d,0x3166a0e]:
 print(hex(BASE+a),mem(a,100))
