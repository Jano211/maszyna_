# Sieć trakcyjna w exe – lista zadań

Stan na 2026-10-24 15:30. Kopia dokumentu Claude Docs „Sieć trakcyjna w exe – lista zadań”
(https://claude.ai/artifact/LboXDQjty15vbb7ieh8AUn) – Jan może odhaczać tam; przy zmianach przenosić tutaj.
Zestawienie z notatek w `tests/catenary/*.md` (PRZEKAZANIE, KONCEPCJA, KONCEPCJA_EDYTOR, BRAMKI, KOTWIENIA, GLOWACKI,
ROZSZERZANIE) i z pracy nad kotwieniami 23–24.10.

Jan 2026-10-24 13:24: „do żadnych złożeń na ten moment nie mam uwag” – złożenia wysięgników (rurowe, teowniki) i
założenia przy nich uznane za przyjęte (odhaczone niżej).

## Przegląd

Wysięgniki i katalog kart są kompletne. Najwięcej otwartej roboty jest w kotwieniach: czekają na modele i mocowania do
słupów. W edytorze zostały przypadki szczególne.

| Obszar | Stan | Najbliższy krok |
| --- | --- | --- |
| Wysięgniki rurowe (07xx, 10xx, 11xx, 12xx) | Wszystkie karty tomów 2–3 w danych, składają się solverem z klocków; Jan bez uwag | — |
| Teowniki (01xx, 09xx) | 356 schematów, klucz bez uwag; Jan bez uwag | — |
| Słupy i mocowania | 1701, 1665–1667, 161x, słupy bramek, zwisy 4850 | 1711 do `traction_supports.txt`; miejsca słupów (szlak / równia / peron) |
| Edytor sieci (F6) | Słupy, dobór karty, łańcuchy, bramki, parasole, kopiowanie, przechyłka | Przejrzeć w grze odsuw na przechyłce |
| Typy sieci | 15 symboli z `traction_layout.txt` mają pliki | Rozstrzygnąć tablicę wieszaków 11 czy 1 dla C120-2C / C95-2C |
| Kotwienia ciężarowe (0461) | Solver, temperatura, edytor, stan sieci, kotwienie do istniejącego słupa | Prowadnica 5070-2 + 5080-1, mocowania do słupów (0455), 6411-01 |
| Kotwienia stałe, środkowe, rozdzielone, szeregowe | Nie zaczęte | Po mocowaniach do słupów |
| LPN-y i linie wzmacniające | Odłożone (brak dokumentacji) | Wrócić, gdy Jan znajdzie karty |

## Zrobione – druty, wysięgniki, słupy

**Druty i przęsła (od 11.10)**

- [x] Fizyka liny nośnej z wieszakami, Y jako jedna lina przez podporę, przęsło bez wieszaków do kotwy (`catenary_solver`)
- [x] Typy sieci z plików `traction_<symbol>.txt`, bryły drutów z bliska, materiały miedź / mosiądz / aluminium w trzech stanach
- [x] Dwie liny nośne (2C120-2C-3/-4), rozstaw drutów 8 cm, Y tylko przy obu sąsiadach z Y
- [x] Przęsło jako `node traction … type <symbol>` (stare exe pomija słowo)

**Solver wysięgników**

- [x] Składacz z klocków (`assembly`) zamiast osobnych solverów; 11xx zgodne z dawnym solverem co do bitu (14 670 przypadków)
- [x] Wysięgnik na słupie: mocowania 4610/4620/4630 z danych, `mount_max`, zbieżność słupa, tyły opasek
- [x] Zestawy ramion na gotowo (6201–6209, 6212, 6203-11) i wsporniki 4240/4250/4260/4280/4290 z regulacją
- [x] Rodziny A–J z KONCEPCJA.md: przechylne, nieprzechylne, sama lina (100x/101x, 129x), dwie sieci (102x), peronowe (103x/104x), lina z ramionami (105x/125x/127x), sieć wewnętrzna (106x/108x), rozjazdowe (109x, 1099), 117x/118x, 120x/128x, 126x, 1267–1269, 1287–1289
- [x] 07xx niskie i wysokie (070x–079x, 90 schematów), `tube_steps` / `tube_prefer`
- [x] Teowniki ze starego katalogu: wszystkie karty 01xx i 09xx (356 schematów), klocki `tee`, `tee_bar`, siodło, węzły 4350/4360/4370/4380
- [x] Izolatory 25 kV (`insulators 25`): podmiana izolatorów, dobór rur i obniżanie zaczepu ukośnika
- [x] Parasole (`umbrella`): wspólne mocowania 4640/4650/4660 i mieszaniec 4670
- [x] Dystanse LOD części nadawane przez exe (20/40/100/300/400 m, znikanie 400/500 m)
- [x] Linki o innej grubości (`model`, `girth`), `pointx`, `radius` części

**Słupy i dane**

- [x] Słupy 1701, 1665/1666/1667 z fundamentem 1492, kratowe 161x (16 wpisów) z palami 1491–1493
- [x] Podział danych: generowany `traction_cantilevers.txt`, ręczne `traction_supports.txt` i `traction_catalog.txt`
- [x] Katalog konstrukcji dla edytora: kategorie, grupy, rodziny, dobór karty z pasma A, grupy słupów
- [x] ROZSZERZANIE.md: co zmieniać gdzie, zasady, sprawdzenie przed commitem
- [x] Sceneria próbna `td_katalog` (wszystkie schematy w rzędach) i `td_wys` / `td_wys25` (69/69 bez uwag)

## Zrobione – edytor sieci i bramki

**Edytor (pole pracy Sieć trakcyjna, F6; strony Słupy / Bramki / Druty)**

- [x] Klik przy torze stawia słup z wysięgnikiem; lico od osi toru, H, odsuw, wykonanie, izolatory 25 kV
- [x] Dobór karty z pasma A (karta 23-0007), katalog kategorii / grup / rodzin, teowniki do wstawiania
- [x] Suwaki na żywo dla zaznaczonego słupa, własna hk, kopiowanie ustawień na sąsiednie słupy, własne cofanie
- [x] Zygzak „Na zmianę” z regułą trzech z rzędu; na łuku (R < 4000 m) odsuw na zewnątrz, co najmniej 0,40
- [x] Odsuw na przechyłce od pochylonej osi toru (karta 23-0007), przechyłka w podpowiedzi (24.10)
- [x] Sprawdzanie rozpiętości przęseł z tablic (43-0002, a = 2√(2Rz), stary katalog 66/72/80 m)
- [x] Betonowe 1701/1711 przy rozjeździe: czerwone podświetlenie i błąd (`no_switch`)
- [x] Szablony rozjazdów w edytorze torów: R190 1:6,6, R190 1:7,5, R500 1:12, R500 1:14
- [x] Narzędzie sieci wydzielone do `editormodecatenary.h/.cpp` (przebudowa jednego pliku)

**Łańcuchy i przęsła**

- [x] Wpis `chain` w pliku sieci; przęsła robione przez exe, łączone z resztą sieci przy wczytaniu i na żywo w edytorze
- [x] Stawianie łańcuchowe, „Rozwieś druty do innego słupa”, sklejanie łańcuchów
- [x] Edycja całego łańcucha: typ sieci, materiał, stan drutów, zasilacz, napięcie
- [x] Poprawka przęseł rozjeżdżających się po zmianie sąsiadów (`catenary_span_refresh`)

**Bramki (BRAMKI.md)**

- [x] `mast` (38 słupów 30xx/31xx), `beam` (dźwigary E90…E385), zwisy 4850-1…5 jako „pływające słupy”
- [x] Bramki wieloprzęsłowe (słup wspólny stawia przęsło o najniższym numerze), pojedyncze elementy dźwigara 3017–3025 i 1910
- [x] Wysięgniki wprost na słupach bramek (mocowania wyk. 04, `below_beam` 0,15)
- [x] Edycja bramki: dźwigar, poziom, obrót, prostopadle do toru, lico od toru, przeciąganie wzdłuż toru, tekstura

**Typy sieci i konwersja**

- [x] Dopisane C95-2C, YpC120-2C, YpC95-2C, YC120-2C, YC95-2C, SKB70-C (stary katalog 13-0002)
- [x] Pierwsza konwersja scenerii: Częstochowa (`cze_nowa*.scm`, 1021 podwieszeń bez uwag), narzędzia w `tests/catenary/konwersja/`

## Zrobione – kotwienia (23–24.10)

Urządzenie naprężające 23-0461 (rolki równoległe 1:4, ciężary żeliwne + Ø306) składa się w exe i w edytorze. Naciągi z
dźwigni zgadzają się z kartą 0002 dla typów 3, 4, 10 i 26. Szczegóły: KOTWIENIA.md.

**Fizyka i dane (`world/catenary_tensioner.*`, `data/traction_tensioners.txt`, test `tt`)**

- [x] Tablica montażowa 0011-1 liczona ze wzoru 13-0011 (b, s od temperatury i odległości od kotwienia stałego); 0011-6/-7/-8 jako przesunięcia
- [x] Dźwignia 6406: otwory z modelu, podział naciągu momentami; otwory na druty po właściwej stronie (poprawka 24.10)
- [x] Lina 1:4 o stałej długości w −25…+40 °C; ucha 4490-02 (`rope_eye 4490-02 0.205`) na obu końcach
- [x] Lina jako bryła proceduralna (24.10 po południu): rura 12 mm (`rope_diameter`), 8 ścianek, łuki na rolkach co 5°, bez łączeń; stal (`catenary_rope_metal`); rysuje ją z bliska przęsło dochodzące do kotwienia (`catenary_span_attach`, uchwyt `solid.rope` w obu rendererach). Model 4490-01 nieużywany. Z daleka (poza zasięgiem brył) liny nie ma; bez typu sieci w przęśle – też nie
- [x] Krążki rolek kręcą się (24.10): `pulley … sheave 4460-11`; 4460-01 = oś + krążek rowka A, 4460-11 = krążek B; obrót o przewiniętą linę od położenia przy +10 °C (ruchoma A 1×, stała A 2×, ruchoma B 3×, stała B 4× przesuwu rolki), wokół +Y urządzenia. Nakrętki osi kręcą się z krążkiem A – Jan: zostaje tak (bez dodatkowych submodeli)
- [x] Stos ciężarów: każdy kolejny obrócony o 90°, cztery wykonania kolorów, osobny kolor najwyższego, wysokie 6322
- [x] Pręty 6401/6402 z modeli, poprawka s na długość pręta, zabezpieczenie 4600 wyk. 1
- [x] Stan sieci: brakujące ciężary, wydłużenie od regulacji, ciężary na gruncie (sieć bez naciągu)
- [x] Brakujące ciężary skracają sieć (sprężystość + zwis) i zmniejszają naciąg przęseł łańcucha (dopisek `tension`) – KOTWIENIA.md
- [x] Druty zaśniedziałe = stare złożenie: uchwyty krańcowe 5110 na przewodach

**Sceneria i edytor**

- [x] Wpis `anchor … endanchor`, `anchor_start` / `anchor_end` w łańcuchu, ostatnie przęsło bez wieszaków do dźwigni
- [x] Temperatura z `Global.AirTemperature`: suwak pogody w edytorze i komenda `settemperature` składają kotwienia od nowa
- [x] Przyciski „Zakotwij początek / koniec łańcucha”, kotwienie do istniejącego słupa (`on`), suwaki stanu i kolorów, usuwanie, cofanie
- [x] Wysokość rolek na słupie (`height`, suwak 5–9 m; ciężary przesuwają się razem z rolkami)
- [x] Łańcuchy do drutów (izolatory, śruby rzymskie, wyrównywacz, uchwyty) celują w ostatnie podwieszenie
- [x] Kotwienia składane przy wczytaniu po łańcuchach (`rebuild_chain`) i przy ruchu skrajnego podwieszenia
- [x] Gałąź `siec-radio`: sam kod exe sieci i radia, bez bujania, styków i plików md

## Do zrobienia – kotwienia

Najpierw prowadnica i mocowania do słupów, bo od nich zależy reszta. Kolejne rodzaje kotwień korzystają z tych samych mocowań.

**Kotwienie 0461 – dokończenie**

- [x] Konstrukcja osobno od urządzenia (24.10 wieczór, Jan): uchwyty 5100, prowadnica, pierścień i stos w układzie słupa (`structure_angle`; istniejący słup – oś prostopadła do jego wysięgnika, własny – w linii drutów), urządzenie od przegubu 4450 ciągnie do drutów; dolny uchwyt prowadnicy odwrócony o 180° (0455)
- [ ] Brakujące ciężary / wydłużenie: mniejszy naciąg ma od razu iść do przęseł (większy zwis) i przestawiać urządzenie (sieć się skraca – ciężary w górę); propozycja w czacie 24.10 19:50, czeka na Jana
- [x] Prowadnica 5070-2 i pierścień 5080-1 (24.10, odcinek 2138): `mount` 5070 w `hole_1` uchwytów 5100 (dolny 5100-03/04 na 0,80, górny 5100-01/02 na 5,30), pierścień szeklą po osi prowadnicy 0,200 od osi stosu, 1,00 m nad spodem stosu – ustawia oś stosu 0,31 od czoła (pod rolką byłoby 0,27; lina schodzi 4 cm ukośnie). Bez uchwytów w danych słupa – bez prowadnicy i pierścienia. Do obejrzenia w grze. Do czego 5060-01 i 4480-01?
- [ ] Mocowania do słupa wg 33-0455: góra pod rolkę (4510-1/-2, 4520-3…-7, 4570-1, 5100-1/-2), dół pod prowadnicę (4520-1/-2, 4530-3, 4560-1, 5100-3/-4) – 5100 (słup 1611, wyk. 11/12) modele Jan robi 24.10; przypisanie reszty wykonań do słupów
- [x] Uchwyty 5100 wokół 4450 (24.10, odcinek 2138): na ścianach wzdłuż drutów, obrócone o 90° względem wysięgnika (Jan); śruby przez szerokość słupa (nowy klucz `width` słupa w traction_supports.txt: C120 0,12 … C200 0,20, karta 33-1611-E a), rozstaw śrub obejmuje bok kratowy; górny 5100-01/02 za `mount_top` 4450, drugi odwrócony za `mount_bottom`, tyły 05 za słupem; C120/C140 śruby 220 (wyk. 11), C160+ 260 (wyk. 12) – ZAŁOŻENIE. Czoło urządzenia liczone od połowy szerokości (wcześniej połowa głębokości – 5 cm za daleko). Test `tt`, `te`
- [ ] 6411-01 (łącznik liny z prętem; teraz 0,10 m bez modelu) – model
- [ ] 4430-01 (`hole_1` −0,0747): ustalić, gdzie stoi
- [x] Stary izolator rolkowy **ESN-2025** (model `esn_2025`, hole_1 0,6004) zamiast 7150 (24.10, odcinek 2138): łańcuchy `messenger_old` / `contact_old` = ESN-2025 – 4420 – [wyrównywacz] – uchwyt, wprost w otworze dźwigni; wpis kotwienia `insulators new|old|messenger|contact`, bez wpisu – rolkowe na obu przy drutach zaśniedziałych; w edytorze lista „Izolatory”; test `tt`, `ts`; na G: traction_tensioners.txt 9593 B
- [ ] Czy stan „przejściowe” też ma dawać stare złożenie (5110 i izolator rolkowy)
- [ ] Łańcuch z ESN-2025 bez 4720 przed izolatorem (tak podał Jan) i uchwyt liny nośnej 5120 też w starym złożeniu – do potwierdzenia w grze
- [ ] Obejrzeć w grze: lina proceduralna (czy siedzi w rowkach), obrót krążków przy suwaku temperatury
- [ ] Obroty części po komplecie modeli (Jan powie, co stoi krzywo)
- [x] Dźwignia przechylona do kierunku drutów (24.10, odcinek 2138): staje prostopadle do wypadkowej z obu łańcuchów, uchwyt 4440 wzdłuż wypadkowej (tylko w płaszczyźnie urządzenia); typowo 0,3–0,5° (`lever_tilt`, test `tt`)

**Kolejne rodzaje**

- [ ] Kotwienie rozdzielone (0457 Ø306, 0458 Ø400): dwa urządzenia, lina na 7,00 m, przewody na 6,30 m; 4470-4, 5090-2/-3, 5040-2/-3, 5080-2
- [ ] Rolki szeregowe (0011-5…-8, 4460-2 + 4460-3, 4440-4/-5/-6, cięgło 4490-2, 4600 wyk. 2: 4600-03/-04/-05) i przebieg liny przez 4 rolki; Kolprojekt 2010 dla sieci 36/37
- [ ] Kotwienia stałe (0402, 0404, liny nośne)
- [ ] Kotwienie środkowe (0406: lina 9831-5, zaciski 1,00 m od słupa środkowego, połączenia V w połowie przęsła); kontrola 12° wysięgnika na skrajnych słupach
- [ ] Kotwienie środkowe do dźwigara bramki (0463…)
- [ ] Podniesienie drutu o 0,30 m na słupie, gdzie krzyżują się przęsła naprężenia
- [ ] Izolatory sekcyjne (przęsło od przęsła) – Jan 24.10: na razie nie. Dwa typy: rolkowe (ESN-2025) i nowe na izolatorach żywicznych; przejezdne i nieprzejezdne (dwa izolatory jak na linie nośnej)

**Wizualizacja i czas**

- [x] Zaznaczanie i rysowanie kotwienia w oknie edytora (24.10, odcinek 2138): LPM na urządzeniu (od czoła słupa do rolki ruchomej albo kolumna ciężarów) zaznacza kotwienie i jego łańcuch (strona Druty, panel przewija do kotwienia), Del usuwa; obrys niebieski, z uwagami czerwony, niezłożone – czerwony słup z napisem (`catenary_anchor_shape`, `catenary_anchor_chain`, test `te`)
- [ ] Obrót wysięgników od temperatury i odległości od kotwienia środkowego (do 12°)
- [ ] Dobowy przebieg temperatury w scenariuszu
- [ ] Zerwanie kotwienia (przęsła liczone z zadanej długości drutu)

## Do zrobienia – edytor, przęsła, bramki

**Przęsła i łańcuchy**

- [ ] Przęsła naprężenia: dwa łańcuchy na jednym słupie (102x, 0120, zerówki); drugi przewód i druga lina z wyniku wysięgnika (`second_wire`, `wire_b`, `messenger_b`, `arms.N.wire`) do danych przęseł
- [x] Wstawianie słupa w środek łańcucha (24.10, odcinek 2138): słup postawiony pod przęsłem (punkt drutu do 1,5 m od linii przęsła, nie bliżej niż 2 m od jego końców) wchodzi do łańcucha między te dwa słupy, w pliku łańcucha; podgląd dwóch nowych przęseł; checkbox „W przęsła”
- [x] Łańcuch w panelu Słupy (24.10): ramka z tym, co zrobi następny klik, „Zakończ łańcuch (Esc)”, „Zakotwij tu i zakończ”; przy zaznaczonym słupie końcowym (albo poza łańcuchem) „Ciągnij łańcuch od tego słupa (C)” – też na stronie Druty; Esc przerywa też rozwieszanie i wybór słupa pod kotwienie
- [ ] Rozstaw drutów 10 cm przy 6207/6208 przechodzący w 8 cm na pierwszych wieszakach
- [ ] Zygzak przy słupach po różnych stronach toru (liczyć po stronie drutu, nie do/od słupa) – niski priorytet
- [ ] Wyciąganie liny nośnej na zewnątrz łuku
- [ ] Typ wieszaka (pojedynczy / podwójny, karty 0570–0579) w pliku typu sieci
- [ ] Głowica 4170 obracana w kierunek liny w przęsłach naprężenia
- [x] „Rozwieś druty”: kończy też Esc (24.10)

**Słupy i podwieszenia**

- [ ] 1711 w `traction_supports.txt` (dziś tylko w grupie `no_switch`)
- [ ] 1617 (fundament wylewany) – pominięty
- [ ] Kilka podwieszeń na jednym słupie po tej samej stronie: wysięgniki rozstawione (0517–0519, konsola 4890), złożenie z terenu z 5 punktami
- [ ] Skórka słupa (161x „white old” wymaga modelu z teksturą wymienną)
- [ ] Panel „Konstrukcje wsporcze”: lista słupów i mocowań, podgląd, „sprawdź” (solver na wszystkich kartach), zapis do `traction_supports.txt`
- [ ] Obrazki wysięgników w panelu (klucz `image` w rodzinie już czytany)
- [ ] Tytuły z (?) w `traction_catalog.txt` (07xx, 120x, 105x/125x/127x, 106x/108x, 012x–016x, 09xx) i miejsca słupów w grupach

**Bramki**

- [ ] Półbramki: dopisać parametry (W, L, H, Hk, Beam_out, Drop_type) do 1901, ESN-540, new_1901/1905/1906/1907; wpis `mast` 1901 (Jan)
- [ ] Dźwigary środkowe 3011–3014 i konsole regulacyjne, bramki > 34,9 m z klocków (tryb ekspert)
- [ ] Zwisy 4840 i ESN-544
- [ ] Przesuwanie zwisu po dźwigarze przeciąganiem
- [ ] Wysokość wspólnego słupa zmieniona w jednym przęśle ma iść do sąsiedniego

**Konwersja scenerii**

- [ ] Częstochowa: 28 słupów z A 3,6–4,9 m bez karty 11xx zostało po staremu
- [ ] Dyrektywa `catenary` nie uwzględnia `origin` (plik sieci ma współrzędne bezwzględne)
- [ ] Kolejne scenerie (narzędzia w `tests/catenary/konwersja/`)

## Do potwierdzenia przez Jana

Te rzeczy są w danych jako ZAŁOŻENIE albo czekają na obejrzenie w grze. Odhaczyć po sprawdzeniu.

**Do obejrzenia w symulatorze**

- [ ] Kotwienia po przebudowie exe: ucha 4490-02, łączniki 4720 w otworach 6406, łańcuchy pochylone do ostatniego słupa, suwak wysokości rolek, dźwignia przechylona, zaznaczanie kotwienia w oknie
- [ ] Odsuw na łuku z przechyłką (kierunek do środka łuku, wartość w podpowiedzi)
- [x] Teowniki: 09xx, 0177/0179/0197, 0180, dwie sieci 018x/019x, łuki 0137–0174, 0167/0169
- [ ] Przęsła łańcuchów po poprawce (linie i bryły nie naraz, nie rozjeżdżają się po edycji sąsiada)
- [ ] Bramka trzymana od toru przy przeciąganiu; mocowania wysięgnika na słupie bramki (depth = 2·offset)
- [ ] Łańcuchy: wstawianie w przęsło, ramka „Łańcuch”, C / Esc (24.10, odcinek 2138)

**Założenia w danych**

- [ ] Tablica wieszaków 11 czy 1 dla C120-2C i C95-2C; YC = krótka Y 10 m od 50 m
- [ ] Kotwienie: przebieg liny 1:4 (koniec przy rolce stałej, 4 żyły), grunt −0,50 m, odsunięcie ramion 4600 0,06 m, liczby 6323 = jak żeliwnych
- [ ] Miejsca słupów: 1701/166x/161x; fundamenty 303x -1…-4 (szlak / równia / peron)
- [x] Teowniki: odstęp sieci 0,30 i przewodów 0,08 w przerwach izolacyjnych, drugi przewód na łuku nietrzymany, hk przyjęte w 09xx, 4380 0,6 m od liny
- [x] 0180 wyk. 1 – czym mocowany do słupa (wyrzucony); 0194 – 6205-02 wisi 5,7 cm za nisko
- [x] 127x: para 6203-11 ±0,13 m / ±5° na próbę; 6206-11/-12 – kąty od Jana; 105x wyk. 2 na ruchomych ramionach
- [x] 1036 na 6010-3 zamiast 6010-1; 120x obejma 4210 w osi przewodu; hk 105x/125x
- [x] 0738/0748: odciąg 6002-09 nie sięga głowicy przy wysięgu ze środka pasma
- [x] 1012–1015 wyk. 1 nie składają się na niskich słupach (1666/1667/1701) – tak ma być?

**Poprawki w modelach (Jan)**

- [ ] 4330-05: drugi pusty `eye` w miejscu `hole_1`; 4340-01: dwa `slide_horizontal_min`
- [ ] 6208-03: zdublowany submodel bez `_LOD`; plik 6506 nazywa się `6505-01.t3d`
- [ ] 4490-01: oś liny przesunięta o 1,4 mm w Y

## Odłożone i na później

**Odłożone decyzją Jana**

- [ ] LPN-y i linie wzmacniające: wysięgniki jako `scheme` z punktami `cable_1..3`, kable jako osobny wpis `cable` w pliku sieci, typy w `traction_cables.txt` – czeka na dokumentację
- [ ] Drugi typ peronowych (Bobry: dwie opaski na okrągłym słupie) – brak kart
- [ ] Obracanie zawiasu 4610-00 (wysięgniki „niegotowe” przy przebudowach); wysuw pręta 4620-00 zostaje 180 mm
- [ ] Izolatory plastikowe: osobna tekstura, tylko renderer z instancjonowaniem
- [ ] Renderer NVRHI (celowo nieruszany)
- [ ] Schodki zwisu na kratownicy dźwigara (co ok. 60 cm) – nie robimy
- Wyrzucone: zastrzał 4800, model 4340-01 (nie występuje w tabelach)

**Kandydaci z Głowackiego (do decyzji; bez automatów rozstawiających)**

- [ ] Domyślne lico słupa: 2,50 m szlak, 2,20 m stacja, 4,00 m przy rozjeździe; powiększenie skrajni na łuku (tabela 15.21)
- [ ] Ostrzeżenie, gdy przęsło przekracza rozpiętość dla odsuwów obu podwieszeń (współczynnik B 2,68 / 3,24 / 3,6 / 4)
- [ ] Odciągi sieciowe co 2. / 3. słup na zewnątrz łuku jako osobny rodzaj podwieszenia
- [ ] Szablon izolowanego przęsła naprężenia: 4-słupowy (22–24 / 53 / 22–24 m, poderwanie 0,30) i 6-słupowy na łukach R < 2000
- [ ] Przęsła pochyłe (hk 1,70 ↔ 1,30) – sprawdzić liczenie wieszaków
- [ ] Walidacje: słup z odciągiem w peronie, skrajnia peronowa 3/4 m, słup ≥ 50 m za kozłem

**Przed merge do mastera MaSzyny**

- [ ] Wyczyścić imiona i ścieżki prywatne w komentarzach: ok. 10 w `world/catenary_*`, ok. 150 w danych i generatorze
- [ ] Zdecydować, czy PRZEKAZANIE.md idzie do mastera
- [x] Dopisać do PRZEKAZANIE.md stan kotwień z 23–24.10 (2026-10-24: krótki wpis w START TUTAJ, szczegóły w KOTWIENIA.md i tu)
