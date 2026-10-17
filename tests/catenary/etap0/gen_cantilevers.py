import re
# -*- coding: utf-8 -*-
# Generuje z etap0/zlozenia.json:
#   data/traction_cantilevers.txt  - czesci, zestawy ramion odciagowych i schematy wysiegnikow dla exe
#   cantilever_ref.txt             - punkty wzorcowe zlozen z Blendera dla testu solvera
# Uruchamiac z katalogu tests/catenary:  python3 etap0/gen_cantilevers.py
# Wymiary czesci pochodza z kart katalogowych (tom 5), nie ze zlozen; ze zlozen brane sa tylko:
# dobor rur do kart, wysokosci mocowan, zestawy ramion odciagowych i punkty do porownania.
import json, re, collections, io

D = json.load(open('etap0/zlozenia.json', encoding='utf-8'))

def family(name): return re.match(r'(\d{4})', name).group(1)
def number(name): return int(name.split('-')[1])
def f(v): return ('%.4f' % (v + 0.0)).replace('-0.0000', '0.0000')

# ---- czesci: wymiary z kart katalogowych --------------------------------------------------------------------------
L6001 = {1: 1600, 2: 2150, 3: 2400, 4: 2650, 5: 2900, 6: 3150, 7: 3400, 8: 3650, 9: 3900, 10: 4100, 11: 4300, 12: 5100, 13: 5300, 14: 4500}
L6007 = {i: 650 + 300 * (i - 1) for i in range(1, 11)}
L6007.update({11: 3850, 12: 4150, 13: 4500, 14: 4800})   # dopisane recznie na karcie 30-6007
L6011 = {i: 1000 + 250 * (i - 1) for i in range(1, 15)}
# wspornik lekkich ramion 4240 (wyk. 1 i 5): regulacja plynna 115-270 mm od osi rury (-1, -2; -3, -4: do 380 mm; Jan); polozenia
# typowego nie ma, ale srodek lepszy niz skrajnosc. Zlozenia z Blendera stoja na 222 mm (-1, -2; Jan), wiec podniesienie 0 = 222 mm,
# a proby co 1 cm w obie strony, od -0,107 (115 mm) do +0,048 m (270 mm)
TUBE_4240 = ' tube_steps ' + ' '.join('%.4f' % x for x in [-0.107] + [0.01 * k for k in range(-10, 5)] + [0.048]) + ' tube_prefer 0.0000'
# 6002: rozstaw otworow (punkt "end" w modelach Jana, L - 60 mm)
L6002 = {i: 640 + 300 * (i - 1) for i in range(1, 9)}
L6002.update({9: 3390, 10: 3690, 11: 3090})
L6111 = {1: 1075, 2: 1375, 3: 1775, 4: 2075, 5: 2475, 6: 2975, 7: 3475, 8: 3975, 14: 4875}   # 14: niekatalogowa, 6111-08 przedluzona (0748, Jan 2026-10-14)

out = io.StringIO()
w = lambda s='': out.write(s + '\r\n')
w('// Wysiegniki sieci trakcyjnej: czesci, zestawy ramion odciagowych i schematy z kart katalogowych.')
w('// Plik wygenerowany przez tests/catenary/etap0/gen_cantilevers.py; reczne poprawki zgina przy ponownym generowaniu.')
w('// Jednostki: metry. Uklad wysiegnika: zero w osi przewodow jezdnych przy podwieszeniu, Y poziomo w strone konstrukcji')
w('// wsporczej, Z w gore, X wzdluz toru.')
w()
w('// katalog modeli czesci, wzgledem katalogu modeli symulatora')
w('models siec_trakcyjna/components/')
w()
w('// ---- czesci -------------------------------------------------------------------------------------------------')
w('// length: rury i ciegna - od osi zaczepu do konca (rura) albo do osi drugiego ucha; izolatory - od zaczepu do osi polaczenia "end"')
w('// pin:    odleglosc osi otworu sworznia od konca rury')
w('// eye:    odleglosc ucha (sworznia) od osi rury, na ktorej siedzi uchwyt')
w('// holes:  odleglosci otworow regulacyjnych od osi ucha')
w('// hook, end: uchwyt liny - punkt zawieszenia na uchu i punkt mocowania linki (Y Z w ukladzie czesci, zero w osi liny)')
for n, L in sorted(L6001.items()):
    w('part 6001-%02d kind strut length %s pin 0.0300 radius 0.0300 endpart   // karta 30-6001: L = %d mm, otwor 30 mm od konca' % (n, f(L / 1000 - 0.03), L))
for n, L in sorted(L6007.items()):
    w('part 6007-%02d kind stay length %s endpart   // karta 30-6007, odmiana 2: L = %d mm' % (n, f(L / 1000), L))
for n, L in sorted(L6002.items()):
    w('part 6002-%02d kind stay length %s radius 0.0300 endpart   // karta 30-6002: rura odciagu sztywnego, rozstaw otworow (punkt "end" modelu)' % (n, f(L / 1000)))
for n, L in sorted(L6011.items()):
    w('part 6011-%02d kind steady length %s hook 0.0150 0.0000 endpart   // karta 36-6011: L+23 mm od osi haka do konca rury; hook: krawedz haka, ktora lezy na uchu (1,5 cm od zera w +Y; 2 cm wbijalo sie w uchwyt, poprawka Jana 2026-10-13)' % (n, f(L / 1000 + 0.023)))
for n, L in sorted(L6111.items()):
    w('part 6111-%02d kind steady length %s endpart   // karta 56-6111: L = %d mm od pierwszego otworu' % (n, f(L / 1000), L))
w('part 4400-01 kind stay_end holes 0.1400 0.1900 0.2400 0.2900 0.3400 endpart   // karta 25-4400: L = 440, a = 100, 5 otworow co 50 mm')
w('part 4400-02 kind stay_end holes 0.1400 0.1900 0.2400 0.2900 0.3400 endpart   // karta 25-4400: L = 375, a = 35')
w('part 4050-01 kind clamp eye 0.0800 endpart   // karta 25-4050: os ucha 70 mm pod osia rury; hak lezy na dolnej krawedzi ucha, 1 cm nizej (Jan, po probie w symulatorze)')
w('part 4070-01 kind clamp eye 0.0830 endpart   // karta 23-4070: sworzen 83 mm od osi rury')
w('part 4210-02 kind clamp eye 0.0410 endpart   // karta 25-4210: h = 41 mm, d = 40 mm')
w('part 4210-01 kind clamp eye 0.0320 endpart   // karta 25-4210: h = 32 mm, d = 24 mm')
w('part 4140-01 kind messenger_clamp hook -0.0062 0.0751 end 0.0056 -0.0357 endpart   // punkt "end" z modelu, "hook" 8,5 mm nad punktem modelu (krawedz, ktora uchwyt wisi na uchu); zero w osi liny nosnej (karta 30-4140: 65 i ok. 105 mm od haka)')
w('part 7100-01 kind insulator length 0.3300 endpart   // karta 35-7100: 330 mm od ucha do sruby M16; tak samo punkt "end" modelu')
w('part 7010-01 kind insulator length 0.3200 endpart')
w('part 7020-01 kind insulator length 0.3200 endpart')
w('part 4700-01 kind shackle length 0.0480 endpart   // karta 23-4700: 48 mm od luku do sworznia; punkt "eye" modelu')
w('part 4490-00 kind cord length 1.0000 endpart   // linka uniwersalna (nie z katalogu): model 1 m od zera do punktu "end", skalowana wzdluz Z')
w()

# ---- zlozenia 11xx ---------------------------------------------------------------------------------------------
K = [k for k in D if D[k]['card'].startswith('11') and not D[k]['v25']]
# nazwy czesci w zlozeniach z Blendera, ktore w plikach modeli brzmia inaczej
RENAMED = {'4140-1': '4140-01', '7100-1': '7100-01', '7010-1': '7010-01', '7020-1': '7020-01', '4400-1': '4400-01', '4400-2': '4400-02', '4490-0': '4490-00'}
RENAMED.update({'6007-%02d-2' % n: '6007-%02d' % n for n in range(1, 15)})
REG = ('4240', '4250', '4260', '6201', '6202', '6203', '6204', '6205', '4130', '4110')

def parts(k):
    a = collections.defaultdict(list)
    for p in D[k]['parts']: a[family(p['part'])].append(p)
    return a

# zestawy ramion odciagowych: najczestsza postac w rodzinie kart i wykonaniu
groups = collections.defaultdict(list)
for k in K:
    v = D[k]; a = parts(k)
    items = []
    for p in sorted(v['parts'], key=lambda p: (p['part'], [round(c, 3) for c in p['pos']])):
        if family(p['part']) in REG:
            items.append((p['part'], tuple(round(c, 4) for c in p['pos']), tuple(round(c, 5) for row in p['rot'] for c in row)))
    clamp = a['4210'][0]
    groups[(v['card'][:3], v['wyk'])].append((tuple(items), round(clamp['pos'][1], 4), round(clamp['pos'][2], 4)))

w('// ---- zestawy ramion odciagowych ------------------------------------------------------------------------------')
w('// Stale polozenie wzgledem przewodow jezdnych, przeniesione ze zlozen Blendera (uklad czesci jak w tamtych projektach).')
w('// tube: wysokosc osi wysiegu pomocniczego; clamp: nominalne polozenie Y obejmy linki 4210 na wysiegu pomocniczym')
w('// same ramiona (uchwyt, ramiona, uchwyty drutu) dojda jako jeden model na zestaw; wpis "item" zostaje w formacie na pozniej')
for key in sorted(groups):
    variants = collections.Counter(groups[key])
    (items, cy, cz), _ = variants.most_common(1)[0]
    # wysokosc rury i polozenie obejmy: mediana, zeby pojedyncze recznie przesuniete zlozenie nie decydowalo
    w('registration %sx-%s tube %s clamp %s' % (key[0], key[1], f(cz), f(cy)))
    # czesci zestawu (uchwyt ramion, ramiona, uchwyty drutu) nie sa wypisywane: Jan zrobi kazdy zestaw jako jeden model,
    # a polozenia ze starych zlozen byly w ukladzie czesci z tamtych projektow Blendera
    w('endregistration')
w()

