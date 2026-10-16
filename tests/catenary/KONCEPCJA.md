# Wysięgniki rurowe na gotowo – koncepcja (2026-10-15)

Cel: każdy wysięgnik z katalogu (tomy 2 i 3, potem teowniki) składa się w exe z części katalogowych.
Robi to jeden solver, a sposób składania opisują dane (`assembly` w `traction_cantilevers.txt`, generowane przez `etap0/gen_cantilevers.py`).
Schematy (karta + wykonanie) powstają z tabel kart. Punkty łączeń biorę z modeli Jana, a wysokości izolatorów, gdy karta podaje „~”, ze złożeń z Blendera.

## 1. Rodziny konstrukcji

| # | Rodzina | Karty | Stan | Czego brakuje |
|---|---|---|---|---|
| A | Przechylne: ukośnik z dolnego mocowania do liny, odciąg z góry, wysięg z pręta 6011 albo rury 6111 | 11xx, 121x–124x, 120x/128x (V), 075x–078x, 129x | zrobione | ramiona rozjazdowe 126x/128x (6206/6209) |
| B | Nieprzechylne poziome z odciągiem z góry | 070x–074x, 079x | zrobione 2026-10-14, do obejrzenia | — |
| C | Nieprzechylne poziome, sama lina nośna: rura 6002 + 4500 + 4050/4140, odciąg 6002 **z dołu** (rozpórka, 1002–1005) albo z góry (1012–1015) | 1002–1005, 1012–1015 | — | tylko nowa składanka (klocki są: `anchor` działa w obie strony) |
| D | Przęsło naprężenia, dwie sieci: ukośnik 6001 z dołu, poziomy odciąg 6002 u góry, **dwa** 4050 + 4140 (dwie liny, ok. 0,25 m), wysięg 6011 na haku 4050 przy nasadzie ukośnika, na nim **4120-3 ×2 wprost** (zerowe), linka V | 1022–1026 | — | dwie OSOBNE sieci (Jan): dwie liny nośne i dwa przewody jezdne, każdy ze swoim uchwytem; wymaga przebudowy wejścia w exe (patrz 2.1) |
| E | Peronowe przelotowe: uziemiony ukośnik 6010 z mocowania 2,60 m niżej, odciąg 6007-11 na szekli 4700 **bez izolatora**, izolacja w zawieszeniu liny (6710 + 7020 + 6720 nad 4140) i wysięg 6111 na 7010-2 przy ukośniku | 1031–1047 (A stałe: 4,70 / 5,00 / 5,30) | — | nowa składanka, izolator w środku konstrukcji, modele 6010, 6710, 6720, 7010-2 |
| F | Łuk / skrócone przęsło naprężenia, sieć zewnętrzna: **wysięg z liny** 9811-3 od 4050 przy nasadzie ukośnika z zaciskami 5270-3 i 6301-3, ramiona 6207 | 1052–1056, 125x, 127x | — | klocek „lina napięta między punktami z zaciskami”, zestawy 6207 |
| G | Sieć wewnętrzna: wysięg 23 odpychany od ukośnika przez 4070 + 4400 (rozpórka), wystaje za drut, na końcu 4290/4250 z ramionami do słupa, ukośna linka 33 od uchwytu liny do końca wysięgu | 1061–1066, 1081–1086 | — | klocek „wysięg na łańcuchu z ukośnika”, linka ukośna (jak `stretch`) |
| H | Rozjazdowe jednej sieci: wysięg od 4050 przy nasadzie ukośnika, dwa uchwyty ramion (poz. 13 obrócony o 180°), ramię 25 pchane, ramię 26 ciągnięte (otwór owalny), spotykają się na drucie | 1091–1099 | — | para ramion z dwóch stron jednego punktu drutu; zestawy jak 6206/6209 |
| I | Głowica 4500 na ukośniku 6001 | 1267–1269, 1287–1289 | — | model 4500 jest, do zrobienia od razu |
| J | Parasole | wszystkie z mocowaniem na opasce | zrobione | mieszaniec 4670 |
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
7. Teowniki, 4670, dobór karty.

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
- Zastrzał 4800: modelu jeszcze nie ma (Jan).
