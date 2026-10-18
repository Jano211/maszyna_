# Sceneria testowa katalogu wysiegnikow: wszystkie schematy z data/traction_cantilevers.txt na slupach 1701,
# w polu obok toru (x ujemne), rzad na grupe kart (111x, 112x, ...), w rzedzie karta po karcie i wykonanie po wykonaniu.
# Wysieg w srodku pasma A, lina nosna pionowo nad przewodem na wysokosci konstrukcyjnej, przewod 5,6 m nad glowka szyny.
# Uruchamiac z katalogu tests/catenary:  python3 gen_tk.py [ostrzezenia.csv]
# Opcjonalny plik ostrzezen (wynik solvera na slupie, kolumny: karta;wyk;...;ostrzezenia) trafia do klucza.
import re,sys
ROW0_X=-30.0      # pierwszy rzad: os slupow
ROW_DX=8.0        # odstep rzedow (td_wys2000 mial 12 m)
COL_DZ=3.5        # odstep wysiegnikow w rzedzie
Z0=150.0          # poczatek rzedow wzdluz toru
WIRE_Y=5.8; RAIL_Y=0.2
PLATFORM=0.76     # peronowe (103x, 104x): slup stoi na peronie, 0,76 m nad glowka szyny
RAISE={103:PLATFORM,104:PLATFORM,101:1.40,11:0.10}   # 101x: odciag 8,66 m nad glowka, 1701 pozwala na 7,30 (tylko na probe);
# 011x (teowniki): mocowanie 4620 na wysokosci liny, 1-2 cm nad mount_max 1701 - slup 0,10 m wyzej
POLE_HALF=0.112   # pol glebokosci slupa 1701 na wysokosci mocowania (jak w gen_td.py)
data=open('data/traction_cantilevers.txt',encoding='utf-8').read()
schemes=[]
for m in re.finditer(r'^scheme\s+(\d+)\s+(\d+)\s+reach\s+(\S+)\s+(\S+)\s+height\s+(\S+)',data,re.M):
    schemes.append((int(m.group(1)),int(m.group(2)),float(m.group(3)),float(m.group(4)),float(m.group(5))))
warn={}
if len(sys.argv)>1:
    for l in open(sys.argv[1],encoding='utf-8'):
        f=l.rstrip('\n').split(';')
        if len(f)>=12: warn[(int(f[0]),int(f[1]))]=f[11]