ASSEMBLIES = """// ---- sposoby skladania -----------------------------------------------------------------------------------------
// Wysiegnik sklada sie klockami w podanej kolejnosci. Uklad: zero w osi przewodow jezdnych, Y do konstrukcji, Z w gore.
// Punkty wejsciowe: wire (0 0), messenger (os liny nosnej), strut_mount, stay_mount, steady_mount (zaczepy izolatorow),
// registration (Y = clamp, Z = tube z zestawu ramion odciagowych).
// Argumenty bez spacji. Wyrazenie: skladniki laczone + i -, skladnik to liczba, <rola>.<pole> (length, pin, eye, hook.y,
// hook.z, end.y, end.z czesci przypisanej roli), <czlon>.<polozenie>, rules.<nazwa>, <punkt>.y, <punkt>.z.
// Punkt: nazwa albo <czlon>@<wyrazenie> (punkt na osi czlonu w tej odleglosci od jego poczatku).
// Kierunek: <czlon>, -<czlon>, <czlon>.up, <czlon>.down (prostopadla w gore / w dol). Os spoczynkowa czesci: +y -y +z -z.
//   point <nowy> <punkt>                          kopia punktu
//   shift <nowy> <punkt> <dy> <dz>                punkt przesuniety
//   side <nowy> <czlon> <odl> <odsuniecie>        punkt na czlonie, odsuniety wzdluz prostopadlej w gore
//   line <czlon> <punkt> toward <punkt> | dir <dy> <dz> | along <czlon>
//   hang <rola> <punkt> <zaczep>                  czesc wiszaca w linii z linka od zaczepu (punkty <rola>.hook, <rola>.end)
//   tangent <czlon> <punkt> <srodek> <promien> <polozenie>   czlon styczny do okregu lezacego pod nim
//   level <czlon> <polozenie> <punkt> <odsuniecie>          polozenie, w ktorym punkt odsuniety ma wysokosc punktu
//   project <czlon> <polozenie> <punkt> <koniec> <zapas>    rzut punktu, najdalej do koniec - zapas
//   limit <czlon> <polozenie> <koniec> <zapas>              ostrzezenie, gdy za polozeniem zostaje mniej niz zapas
//   window <czlon> <polozenie> <od> <do>                    ostrzezenie poza przedzialem ("-" = bez granicy)
//   chain <czlon> <polozenie> <zaczep> <odsuniecie> <dlugosc> <rola z otworami> <polozenie wzorcowe> <min> <zalecany> <max>
//                                                 dobor skokowy: lancuch od zaczepu do punktu na czlonie, odstep od wzorca
//   place <rola> <punkt> <kierunek> <os>          czesc; ?<rola> = tylko gdy schemat ma te role
//   roll <rola> <warunek>                          cwierc obrotu wokol wlasnej osi, gdy schemat ma role warunek ("-" zawsze)
//   stretch <rola> <od> <do> <os>                  czesc rozciagnieta miedzy punktami, skalowana wzdluz osi
//   registration                                   zestaw ramion odciagowych
//   repeat <n> <punkt> ... endrepeat               powtarzanie, az punkt sie ustali (najwyzej n razy)

// wysiegnik przechylny (11xx) bez wlasnego izolatora wysiegu pomocniczego: rura 6011 wisi hakiem na dolnym uchwycie 4050
assembly swing
  point cord_clamp_at registration
  repeat 6 cord_clamp_at
    shift cord_anchor cord_clamp_at 0 cord_clamp.eye
    hang messenger_clamp messenger cord_anchor
    tangent strut strut_mount messenger_clamp.hook head_clamp.eye head_clamp
    level strut steady_clamp registration -steady_clamp.eye
    side steady_eye strut strut.steady_clamp -steady_clamp.eye
    shift steady_start steady_eye -steady.hook.y 0
    line steady steady_start dir -1 0
    project steady cord_clamp registration steady.length rules.cord_clamp_inset
    point cord_clamp_at steady@steady.cord_clamp
  endrepeat
  shift cord_anchor cord_clamp_at 0 cord_clamp.eye
  limit strut head_clamp strut_insulator.length+strut.length rules.clamp_margin
  window strut steady_clamp strut_insulator.length+rules.clamp_margin strut.head_clamp-rules.stay_gap_min
  chain strut stay_clamp stay_mount stay_clamp.eye stay_insulator.length+stay.length stay_end head_clamp rules.stay_gap_min rules.stay_gap rules.stay_gap_max
  window strut stay_clamp strut_insulator.length+rules.clamp_margin -
  side stay_pin strut strut.stay_clamp stay_clamp.eye
  line stay stay_mount toward stay_pin
  place ?stay_shackle stay@-stay_shackle.length stay -y
  place stay_insulator stay_mount stay -y
  roll stay_insulator stay_shackle
  place stay stay@stay_insulator.length stay -y
  place stay_end stay_pin -stay +y
  place stay_clamp strut@strut.stay_clamp strut.up +z
  place strut_insulator strut_mount strut -y
  place strut strut@strut_insulator.length strut -y
  place head_clamp strut@strut.head_clamp strut.down -z
  place steady_clamp strut@strut.steady_clamp strut.down -z
  place steady steady_start steady -y
  place cord_clamp cord_clamp_at steady.up +z
  stretch ?cord cord_anchor messenger_clamp.end +z
  hangarms ?arms arms_bracket ?arms_head steady registration
  registration
endassembly

// wysiegnik przechylny (11x0) z wysiegiem pomocniczym 6111 na wlasnym izolatorze 7010 przy konstrukcji
assembly swing_steady
  point cord_clamp_at registration
  repeat 6 cord_clamp_at
    shift cord_anchor cord_clamp_at 0 cord_clamp.eye
    hang messenger_clamp messenger cord_anchor
    tangent strut strut_mount messenger_clamp.hook head_clamp.eye head_clamp
    line steady_insulator steady_mount toward registration
    point steady_start steady_insulator@steady_insulator.length
    line steady steady_start along steady_insulator
    project steady cord_clamp registration steady.length rules.cord_clamp_inset
    point cord_clamp_at steady@steady.cord_clamp
  endrepeat
  shift cord_anchor cord_clamp_at 0 cord_clamp.eye
  limit strut head_clamp strut_insulator.length+strut.length rules.clamp_margin
  chain strut stay_clamp stay_mount stay_clamp.eye stay_insulator.length+stay.length stay_end head_clamp rules.stay_gap_min rules.stay_gap rules.stay_gap_max
  window strut stay_clamp strut_insulator.length+rules.clamp_margin -
  side stay_pin strut strut.stay_clamp stay_clamp.eye
  line stay stay_mount toward stay_pin
  place ?stay_shackle stay@-stay_shackle.length stay -y
  place stay_insulator stay_mount stay -y
  roll stay_insulator stay_shackle
  place stay stay@stay_insulator.length stay -y
  place stay_end stay_pin -stay +y
  place stay_clamp strut@strut.stay_clamp strut.up +z
  place strut_insulator strut_mount strut -y
  place strut strut@strut_insulator.length strut -y
  place head_clamp strut@strut.head_clamp strut.down -z
  place steady_insulator steady_mount steady_insulator -y
  place steady steady_start steady -y
  place cord_clamp cord_clamp_at steady.up +z
  stretch ?cord cord_anchor messenger_clamp.end +z
  hangarms ?arms arms_bracket ?arms_head steady registration
  registration
endassembly
// samo podwieszenie liny nosnej (129x): ukosnik i odciag sztywny jak w 12xx, bez wysiegu pomocniczego i linki;
// bez linki nic nie odciaga liny w dol; uchwyt liny 4140 przechylony hakiem w strone slupa, 60 st. od pionu (Jan 2026-10-14: ok. 120 st. od ulozenia wzdluz ukosnika)
assembly messenger_only
  line clamp_axis messenger dir 0.8660 0.5000
  hang messenger_clamp messenger clamp_axis@-1
  tangent strut strut_mount messenger_clamp.hook head_clamp.eye head_clamp
  limit strut head_clamp strut_insulator.length+strut.length rules.clamp_margin
  chain strut stay_clamp stay_mount stay_clamp.eye stay_insulator.length+stay.length stay_end head_clamp rules.stay_gap_min rules.stay_gap rules.stay_gap_max
  window strut stay_clamp strut_insulator.length+rules.clamp_margin -
  side stay_pin strut strut.stay_clamp stay_clamp.eye
  line stay stay_mount toward stay_pin
  place stay_insulator stay_mount stay -y
  place stay stay@stay_insulator.length stay -y
  place stay_end stay_pin -stay +y
  place stay_clamp strut@strut.stay_clamp strut.up +z
  place strut_insulator strut_mount strut -y
  place strut strut@strut_insulator.length strut -y
  place head_clamp strut@strut.head_clamp strut.down -z
endassembly
"""
w('// ustawienia montazowe; cord_clamp_inset: obejma linki 4210 co najmniej 2 cm od konca wysiegu (inaczej wystaje poza rure; Jan 2026-10-13)')
w('// umbrella_spread: parasol na mocowaniach regulowanych - wspolna wysokosc (srednia), gdy obie strony roznia sie najwyzej o tyle (Jan 2026-10-14)')
w('rules cord_clamp_inset 0.0200 umbrella_spread 0.1000 endrules')
w()
# linka V (przesla naprezenia 120x, rozjazdowe 128x, 07xx): dwie linki od obejmy 4210 do liny nosnej, rozsuniete
# wzdluz toru o +-0,125 m (karty 1201, 1282: ~0,25 m miedzy zaczepami na linie)
def _vee(name, new):
    body = re.search(r'assembly %s\n(.*?)endassembly\n' % name, ASSEMBLIES, re.S).group(1)
    assert '  stretch ?cord cord_anchor messenger_clamp.end +z\n' in body
    body = body.replace('  stretch ?cord cord_anchor messenger_clamp.end +z\n', '  vee cord cord_anchor messenger 0.125 +z\n')
    return '\n// jak %s, ale z linka V do liny nosnej\nassembly %s\n%sendassembly\n' % (name, new, body)
ASSEMBLIES += _vee('swing', 'swing_vee') + _vee('swing_steady', 'swing_steady_vee')
# 117x/118x (tom 3 s. 88-98, nieprzechylne w przesle naprezenia): jak swing_vee (z ramionami), do tego uchwyt 4120-3 wprost na
# wysiegu dla drugiego (podniesionego) przewodu 0,08 m od przewodu ramion: 117x dalej od slupa, 118x blizej slupa
def _second_wire(new, dy):
    body = re.search(r'assembly swing_vee\n(.*?)endassembly\n', ASSEMBLIES, re.S).group(1)
    extra = ('  shift second_ref wire %s 0\n  project steady second_clamp second_ref steady.length rules.cord_clamp_inset\n'
             '  point second_tube steady@steady.second_clamp\n  shift second_wire second_tube 0 -second_clamp.eye.z\n'
             '  place ?second_clamp second_wire steady -y\n  export second_wire\n') % dy
    return '\n// jak swing_vee, z uchwytem 4120 drugiego przewodu na wysiegu (%s m w Y od przewodu ramion)\nassembly %s\n%s%sendassembly\n' % (dy, new, body, extra)
ASSEMBLIES += _second_wire('swing_vee_wire_out', '-0.08') + _second_wire('swing_vee_wire_in', '0.08')
# 105x, 125x (tom 3 s. 27-31, 139-144): bez wysiegu z rury - ramiona wisza uchem na linie 9811-3 (tu 4490-00) napietej poziomo od
# uchwytu 4050 na ukosniku do ucha ramion (zaciski 5270/6301 pomijane - Jan); uchwyt liny 4140 wisi pionowo, w 105x linki z luku
# ramion 6207 do liny nosnej (armcords). Przechylne: odciag linowy jak swing.
ROPE_ARMS = """
// ramiona na linie (105x, 125x): zestaw w osi przewodu, lina od 4050 na ukosniku do ucha zestawu (4050 na wysokosci ucha,\n// ale nie na izolatorze - wtedy lina idzie lekko w dol do ucha)
assembly rope_arms
  shift arms_side wire 1 0
  setarms ?arms arms_side
  hang messenger_clamp messenger wire
  tangent strut strut_mount messenger_clamp.hook head_clamp.eye head_clamp
  level strut rope_clamp arms_holder -rope_clamp.eye
  atleast strut rope_clamp strut_insulator.length+rules.clamp_margin
  side rope_eye strut strut.rope_clamp -rope_clamp.eye
  limit strut head_clamp strut_insulator.length+strut.length rules.clamp_margin
  window strut rope_clamp strut_insulator.length+rules.clamp_margin strut.head_clamp-rules.stay_gap_min
  chain strut stay_clamp stay_mount stay_clamp.eye stay_insulator.length+stay.length stay_end head_clamp rules.stay_gap_min rules.stay_gap rules.stay_gap_max
  window strut stay_clamp strut_insulator.length+rules.clamp_margin -
  side stay_pin strut strut.stay_clamp stay_clamp.eye
  line stay stay_mount toward stay_pin
  place ?stay_shackle stay@-stay_shackle.length stay -y
  place stay_insulator stay_mount stay -y
  roll stay_insulator stay_shackle
  place stay stay@stay_insulator.length stay -y
  place stay_end stay_pin -stay +y
  place stay_clamp strut@strut.stay_clamp strut.up +z
  place strut_insulator strut_mount strut -y
  place strut strut@strut_insulator.length strut -y
  place head_clamp strut@strut.head_clamp strut.down -z
  place rope_clamp strut@strut.rope_clamp strut.down -z
  armropes rope rope_eye
  armcords ?arms_cord ?arms_cord_clamp messenger 0.125
endassembly
"""
ROPE_ARMS += ("\n// jak rope_arms, ale lina z wlasnego zaczepu na slupie (mocowanie steady) przez izolator 7020 (1250, 1270)\nassembly rope_arms_mount\n"
    + re.search(r'assembly rope_arms\n(.*?)endassembly\n', ROPE_ARMS, re.S).group(1)
      .replace('  level strut rope_clamp arms_holder -rope_clamp.eye\n  atleast strut rope_clamp strut_insulator.length+rules.clamp_margin\n  side rope_eye strut strut.rope_clamp -rope_clamp.eye\n', '')
      .replace('  window strut rope_clamp strut_insulator.length+rules.clamp_margin strut.head_clamp-rules.stay_gap_min\n', '')
      .replace('  place rope_clamp strut@strut.rope_clamp strut.down -z\n  armropes rope rope_eye\n',
               '  line rope_line steady_mount toward arms_holder\n  place rope_insulator steady_mount rope_line -y\n  point rope_eye rope_line@rope_insulator.length\n  armropes rope rope_eye\n')
    + "endassembly\n")
ASSEMBLIES += ROPE_ARMS
# 102x (tom 3 s. 12-16, dwie sieci w przesle naprezenia, nieprzechylne): jak swing_vee bez ramion; na wysiegu 6011 dwa uchwyty 4120-3
# dla przewodow obu sieci 0,08 m od siebie (+-0,04 wokol A, odczyt z rysunku 1022); drugi uchwyt liny 4140 na drugim 4050 na ukosniku,
# drugi 4050 na ukosniku 0,08 m dalej od slupa, 4140 wisi z niego pionowo - lina drugiej sieci wypada wyzej, jak na rysunku 1022
# (punkt w wyniku: messenger_clamp_b.y/.z); wysieg podparty
# plaskownikiem 6506 (0,15 m miedzy otworami) od uchwytu 4760 na ukosniku do obejmy 4210 na wysiegu, pionowo.
ASSEMBLIES += '\n// dwie sieci (102x): swing_vee + drugi przewod i druga lina nosna, wysieg podparty plaskownikiem 6506 do 4760 na ukosniku\nassembly double_net\n' + \
    re.search(r'assembly swing_vee\n(.*?)endassembly\n', ASSEMBLIES, re.S).group(1).replace('  hangarms ?arms arms_bracket ?arms_head steady registration\n', '').replace('  registration\n', '') + \
    """  shift wire_a wire 0.04 0
  shift wire_b wire -0.04 0
  place contact_clamp_a wire_a steady -y
  place contact_clamp_b wire_b steady -y
  export wire_a
  export wire_b
  shift messenger_b_ref messenger_clamp.hook -0.08 0
  across strut head_clamp_b messenger_b_ref -head_clamp_b.eye
  side head_b_eye strut strut.head_clamp_b -head_clamp_b.eye
  place head_clamp_b strut@strut.head_clamp_b strut.down -z
  hangat messenger_clamp_b head_b_eye
  export messenger_clamp_b
  shift link_top registration 0 link_cord_clamp.eye+link.length
  level strut link_clamp link_top -link_clamp.eye
  side link_high strut strut.link_clamp -link_clamp.eye
  shift link_low link_high 0 -link.length
  project steady link_cord_clamp link_low steady.length 0
  place link_cord_clamp steady@steady.link_cord_clamp steady.up +z
  line link_line link_low toward link_high
  place link link_low link_line +y
  place link_clamp strut@strut.link_clamp strut.down -z
endassembly
"""
# 109x (tom 3 s. 44-51, rozjazdowe jednej sieci, przechylne): jak swing_vee, ramiona z dwoch stron do jednego przewodu - pierwsze
# (6202-1) do slupa na 4250-3, drugie (6212-1, model 6202 - Jan) na drugim 4250-3 przy koncu wysiegu, obroconym o 180 st.
ASSEMBLIES += '\n// jak swing_vee, z drugim zestawem ramion od slupa do tego samego przewodu (109x)\nassembly swing_vee_pair\n' + \
    re.search(r'assembly swing_vee\n(.*?)endassembly\n', ASSEMBLIES, re.S).group(1).replace(
        '  hangarms ?arms arms_bracket ?arms_head steady registration\n', '  shift arms_a_side wire 1 0\n  hangarms ?arms arms_bracket ?arms_head steady arms_a_side\n') + \
    '  shift arms_b_side wire -1 0\n  hangarms ?arms_b arms_b_bracket ?arms_b_head steady arms_b_side\nendassembly\n'
ASSEMBLIES = ASSEMBLIES.replace('//   repeat <n> <punkt> ... endrepeat               powtarzanie, az punkt sie ustali (najwyzej n razy)',
    '//   repeat <n> <punkt> ... endrepeat               powtarzanie, az punkt sie ustali (najwyzej n razy)\n//   hangarms <zestaw> <wspornik> <glowica> <rura> <strona>  zestaw ramion w osi przewodu, wspornik na rurze nad punktem holder\n//                                                 (zwis 4240 dobierany w slide_min..slide_max); strona: punkt z Y < 0 = zestaw obrocony o 180 st.\n//   vee <rola> <od> <do> <rozsuw> <os>             dwie czesci od punktu do punktow rozsunietych wzdluz toru o +-rozsuw (linka V)')
