# Wysięgniki rurowe na gotowo – koncepcja (2026-10-15)

Cel: każdy wysięgnik z katalogu (tomy 2 i 3, potem teowniki) składa się w exe z części katalogowych.
Robi to jeden solver, a sposób składania opisują dane (`assembly` w `traction_cantilevers.txt`, generowane przez `etap0/gen_cantilevers.py`).
Schematy (karta + wykonanie) powstają z tabel kart. Punkty łączeń biorę z modeli Jana, a wysokości izolatorów, gdy karta podaje „~”, ze złożeń z Blendera.

## 1. Rodziny konstrukcji

| # | Rodzina | Karty | Stan | Czego brakuje |
|---|---|---|---|---|
| A | Przechylne: ukośnik z dolnego mocowania do liny, odciąg z góry, wysięg z pręta 6011 albo rury 6111 | 11xx, 121x–124x, 120x/128x (V), 075x–078x, 129x | zrobione (126x/128x z 6206/6209 – 2026-10-17) | — |
| B | Nieprzechylne poziome z odciągiem z góry | 070x–074x, 079x | zrobione 2026-10-14, do obejrzenia | — |
| C | Nieprzechylne poziome, sama lina nośna: rura 6002 + 4500 + 4050/4140, odciąg 6002 **z dołu** (rozpórka, 1002–1005) albo z góry (1012–1015) | 1002–1005, 1012–1015 | zrobione 2026-10-15 (`level_push_in`, `level_arms_in`) | potwierdzić 4500-01 jako obejmę na rurze |
| D | Przęsło naprężenia, dwie sieci: ukośnik 6001 z dołu, poziomy odciąg 6002 u góry, **dwa** 4050 + 4140 (dwie liny, ok. 0,25 m), wysięg 6011 na haku 4050 przy nasadzie ukośnika, na nim **4120-3 ×2 wprost** (zerowe), linka V | 1022–1026 | zrobione 2026-10-17 (`double_net`; druga lina i drugi przewód jako punkty w wynikach dla edytora) | — |
| E | Peronowe przelotowe: uziemiony ukośnik 6010 z mocowania 2,60 m niżej, odciąg 6007-11 na szekli 4700 **bez izolatora**, izolacja w zawieszeniu liny (6710 + 7020 + 6720 nad 4140) i wysięg 6111 na 7010-2 przy ukośniku | 1031–1047 (A stałe: 4,70 / 5,00 / 5,30) | zrobione (peronowe) | — |
| F | Łuk / skrócone przęsło naprężenia, sieć zewnętrzna: **wysięg z liny** 9811-3 od 4050 przy nasadzie ukośnika z zaciskami 5270-3 i 6301-3, ramiona 6207 | 1052–1056, 125x, 127x | zrobione 2026-10-17 (`rope_arms`, `armropes`; 127x para 6203-11 na próbę ±0,13 m / ±5°) | — |
| G | Sieć wewnętrzna: wysięg 23 odpychany od ukośnika przez 4070 + 4400 (rozpórka), wystaje za drut, na końcu 4290/4250 z ramionami do słupa, ukośna linka 33 od uchwytu liny do końca wysięgu | 1061–1066, 1081–1086 | zrobione 2026-10-17 (`inner_push_*`, 6207/6208) | — |
| H | Rozjazdowe jednej sieci: wysięg od 4050 przy nasadzie ukośnika, dwa uchwyty ramion (poz. 13 obrócony o 180°), ramię 25 pchane, ramię 26 ciągnięte (otwór owalny), spotykają się na drucie | 1091–1099 | zrobione 2026-10-17 (`swing_vee_pair`, 6202 + 6212; 1099 `inner_push_pair`) | — |
| I | Jak G: wysięg 6001 rozpierany od ukośnika, 4500-01 na jego końcu, linka 9811-3 w V do liny | 1267–1269, 1287–1289 | zrobione 2026-10-17 (6206/6209 na 4280) | — |
| J | Parasole | wszystkie z mocowaniem na opasce | zrobione (z 4670) | – |
| K | Teowniki | `kolprojekt1.pdf` (skan bez tekstu) | — | czytanie automatem drogie: najpierw spis/objaśnienia, potem po jednej karcie na rodzinę, tabele tylko wybranych kart; najlepiej, gdy Jan wskaże zakres stron |

## 2. Klocki solvera do dopisania