OLD=lambda c: c<700 or 900<=c<1000   # stary katalog (Kolprojekt, teowniki 01xx/09xx): rzedy za proba 25 kV, reszta pola bez zmian
groups=sorted(set(c//10 for c,*_ in schemes if not OLD(c)))
old_groups=sorted(set(c//10 for c,*_ in schemes if OLD(c)))
L=['// Sceneria testowa katalogu wysiegnikow: kazdy schemat raz, na slupie 1701. Klucz polozen: td_katalog_klucz.csv.',
   '// Rzedy wzdluz toru (os Z), wysiegnik od slupa w strone toru (+X). Plik generowany (tests/catenary/gen_tk.py).']
K=['rzad;grupa;nr_w_rzedzie;karta;wykonanie;x_slupa;z;wysieg_A;pasmo_A;hk;ostrzezenia']
for r,g in enumerate(groups):
    px=ROW0_X-ROW_DX*r
    row=sorted(s for s in schemes if s[0]//10==g)
    for i,(card,wyk,amin,amax,hk) in enumerate(row):
        z=Z0+COL_DZ*i; a=0.5*(amin+amax); wx=px+a+POLE_HALF
        rail=RAIL_Y+RAISE.get(g,0.0)   # slup podniesiony: peronowe na peronie, 101x - 1701 za niski
        L.append('node 1000 0 none suspension scheme %d %d at %.3f %.3f %.3f rope %.3f %.3f %.3f pole 1701 %.3f %.3f %.3f endsuspension'
                 %(card,wyk,wx,WIRE_Y,z,wx,WIRE_Y+hk,z,px,rail,z))
        K.append('%d;%dx;%d;%d;%d;%.3f;%.1f;%.3f;%.2f-%.2f;%.3f;%s'%(r+1,g,i+1,card,wyk,px,z,a,amin,amax,hk,warn.get((card,wyk),'')))
# ostatni rzad: parasole (dwa wysiegniki na jednym slupie, slowo umbrella), po obu stronach slupa
UMBRELLAS=[((1112,1),(1112,1)),((1112,1),(1122,1)),((1114,2),(1124,3)),((1212,1),(1222,1)),((1232,1),(1242,5)),
           ((1203,1),(1283,3)),((1292,1),(1112,1)),((772,2),(1112,1)),((752,2),(752,3)),((1116,1),(1126,4)),
           # 4670 (ukosnik jednej strony + odciag drugiej na wspolnej wysokosci, wysokosci z solvera na 1701)
           ((770,2),(790,1)),((1002,1),(738,1)),((1002,1),(1112,1)),((781,3),(748,2))]
byid={(c,e):(amin,amax,hk) for c,e,amin,amax,hk in schemes}
px=ROW0_X-ROW_DX*len(groups)-4.0
for i,pair in enumerate(UMBRELLAS):
    z=Z0+COL_DZ*i
    for side,(card,wyk) in zip((1,-1),pair):
        amin,amax,hk=byid[(card,wyk)]; a=0.5*(amin+amax); wx=px+side*(a+POLE_HALF)
        L.append('node 1000 0 none suspension scheme %d %d at %.3f %.3f %.3f rope %.3f %.3f %.3f pole 1701 %.3f %.3f %.3f umbrella u%d endsuspension'
                 %(card,wyk,wx,WIRE_Y,z,wx,WIRE_Y+hk,z,px,RAIL_Y,z,i+1))
        K.append('%d;parasol u%d;%d;%d;%d;%.3f;%.1f;%.3f;%.2f-%.2f;%.3f;%s'%(len(groups)+1,i+1,i+1,card,wyk,px,z,a,amin,amax,hk,'strona +X' if side>0 else 'strona -X'))
# proba izolatorow 25 kV (Jan 2026-10-17, modele 7010/7020/7100-25; kart 25 kV nie ma, "na oko"): slowo insulators 25,
# rury z izolatorem dobiera exe (catenary_scene.cpp); obok ten sam schemat na 3 kV dla porownania
TRIAL25=[(1116,1),(1114,2),(1112,1),(1212,1),(1111,1),(1232,1)]
px=ROW0_X-ROW_DX*len(groups)-16.0
for i,(card,wyk) in enumerate(TRIAL25):
    amin,amax,hk=byid[(card,wyk)]; a=0.5*(amin+amax); wx=px+a+POLE_HALF
    for j,extra in enumerate(('',' insulators 25')):
        z=Z0+COL_DZ*(2*i+j)
        L.append('node 1000 0 none suspension scheme %d %d at %.3f %.3f %.3f rope %.3f %.3f %.3f pole 1701 %.3f %.3f %.3f%s endsuspension'
                 %(card,wyk,wx,WIRE_Y,z,wx,WIRE_Y+hk,z,px,RAIL_Y,z,extra))
        K.append('%d;proba 25 kV;%d;%d;%d;%.3f;%.1f;%.3f;%.2f-%.2f;%.3f;%s'%(len(groups)+2,2*i+j+1,card,wyk,px,z,a,amin,amax,hk,
                 '3 kV (porownanie)' if j==0 else '25 kV: izolatory -25, rury dobiera exe'))
# teowniki (stary katalog): rzad na grupe kart, za rzedem proby 25 kV
for r,g in enumerate(old_groups):
    px=ROW0_X-ROW_DX*len(groups)-28.0-ROW_DX*r
    row=sorted(s for s in schemes if s[0]//10==g)
    for i,(card,wyk,amin,amax,hk) in enumerate(row):
        z=Z0+COL_DZ*i; a=0.5*(amin+amax); wx=px+a+POLE_HALF
        rail=RAIL_Y+RAISE.get(g,0.0)
        L.append('node 1000 0 none suspension scheme %d %d at %.3f %.3f %.3f rope %.3f %.3f %.3f pole 1701 %.3f %.3f %.3f endsuspension'
                 %(card,wyk,wx,WIRE_Y,z,wx,WIRE_Y+hk,z,px,rail,z))
        K.append('%d;teowniki %02dx;%d;%04d;%d;%.3f;%.1f;%.3f;%.2f-%.2f;%.3f;%s'%(len(groups)+3+r,g,i+1,card,wyk,px,z,a,amin,amax,hk,warn.get((card,wyk),'')))
open('td_katalog.scm','w',newline='').write('\r\n'.join(L)+'\r\n')
open('td_katalog_klucz.csv','w',encoding='utf-8-sig',newline='').write('\r\n'.join(K)+'\r\n')
src=open('td.scn',encoding='utf-8',errors='replace',newline='').read()
out=src   # stare slupy sb165 przy torze zostaja, pole testowe lezy obok
line='// katalog wysiegnikow skladanych przez exe (gen_tk.py)\r\ncatenary td_katalog.scm endcatenary\r\n\r\n'
found=re.search(r'(?im)^firstinit\b',out); assert found,'brak FirstInit'
out=out[:found.start()]+line+out[found.start():]
out=out.replace('//$n Tor doświadczalny','//$n Tor doświadczalny - katalog wysięgników',1)
open('td_katalog.scn','w',encoding='utf-8',newline='').write(out)
print('schematow',len(schemes),'rzedow',len(groups),'x od',ROW0_X,'do',ROW0_X-ROW_DX*(len(groups)-1),
      'z od',Z0,'do',Z0+COL_DZ*(max(sum(1 for s in schemes if s[0]//10==g) for g in groups)-1))
