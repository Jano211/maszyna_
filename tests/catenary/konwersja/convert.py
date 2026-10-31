# konwersja starej sieci Rainsted (siec_trakcyjna_ph) na nowa siec: slupy z wysiegnikami -> podwieszenia, przesla -> type
import sys,math,re,collections,subprocess
sys.path.insert(0,__import__('os').path.dirname(__file__))
from parse_siec import *; from tracks import *
SRC,TRK,INC,OUT,NETFILE,NETREF,REPO=sys.argv[1:8]
OFF=(0.0,0.0,0.0)  # od 2026-10-31 dyrektywa catenary bierze origin, wiec plik sieci ma wspolrzedne jak sceneria (8. argument niepotrzebny)
lines,incs,spans,head=load_siec(SRC,INC)
TI=TrackIndex(load_tracks(TRK))
POLES={'sts2':'1613-11','sts1':'1613-11','stb1':'1701'}
rx=re.compile(r'^(sts1|sts2|stb1)\+(\d)-(\d+)_([23])(k|d)\.inc$')
cands=[];skipped=collections.Counter();convert={}
for inc in incs:
    n=inc['file'].split('/')[-1]
    m=rx.match(n)
    if not m:
        skipped[re.sub(r'#.*','',n)]+=1; continue
    h,text=head(inc['file'])
    if not h: skipped['(brak naglowka) '+n]+=1; continue
    pm=re.search(r'node\s+\S+\s+\S+\s+none\s+model\s+(\S+)\s+\S+\s+\S+\s+\S+\s+siec_trakcyjna_ph/(sts160|slup_stalowy_a|stb_zk)\.t3d',text)
    px=float(pm.group(1)) if pm else 0.0
    x,y,z,a=map(float,inc['params'][1:5]); r=math.radians(a)
    X=(-math.cos(r),math.sin(r))
    wx=x+h['h0']*X[0]; wz=z+h['h0']*X[1]; wy=y+h['v0']
    polex=x+px*X[0]; polez=z+px*X[1]
    t,d=TI.nearest(wx,wz)
    if t is None or d>0.6: skipped['(brak toru pod drutem) '+n]+=1; continue
    tx,ty,tz,ux,uz,tname=t
    rail=ty+0.18
    radius=float(tname.split('|')[1])
    curve=1 if 0<radius<4000 else 0
    toward=1 if ((wx-tx)*(polex-tx)+(wz-tz)*(polez-tz))>0 else 0
    wires=2 if m.group(4)=='3' else 1
    hk=h['v1']-h['v0']
    name='cze_w%04d'%(len(cands)+1)
    cands.append((name,inc,POLES[m.group(1)],(polex,rail,polez),(wx,wy,wz),hk,curve,toward,wires,m.group(1)))
inp=''.join('%s %s %.3f %.3f %.3f %.3f %.3f %.3f %.3f %d %d %d t_tube_swing 0.15\n'%(c[0],c[2],*c[3],*c[4],c[5],c[6],c[7],c[8]) for c in cands)
res=subprocess.run([REPO+'/tests/catenary/conv_pick',REPO+'/tests/catenary/data'],input=inp,capture_output=True,text=True,cwd=REPO).stdout
R={}
for l in res.splitlines():
    head_,_,notes=l.partition(' | ')
    f=head_.split(); R[f[0]]=(int(f[1]),int(f[2]),float(f[3]),float(f[4]),int(f[5]),notes,float(f[6]),float(f[7]))
ropeat={};shifts=[];out=[];stats=collections.Counter();warnkinds=collections.Counter();cards=collections.Counter()
for c in cands:
    card,exe,ropey,A,nw,notes,npx,npz=R[c[0]]
    if card==0: stats['bez karty']+=1; skipped['(bez karty, A=%.2f) '%A+c[1]['file'].split('/')[-1]]+=1; continue
    stats['bez uwag' if nw==0 else 'z uwagami' if nw>0 else 'nie sklada']+=1
    for w in notes.split(' ;; '):
        if w: warnkinds[re.sub(r'[-0-9.]+','#',w.split(': ',1)[-1])[:90]]+=1
    cards[card//10*10]+=1
    convert[c[1]['line']]=c[0]
    ropeat[(round(c[4][0],1),round(c[4][2],1))]=ropey
    p=(npx,c[3][1],npz);a=c[4]
    shifts.append(math.hypot(npx-c[3][0],npz-c[3][2]))
    ox,oy,oz=OFF
    out.append('node 1000 0 %s suspension scheme %d %d at %.3f %.3f %.3f rope %.3f %.3f %.3f pole %s %.3f %.3f %.3f endsuspension'%(c[0],card,exe,a[0]+ox,a[1]+oy,a[2]+oz,a[0]+ox,ropey+oy,a[2]+oz,c[2],p[0]+ox,p[1]+oy,p[2]+oz))
# plik sieci
open(NETFILE,'w',newline='\r\n').write('// Siec trakcyjna stacji Czestochowa - konwersja z l61\\siec\\cze.scm (slupy z wysiegnikami, siec_trakcyjna_ph)\n// slupy: sts1/sts2 -> 1613-11, stb1 -> 1701; karty z grupy rurowe przechylne 11xx, lapki lekkie gdzie sie da\n'+'\n'.join(out)+'\n')
# nowa sceneria: przekonwertowane includy zakomentowane, przesla z typem, dyrektywa catenary na poczatku
TYPES={'2':'C95-C','3':'YC120-2C150','4':'YC120-2C150'}
spanline={s['end']:s for s in spans}
# liny nosne przesel: przy przekonwertowanych podwieszeniach na wysokosc liny z karty (inaczej lina przesla nie trafi w uchwyt)
lifted=0
for s in spans:
    i=s['line']; pts=[lines[i+k].split() for k in (1,2,3,4)]
    for wire,rope in ((0,2),(1,3)):
        if any(len(q)!=3 for q in pts): break
        key=(round(float(pts[wire][0]),1),round(float(pts[wire][2]),1))
        if key in ropeat:
            pts[rope][1]='%.3f'%ropeat[key]; lifted+=1
            lines[i+1+rope]=' '.join(pts[rope])
typed=collections.Counter()
new=['catenary %s endcatenary'%NETREF,'']
for i,l in enumerate(lines):
    if i in convert:
        new.append('//conv '+l); continue
    if i in spanline:
        s=spanline[i]; t=s['tok']; v=list(map(float,t[12:24]))
        hk=(v[7]-v[1]+v[10]-v[4])/2
        if hk>1.0 and t[26] in TYPES and t[25]!='0' and l.strip()=='endtraction':
            prev=new[-1]
            new[-1]=prev+' type '+TYPES[t[26]]; typed[TYPES[t[26]]]+=1
    new.append(l)
open(OUT,'w',newline='\r\n').write('\n'.join(new))
print('kandydaci',len(cands),dict(stats)); print('odsuniecie slupow: max %.3f, srednio %.3f, ile >0: %d'%(max(shifts),sum(shifts)/len(shifts),sum(1 for x in shifts if x>1e-4))); print('typy przesel',dict(typed)); print('konce lin przesel wyrownane do kart',lifted); print('karty',sorted(cards.items()))
print('uwagi:'); [print('  %4d %s'%(n,k)) for k,n in warnkinds.most_common(12)]
print('pominiete (stare zostaja):'); [print('  %4d %s'%(n,k)) for k,n in skipped.most_common(25)]; [print('  %4d %s'%(n,k)) for k,n in skipped.items() if 'bez karty' in k]