# 07xx niskie (tom 2, karty 0700-0748, 0790-0795): pozioma rura od mocowania; odciag z gory ~1,20 m
STAY_PARTS = """  line stay stay_mount toward stay_pin
  place stay_insulator stay_mount stay -y
  place stay stay@stay_insulator.length stay -y
  place stay_end stay_pin -stay +y
"""
def _level_arms(name, sign, gaps, below=False):
    # 071x (sign +1, ramiona do slupa): 4070 najwyzej 0,30 m przed 4050; 072x (-1, ramiona od slupa, zlozenia z Blendera:
    # 4070 ok. 0,7 m za 4050): 4070 najwyzej 0,30 m przed wspornikiem ramion 4290 (rysunek karty 0720)
    reference = 'head_clamp'
    extra = ''
    if sign < 0:
        reference = 'arms_at'
        extra = ("  shift bracket_at wire -arms.holder.y+arms_head.eye.y 0\n"
                 "  project strut arms_at bracket_at strut_insulator.length+strut.length 0\n")
    return ("assembly %s\n  shift below messenger 0 -1\n  hang messenger_clamp messenger below\n"
            "  tangent strut strut_mount messenger_clamp.hook head_clamp.eye head_clamp\n"
            "  limit strut head_clamp strut_insulator.length+strut.length rules.clamp_margin\n" + extra +
            "  chain strut stay_clamp stay_mount " + ('-stay_clamp.eye.z' if below else 'stay_clamp.eye') + " stay_insulator.length+stay.length stay_end " + reference + " %s\n"
            "  window strut stay_clamp strut_insulator.length+rules.clamp_margin strut_insulator.length+strut.length-rules.clamp_margin\n"
            "  side stay_pin strut strut.stay_clamp %s\n%s"
            "  place stay_clamp strut@strut.stay_clamp strut.%s +z\n  place strut_insulator strut_mount strut -y\n"
            "  place strut strut@strut_insulator.length strut -y\n  place head_clamp strut@strut.head_clamp strut.down -z\n"
            "  shift arms_side wire %d 0\n  hangarms ?arms arms_bracket ?arms_head strut arms_side\nendassembly\n") % (
                name, gaps, '-stay_clamp.eye.z' if below else 'stay_clamp.eye', STAY_PARTS, 'down' if below else 'up', sign)
def _contacts(line):
    # uchwyty przewodu 4120 wprost na rurze: zero w osi drutu, eye w osi rury (Jan); dwa druty co 0,08 m wzdluz rury
    return ("  shift wire_a wire 0.04 0\n  shift wire_b wire -0.04 0\n"
            "  place ?contact_clamp wire %s -y\n  place ?contact_clamp_a wire_a %s -y\n  place ?contact_clamp_b wire_b %s -y\n") % (line, line, line)
def _top_head(name, sign, contact):
    # 070x (uchwyty 4120 na wysiegu), 073x/074x (ramiona na wysiegu, sign jak w _level_arms): gorna pozioma rura 6003
    # z 4050 + 4140 i glowica 4500 na koncu, odciag do glowicy (zaczep odciagu przesuwany w pionie, krok anchor);
    # wysieg 6111 na wlasnym izolatorze, linka V od obejmy 4210 do liny nosnej
    target = 'clamp_eye' if contact else 'registration'
    head = "assembly %s\n" % name
    if contact: head += "  shift clamp_eye wire contact_eye.eye.y contact_eye.eye.z\n"
    return (head + "  point cord_clamp_at %s\n  repeat 6 cord_clamp_at\n    shift cord_anchor cord_clamp_at 0 cord_clamp.eye\n"
            "    hang messenger_clamp messenger cord_anchor\n    tangent strut strut_mount messenger_clamp.hook head_clamp.eye head_clamp\n"
            "    line steady_insulator steady_mount toward %s\n    point steady_start steady_insulator@steady_insulator.length\n"
            "    line steady steady_start along steady_insulator\n    project steady cord_clamp %s steady.length rules.cord_clamp_inset\n"
            "    point cord_clamp_at steady@steady.cord_clamp\n  endrepeat\n  shift cord_anchor cord_clamp_at 0 cord_clamp.eye\n"
            "  limit strut head_clamp strut_insulator.length+strut.length rules.clamp_margin\n"
            "  window strut head_clamp strut_insulator.length+strut.length-0.30 -\n"
            "  side stay_pin strut strut_insulator.length+strut.length-stay_clamp.eye.y stay_clamp.eye.z\n"
            "  anchor stay_mount stay_pin stay_insulator.length+stay.length stay_end\n%s"
            "  place stay_clamp strut@strut_insulator.length+strut.length strut -y\n  place strut_insulator strut_mount strut -y\n"
            "  place strut strut@strut_insulator.length strut -y\n  place head_clamp strut@strut.head_clamp strut.down -z\n"
            "  place steady_insulator steady_mount steady_insulator -y\n  place steady steady_start steady -y\n"
            "  place cord_clamp cord_clamp_at steady.up +z\n  vee cord cord_anchor messenger 0.125 +z\n") % (target, target, target, STAY_PARTS) + \
           (_contacts('steady') if contact else "  shift arms_side wire %d 0\n  hangarms ?arms arms_bracket ?arms_head steady arms_side\n" % sign) + "endassembly\n"
LEVEL_CONTACT = ("assembly level_contact\n  shift clamp_eye wire contact_eye.eye.y contact_eye.eye.z\n  line strut strut_mount toward clamp_eye\n"
    "  project strut contact_at clamp_eye strut_insulator.length+strut.length rules.clamp_margin\n"
    "  chain strut stay_clamp stay_mount stay_clamp.eye stay_insulator.length+stay.length stay_end contact_at rules.stay_gap_min rules.stay_gap rules.stay_gap_max\n"
    "  window strut stay_clamp strut_insulator.length+rules.clamp_margin -\n  side stay_pin strut strut.stay_clamp stay_clamp.eye\n" + STAY_PARTS +
    "  place stay_clamp strut@strut.stay_clamp strut.up +z\n  place strut_insulator strut_mount strut -y\n  place strut strut@strut_insulator.length strut -y\n"
    + _contacts('strut') + "endassembly\n")
# 108x (tom 3 s. 38-43): wysieg 6001 poziomo od dolnej czesci ukosnika (4070 uchem w dol + 4400 w osi wysiegu), wychodzi za drut,
# na koncu glowica 4500-01; linka 9811-1 od uchwytu liny 4140 do ucha 4500 (4140 wisi w jej linii), ramiona od slupa na 4290.
# Otwor 4400: srodkowy (0,24 m od ucha) - karta nie mowi, Jan: dobrac tak, zeby bylo dobrze.
INNER_PUSH = """
// 108x (tom 3): wysieg 6001 rozpierany od ukosnika (4070 + 4400, srodkowy otwor 0,24 m), glowica 4500 na koncu, linka od 4140 do 4500, ramiona od slupa
assembly inner_push
  point tip_eye registration
  repeat 8 tip_eye
    hang messenger_clamp messenger tip_eye
    tangent strut strut_mount messenger_clamp.hook head_clamp.eye head_clamp
    level strut steady_clamp registration -steady_clamp.eye
    side steady_eye strut strut.steady_clamp -steady_clamp.eye
    shift steady_start steady_eye -0.24 0
    line steady steady_start dir -1 0
    point tip steady@steady.length
    shift tip_eye tip 0 steady_head.eye.z
  endrepeat
  limit strut head_clamp strut_insulator.length+strut.length rules.clamp_margin
  window strut steady_clamp strut_insulator.length+rules.clamp_margin strut.head_clamp-rules.stay_gap_min
  chain strut stay_clamp stay_mount stay_clamp.eye stay_insulator.length+stay.length stay_end head_clamp rules.stay_gap_min rules.stay_gap rules.stay_gap_max
  window strut stay_clamp strut_insulator.length+rules.clamp_margin -
  side stay_pin strut strut.stay_clamp stay_clamp.eye
""" + STAY_PARTS + """  place stay_clamp strut@strut.stay_clamp strut.up +z
  place strut_insulator strut_mount strut -y
  place strut strut@strut_insulator.length strut -y
  place head_clamp strut@strut.head_clamp strut.down -z
  place steady_clamp strut@strut.steady_clamp strut.down +z
  place steady_end steady_eye steady +y
  place steady steady_start steady -y
  place steady_head tip steady -y
  stretch ?cord tip_eye messenger_clamp.end +z
  shift arms_side wire -1 0
  hangarms ?arms arms_bracket ?arms_head steady arms_side
endassembly
"""
# peronowe 103x/104x (tom 3 s. 17-26): ukosnik 6010 uziemiony (bez izolatora), odciag 6007 na szekli 4700 bez izolatora
# do stalego ucha 6010, lina na kolumnie 6710 (U-sruba przez dwa otwory) - blacha 6710-21 - izolator 7020 - 6720 - 4140,
# wysieg 6111 na izolatorze 7010 wiszacym na przesuwnej obejmie 6010-21, linka V od obejmy 4210 do liny
PLATFORM = """
// peronowe (tom 3, 103x/104x): ukosnik 6010 z zaczepu stycznie do okregu wokol osi blachy 6710-21 nad lina,
// 6710 w srodku najblizszej pary otworow, kolumna blacha - 7020 - 6720 - 4140 w pionie; odciag do stalego ucha 6010
assembly platform
  platform strut_mount stay_mount messenger
  point plate_eye column@plate.pivot.z-plate.eye.z
  point hanger_top column@plate.pivot.z-plate.eye.z+hanger_insulator.length
  point hanger_eye column@plate.pivot.z-plate.eye.z+hanger_insulator.length-hanger.eye.z
  point plate_at column@plate.pivot.z
  line clamp_up messenger toward hanger_eye
  line stay stay_mount toward stay_pin
  level strut steady_clamp registration steady_clamp.eye.z
  window strut steady_clamp -strut.slide_min.y -strut.slide_max.y
  side steady_eye strut strut.steady_clamp steady_clamp.eye.z
  shift steady_level steady_eye -1 0
  line steady_insulator steady_eye toward steady_level
  point steady_start steady_insulator@steady_insulator.length
  line steady steady_start along steady_insulator
  project steady cord_clamp registration steady.length rules.cord_clamp_inset
  point cord_clamp_at steady@steady.cord_clamp
  shift cord_anchor cord_clamp_at 0 cord_clamp.eye
  place ?stay_shackle stay@-stay_shackle.length stay -y
  place stay stay_mount stay -y
  place stay_end stay_pin -stay +y
  place strut strut_pin strut -y
  place steady_clamp strut@strut.steady_clamp strut -y
  place hanger_mount strut@strut.top strut -y
  place plate plate_at column -z
  place hanger_insulator hanger_top -column -y
  roll hanger_insulator -
  place hanger hanger_top column -z
  place messenger_clamp messenger clamp_up +z
  place steady_insulator steady_eye steady_insulator -y
  place steady steady_start steady -y
  place cord_clamp cord_clamp_at steady.up +z
  vee cord cord_anchor messenger 0.125 +z
  hangarms ?arms arms_bracket ?arms_head steady registration
endassembly
"""
ASSEMBLIES += ('\n// 07xx niskie (tom 2): 071x/072x jedna pozioma rura z ramionami, 070x/073x/074x gorna rura 6003 z glowica 4500 i wysieg 6111,\n'
    '// 079x pozioma rura na wysokosci drutu z uchwytami 4120 (bez liny nosnej). anchor <punkt> <cel> <dlugosc> <czesc z otworami>:\n'
    '// zaczep przesuwany w pionie, az lancuch z najblizszym otworem siegnie celu\n'
    + _level_arms('level_arms_in', 1, 'rules.stay_gap_min rules.stay_gap rules.stay_gap_max')
    + _level_arms('level_arms_out', -1, '0.05 0.20 0.30')
    + '\n// 100x (tom 3): pozioma rura z 4050 + 4140, rozpora 6002 z dolu do uchwytu 4500-01 na rurze (ucho w dol)\n'
    + _level_arms('level_push_in', 1, 'rules.stay_gap_min rules.stay_gap rules.stay_gap_max', below=True)
    + INNER_PUSH
    + '\n// 106x (tom 3 s. 32-37): jak inner_push, do tego obejmy 4210 na lukach ramion 6207 (cord_nom) i linki z nich do liny nosnej:\n'
    + '// para - kazde ramie pionowo do liny, przesuniete wzdluz toru o +-0,125 m (~0,25 m na karcie); pojedyncze - linka w V\n'
    + INNER_PUSH.replace('assembly inner_push', 'assembly inner_push_cords').replace('endassembly', '  armcords ?arms_cord ?arms_cord_clamp messenger 0.125\nendassembly')
    + PLATFORM
    + _top_head('top_head_contact', 1, True) + _top_head('top_head_in', 1, False) + _top_head('top_head_out', -1, False) + LEVEL_CONTACT)
# 1099 (tom 3 s. 52): jak inner_push (wysieg 6001 od ukosnika, 4500 na koncu), linka w V od 4500 do liny nosnej, ramiona z dwoch
# stron do jednego przewodu na dwoch 4280-3 (6202-1 do slupa, 6212-1 = 6202 od slupa)
ASSEMBLIES += ('\n// jak inner_push, linka V od glowicy do liny, dwa zestawy ramion z dwoch stron (1099)\nassembly inner_push_pair\n' +
    re.search(r'assembly inner_push\n(.*?)endassembly\n', ASSEMBLIES, re.S).group(1)
      .replace('  stretch ?cord tip_eye messenger_clamp.end +z\n', '  vee cord tip_eye messenger 0.125 +z\n')
      .replace('  shift arms_side wire -1 0\n  hangarms ?arms arms_bracket ?arms_head steady arms_side\n',
               '  shift arms_side wire 1 0\n  hangarms ?arms arms_bracket ?arms_head steady arms_side\n  shift arms_b_side wire -1 0\n  hangarms ?arms_b arms_b_bracket ?arms_b_head steady arms_b_side\n') +
    'endassembly\n')
# 1267-1269 (tom 3 s. 152-154): jak inner_push, linka V od glowicy 4500 do liny nosnej, jedno ramie od slupa
ASSEMBLIES += ('\n// jak inner_push, linka V od glowicy do liny (1267-1269)\nassembly inner_push_vee\n' +
    re.search(r'assembly inner_push\n(.*?)endassembly\n', ASSEMBLIES, re.S).group(1)
      .replace('  stretch ?cord tip_eye messenger_clamp.end +z\n', '  vee cord tip_eye messenger 0.125 +z\n') + 'endassembly\n')
for l in ASSEMBLIES.splitlines(): w(l)
w()

# zestawy ramion odciagowych i wsporniki (modele Jana 2026-10-14, punkty odczytane z plikow t3d)
ARMSETS = {'6201-01': (0.8210, 0.0000), '6201-02': (0.8590, 0.0000), '6201-03': (1.1104, 0.0015), '6201-04': (1.1563, -0.0023),
           '6202-01': (0.7656, 0.1791), '6203-01': (0.8817, 0.0219), '6204-02': (0.7958, 0.1794), '6205-02': (0.8278, 0.0219)}
w('// zestawy ramion: gotowy model, zero w osi przewodu jezdnego, holder = punkt zawieszenia na wsporniku')
for name, (y, z) in sorted(ARMSETS.items()):
    w('part %s kind arms point holder %s %s endpart' % (name, f(y), f(z)))