1. **Dwie sieci na jednym wysięgniku** (D, przęsło naprężenia): wpis podwieszenia z dwoma kompletami `at`/`rope` (i przęsłami obu sieci), solver dostaje `wire_b`/`messenger_b`, role z przyrostkiem `_b` (uchwyt liny, uchwyt przewodu). Do przerobienia: `catenary_suspension` (parser, powiązanie z przęsłami obu sieci), `catenary_scene` (punkty drugiej sieci), wejście solvera.
7. **Zastrzał przeciwwiatrowy** (Jan): dokładka, którą może mieć KAŻDY wysięgnik, w katalogach wysięgników zwykle nieujęta; typowo część **4800** (Jan); w 102x nietypowy, składany z pojedynczych elementów (4760-1 poz. 17, 6506-1 poz. 34, przy nasadzie ukośnika) – osobna rola tylko dla 102x. Klocek opcjonalny `windbrace` na końcu każdej składanki (działa, gdy schemat albo wpis ma role zastrzału); w scenerii przełącznik we wpisie podwieszenia.
2. **Lina napięta** (F, G-ukośna 33): człon między dwoma punktami, części wzdłuż niego w stałych odstępach (zaciski 5270-3/6301-3) albo w punktach (ramiona). Dziś mamy `stretch` (jedna część rozciągnięta) i `vee`; trzeba wersji „lina + części na niej”.
3. **Wysięg na łańcuchu z ukośnika** (G): odpowiednik `chain`, ale dla wysięgu, nie odciągu (rura dociskana 4070 + 4400 do ukośnika).
4. **Para ramion pchane/ciągnięte** (H): dwa zestawy z dwóch stron drutu, każdy na swoim uchwycie, drugi obrócony o 180°.
5. **Izolator w środku konstrukcji** (E): wisząca kolumna 6710–7020–6720–4140 (jak `hang`, ale łańcuch kilku części).
6. **Dobór karty z odległości i wysokości** – na koniec, gdy wszystkie rodziny będą w danych.

## 3. Modele od Jana

- Zestawy ramion: 6206, 6207, 6209 (+ ewentualnie 6208), punkty `wire` i `holder` jak w 62xx.
- Zastrzał 4800 (typowy, do każdego wysięgnika); uchwyty/łączniki: 4760-1, 6506-1 (nietypowy zastrzał 102x), 5270-3, 6301-3 (F), 6710, 6720 (E), 7010-2.
- Rury: 6010 (E), 6007-11 (jest?), 6011-3/-4 w D (są).

## 4. Proponowana kolejność

1. I (1267–1269, 1287–1289) i C (100x/101x): najtańsze, klocki już są.
2. G (106x, 108x): jeden nowy klocek.
3. H (109x) razem z 126x/128x: ramiona rozjazdowe, po modelach 6206/6209.
4. F (105x, 125x, 127x): lina z zaciskami.
5. D (102x): dwie liny nośne.
6. E (peronowe).
7. Teowniki, dobór karty.

Każdy krok kończy się rzędem w `td_katalog` i ostrzeżeniami w kluczu, a potem przeglądem w symulatorze.

## 5. Odpowiedzi Jana (2026-10-15 00:14)

- Teowniki: `E:\SYF\INNE\PDF\Katalog ESN\kolprojekt1.pdf`, bez warstwy tekstowej; strony 62–119 (od karty 108) oraz od 257 (karta 0931) do końca.
- Klasyczny zastrzał przeciwwiatrowy to **4800** (dokładka do dowolnego wysięgnika, patrz 2.7). Zestaw z 4760-1 + 6506-1 w 102x to nietypowy, składany zastrzał – osobny przypadek tylko dla 102x.
- 102x: dwie osobne liny nośne (dwie sieci przęsła naprężenia); exe trzeba przebudować pod takie podwieszenia.

## 6. Teowniki – pierwszy rzut oka (2026-10-15 00:30)

