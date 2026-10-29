# Sceneria testowa wysiegnikow: td.scn bez starych slupow sb165, z plikiem podwieszen wygenerowanym z ich polozen
# Uruchamiac z katalogu tests/catenary:  python3 gen_td.py
import re,math,sys
src=open('td.scn',encoding='utf-8',errors='replace').read()
spans=[]
for m in re.finditer(r'node\s+\S+\s+\S+\s+\S+\s+traction\s+([\s\S]*?)endtraction',src):
    t=m.group(1).split(); p=list(map(float,t[7:19]))
    spans.append((p[0:3],p[3:6],p[6:9],p[9:12]))
poles=[(m.group(1),float(m.group(2)),float(m.group(3)),float(m.group(4))) for m in re.finditer(r'include\s+(tra/sb165-3[dk]\.inc)\s+\S+\s+(\S+)\s+(\S+)\s+(\S+)\s+\S+\s+end',src)]
entries=[];skipped=0;cards={}
for name,px,py,pz in poles:
    best=None
    for s in spans:
        for end,(w,r) in enumerate(((s[0],s[2]),(s[1],s[3]))):
            d=math.hypot(w[0]-px,w[2]-pz)
            if abs(w[2]-pz)<0.6 and d<6 and (best is None or d<best[0]): best=(d,w,r,s)
    if best is None: skipped+=1; continue
    d,w,r,s=best
    hx,hz=px-w[0],pz-w[2]; n=math.hypot(hx,hz); hx/=n; hz/=n
    D=math.hypot(px-w[0],pz-w[2])          # os slupa 1701 w miejscu osi starego slupa
    reach=D-0.5*(0.175+0.21*(7.46-5.44)/8.75)   # do czola slupa 1701 na wysokosci mocowania ukosnika
    axis_x=(s[0][0]+s[1][0])/2; axis_z=(s[0][2]+s[1][2])/2
    # odsuw do slupa: przewod blizej slupa niz os toru
    toward=(w[0]-axis_x)*hx+(0)*hz>1e-6 if abs(hz)<0.5 else False
    digit=max(0,min(8,int(math.floor((reach-1.45)/0.30+1e-9))))
    card=(1110 if toward else 1120)+digit
    cards[card]=cards.get(card,0)+1
    entries.append((card,w,r,(px,w[1]-5.6,pz),reach))   # glowka szyny 5,6 m pod przewodem
def scm(copies,fn):
    L=['// Probny plik podwieszen do td_wys.scn: slupy 1701 z wysiegnikami w miejscach dawnych slupow sb165.',
       '// Wpisy z punktami podanymi wprost: at = os przewodow jezdnych, rope = os liny nosnej, pole = typ slupa i jego os na wysokosci glowki szyny.',
       '// Plik generowany (tests/catenary/gen_td.py).']
    k=0
    for c in range(copies):
        dx=12.0*c    # kolejne kopie obok toru, co 12 m, tylko do proby wydajnosci
        for card,w,r,a,reach in entries:
            L.append('node 1000 0 none suspension scheme %d 1 at %.3f %.3f %.3f rope %.3f %.3f %.3f pole 1701 %.3f %.3f %.3f endsuspension'%(card,w[0]+dx,w[1],w[2],r[0]+dx,r[1],r[2],a[0]+dx,a[1],a[2])); k+=1
    open(fn,'w',newline='').write('\r\n'.join(L)+'\r\n'); return k
def scn(scmname,fn):
    out=re.sub(r'include\s+tra/sb165-3[dk]\.inc[^\r\n]*\r?\n','',src)
    # dyrektywa musi stac przed FirstInit: wtedy exe wiaze podwieszenia i sklada wysiegniki razem z reszta scenerii
    line='// wysiegniki skladane przez exe z czesci katalogowych\r\ncatenary %s endcatenary\r\n\r\n'%scmname
    found=re.search(r'(?im)^firstinit\b',out)
    assert found, 'brak FirstInit w scenerii'
    out=out[:found.start()]+line+out[found.start():]
    open(fn,'w',encoding='utf-8',newline='').write(out)
n1=scm(1,'td_wys.scm'); scn('td_wys.scm','td_wys.scn')
n2=scm(29,'td_wys2000.scm'); scn('td_wys2000.scm','td_wys2000.scn')
print('slupow',len(poles),'pominietych',skipped,'wpisow',n1,'/',n2,'karty',sorted(cards.items()))
print('zasiegi:',sorted(set(round(e[4],3) for e in entries)))