w('// wsporniki ramion na rurze wysiegu: zero na spodzie rury; 4240 = zwis (-02 krotki, -04 dlugi) + glowica (-21 jedna lapka, -22 dwie)')
w('part 4240-02 kind arm_holder point slide_min 0.0000 -0.0971 point slide_max 0.0000 -0.2521 endpart')
w('part 4240-04 kind arm_holder point slide_min 0.0000 -0.0971 point slide_max 0.0000 -0.3621 endpart')
w('part 4240-21 kind arm_holder_head point eye -0.0500 0.0000 endpart')
w('part 4240-22 kind arm_holder_head point eye -0.0450 0.0000 endpart')
for n in range(1, 5): w('part 4250-%02d kind arm_holder point eye 0.0000 -0.0300 endpart' % n)
for n in range(1, 3): w('part 4260-%02d kind arm_holder point eye 0.0000 -0.1898 endpart' % n)
# 4290: korpus z 7 otworami co 90 mm (punkty hole_N z modelu), glowica -21 jedna lapka, -22 dwie (Jan); korpus -02 na rure 38 mm, -04 na teownik, -06 na rure 60 mm (Jan); 07xx maja 6011, wiec -02
for n in (2, 4, 6): w('part 4290-%02d kind arm_holder %s endpart' % (n, ' '.join('point hole_%d 0.0000 %s' % (k + 1, f(-0.0934 - 0.09 * k)) for k in range(7))))
for n in (21, 22): w('part 4290-%02d kind arm_holder_head point eye -0.0699 0.0000 endpart' % n)
w()
# zestaw ramion dla karty i wykonania (tabele kart i zlozenia z Blendera): (zestaw, wspornik, glowica)
def arms_for(card, wyk):
    group = card // 10
    straight = group in (111, 112, 121, 122)
    curve = group in (113, 114, 123, 124)
    if not (straight or curve): return []
    table = {1: ('6201-02' if straight else '6201-04', '4240-02', '4240-22'),
             2: ('6204-02' if straight else '6205-02', '4250-04' if straight else '4260-02', None),
             3: ('6202-01' if straight else '6203-01', '4250-03' if straight else '4260-01', None),
             4: ('6202-01' if straight else '6203-01', '4250-03' if straight else '4260-01', None),
             5: ('6201-01' if straight else '6201-03', '4240-02', '4240-21')}
    arms, bracket, head = table[int(wyk)]
    roles = [('arms', arms), ('arms_bracket', bracket)]
    if head: roles.append(('arms_head', head))
    return roles

w('// ---- schematy: karta i wykonanie -----------------------------------------------------------------------------')
w('// reach: pasmo A z karty (do czola konstrukcji, razem z mocowaniem); height: wysokosc konstrukcyjna')
w('// mount: nominalne wysokosci zaczepow izolatorow nad przewodem jezdnym; assembly: sposob skladania; role <rola> <czesc>')
ref = io.StringIO()
r = lambda s='': ref.write(s + '\n')
r('# Punkty wzorcowe zlozen 11xx z Blendera (etap0/zlozenia.json) do testu solvera wysiegnika; plik generowany.')
r('# nazwa karta wyk | zaczepy: odciag(y z) ukosnik(y z) wysieg pomocniczy(y z albo - -) | lina(y z)')
r('# | kierunek ukosnika(dy dz) | uchwyt glowny(y z) uchwyt odciagu(y z) uchwyt dolny(y z albo - -) hak wysiegu(y z albo - -) koncowka 4400(y z) obejma linki(y z)')
for k in K:
    v = D[k]; a = parts(k)
    card = v['card']; digit = int(card[3])
    three = '6111' in a
    strut = a['6001'][0]; stay = a['6007'][0]; steady = (a['6111'] if three else a['6011'])[0]
    hk = v['anchors']['Lina_nosna'][2]
    roles = [('stay_shackle', a['4700'][0]['part']), ('stay_insulator', a['7020'][0]['part']), ('stay', stay['part']),
             ('stay_end', a['4400'][0]['part']), ('stay_clamp', a['4070'][0]['part']),
             ('strut_insulator', a['7100'][0]['part']), ('strut', strut['part']), ('head_clamp', a['4050'][0]['part']),
             ('messenger_clamp', a['4140'][0]['part']), ('cord', a['4490'][0]['part']), ('cord_clamp', a['4210'][0]['part']),
             ('steady', steady['part'])]
    roles.append(('steady_insulator', a['7010'][0]['part']) if three else ('steady_clamp', a['4050'][0]['part']))
    line = 'scheme %s %s reach %s %s height %s registration %sx-%s' % (card, v['wyk'], f(1.45 + 0.3 * digit), f(1.75 + 0.3 * digit), f(hk), card[:3], v['wyk'])
    line += ' mount stay %s mount strut %s' % (f(a['7020'][0]['pos'][2] if card != '1118' else 1.90), f(a['7100'][0]['pos'][2]))
    if three: line += ' mount steady %s' % f(a['7010'][0]['pos'][2])
    if v['wyk'] in ('1', '5', 1, 5): line += TUBE_4240
    line += ' assembly %s' % ('swing_steady' if three else 'swing')
    roles += arms_for(int(card), v['wyk'])
    for role, part in roles: line += ' role %s %s' % (role, RENAMED.get(part, part))
    w(line + ' endscheme')
    # punkty wzorcowe
    d = [-strut['rot'][1][1], -strut['rot'][2][1]]           # kierunek rury: lokalne -Y
    o = strut['pos'][1:]
    def onaxis(p):
        s = (p['pos'][1] - o[0]) * d[0] + (p['pos'][2] - o[1]) * d[1]
        return (o[0] + s * d[0], o[1] + s * d[1])
    heads = sorted(a['4050'], key=lambda p: -p['pos'][2])
    T = a['7020'][0]['pos']; H = a['7100'][0]['pos']
    vals = [T[1], T[2], H[1], H[2]]
    vals += list(a['7010'][0]['pos'][1:]) if three else [None, None]
    vals += [v['anchors']['Lina_nosna'][1], hk, d[0], d[1]]
    vals += list(onaxis(heads[0])) + list(onaxis(a['4070'][0]))
    vals += list(onaxis(heads[1])) if not three else [None, None]
    vals += [steady['pos'][1] + 0.0206, steady['pos'][2]] if not three else [None, None]   # os haka 6011 lezy 2 cm od zera modelu
    vals += list(a['4400'][0]['pos'][1:]) + list(a['4210'][0]['pos'][1:])
    r('%s %s %s %s' % (k, card, v['wyk'], ' '.join('-' if x is None else f(x) for x in vals)))