- `kolprojekt1.pdf` s. 62–119: karty 01xx (stary katalog Kolprojektu, rysunki i tabele pisane ręcznie, OCR nie przejdzie – tabele czytać okiem, po jednej na rodzinę). Rodziny jak w tomie 3: podwieszenia przelotowe proste/łuk, do słupa/od słupa, przęsła naprężenia, łuk, rozjazdowe, uzbrojenie przewodu.
- Przykład 23-0111 (przelotowe, prosta, odsuw do słupa, przechylne; wykonania 2/3/4 = A 1,75–2,20 / 2,20–2,65 / 2,65–3,10): ukośnik 6101-2/3/4 (teownik) z dolnego izolatora 7110-1, u góry poziomy pręt 4310-2/3/4 z izolatorem 7120-1 i siodłem liny 4160-1 na końcu nad drutem (lina LEŻY na wysięgniku, nie wisi na 4140); wysięg 6111-1/2/3 na węźle 4350-1 na ukośniku, ramiona 6201-1/-2 na 4240-1; pionowe cięgno 9812-1 (1,60 m) od 4230-1 + 6301 + 6311 na ukośniku do 4210-2 na końcu wysięgu. Inne części niż w rurowych: 4160, 4230, 4310, 4350, 6101, 7110, 7120, 9812.
- s. 257–koniec (od karty 0931): jeszcze nieprzejrzane.
- Wniosek: teowniki to osobny zestaw części i 2–3 nowe składanki (lina na siodle zamiast wiszącej, wysięg na węźle ukośnika, cięgno pionowe). Klocki solvera w większości są (`tangent`/`line`, `stretch`, `hangarms`); nowe: siodło liny (część stawiana pod liną, ukośnik/pręt przechodzi przez jej punkt).
- Zastrzał 4800: wyrzucony z listy do zrobienia (Jan, 2026-10-17).

## 7. Rodziny G i I – karty przeczytane 2026-10-15 23:00 (bez kodu)

- 1061 (s. 32, odsuw od słupa, sieć wewnętrzna, wyk. 1, 3, 4): ukośnik 6001-3 (wyk. 4: -2) z dolnego 7100, na górze 4050 + 4140; u góry pozioma 6002-2 z 7100 do 4070 + 4400 na ukośniku (max 0,30 m przed 4050, jak 12xx); wysięg 23 = 6001-4 (wyk. 4: -3) POZIOMO od dolnej części ukośnika: 4070 na ukośniku + 4400 w osi wysięgu; wysięg wychodzi za drut, na końcu 4500-1 (poz. 16) i zaślepka; na nim 4290-5/-6 z ramionami 6207 (wyk. 1: 6207-1 + 6207-2, wyk. 3/4: 6207-3), od słupa. Linka 9811-1 (7,00 m): pionowa od uchwytu liny do obejmy 4210-2 na wysięgu w osi drutu oraz ukośna od 4500 na końcu wysięgu do liny w V (~0,25 m). „wyk. 1,3 1,70 / wyk. 4 1,30” to chyba hk (strzałka kończy się na drucie). Odstęp izolatorów ~2,00 / ~1,60 m.
- 1081 (s. 38, odsuw od słupa, sieć zewnętrzna, wyk. 2, 4): to samo bez linki pionowej; jedna linka 9811-1 (2,40 m) od uchwytu liny 4140 do 4500 na końcu wysięgu (4140 wisi w jej linii – klocek `hang`); ukośnik 6001-2, odciąg 6002-2, wysięg 6001-2; ramiona 6205-1 + 6205-2 (wyk. 2) / 6203-1 (wyk. 4) na 4290-6 / -5 – modele są. Wymiar „1,20 m” NIEJASNY: lina–oś wysięgu czy lina–drut (strzałka kończy się na wysięgu, przy 1061 analogiczna sięga drutu). Przy górnym izolatorze 0,20 m nad liną i ~1,60 m odstępu izolatorów wysięg 1,20 m pod liną trafiałby w dolny izolator – rysunek nie jest w skali.
- 1267 (s. 152, rozjazdowe nieprzechylne, od słupa, wyk. 2–4): ta sama konstrukcja co 1061 (wysięg 6001-9/-10 rozpierany 4070 + 4400 od ukośnika, 4500-1 na końcu, linka 9811-3 4,00/3,50 m w V do liny); ramiona 6209-1 + 6209-2 (wyk. 2) / 6206-1 (wyk. 3, 4) na 4280-4 / -3, uchwyty 4110.
- Klocki dla G/I: wysięg poziomy od ukośnika (`level` + `side` po dolnej stronie ukośnika, 4400 w osi wysięgu), 4500 na końcu wysięgu (jak w `top_head`), `hang` 4140 w linii linki ukośnej (108x), `vee` od 4500 do liny (106x, 126x/128x), linka pionowa (`stretch`, 106x), ramiona od słupa (`hangarms`, strona −1). Nowego kodu w exe prawdopodobnie nie trzeba – wystarczy składanka w danych.
- Pytania do Jana: (1) w 108x „1,20 m” to lina–wysięg czy lina–drut? (2) który otwór 4400 w wysięgu: stały środkowy czy dobierany? (3) czy jest złożenie 106x/108x w Blenderze (w `zlozenia.json` 10xx nie ma)?
- 2026-10-15 23:20: 108x zrobione (`inner_push`, opis w PRZEKAZANIE.md). Odpowiedzi Jana: „1,20 m” = hk; złożeń 10xx nie ma; otwór 4400 do dobrania (na razie środkowy).

