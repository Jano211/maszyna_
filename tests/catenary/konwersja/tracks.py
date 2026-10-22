import re,sys,math
def load_tracks(path):
    toks=open(path,encoding='latin-1').read().replace('\r','').split('\n')
    tracks=[]
    i=0
    while i<len(toks):
        l=toks[i].split()
        if len(l)>4 and l[0]=='node' and l[4]=='track':
            kind=l[5]
            try:
                rad=lambda k: abs(float(toks[i+k].split()[0]))
                if kind in('normal','road','cross','river','table'):
                    pts=[list(map(float,toks[i+k].split()[:3])) for k in (2,3,4,5)]
                    tracks.append((l[3]+'|'+str(rad(6)),pts))
                elif kind=='switch':
                    pts=[list(map(float,toks[i+k].split()[:3])) for k in (2,3,4,5)]
                    tracks.append((l[3]+'|'+str(rad(6)),pts))
                    pts2=[list(map(float,toks[i+k].split()[:3])) for k in (7,8,9,10)]
                    tracks.append((l[3]+'#2|'+str(rad(11)),pts2))
            except Exception as e:
                pass
        i+=1
    return tracks
def sample(pts,n=24):
    p1,c1,c2,p2=pts
    if c1==[0,0,0] and c2==[0,0,0]:
        a=p1;b=p2;c=[p1[k]+(p2[k]-p1[k])/3 for k in range(3)];d=[p1[k]+(p2[k]-p1[k])*2/3 for k in range(3)]
    else:
        a=p1;b=p2;c=[p1[k]+c1[k] for k in range(3)];d=[p2[k]+c2[k] for k in range(3)]
    out=[]
    for j in range(n+1):
        t=j/n;u=1-t
        out.append([u*u*u*a[k]+3*u*u*t*c[k]+3*u*t*t*d[k]+t*t*t*b[k] for k in range(3)])
    return out
import collections, math
class TrackIndex:
    def __init__(self, T):
        self.grid=collections.defaultdict(list)
        for name,pts in T:
            L=math.dist(pts[0],pts[3])
            n=max(4,int(L/2))
            s=sample(pts,n)
            for k in range(len(s)-1):
                a,b=s[k],s[k+1]
                cx=int(((a[0]+b[0])/2)//10); cz=int(((a[2]+b[2])/2)//10)
                self.grid[(cx,cz)].append((a,b,name))
    def nearest(self,x,z,r=1):
        best=None;bd=1e9
        for dx in range(-r,r+1):
            for dz in range(-r,r+1):
                for a,b,name in self.grid.get((int(x//10)+dx,int(z//10)+dz),[]):
                    ux,uz=b[0]-a[0],b[2]-a[2]; L2=ux*ux+uz*uz
                    t=0 if L2<1e-9 else max(0,min(1,((x-a[0])*ux+(z-a[2])*uz)/L2))
                    px,pz=a[0]+ux*t,a[2]+uz*t
                    d=math.hypot(px-x,pz-z)
                    if d<bd:
                        bd=d;best=(px,a[1]+(b[1]-a[1])*t,pz,ux,uz,name)
        return best,bd