# ---- 12xx (tom 3, karty od s. 101): tabele czesci odczytane z kart 2026-10-13
# tabele kart odczytane OCR-em komorek (linie siatki -> wiersze i kolumny, tesseract --psm 7); 126x maja tylko wykonania 2-4.
# Wszystkie karty podaja uchwyt liny 4140-2: ten sam co 4140-01 (inny producent, te same wymiary – Jan).
# 1267-1269 (glowica 4500, wysieg z rury 6001) jeszcze nie: inna mechanika.
C12 = {   # (karta, wykonanie): (ukosnik 21, rura odciagu 22, wysieg pomocniczy 23)
    (1210, 1): ('6001-01', '6002-01', '6111-01'), (1210, 2): ('6001-01', '6002-01', '6111-01'), (1210, 3): ('6001-01', '6002-01', '6111-01'), (1210, 4): ('6001-01', '6002-01', '6111-01'), (1210, 5): ('6001-01', '6002-01', '6111-01'),
    (1211, 1): ('6001-03', '6002-02', '6011-02'), (1211, 2): ('6001-03', '6002-02', '6011-02'), (1211, 3): ('6001-03', '6002-02', '6011-02'), (1211, 4): ('6001-02', '6002-02', '6011-02'), (1211, 5): ('6001-03', '6002-02', '6011-02'),
    (1212, 1): ('6001-04', '6002-03', '6011-03'), (1212, 2): ('6001-04', '6002-03', '6011-03'), (1212, 3): ('6001-04', '6002-03', '6011-03'), (1212, 4): ('6001-03', '6002-03', '6011-03'), (1212, 5): ('6001-04', '6002-03', '6011-03'),
    (1213, 1): ('6001-05', '6002-04', '6011-04'), (1213, 2): ('6001-05', '6002-04', '6011-04'), (1213, 3): ('6001-05', '6002-04', '6011-04'), (1213, 4): ('6001-04', '6002-04', '6011-04'), (1213, 5): ('6001-05', '6002-04', '6011-04'),
    (1214, 1): ('6001-06', '6002-05', '6011-05'), (1214, 2): ('6001-06', '6002-05', '6011-05'), (1214, 3): ('6001-06', '6002-05', '6011-05'), (1214, 4): ('6001-05', '6002-05', '6011-05'), (1214, 5): ('6001-06', '6002-05', '6011-05'),
    (1215, 1): ('6001-07', '6002-06', '6011-06'), (1215, 2): ('6001-07', '6002-06', '6011-06'), (1215, 3): ('6001-07', '6002-06', '6011-06'), (1215, 4): ('6001-06', '6002-06', '6011-06'), (1215, 5): ('6001-07', '6002-06', '6011-06'),
    (1216, 1): ('6001-08', '6002-07', '6011-07'), (1216, 2): ('6001-08', '6002-07', '6011-07'), (1216, 3): ('6001-08', '6002-07', '6011-07'), (1216, 4): ('6001-07', '6002-07', '6011-07'), (1216, 5): ('6001-08', '6002-07', '6011-07'),
    (1217, 1): ('6001-09', '6002-08', '6011-08'), (1217, 2): ('6001-09', '6002-08', '6011-08'), (1217, 3): ('6001-09', '6002-08', '6011-08'), (1217, 4): ('6001-08', '6002-08', '6011-08'), (1217, 5): ('6001-09', '6002-08', '6011-08'),
    (1220, 1): ('6001-01', '6002-01', '6111-05'), (1220, 2): ('6001-01', '6002-01', '6111-05'), (1220, 3): ('6001-01', '6002-01', '6111-05'), (1220, 4): ('6001-01', '6002-01', '6111-05'), (1220, 5): ('6001-01', '6002-01', '6111-05'),
    (1221, 1): ('6001-03', '6002-02', '6011-07'), (1221, 2): ('6001-03', '6002-02', '6011-07'), (1221, 3): ('6001-03', '6002-02', '6011-07'), (1221, 4): ('6001-02', '6002-02', '6011-07'), (1221, 5): ('6001-03', '6002-02', '6011-07'),
    (1222, 1): ('6001-04', '6002-03', '6011-08'), (1222, 2): ('6001-04', '6002-03', '6011-08'), (1222, 3): ('6001-04', '6002-03', '6011-08'), (1222, 4): ('6001-03', '6002-03', '6011-08'), (1222, 5): ('6001-04', '6002-03', '6011-08'),
    (1223, 1): ('6001-05', '6002-04', '6011-09'), (1223, 2): ('6001-05', '6002-04', '6011-09'), (1223, 3): ('6001-05', '6002-04', '6011-09'), (1223, 4): ('6001-04', '6002-04', '6011-09'), (1223, 5): ('6001-05', '6002-04', '6011-09'),
    (1224, 1): ('6001-06', '6002-05', '6011-10'), (1224, 2): ('6001-06', '6002-05', '6011-10'), (1224, 3): ('6001-06', '6002-05', '6011-10'), (1224, 4): ('6001-05', '6002-05', '6011-10'), (1224, 5): ('6001-06', '6002-05', '6011-10'),
    (1225, 1): ('6001-07', '6002-06', '6011-11'), (1225, 2): ('6001-07', '6002-06', '6011-11'), (1225, 3): ('6001-07', '6002-06', '6011-11'), (1225, 4): ('6001-06', '6002-06', '6011-11'), (1225, 5): ('6001-07', '6002-06', '6011-11'),
    (1226, 1): ('6001-08', '6002-07', '6011-12'), (1226, 2): ('6001-08', '6002-07', '6011-12'), (1226, 3): ('6001-08', '6002-07', '6011-12'), (1226, 4): ('6001-07', '6002-07', '6011-12'), (1226, 5): ('6001-08', '6002-07', '6011-12'),
    (1227, 1): ('6001-09', '6002-08', '6011-13'), (1227, 2): ('6001-09', '6002-08', '6011-13'), (1227, 3): ('6001-09', '6002-08', '6011-13'), (1227, 4): ('6001-08', '6002-08', '6011-13'), (1227, 5): ('6001-09', '6002-08', '6011-13'),
    (1230, 1): ('6001-01', '6002-01', '6111-01'), (1230, 2): ('6001-01', '6002-01', '6111-01'), (1230, 3): ('6001-01', '6002-01', '6111-01'), (1230, 4): ('6001-01', '6002-01', '6111-01'), (1230, 5): ('6001-01', '6002-01', '6111-01'),
    (1231, 1): ('6001-02', '6002-02', '6111-01'), (1231, 2): ('6001-02', '6002-02', '6111-01'), (1231, 3): ('6001-02', '6002-02', '6111-01'), (1231, 4): ('6001-02', '6002-02', '6111-01'), (1231, 5): ('6001-02', '6002-02', '6111-01'),
    (1232, 1): ('6001-04', '6002-03', '6011-03'), (1232, 2): ('6001-04', '6002-03', '6011-03'), (1232, 3): ('6001-04', '6002-03', '6011-03'), (1232, 4): ('6001-03', '6002-03', '6011-03'), (1232, 5): ('6001-04', '6002-03', '6011-03'),
    (1233, 1): ('6001-05', '6002-04', '6011-04'), (1233, 2): ('6001-05', '6002-04', '6011-04'), (1233, 3): ('6001-05', '6002-04', '6011-04'), (1233, 4): ('6001-04', '6002-04', '6011-04'), (1233, 5): ('6001-05', '6002-04', '6011-04'),
    (1234, 1): ('6001-06', '6002-05', '6011-05'), (1234, 2): ('6001-06', '6002-05', '6011-05'), (1234, 3): ('6001-06', '6002-05', '6011-05'), (1234, 4): ('6001-05', '6002-05', '6011-05'), (1234, 5): ('6001-06', '6002-05', '6011-05'),
    (1235, 1): ('6001-07', '6002-06', '6011-06'), (1235, 2): ('6001-07', '6002-06', '6011-06'), (1235, 3): ('6001-07', '6002-06', '6011-06'), (1235, 4): ('6001-06', '6002-06', '6011-06'), (1235, 5): ('6001-07', '6002-06', '6011-06'),
    (1236, 1): ('6001-08', '6002-07', '6011-07'), (1236, 2): ('6001-08', '6002-07', '6011-07'), (1236, 3): ('6001-08', '6002-07', '6011-07'), (1236, 4): ('6001-07', '6002-07', '6011-07'), (1236, 5): ('6001-08', '6002-07', '6011-07'),
    (1237, 1): ('6001-09', '6002-08', '6011-08'), (1237, 2): ('6001-09', '6002-08', '6011-08'), (1237, 3): ('6001-09', '6002-08', '6011-08'), (1237, 4): ('6001-08', '6002-08', '6011-08'), (1237, 5): ('6001-09', '6002-08', '6011-08'),
    (1240, 1): ('6001-01', '6002-01', '6111-06'), (1240, 2): ('6001-01', '6002-01', '6111-05'), (1240, 3): ('6001-01', '6002-01', '6111-05'), (1240, 4): ('6001-01', '6002-01', '6111-05'), (1240, 5): ('6001-01', '6002-01', '6111-06'),
    (1241, 1): ('6001-03', '6002-02', '6011-08'), (1241, 2): ('6001-03', '6002-02', '6011-07'), (1241, 3): ('6001-03', '6002-02', '6011-07'), (1241, 4): ('6001-02', '6002-02', '6011-07'), (1241, 5): ('6001-03', '6002-02', '6011-08'),
    (1242, 1): ('6001-04', '6002-03', '6011-09'), (1242, 2): ('6001-04', '6002-03', '6011-08'), (1242, 3): ('6001-04', '6002-03', '6011-08'), (1242, 4): ('6001-03', '6002-03', '6011-08'), (1242, 5): ('6001-04', '6002-03', '6011-09'),
    (1243, 1): ('6001-05', '6002-04', '6011-10'), (1243, 2): ('6001-05', '6002-04', '6011-09'), (1243, 3): ('6001-05', '6002-04', '6011-09'), (1243, 4): ('6001-04', '6002-04', '6011-09'), (1243, 5): ('6001-05', '6002-04', '6011-10'),
    (1244, 1): ('6001-06', '6002-05', '6011-11'), (1244, 2): ('6001-06', '6002-05', '6011-10'), (1244, 3): ('6001-06', '6002-05', '6011-10'), (1244, 4): ('6001-05', '6002-05', '6011-10'), (1244, 5): ('6001-06', '6002-05', '6011-11'),
    (1245, 1): ('6001-07', '6002-06', '6011-12'), (1245, 2): ('6001-07', '6002-06', '6011-11'), (1245, 3): ('6001-07', '6002-06', '6011-11'), (1245, 4): ('6001-06', '6002-06', '6011-11'), (1245, 5): ('6001-07', '6002-06', '6011-12'),
    (1246, 1): ('6001-08', '6002-07', '6011-13'), (1246, 2): ('6001-08', '6002-07', '6011-12'), (1246, 3): ('6001-08', '6002-07', '6011-12'), (1246, 4): ('6001-07', '6002-07', '6011-12'), (1246, 5): ('6001-08', '6002-07', '6011-13'),
    (1247, 1): ('6001-09', '6002-08', '6011-14'), (1247, 2): ('6001-09', '6002-08', '6011-13'), (1247, 3): ('6001-09', '6002-08', '6011-13'), (1247, 4): ('6001-08', '6002-08', '6011-13'), (1247, 5): ('6001-09', '6002-08', '6011-14'),
    (1260, 2): ('6001-01', '6002-01', '6111-06'), (1260, 3): ('6001-01', '6002-01', '6111-06'), (1260, 4): ('6001-01', '6002-01', '6111-06'),
    (1261, 2): ('6001-03', '6002-02', '6011-08'), (1261, 3): ('6001-03', '6002-02', '6011-08'), (1261, 4): ('6001-02', '6002-02', '6011-08'),
    (1262, 2): ('6001-04', '6002-03', '6011-09'), (1262, 3): ('6001-04', '6002-03', '6011-09'), (1262, 4): ('6001-03', '6002-03', '6011-09'),
    (1263, 2): ('6001-05', '6002-04', '6011-10'), (1263, 3): ('6001-05', '6002-04', '6011-10'), (1263, 4): ('6001-04', '6002-04', '6011-10'),
    (1264, 2): ('6001-06', '6002-05', '6011-11'), (1264, 3): ('6001-06', '6002-05', '6011-11'), (1264, 4): ('6001-05', '6002-05', '6011-11'),
    (1265, 2): ('6001-07', '6002-06', '6011-12'), (1265, 3): ('6001-07', '6002-06', '6011-12'), (1265, 4): ('6001-06', '6002-06', '6011-12'),
    (1266, 2): ('6001-08', '6002-07', '6011-13'), (1266, 3): ('6001-08', '6002-07', '6011-13'), (1266, 4): ('6001-07', '6002-07', '6011-13'),
}
REG12 = {121: '111x', 122: '112x', 123: '113x', 124: '114x', 126: '112x'}   # zestawy ramion: karty 11xx z tym samym odsuwem
w('// 12xx nieprzechylne: odciag sztywny z rury 6002 (izolator 7100, koncowka 4400-01, bez szekli); zaczepy jak w 11xx')
for (card, wyk), (strut, stay, steady) in sorted(C12.items()):
    digit = card % 10
    low = wyk == 4
    line = 'scheme %d %d reach %s %s height %s registration %s-%d' % (card, wyk, f(1.45 + 0.3 * digit), f(1.75 + 0.3 * digit), f(1.30 if low else 1.70), REG12[card // 10], wyk)
    three = steady.startswith('6111')
    if three:   # jak 1110: trzy zaczepy (wartosci ze zlozenia 1110)
        line += ' mount stay %s mount strut %s mount steady %s' % (f(1.3784 if low else 1.7784), f(0.3784 if low else 0.7784), f(0.2297 if wyk in (2, 3, 4) else 0.2200))
    else:
        line += ' mount stay %s mount strut %s' % (f(1.50 if low else 1.90), f(-0.10))
    if wyk in (1, 5) and card // 10 in (121, 122, 123, 124): line += TUBE_4240   # 4240-1 / 4240-2 w tabelach
    line += ' assembly %s' % ('swing_steady' if three else 'swing')
    roles = [('stay_insulator', '7100-01'), ('stay', stay), ('stay_end', '4400-01'), ('stay_clamp', '4070-01'),
             ('strut_insulator', '7100-01'), ('strut', strut), ('head_clamp', '4050-01'),
             ('messenger_clamp', '4140-01'), ('cord', '4490-00'), ('cord_clamp', '4210-02'), ('steady', steady)]
    roles.append(('steady_insulator', '7010-01') if three else ('steady_clamp', '4050-01'))
    roles += arms_for(card, wyk)
    for role, part in roles: line += ' role %s %s' % (role, part)
    w(line + ' endscheme')
w()

# 129x: samo podwieszenie liny nosnej (tom 3 s. 171-178), jedno wykonanie (w katalogu "0", tu 1). Izolatory ~1,00 m w pionie.
# ZALOZENIE do potwierdzenia: lina na wysokosci 1,70 nad punktem zerowym ukladu, gorny zaczep 0,20 m nad lina (jak w 11xx),
# dolny 1,00 m nizej; pasmo A liczone do osi liny nosnej.
w('registration 129x tube 0.0000 clamp 0.0000 endregistration   // brak ramion odciagowych')
for n in range(8):
    card = 1290 + n
    line = 'scheme %d 1 reach %s %s height 1.7000 registration 129x mount stay 1.9000 mount strut 0.9000 assembly messenger_only' % (card, f(1.45 + 0.3 * n), f(1.75 + 0.3 * n))
    for role, part in [('stay_insulator', '7100-01'), ('stay', '6002-%02d' % (n + 1)), ('stay_end', '4400-01'), ('stay_clamp', '4070-01'),
                       ('strut_insulator', '7100-01'), ('strut', '6001-%02d' % (n + 1)), ('head_clamp', '4050-01'), ('messenger_clamp', '4140-01')]:
        line += ' role %s %s' % (role, part)
    w(line + ' endscheme')
w()
# 120x przesla naprezenia (s. 101-106), wykonania 1-4: wyk. 1, 3 izolatory ~2,00 m w pionie, wyk. 2, 4 ~1,60 m (karta 1201);
# 128x rozjazdowe podwojne od slupa (s. 161-167), wykonania 3 i 4. Obie z linka V. 1287-1289 (glowica 4500) jeszcze nie.
# ZALOZENIE: przy 120x obejma 4210 w osi przewodu jezdnego (na rysunku tuz za osia), wysokosc rury 0,22 m.
w('registration 120x tube 0.2200 clamp 0.0000 endregistration   // zalozenie, do sprawdzenia z modelami ramion')
CV = {}
for n in range(6):   # 1201-1206: ukosnik 6001-(n+3) dla wyk. 1, 3, 6001-(n+2) dla 2, 4; 6002-(n+2); 6011-(n+4)
    for wyk in range(1, 5):
        CV[(1201 + n, wyk)] = ('6001-%02d' % (n + (3 if wyk in (1, 3) else 2)), '6002-%02d' % (n + 2), '6011-%02d' % (n + 4), '120x')
CV.update({(1280, 3): ('6001-01', '6002-01', '6111-06', '112x'), (1280, 4): ('6001-01', '6002-01', '6111-06', '112x')})
for n in range(1, 8):   # 1281-1287? tylko 1281-1286 (1287 ma glowice 4500)
    if 1280 + n >= 1287: break
    CV[(1280 + n, 3)] = ('6001-%02d' % (n + 2), '6002-%02d' % (n + 1), '6011-%02d' % (n + 7), '112x')
    CV[(1280 + n, 4)] = ('6001-%02d' % (n + 1), '6002-%02d' % (n + 1), '6011-%02d' % (n + 7), '112x')
for (card, wyk), (strut, stay, steady, reg) in sorted(CV.items()):
    digit = card % 10
    low = (wyk in (2, 4)) if card < 1210 else wyk == 4
    line = 'scheme %d %d reach %s %s height %s registration %s' % (card, wyk, f(1.45 + 0.3 * digit), f(1.75 + 0.3 * digit), f(1.30 if low else 1.70), reg + ('' if reg == '120x' else '-%d' % wyk))
    three = steady.startswith('6111')
    if three:
        line += ' mount stay %s mount strut %s mount steady %s' % (f(1.3784 if low else 1.7784), f(0.3784 if low else 0.7784), f(0.2297))
    else:
        line += ' mount stay %s mount strut %s' % (f(1.50 if low else 1.90), f(-0.10))
    line += ' assembly %s' % ('swing_steady_vee' if three else 'swing_vee')
    roles = [('stay_insulator', '7100-01'), ('stay', stay), ('stay_end', '4400-01'), ('stay_clamp', '4070-01'),
             ('strut_insulator', '7100-01'), ('strut', strut), ('head_clamp', '4050-01'),
             ('messenger_clamp', '4140-01'), ('cord', '4490-00'), ('cord_clamp', '4210-02'), ('steady', steady)]
    roles.append(('steady_insulator', '7010-01') if three else ('steady_clamp', '4050-01'))
    roles += arms_for(card, wyk)
    for role, part in roles: line += ' role %s %s' % (role, part)
    w(line + ' endscheme')
w()
# 07xx (tom 2): nietypowe wysokosci konstrukcyjne, wykonania 2 (dwudrutowa) i 3 (jednodrutowa), czesci w obu takie same
# (rozni sie uchwyt ramion: 4290-02 przy dwoch przewodach, 4290-01 przy jednym). 075x/076x (s. 62-71): hk 1,30-1,80, jak 12xx;
# 077x/078x (s. 72-83): hk > 1,80, trzy zaczepy, wysieg 6111 na wlasnym izolatorze 7010.
# Czesci z tabel kart; wysokosci liny, zaczepow i polozenie obejmy 4210 ze zlozen Jana z Blendera (wyk. 3).
# Uchwyt ramion 4290: 7 polozen co 90 mm (100-640 mm pod rura, Jan); rura 0,2325 m nad przewodem przy polozeniu najwyzszym
# (zlozenia 077x: 0,233; 075x/076x: 0,412-0,415 = dwa skoki nizej), tube_steps = kolejne podniesienia.
C07 = {}
for n in range(1, 6):
    C07[750 + n] = ('6001-%02d' % (n + 1), '6002-%02d' % (n + 1), '6011-%02d' % (n + 1))
    C07[760 + n] = ('6001-%02d' % (n + 1), '6002-%02d' % (n + 1), '6011-%02d' % (n + 6))
for n, steady in enumerate([1, 1, 2, 3, 4, 5]):
    C07[770 + n] = ('6001-%02d' % (n + 1), '6002-%02d' % (n + 1), '6111-%02d' % steady)
for n, steady in enumerate([5, 6, 7, 7, 8, 8]):
    C07[780 + n] = ('6001-%02d' % (n + 1), '6002-%02d' % (n + 1), '6111-%02d' % steady)
TUBE_4290 = 0.2325
w('// 07xx: nietypowe wysokosci konstrukcyjne (tom 2); wysokosci ze zlozen z Blendera, uchwyt ramion 4290 regulowany (tube_steps)')
for card, (strut, stay, steady) in sorted(C07.items()):
    v = D['%04d-03' % card]
    P = v['parts']
    hk = v['anchors']['Lina_nosna'][2]
    ins = sorted(p['pos'][2] for p in P if p['part'].startswith('7100'))
    clamp = [p['pos'][1] for p in P if p['part'].startswith('4210')][0]
    w('registration %04d tube %s clamp %s endregistration   // zlozenie %04d-03' % (card, f(TUBE_4290), f(clamp), card))
    three = steady.startswith('6111')
    for wyk in (2, 3):
        digit = card % 10
        line = 'scheme %d %d reach %s %s height %s registration %04d' % (card, wyk, f(1.45 + 0.3 * digit), f(1.75 + 0.3 * digit), f(hk), card)
        line += ' mount stay %s mount strut %s' % (f(ins[1]), f(ins[0]))
        if three: line += ' mount steady %s' % f([p['pos'][2] for p in P if p['part'].startswith('7010')][0])
        line += ' tube_steps ' + ' '.join(f(0.09 * k) for k in range(7))
        tube = [p['pos'][2] for p in P if p['part'][:4] in ('6011', '6111')][0]
        line += ' tube_prefer %s' % f(round((tube - TUBE_4290) / 0.09) * 0.09)   # polozenie ze zlozenia
        line += ' assembly %s' % ('swing_steady' if three else 'swing')
        roles = [('stay_insulator', '7100-01'), ('stay', stay), ('stay_end', '4400-01'), ('stay_clamp', '4070-01'),
                 ('strut_insulator', '7100-01'), ('strut', strut), ('head_clamp', '4050-01'),
                 ('messenger_clamp', '4140-01'), ('cord', '4490-00'), ('cord_clamp', '4210-02'), ('steady', steady)]
        roles.append(('steady_insulator', '7010-01') if three else ('steady_clamp', '4050-01'))
        # ramiona z tabel tomu 2: wyk. 2 dwa przewody (6205-1 + 6205-2), wyk. 3 jeden (6203-1); 075x/076x na 4290, 077x/078x na 4260
        double = wyk == 2
        if card // 10 in (75, 76): roles += [('arms', '6205-02' if double else '6203-01'), ('arms_bracket', '4290-06'), ('arms_head', '4290-22' if double else '4290-21')]   # 6001/6002 maja 60 mm (Jan)
        else: roles += [('arms', '6205-02' if double else '6203-01'), ('arms_bracket', '4260-02' if double else '4260-01')]
        for role, part in roles: line += ' role %s %s' % (role, part)
        w(line + ' endscheme')
w()
# 07xx niskie: czesci z tabel kart (tom 2, s. 30-61, 84-89), wysokosci izolatorow ze zlozen z Blendera (izolatory sa tam dobre,
# rury pospawane z izolatorami - Jan). Modele 4120-03/-04, 4500-02, 6003 jeszcze nie istnieja: punkty tymczasowe ze zlozen.
w('// 07xx niskie: uchwyty przewodu na rurze 4120 (zero w osi drutu, eye w osi rury - Jan), glowica 4500 (zero w osi rury na jej koncu,')
w('// eye = sworzen 4400), rury 6003 (modele Jana)')
for n, z in ((1, 0.044), (2, 0.0504), (3, 0.0519), (4, 0.0629)): w('part 4120-%02d kind contact_clamp point eye 0.0000 %s endpart   // model Jana 2026-10-14' % (n, f(z)))
for n in (1, 2): w('part 4500-%02d kind head point eye 0.0000 0.0630 endpart   // model Jana 2026-10-14' % n)
# 6003 (modele Jana 2026-10-14): zero w otworze na izolator, end = otwor montazowy 3 cm przed koncem rury (tam glowica 4500),
# eye = otwor 8 cm od poczatku (nie wiadomo, co tam idzie); length = do konca rury
E6003 = {1: 0.64, 2: 0.94, 3: 1.24, 4: 1.54, 5: 1.84, 6: 2.14, 7: 2.44, 8: 2.74, 9: 3.39, 10: 3.69}
for n, e in sorted(E6003.items()): w('part 6003-%02d kind strut length %s pin 0.0300 radius 0.0300 endpart   // model: otwor end %.2f m od zera' % (n, f(e + 0.03), e))
def _blend(key):
    v = D.get(key)
    if v is None: return None
    P = v['parts']
    ins = sorted(p['pos'][2] for p in P if p['part'].startswith('7100'))
    st = [p['pos'][2] for p in P if p['part'].startswith('7010')]
    reg = [p['pos'][1] for p in P if p['part'].startswith('4210')]
    return dict(hk=v['anchors']['Lina_nosna'][2], strut=ins[0], stay=ins[1], steady=st[0] if st else None, clamp=reg[0] if reg else 0.0)
T = lambda a, b: [int(x) for x in b]
LOW = []
for n in range(6):      # 070x: 6003-(3..8), 6002-(3..8), 6111-(3,4,5,5,6,6); 4120-3 x2 (wyk. 2) / x1 (wyk. 3)
    for wyk in (2, 3): LOW.append((700 + n, wyk, 'top_head_contact', '6003-%02d' % (n + 3), '6002-%02d' % (n + 3), '6111-%02d' % [3, 4, 5, 5, 6, 6][n], '0700-%02d' % wyk))
for n in range(6):      # 071x: 6003-(3..8) poziomo (Jan 2026-10-17: 6002 ma sworzen do 4400, tu niepotrzebny), odciag 6002-(2..7); 072x: 6001-(3,5,6,7,8,9), odciag 6002-(5,6,7,8,9,9)
    for wyk in (2, 3):
        LOW.append((710 + n, wyk, 'level_arms_in', '6003-%02d' % (n + 3), '6002-%02d' % (n + 2), None, '%04d-%02d' % (710 + n, wyk)))
        LOW.append((720 + n, wyk, 'level_arms_out', '6001-%02d' % [3, 5, 6, 7, 8, 9][n], '6002-%02d' % [5, 6, 7, 8, 9, 9][n], None, '%04d-%02d' % (720 + n, wyk)))
for i, n in enumerate([0, 1, 2, 3, 4, 5, 8]):   # 073x/074x: 6003-(3..8,10), 6002-(3..8,9); 6111 073x (2..6,6,7), 074x (5,6,7,7,8,8,8)
    for wyk in (1, 2, 3):
        LOW.append((730 + n, wyk, 'top_head_in', '6003-%02d' % [3, 4, 5, 6, 7, 8, 10][i], '6002-%02d' % [3, 4, 5, 6, 7, 8, 9][i], '6111-%02d' % [2, 3, 4, 5, 6, 6, 7][i], '%04d-%03d' % (730 + n, wyk)))
        LOW.append((740 + n, wyk, 'top_head_out', '6003-%02d' % [3, 4, 5, 6, 7, 8, 10][i], '6002-%02d' % [3, 4, 5, 6, 7, 8, 9][i], '6111-%02d' % [5, 6, 7, 7, 8, 8, 14][i], '%04d-%03d' % (740 + n, wyk)))
for n in range(6):      # 079x: pozioma 6003-(3..8) (jak 071x) na wysokosci drutu, odciag 6002-(2..7); 4120-4 x2 (wyk. 1) / x1 (wyk. 3)
    for wyk in (1, 3): LOW.append((790 + n, wyk, 'level_contact', '6003-%02d' % (n + 3), '6002-%02d' % (n + 2), None, '0790-%02d' % wyk))
w('// 07xx niskie: wysokosci zaczepow i hk ze zlozen z Blendera (079x wszystkie z 0790); registration = os wysiegu i obejma 4210')
for card, wyk, asm, strut, stay, steady, key in sorted(LOW):
    b_ = _blend(key)
    reg = '%04d-%d' % (card, wyk)
    hk = b_['hk'] if card // 10 != 79 else 1.0
    if steady: w('registration %s tube %s clamp %s endregistration' % (reg, f(b_['steady']), f(b_['clamp'])))
    else: w('registration %s tube 0.0000 clamp 0.0000 endregistration' % reg)
    digit = card % 10
    line = 'scheme %d %d reach %s %s height %s registration %s mount stay %s mount strut %s' % (card, wyk, f(1.45 + 0.3 * digit), f(1.75 + 0.3 * digit), f(hk), reg, f(b_['stay']), f(b_['strut']))
    if steady: line += ' mount steady %s' % f(b_['steady'])
    line += ' assembly %s' % asm
    roles = [('stay_insulator', '7100-01'), ('stay', stay), ('stay_end', '4400-01'), ('strut_insulator', '7100-01'), ('strut', strut)]
    double = (wyk == 2) if card // 10 in (70, 71, 72) else (wyk == 1)
    g = card // 10
    if g in (71, 72):
        roles += [('stay_clamp', '4070-01'), ('head_clamp', '4050-01'), ('messenger_clamp', '4140-01'),
                  ('arms', '6205-02' if double else '6203-01'), ('arms_bracket', '4290-06'), ('arms_head', '4290-22' if double else '4290-21')]   # 6001/6002 maja 60 mm (Jan)
    if g in (70, 73, 74):
        roles += [('stay_clamp', '4500-02'), ('head_clamp', '4050-01'), ('messenger_clamp', '4140-01'), ('steady_insulator', '7010-01'), ('steady', steady),
                  ('cord', '4490-00'), ('cord_clamp', '4210-02')]
    if g == 70:
        roles += [('contact_clamp_a', '4120-03'), ('contact_clamp_b', '4120-03'), ('contact_eye', '4120-03')] if double else [('contact_clamp', '4120-03'), ('contact_eye', '4120-03')]
    if g in (73, 74):
        roles += {1: [('arms', '6201-02'), ('arms_bracket', '4240-02'), ('arms_head', '4240-22')],
                  2: [('arms', '6205-02'), ('arms_bracket', '4260-02')], 3: [('arms', '6203-01'), ('arms_bracket', '4260-01')]}[wyk]
    if g == 79:
        roles += [('stay_clamp', '4070-01')] + ([('contact_clamp_a', '4120-04'), ('contact_clamp_b', '4120-04'), ('contact_eye', '4120-04')] if double else [('contact_clamp', '4120-04'), ('contact_eye', '4120-04')])
    for role, part in roles: line += ' role %s %s' % (role, part)
    w(line + ' endscheme')
w()
# 100x/101x (tom 3 s. 4-11): samo podwieszenie liny nosnej w przesle naprezenia, nieprzechylne, jedno wykonanie.
# Pozioma rura 6002 (poz. 21) z 4050 + 4140 nad lina, drugi izolator ~1,20 m nizej (100x, rozpora 6002 z dolu do 4500-01)
# albo wyzej (101x, odciag 6002 z gory do 4070); 4500/4070 najwyzej 0,30 m przed 4050. Pasmo A do osi liny, jak 129x.
# Wysokosc rury: lina 1,70 + hak 4140 + ucho 4050, zeby rura od izolatora byla pozioma. 1005: w tabeli 7100-2 - tu 7100-01.
# Pozioma rura (poz. 21) jako 6003 zamiast 6002 (Jan 2026-10-17, probnie): tu nie laczy sie z 4400, a 6002 ma w modelu sworzen
# do 4400; 6003 ma te same wymiary i punkty (jeden wiecej). Rozpora/odciag zostaje 6002, bo idzie do 4400-01.
w('// 100x/101x: samo podwieszenie liny nosnej w przesle naprezenia (tom 3 s. 4-11)')
w('registration 10xx tube 0.0000 clamp 0.0000 endregistration   // brak ramion i wysiegu')
TUBE10 = 1.70 + 0.0751 + 0.0800
for n in range(4):
    for base, asm, clamp, dz in ((1002, 'level_push_in', '4500-01', -1.20), (1012, 'level_arms_in', '4070-01', 1.20)):
        card = base + n
        digit = card % 10
        line = 'scheme %d 1 reach %s %s height 1.7000 registration 10xx mount stay %s mount strut %s assembly %s' % (
            card, f(1.45 + 0.3 * digit), f(1.75 + 0.3 * digit), f(TUBE10 + dz), f(TUBE10), asm)
        for role, part in [('stay_insulator', '7100-01'), ('stay', '6002-%02d' % (n + 4)), ('stay_end', '4400-01'), ('stay_clamp', clamp),
                           ('strut_insulator', '7100-01'), ('strut', '6003-%02d' % (n + 5)), ('head_clamp', '4050-01'), ('messenger_clamp', '4140-01')]:
            line += ' role %s %s' % (role, part)
        w(line + ' endscheme')
# 108x: hk 1,20 (Jan), izolatory ~1,60 m (gorny 0,20 m nad lina), wysieg na wysokosci, przy ktorej trzeci otwor 4290-06 trafia
# w punkt zawieszenia ramion 6205-02/6203-01 (0,0219 nad drutem): 0,0219 + 0,03 + 0,0934 + 2 * 0,09 = 0,3253 ... (patrz TUBE108)
TUBE108 = 0.0219 + 0.0300 + 0.0934 + 0.09 * 3
w('registration 108x tube %s clamp 0.0000 endregistration   // os wysiegu 6001 (4. otwor 4290 w punkcie zawieszenia ramion)' % f(TUBE108))
for n in range(6):
    card = 1081 + n
    strut, stay, steady = '6001-%02d' % (n + 2), '6002-%02d' % (n + 2), '6001-%02d' % [2, 2, 3, 3, 4, 5][n]
    for wyk in (2, 4):
        line = 'scheme %d %d reach %s %s height 1.2000 registration 108x mount stay 1.4000 mount strut -0.2000 assembly inner_push' % (card, wyk, f(1.45 + 0.3 * (n + 1)), f(1.75 + 0.3 * (n + 1)))
        roles = [('stay_insulator', '7100-01'), ('stay', stay), ('stay_end', '4400-01'), ('stay_clamp', '4070-01'),
                 ('strut_insulator', '7100-01'), ('strut', strut), ('head_clamp', '4050-01'), ('messenger_clamp', '4140-01'),
                 ('steady_clamp', '4070-01'), ('steady_end', '4400-01'), ('steady', steady), ('steady_head', '4500-01'), ('cord', '4490-00')]
        roles += [('arms', '6205-02'), ('arms_bracket', '4290-06'), ('arms_head', '4290-22')] if wyk == 2 else [('arms', '6203-01'), ('arms_bracket', '4290-06'), ('arms_head', '4290-21')]
        for role, part in roles: line += ' role %s %s' % (role, part)
        w(line + ' endscheme')
# 106x (tom 3 s. 32-37) - NA PROBE tylko 1061 wyk. 1: mechanika 108x (inner_push), ramiona 6207-1 + 6207-2 na 4290-6.
# Ramiona 6207 to osobne modele Jana (2026-10-17): zero w osi drutu (z 4110 osobno, w tym samym punkcie), holder = ucho, cord_* = obejma 4210.
# Para sztywna (Jan 15:28): 6207-01 (krotsze) -0,13 m w X i -4,75 st. wokol Z, 6207-02 (dluzsze) +0,15 m w X i +4,75 st.;
# w Y dluzsze o 0,1*cos(4,75) dalej od ucha, zeby oba ucha wisialy na jednym sworzniu 4290-22 (rozstaw drutow ~10 cm).
# Zero zestawu w polowie miedzy drutami. Wysieg tak wysoko, zeby hole_6 4290 trafil w ucho (rura nad lukiem 6207, gora luku 0,464).
import math
YAW6207 = 4.75
c6207 = math.cos(math.radians(YAW6207))
h1, h2 = 1.4 * c6207, 1.5 * c6207          # Y ucha po obrocie
y1, y2 = 0.0, h1 - h2                        # 6207-02 dalej od ucha
mid = 0.5 * (y1 + y2)
w('// 6207: ramiona odciagowe specjalne lukowe, osobne modele (zero w osi drutu, 4110-01 osobno w tym samym punkcie); cord_* - zakres obejmy 4210')
w('part 4110-01 kind clamp endpart   // uchwyt przewodu jezdnego, zero w osi drutu')
w('part 6207-01 kind arms point holder 1.4000 -0.0060 point cord_nom 0.0000 0.4451 point cord_min 0.8258 0.4451 point cord_max -0.1323 0.4451 pointx holder -0.0093 pointx cord_nom -0.0099 pointx cord_min -0.0059 pointx cord_max -0.0068 endpart   // punkty X po poprawce Jana 19:25')
w('part 6207-02 kind arms point holder 1.5000 -0.0060 point cord_nom 0.0000 0.4451 point cord_min 0.9287 0.4451 point cord_max -0.1323 0.4451 pointx holder -0.0046 pointx cord_nom -0.0074 pointx cord_min -0.0119 pointx cord_max -0.0074 endpart')
w('part 6207-12 kind arms point holder %s -0.0060%s endpart   // para sztywna 6207-1 + 6207-2 (Jan 2026-10-17), zero w polowie miedzy drutami' % (
    f(h1 - mid), ''.join(' member %s %s %s 0.0000 %s' % (pp, f(x), f(y - mid), f(a)) for pp, x, y, a in
                         (('6207-01', -0.13, y1, -YAW6207), ('4110-01', -0.13, y1, -YAW6207), ('6207-02', 0.15, y2, YAW6207), ('4110-01', 0.15, y2, YAW6207)))))
# 6208 (Jan 2026-10-17 17:20): para sztywna jak 6207 - 6208-01 -0,13 m w X i -4 st., 6208-02 +0,15 m i +4 st.; zero w osi drutu, 4110 osobno;
# 6208-03 pojedyncze, komplet z 4110. W kartach tomu 3 6208 nie wystepuje (gdzie chodzi - nie ustalono), schematow jeszcze nie ma.
c6208 = math.cos(math.radians(4.0))
g1, g2 = 1.45883 * c6208, 1.55883 * c6208
gm = 0.5 * (g1 - g2)
w('part 6208-01 kind arms point holder 1.4588 0.0000 point cord_nom 0.0000 0.2283 point cord_min 1.3380 0.2283 point cord_max -0.1232 0.2283 pointx cord_nom -0.0074 pointx cord_min -0.0074 pointx cord_max -0.0074 endpart')
w('part 6208-02 kind arms point holder 1.5588 0.0000 point cord_nom 0.0000 0.2283 point cord_min 1.4406 0.2283 point cord_max -0.1218 0.2283 pointx cord_nom -0.0074 pointx cord_min -0.0074 pointx cord_max -0.0074 endpart')
w('part 6208-03 kind arms point holder 1.4588 0.0000 point cord_nom 0.0000 0.2283 point cord_min 1.3380 0.2283 point cord_max -0.1244 0.2283 endpart   // pojedyncze, komplet z 4110')
w('part 6208-12 kind arms point holder %s 0.0000%s endpart   // para sztywna 6208-1 + 6208-2 (Jan 2026-10-17), zero w polowie miedzy drutami' % (
    f(g1 - gm), ''.join(' member %s %s %s 0.0000 %s' % (pp, f(x), f(y), f(a)) for pp, x, y, a in
                        (('6208-01', -0.13, -gm, -4.0), ('4110-01', -0.13, -gm, -4.0), ('6208-02', 0.15, (g1 - g2) - gm, 4.0), ('4110-01', 0.15, (g1 - g2) - gm, 4.0)))))
TUBE106 = -0.006 + 0.5434 + 0.03
w('registration 106x tube %s clamp 0.0000 endregistration   // os wysiegu 6001: hole_6 4290 w uchu 6207' % f(TUBE106))
w('part 6207-03 kind arms point holder 1.4000 -0.0060 point cord_nom 0.0000 0.4451 point cord_min 0.8258 0.4451 point cord_max -0.1323 0.4451 endpart   // pojedyncze, komplet z 4110')
# 1061: wyk. 1 para 6207-1 + 6207-2 na 4290-6 (glowica podwojna 4290-22); wyk. 3, 4 pojedyncze 6207-3 na 4290-5 (= 4290-06 + glowica 4290-21,
# jak w 108x); wyk. 4 niskie: hk 1,30, ukosnik 6001-2, wysieg 6001-3, zaczepy 1,50 / -0,10 (jak 11xx)
# 1061-1066 (tabele s. 32-37): wyk. 1, 3 ukosnik 6001-(n+2), wysieg 6001-(n+3); wyk. 4 o numer nizej; odciag 6002-(n+1); n = karta - 1060
for n in range(1, 7):
    card = 1060 + n
    for wyk, hk, stay_mount, arms, head in ((1, 1.70, 1.90, '6207-12', '4290-22'), (3, 1.70, 1.90, '6207-03', '4290-21'), (4, 1.30, 1.50, '6207-03', '4290-21')):
        low = 1 if wyk == 4 else 0
        strut, steady = '6001-%02d' % (n + 2 - low), '6001-%02d' % (n + 3 - low)
        line = 'scheme %d %d reach %s %s height %s registration 106x mount stay %s mount strut -0.1000 assembly inner_push_cords' % (
            card, wyk, f(1.45 + 0.3 * n), f(1.75 + 0.3 * n), f(hk), f(stay_mount))
        for role, part in [('stay_insulator', '7100-01'), ('stay', '6002-%02d' % (n + 1)), ('stay_end', '4400-01'), ('stay_clamp', '4070-01'),
                           ('strut_insulator', '7100-01'), ('strut', strut), ('head_clamp', '4050-01'), ('messenger_clamp', '4140-01'),
                           ('steady_clamp', '4070-01'), ('steady_end', '4400-01'), ('steady', steady), ('steady_head', '4500-01'), ('cord', '4490-00'),
                           ('arms', arms), ('arms_bracket', '4290-06'), ('arms_head', head), ('arms_cord', '4490-00'), ('arms_cord_clamp', '4210-02')]:
            line += ' role %s %s' % (role, part)
        w(line + ' endscheme')
# 102x: 1022-1026 wyk. 1 (hk 1,30, izolatory ~2,00) i 2 (hk 1,50, ~1,60); ZALOZENIE: gorny zaczep wyk. 1 na wysokosci hk (odciag 22
# poziomo przy linie na rysunku), wyk. 2 1,40 - inaczej dolny zaczep (-0,10) wypada za wysoko i 4050 wysiegu siada na izolatorze; ukosnik 6001-(n+2), odciag 6002-(n+1), wysieg 6011-(n+1) / 6011-(n+2); obejma linki V 0,16 m za A (rysunek).
w('part 4760-01 kind clamp eye 0.0462 endpart   // uchwyt plaskownika na ukosniku (model Jana 2026-10-17), ucho 46 mm pod osia rury')
w('part 6506-01 kind link length 0.1500 endpart   // plaskownik 6506 (model Jana; plik 6505-01!), 0,15 m miedzy otworami, zero w dolnym otworze')
w('registration 102x tube 0.0519 clamp -0.1600 endregistration   // rura wysiegu 4120-3 nad przewodami, obejma linki V za przewodami')
for n in range(2, 7):
    for wyk, hk, span, top in ((1, 1.30, 2.00, 1.30), (2, 1.50, 1.60, 1.40)):
        line = 'scheme %d %d reach %s %s height %s registration 102x mount stay %s mount strut %s assembly double_net' % (
            1020 + n, wyk, f(1.45 + 0.3 * n), f(1.75 + 0.3 * n), f(hk), f(top), f(top - span))
        for role, part in [('stay_insulator', '7100-01'), ('stay', '6002-%02d' % (n + 1)), ('stay_end', '4400-01'), ('stay_clamp', '4070-01'),
                           ('strut_insulator', '7100-01'), ('strut', '6001-%02d' % (n + 2)), ('head_clamp', '4050-01'), ('messenger_clamp', '4140-01'),
                           ('cord', '4490-00'), ('cord_clamp', '4210-02'), ('steady', '6011-%02d' % (n + wyk)), ('steady_clamp', '4050-01'),
                           ('contact_clamp_a', '4120-03'), ('contact_clamp_b', '4120-03'), ('head_clamp_b', '4050-01'), ('messenger_clamp_b', '4140-01'),
                           ('link', '6506-01'), ('link_clamp', '4760-01'), ('link_cord_clamp', '4210-02')]:
            line += ' role %s %s' % (role, part)
        w(line + ' endscheme')
w('part 6212-01 kind arms point holder 0.7656 0.1791 endpart   // ramie 6212-1 bez uchwytu przewodu (model Jana 2026-10-17), zawieszenie jak 6202')
w('part 6206-01 kind arms point holder 1.2100 0.0792 endpart   // pojedyncze stale, komplet z 4110 (model Jana 2026-10-17)')
w('part 6206-02 kind arms point holder 1.3700 0.1192 endpart   // pojedyncze stale, komplet z 4110 (model Jana 2026-10-17)')
# 6206-11/-12 (Jan 17:59): ruchome ramiona (6206-1 / 6206-2) pod osobne 4110, zero w osi drutu. Para w 1287-1289 to dwa 6206-1 (tabela):
# jedno +0,13 m w X i +5 st., drugie -0,14 m i -5 st. (Jan) - ucha schodza sie ok. 6 cm od siebie na 4280-4, druty w tej samej linii Y.
w('part 6206-11 kind arms point holder 1.2100 0.0792 endpart   // ruchome 6206-1 bez 4110 (model Jana 2026-10-17)')
w('part 6206-12 kind arms point holder 1.3700 0.1192 endpart   // ruchome 6206-2 bez 4110 (model Jana 2026-10-17)')
w('part 6206-1111 kind arms point holder %s 0.0792 member 6206-11 0.1300 0.0000 0.0000 5.0000 member 4110-01 0.1300 0.0000 0.0000 5.0000 '
  'member 6206-11 -0.1400 0.0000 0.0000 -5.0000 member 4110-01 -0.1400 0.0000 0.0000 -5.0000 endpart   // para 2x 6206-1 (1287-1289)' % f(1.21 * math.cos(math.radians(5.0))))
w('registration 1287 tube %s clamp 0.0000 endregistration   // os wysiegu 6001: ucho 4280-4 w punkcie zawieszenia 6206-11' % f(0.0792 + 0.196 + 0.03))
for card, pipes, stay in ((1287, ((9, 10), (8, 9)), 8), (1288, ((10, 11), (9, 10)), 11), (1289, ((14, 14), (11, 11)), 9)):
    n = card % 10
    for wyk, hk, stay_mount in ((3, 1.70, 1.90), (4, 1.30, 1.50)):
        strut, steady = pipes[0] if wyk == 3 else pipes[1]
        line = 'scheme %d %d reach %s %s height %s registration 1287 mount stay %s mount strut -0.1000 assembly inner_push_vee' % (
            card, wyk, f(1.45 + 0.3 * n), f(1.75 + 0.3 * n), f(hk), f(stay_mount))
        for role, part in [('stay_insulator', '7100-01'), ('stay', '6002-%02d' % stay), ('stay_end', '4400-01'), ('stay_clamp', '4070-01'),
                           ('strut_insulator', '7100-01'), ('strut', '6001-%02d' % strut), ('head_clamp', '4050-01'), ('messenger_clamp', '4140-01'),
                           ('steady_clamp', '4070-01'), ('steady_end', '4400-01'), ('steady', '6001-%02d' % steady), ('steady_head', '4500-01'),
                           ('cord', '4490-00'), ('arms', '6206-1111'), ('arms_bracket', '4280-04')]:
            line += ' role %s %s' % (role, part)
        w(line + ' endscheme')
# 1267-1269 wyk. 3, 4: 6206-1 od slupa na 4280-3, wysieg 6001 od ukosnika (4070 + 4400), 4500-1, linka V; wyk. 2 (para 6209) - czeka na modele.
# Rury z tabel: 1267 ukosnik/wysieg 6001-9/-10 (wyk. 4: -8/-9), odciag 6002-8; 1268: -10/-11 (-9/-10), 6002-11; 1269: -14/-14 (-11/-11), 6002-9
w('registration 1267 tube %s clamp 0.0000 endregistration   // os wysiegu 6001: ucho 4280-3 w punkcie zawieszenia 6206-01' % f(0.0792 + 0.196 + 0.03))
for card, pipes, stay in ((1267, ((9, 10), (8, 9)), 8), (1268, ((10, 11), (9, 10)), 11), (1269, ((14, 14), (11, 11)), 9)):
    n = card % 10
    for wyk, hk, stay_mount in ((3, 1.70, 1.90), (4, 1.30, 1.50)):
        strut, steady = pipes[0] if wyk == 3 else pipes[1]
        line = 'scheme %d %d reach %s %s height %s registration 1267 mount stay %s mount strut -0.1000 assembly inner_push_vee' % (
            card, wyk, f(1.45 + 0.3 * n), f(1.75 + 0.3 * n), f(hk), f(stay_mount))
        for role, part in [('stay_insulator', '7100-01'), ('stay', '6002-%02d' % stay), ('stay_end', '4400-01'), ('stay_clamp', '4070-01'),
                           ('strut_insulator', '7100-01'), ('strut', '6001-%02d' % strut), ('head_clamp', '4050-01'), ('messenger_clamp', '4140-01'),
                           ('steady_clamp', '4070-01'), ('steady_end', '4400-01'), ('steady', '6001-%02d' % steady), ('steady_head', '4500-01'),
                           ('cord', '4490-00'), ('arms', '6206-01'), ('arms_bracket', '4280-03')]:
            line += ' role %s %s' % (role, part)
        w(line + ' endscheme')
# 1099 wyk. 3 (hk 1,70, ukosnik i wysieg 6001-14) i 4 (hk 1,30, 6001-11), odciag 6002-9; 4500-1 na koncu (karta: 4500-2 z odreczna
# uwaga "nie moze byc do 6001, powinno byc 4500-1"); uchwyty 4280-3: zero na spodzie rury, ucho 0,196 m nizej (model Jana)
for n in (1, 3, 4):
    w('part 4280-%02d kind arm_holder point eye 0.0000 -0.1960 endpart   // model Jana: zero na spodzie rury, ucho 196 mm nizej' % n)
w('registration 1099 tube %s clamp 0.0000 endregistration   // os wysiegu 6001: ucho 4280-3 w punkcie zawieszenia 6202-01' % f(0.17914 + 0.196 + 0.03))
for wyk, hk, stay_mount, pipe in ((3, 1.70, 1.90, '6001-14'), (4, 1.30, 1.50, '6001-11')):
    line = 'scheme 1099 %d reach 4.1500 4.4500 height %s registration 1099 mount stay %s mount strut -0.1000 assembly inner_push_pair' % (wyk, f(hk), f(stay_mount))
    for role, part in [('stay_insulator', '7100-01'), ('stay', '6002-09'), ('stay_end', '4400-01'), ('stay_clamp', '4070-01'),
                       ('strut_insulator', '7100-01'), ('strut', pipe), ('head_clamp', '4050-01'), ('messenger_clamp', '4140-01'),
                       ('steady_clamp', '4070-01'), ('steady_end', '4400-01'), ('steady', pipe), ('steady_head', '4500-01'), ('cord', '4490-00'),
                       ('arms', '6202-01'), ('arms_bracket', '4280-03'), ('arms_b', '6212-01'), ('arms_b_bracket', '4280-03')]:
        line += ' role %s %s' % (role, part)
    w(line + ' endscheme')
# 109x: 1091-1098 wyk. 3 i 4 (1099 to inna mechanika - osobno). Ukosnik 6001-(n+2) (wyk. 4: n+1), odciag linowy 6007-(n+1) na 4700 + 7020,
# wysieg 6011-(n+6); ramiona 6202-01 (zestaw z 4110) dwa razy na 4250-03; obejma linki przy koncu wysiegu (jak 112x), linka V.
w('registration 109x tube 0.2284 clamp -1.1450 endregistration   // rura jak 111x-3 (6202 na 4250-3), obejma linki przy koncu wysiegu')
for n in range(1, 9):
    for wyk, hk, stay_mount in ((3, 1.70, 1.90), (4, 1.30, 1.50)):
        line = 'scheme %d %d reach %s %s height %s registration 109x mount stay %s mount strut -0.1000 assembly swing_vee_pair' % (
            1090 + n, wyk, f(1.45 + 0.3 * n), f(1.75 + 0.3 * n), f(hk), f(stay_mount))
        for role, part in [('stay_shackle', '4700-01'), ('stay_insulator', '7020-01'), ('stay', '6007-%02d' % (n + 1)), ('stay_end', '4400-02'),
                           ('stay_clamp', '4070-01'), ('strut_insulator', '7100-01'), ('strut', '6001-%02d' % (n + (2 if wyk == 3 else 1))),
                           ('head_clamp', '4050-01'), ('messenger_clamp', '4140-01'), ('cord', '4490-00'), ('cord_clamp', '4210-02'),
                           ('steady', '6011-%02d' % (n + 6)), ('steady_clamp', '4050-01'),
                           ('arms', '6202-01'), ('arms_bracket', '4250-03'), ('arms_b', '6212-01'), ('arms_b_bracket', '4250-03')]:
            line += ' role %s %s' % (role, part)
        w(line + ' endscheme')
# 105x: 1052-1056 wyk. 2, 3, 4 (sieci zewnetrzne, do slupa): wyk. 2 para 6207-1 + 6207-2 (zestaw 6207-12), wyk. 3, 4 pojedyncze 6207-3;
# hk wyk. 2, 4 1,20 (izolatory ~1,60: 1,40 / -0,20 jak 108x), wyk. 3 1,70 (1,90 / -0,10); ukosnik 6001-(n+1), wyk. 3 6001-(n+2); odciag 6007-(n+1)
# 125x: 1251-1254, 1257 wyk. 2, 3, 4 (do slupa), wyk. 2 para 6205-02, wyk. 3, 4 6203-01, bez linek; hk wyk. 2, 3 1,70, wyk. 4 1,30 (jak 12xx);
# ukosnik 6001-(n+2), wyk. 4 6001-(n+1); odciag 6007-(n+1). 1250: lina z wlasnego zaczepu (rope_arms_mount). 127x - czeka na modele.
w('registration ropex tube 0.0000 clamp 0.0000 endregistration   // ramiona na linie: bez wysiegu')
# 127x (tom 3 s. 155-160, ramiona podwojne): para ruchomych 6203-11 pod osobne 4110 na jednej linie - na probe (Jan 18:05) +-5 st.
# i symetrycznie +-0,13 m w X; tylko wyk. 3, 4; rury i hk jak 125x (1270 jak 1250: lina z wlasnego zaczepu)
w('part 6203-11 kind arms point holder 0.8817 0.0219 endpart   // ruchome 6203-1 bez 4110 (model Jana 2026-10-17)')
w('part 6203-1111 kind arms point holder %s 0.0219 member 6203-11 0.1300 0.0000 0.0000 5.0000 member 4110-01 0.1300 0.0000 0.0000 5.0000 '
  'member 6203-11 -0.1300 0.0000 0.0000 -5.0000 member 4110-01 -0.1300 0.0000 0.0000 -5.0000 endpart   // para 2x 6203-1 (127x), na probe' % f(0.88169 * math.cos(math.radians(5.0))))
for card in list(range(1052, 1057)) + [1250, 1251, 1252, 1253, 1254, 1257] + [1270, 1271, 1272, 1273, 1274, 1277]:
    n = card % 10
    for wyk in (2, 3, 4):
        if card >= 1270 and wyk == 2: continue
        if card < 1100:
            hk, stay_mount, strut_mount = ((1.70, 1.90, -0.10) if wyk == 3 else (1.20, 1.40, -0.20))
            strut = '6001-%02d' % (n + (2 if wyk == 3 else 1))
            arms = '6207-12' if wyk == 2 else '6207-03'
            extra = [('arms_cord', '4490-00'), ('arms_cord_clamp', '4210-02')]
        else:
            hk, stay_mount, strut_mount = ((1.30, 1.50, -0.10) if wyk == 4 else (1.70, 1.90, -0.10))
            strut = '6001-%02d' % (n + (1 if wyk == 4 else 2))
            arms = '6205-02' if wyk == 2 else ('6203-1111' if card >= 1270 else '6203-01')
            extra = []
        own = ''
        if card in (1250, 1270):
            # 1250 (Jan): gorny - dolny izolator (lina) 1,60 m (wyk. 2, 3) / 1,20 m (wyk. 4), gorny - srodkowy (ukosnik) ~1,00 m; stara hk z 12xx
            span = 1.20 if wyk == 4 else 1.60
            stay_mount, strut_mount = hk + 0.20, hk + 0.20 - 1.00
            own = ' mount steady %s' % f(hk + 0.20 - span)
            strut = '6001-01'
        line = 'scheme %d %d reach %s %s height %s registration ropex mount stay %s mount strut %s%s assembly %s' % (
            card, wyk, f(1.45 + 0.3 * n), f(1.75 + 0.3 * n), f(hk), f(stay_mount), f(strut_mount), own, 'rope_arms_mount' if own else 'rope_arms')
        for role, part in [('stay_shackle', '4700-01'), ('stay_insulator', '7020-01'), ('stay', '6007-%02d' % (n + 1)), ('stay_end', '4400-02'),
                           ('stay_clamp', '4070-01'), ('strut_insulator', '7100-01'), ('strut', strut), ('head_clamp', '4050-01'),
                           ('messenger_clamp', '4140-01'), ('rope', '4490-00'), ('arms', arms)] + extra + \
                          ([('rope_insulator', '7020-01')] if own else [('rope_clamp', '4050-01')]):
            line += ' role %s %s' % (role, part)
        w(line + ' endscheme')
# 117x (1172-1176, do slupa) i 118x (1181-1186, od slupa): tylko wyk. 1 -> hk 1,70 (objasnienia tomu 3 s. 53, Jan); odstep izolatorow
# ~1,60 m z karty: zaczepy 1,50 / -0,10 (dolny pod wysiegiem jak w 12xx; gorny idzie za lina nosna, gdy ta wyzej). Pasmo A z karty do przewodu na 4120-3, w danych przesuniete o 0,08 m
# do przewodu ramion; zaczep drugiego przewodu w wyniku jako second_wire.y/.z. Ukosnik 6001-(n+2), odciag 6002-(n+1), wysieg 6011-(n+2) (117x)
# albo 6011-(n+8) (118x), n = ostatnia cyfra karty. 117x: ramiona 6201-1 do slupa na 4240-2, obejma linki w osi przewodu ramion;
# 118x: 6201-3 od slupa na 4240-2, obejma przy koncu wysiegu jak 114x-5.
w('registration 117x tube 0.2200 clamp 0.0000 endregistration   // jak 111x-5, obejma linki nad przewodem ramion (karta 1176)')
for base, nums, steady_off, arms, reg, asm in ((1170, range(2, 7), 2, '6201-01', '117x', 'swing_vee_wire_out'),
                                               (1180, range(1, 7), 8, '6201-03', '114x-5', 'swing_vee_wire_in')):
    for n in nums:
        shift = -0.08 if base == 1170 else 0.08   # pasmo A z karty do przewodu na 4120-3; reach w danych do przewodu ramion
        line = 'scheme %d 1 reach %s %s height 1.7000 registration %s mount stay 1.5000 mount strut -0.1000 assembly %s' % (
            base + n, f(1.45 + 0.3 * n + shift), f(1.75 + 0.3 * n + shift), reg, asm)
        for role, part in [('stay_insulator', '7100-01'), ('stay', '6002-%02d' % (n + 1)), ('stay_end', '4400-01'), ('stay_clamp', '4070-01'),
                           ('strut_insulator', '7100-01'), ('strut', '6001-%02d' % (n + 2)), ('head_clamp', '4050-01'), ('messenger_clamp', '4140-01'),
                           ('cord', '4490-00'), ('cord_clamp', '4210-02'), ('steady', '6011-%02d' % (n + steady_off)), ('steady_clamp', '4050-01'),
                           ('arms', arms), ('arms_bracket', '4240-02'), ('arms_head', '4240-21'), ('second_clamp', '4120-03')]:
            line += ' role %s %s' % (role, part)
        w(line + ' endscheme')
# peronowe 103x/104x: czesci (modele Jana 2026-10-15, karta 23-6010 z UZUPELNIENIA nr 2)
L6010 = {1: 5.350, 2: 5.600, 3: 5.900, 4: 6.150}
A6010 = {1: 1.950, 2: 2.050, 3: 2.150, 4: 2.250}   # wymiar a z karty (od oka na dole) - tylko punkt informacyjny, opaske ustawia wysokosc wysiegu
w('// peronowe: 6010 = ukosnik specjalny dla skrajni 5,00 m (rura 108, zero w oku na dole, rura w -Y); hole_N - otwory na U-srube 6710')
w('// co 50 mm od gory; eye - stale ucho odciagu 0,31 m od gory, 0,10 m nad osia (modele obrocone 2026-10-16); slide_min/max - zakres opaski 6010-21')
for n, L in sorted(L6010.items()):
    pts = ' '.join('point hole_%d %s 0.0000' % (k, f(-(L - 0.05 * k))) for k in range(1, 5))
    w('part 6010-%02d kind strut length %s %s point eye %s 0.1001 point slide_min %s 0.0000 point slide_max %s 0.0000 point slide_nom %s 0.0000 endpart' % (n, f(L), pts, f(-(L - 0.30936)), f(-0.15823), f(-(L - 0.36685)), f(-A6010[n])))
w('part 6010-21 kind clamp point eye 0.0000 -0.1000 endpart   // opaska wysiegu na 6010, regulowana w slide_min..slide_max')
w('part 6710-01 kind clamp point eye 0.0000 -0.0909 endpart   // U-sruba przez dwa otwory 6010; zero w osi rury miedzy otworami, eye = os obrotu blachy')
w('part 6710-21 kind clamp point pivot 0.0000 -0.0909 point eye 0.0000 -0.1502 endpart   // blacha 6710, obraca sie wokol X w pivot; zero = zero 6710-01')
w('part 6720-01 kind clamp point eye 0.0000 -0.0813 endpart   // lacznik pod izolatorem 7020, zero na gorze, eye = zaczep uchwytu liny')
w('registration 103x-1 tube 0.2200 clamp 0.1689 endregistration   // jak 111x-1 (do slupa)')
w('registration 103x-2 tube 0.2284 clamp 0.1689 endregistration   // jak 111x-2')
w('registration 103y-1 tube 0.2200 clamp -1.1450 endregistration   // jak 112x-1 (od slupa)')
w('registration 103y-2 tube 0.2297 clamp -1.1450 endregistration   // jak 112x-2')
import math
def _platform_mount(L, D):
    # nominalny kat ukosnika: os blachy (srodek par 2-3, 0,125 m od gory) nad lina w odleglosci D od oka ukosnika
    s_ = L - 0.125; r_ = 0.0909; lo, hi = 0.05, 1.5
    for _ in range(80):
        a = 0.5 * (lo + hi)
        if s_ * math.cos(a) + r_ * math.sin(a) - 0.0062 > D: lo = a
        else: hi = a
    rise = s_ * math.sin(a) - r_ * math.cos(a)
    return 0.0751 + 0.0813 + 0.32 + 0.0593 - rise
PERON = {   # karta: (A, ukosnik 6010, odciag 6007, wysieg 6111, od slupa, luk); 1036 ma w tabeli 6010-1 - przyjete 6010-3 jak w blizniaczej 1046 (z 6010-1 nie da sie zlozyc)
    1031: (4.70, 1, 11, 5, False, False), 1032: (5.00, 2, 12, 5, False, False), 1033: (5.30, 3, 13, 6, False, False),
    1036: (5.00, 3, 13, 8, True, False), 1037: (5.30, 4, 14, 8, True, False),
    1041: (4.70, 1, 11, 5, False, True), 1042: (5.00, 2, 12, 5, False, True), 1043: (5.30, 3, 13, 6, False, True),
    1046: (5.00, 3, 13, 8, True, True), 1047: (5.30, 4, 14, 8, True, True)}
w('// peronowe: A stale (do czola slupa razem z mocowaniem, oko 6010 ok. 0,23 m od czola), hk 1,70 (wymiar a karty 6010 jest dla hk 1,70),')
w('// zaczepy 2,60 m od siebie; wysokosc dolnego z nominalnego kata ukosnika (os blachy nad lina)')
for card, (A, ns, nst, nw, outward, curve) in sorted(PERON.items()):
    strut_z = 1.70 + _platform_mount(L6010[ns], A - 0.2285)
    for wyk in (1, 2):
        reg = "103%s-%d" % ("y" if outward else "x", wyk)
        line = 'scheme %d %d reach %s %s height 1.7000 registration %s mount stay %s mount strut %s assembly platform' % (card, wyk, f(A), f(A), reg, f(strut_z + 2.60), f(strut_z))
        roles = [('stay_shackle', '4700-01'), ('stay', '6007-%02d' % nst), ('stay_end', '4400-02'), ('strut', '6010-%02d' % ns),
                 ('steady_clamp', '6010-21'), ('hanger_mount', '6710-01'), ('plate', '6710-21'), ('hanger_insulator', '7020-01'),
                 ('hanger', '6720-01'), ('messenger_clamp', '4140-01'), ('steady_insulator', '7010-01'), ('steady', '6111-%02d' % nw),
                 ('cord', '4490-00'), ('cord_clamp', '4210-02')]
        if wyk == 1: roles += [('arms', '6201-04' if curve else '6201-02'), ('arms_bracket', '4240-02'), ('arms_head', '4240-22')]
        else: roles += [('arms', '6204-02'), ('arms_bracket', '4250-04')]   # 1041-1043 maja w tabeli 6205 - blad katalogu (Jan: mocowanie stale 4250-4, 6205 do niego nie pasuje)
        for role, part in roles: line += ' role %s %s' % (role, part)
        w(line + ' endscheme')
w()
# mocowania do slupa: punkty z modeli Jana (eksport 2026-10-13); zero na plaszczyznie przylegania do slupa, slup w +Y
w()
w('// mocowania do slupa (karty 40-4610, 30-4620, 40-4630): axis = pionowa os obrotu wspornika ukosnika, eye = ucho zaczepu izolatora,')
w('// hinge = zawias z uchem (Y Z ucha wzgledem osi obrotu), rod = pret z uchem stawiany w eye, back = tyl na tylnej scianie slupa')
BACK = {'03': '4610-20', '05': '4610-20', '07': '4610-21', '08': '4610-21', '10': '4610-22'}
for ex in ('01', '02', '03', '04', '05', '06', '07', '08', '10'):
    back = (' back ' + BACK[ex]) if ex in BACK else ''
    twin = ('10' if ex == '10' else '05') if ex in BACK else None   # parasol: polowka z nakretkami (Jan: -05 do 03/05/07/08, osobno -10)
    deep = 0.010 if ex == '04' else 0.0          # wykonanie 4: katownik miedzy uchwytem a slupem
    tw = lambda n: (' twin %d-%s' % (n, twin)) if twin else ''
    w('mount 4610-%s axis %s 0.0243 hinge 4610-00 -0.0798 0.0344%s%s endmount' % (ex, f(-0.1487 - deep), back, tw(4640)))
    w('mount 4620-%s eye -0.1800 0.0000 rod 4620-00%s%s endmount' % (ex, back, tw(4650)))
    w('mount 4630-%s eye %s 0.0395%s%s endmount' % (ex, f(-0.097 - deep), back, tw(4660)))
w()
w('// slupy: wysokosci od glowki szyny, zbieznosc od gory na dlugosci "length", depth = wymiar od toru w glab u gory i u dolu')
w('pole 1701 model siec_trakcyjna/slupy/1701 top 7.4600 length 8.7500 depth 0.1750 0.3850 mount_max 7.3000 strut 4610-03 stay 4620-03 steady 4630-03 endpole   // slup betonowy (Jan, 2026-10-11)')

open('data/traction_cantilevers.txt', 'w', encoding='utf-8', newline='').write(out.getvalue())
open('cantilever_ref.txt', 'w', encoding='utf-8', newline='').write(ref.getvalue())
print('schematow: %d, zestawow ramion: %d' % (len(K), len(groups)))