## 8. Teowniki – przegląd wszystkich kart (2026-10-17 22:30)

Źródło: `kolprojekt1.pdf` (298 s.), dział 0100 s. 59–119 (objaśnienia s. 60–61), dział 0900 s. 255–298 (objaśnienia s. 256). Numer karty w prawym górnym rogu. Jan: teowniki są tylko w tym pliku; ma modele wszystkich części, eksportuje je z pustymi punktami łączeń jak w rurowych (2026-10-17 22:02).

**Numeracja 01xx (objaśnienia):** trzecia cyfra 1/2 – sieć 1, 2 (YpC120-2C, YpC95-2C), 3/4 – sieć 3, 4 (C120-2C, C95-2C), 5/6 – sieć 5 (KB70, SKB 70-C), 7/8 – rozjazdy sieci 5, 9 – rozjazdy z innymi. Czwarta cyfra nieparzysta = do słupa, parzysta = od słupa, 0 = specjalne (przęsło naprężenia). Wykonania (A od czoła konstrukcji wsporczej): 1 – 1,30–1,75 (wsporniki bramek, wysięg na osobnym izolatorze, tylko nieprzechylne; ukośnik 6102-1, pręt 4330-1), 2 – 1,75–2,20, 3 – 2,20–2,65, 4 – 2,65–3,10 (2–4 podstawowe, przechylne i nieprzechylne), 5 – 3,70–4,50 (0124 do 4,30; nieprzechylne, skrajnia 4,00 m, ukośnik 6103-1, pręt 4330-5, węzeł 4360-1, izolator 7110-2). Zespoły nie zawierają mocowań do konstrukcji (dział 0500).
**09xx:** wysokości konstrukcyjne inne niż normalne; 0930–0959 dwa przewody (sieć 3, 4), 0960–0999 jeden przewód (sieć 5); wykonania jak w 0100.

**Mechanika podstawowa (0111):** pręt górny (poz. 8: 4310-2/3/4 przechylne z izolatorem 7120-1; 4320-2/3/4 / 4330 nieprzechylne z 7110) poziomo od górnego mocowania, na końcu nad drutem siodło liny 4160-1 (lina LEŻY na pręcie; w przęsłach naprężenia i rozjazdowych 4170-1); ukośnik teowy 6101-2/3/4 (poz. 10) z dolnego izolatora 7110-1 do pręta górnego; wysięg 6111 (poz. 11) na węźle 4350-1 na ukośniku; ramiona na 4240/4250/4260 na wysięgu jak w 11xx; cięgno 9812-1 (poz. 21) od 4230-1 (na ukośniku/pręcie) do 4210-2 na wysięgu (do słupa pionowo, od słupa ukośnie od końca pręta). Odstęp mocowań ~1,80 m (wyk. 1 ~0,75, wyk. 5 ~2,25).

**Karty 01xx (strona: karta):** 62 0108, 63 0109 (sama lina, przechylne / nieprzechylne); 64–67 0111–0114 (przelotowe, sieć 1,2, przechylne: prosta do/od, łuk do/od); 68–69 0115–0116 (sekcyjna przerwa izolacyjna, + 4120-3 drugi przewód); 70 0120 (przęsło naprężenia, 4170, 2× 4120-3, bez ramion); 71–74 0121–0124 (nieprzechylne, wyk. 1–5); 75–78 0131–0134 (sieć 3,4 przechylne; ramiona 6204 na 4250-4 / 6205 na 4260-2, 4110); 79–80 0135–0136 (przerwa izolacyjna 3,4); 81–85 0137–0140 (przęsło naprężenia na łuku, sieć wewn./zewn.; wysięg 6113 na 4290-4, 6207 na linie 9814-3 z 5270-3/6301-3); 86–89 0141–0144 (3,4 nieprzechylne); 90–93 0151–0154 (sieć 5 przechylne, 6202-1 na 4250-3 / 6203-1 na 4260-1); 94–95 0155–0156 (przerwa izolacyjna 5); 96–97 0158–0159 (skrócone przęsło naprężenia, 4290-3, 6113, 6207-3); 98 0160 (przęsło naprężenia 5); 99–102 0161–0164 (5 nieprzechylne); 103 0167 (rozstawione wysięgniki pomocnicze: 2× 6111 na 7110, 4260-1, 6203-1, cięgna do 5420-5 na linie); 104 0169 (ramiona odciągowe specjalne: lina 9814-3 z 5270-3, 2× 6207-3, 7120); 105–107 0172–0174 (przęsło naprężenia na łuku 5); 108–111 0177–0180 (rozjazdowe jednej sieci: 0177/0178 lina 9814-3 + 6203-1 z 5270-3, 0179 para 6203-1, 0180 6206-1 na 4280-2 na 6113); 112–114 0181, 0182, 0184 (rozjazdowe dwóch sieci, skrzyżowanie pojedyncze: druga lina na poprzeczce 6124-1/-2 z 4370-2/-5, hk 1,70 / 1,30; 0184 wysięg 6115-3/-4 na 4280-1/-2); 115–117 0192–0194 (skrzyżowanie podwójne); 118–119 0196–0197 (nad rozjazdem krzyżowym podwójnym S49 1:9/300, 6206-2, hk 1,70).
Uwaga: na s. 70 (0120) i podobnych wyk. 1 ma osobny izolator wysięgu (3× 7110).

**Karty 09xx (strona: karta):** 257–260 0931–0934 i 274–277 0961–0964 (hk 0,25–0,45): pozioma teowa 6107-1…6 (poz. 10) na 7110-1 na wysokości liny, siodło 4160 na jej końcu (szczęki 4160 w odwrotnej kolejności), odciąg linowy 9832-2 (stalowy, poz. 22) od 7130-1 przez 6509-1, 6305-1, 5290-1 ×4 do węzła 4380-1 na poziomej (max 0,6 m od liny), ramiona na 4290-3/-4 wprost na teowniku (6204, 6205, 6202, 6203); 261–264 0935–0938 i 278–281 0965–0968 (hk 0,45–1,30): to samo + wysięg 6111 na osobnym 7110 (na nim 4250/4260 z ramionami), linka 9812-1 w V od liny (0,2 m) do 4210; 265 0940 (przęsło naprężenia hk 0,45–1,30); 266–269 0941–0944 i 291–294 0981–0984 (hk 1,70–2,10: jak 0111, ramiona na 4290-1/-2 na wysięgu); 270–273 0945–0948 i 295–298 0985–0988 (hk ≥ 2,10: wysięg na osobnym dolnym izolatorze 7110, nie na węźle ukośnika); 282 0970 (przęsło naprężenia hk 1,70, sieć 5); 283–290 0971–0978 (hk 1,70, sieć 5, przechylne i nieprzechylne).

**Części z tabel – wspólne z rurowymi (modele są):** 4110-1, 4120-3, 4130-1, 4210-2, 4240-1, 4250-3/-4, 4260-1/-2, 4280-1…4, 4290-1…4 (korpus -02 na rurze 38, -04 na teowniku), 6111-1…8, zestawy 6201, 6202, 6203, 6204, 6205, 6206, 6207; 5270-3, 6301, 6311 pomijane, linki 9812/9814 = 4490-00.
**Części teownikowe (nowe):** ukośniki 6101-2/3/4, 6102-1, 6103-1; pręty górne 4310-2/3/4, 4320-2/3/4, 4330-1/-5; poziome teowe 6107-1…6 (09 niskie); wysięgi teowe 6113-1…4, 6115-3/-4; poprzeczka 6124-1/-2; siodło 4160-1, uchwyt liny 4170-1, 4370-2/-5; węzły 4350-1, 4360-1, 4380-1; uchwyt cięgna 4230-1; 5420-5; izolatory 7110-1, 7110-2, 7120-1, 7130-1; odciąg linowy 9832-2 z 6509-1, 6305-1, 6309-1, 5290-1.
