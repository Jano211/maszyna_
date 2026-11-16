# Modele do zrobienia – sieć trakcyjna (stan 2026-11-16 wieczór)

Lista od A do Z: wszystko, czego kod albo dane wołają, a modelu nie ma (bryły zastępcze, pominięte części), plus to, co dojdzie
z zasilaniem. Sprawdzone z `G:\Maszyna2510\models\siec_trakcyjna\components` i `slupy` (16.11). Kopia na G: w
`models\siec_trakcyjna\MODELE_DO_ZROBIENIA.txt`. Odhaczać tu.
Oznaczenia: [ ] do zrobienia, [~] jest coś zastępczego / częściowo, [?] do sprawdzenia, ★ – najpierw (blokuje kod albo widać z kabiny).

## A. Tekstury i materiały
- [ ] ★ Atlas `siec` (8k): warianty zużycia – świeży / po paru latach / zgnity 10-letni / skrajnie zgnity 50-letni (dziś tylko
      new/used); przy okazji przepakowanie UV (wyrzucić z atlasu to, co poszło na własne tekstury) i maski z vertex colorów (mapa ID),
      żeby reimport siatki w Painterze nie rozjeżdżał masek.
- [ ] Nowa tekstura `odlaczniki_{new,used,old,extreme}` (+ normals): odłączniki, skrzynki napędów, konstrukcje pod odłącznik; później
      odgromniki, iskierniki. Części, które dziś są na atlasie `siec`, a mają iść też na odłączniki – kopie z własnym UV.
- [ ] Izolatory plastikowe (wykonania -02 rurowych 7100/7010/7020): osobna tekstura, tylko renderer z instancjonowaniem.
- [?] Słupy: warianty kolorów i zużycia wg ustaleń z 20.10 (161X szary/biały, reszta wg rodzin) – sprawdzić, co już jest.

## B. Słupy i fundamenty
Są: 1491–1493 (pale), 161x (1611/1613/1614/1617), 1665–1667, 1701, maszty bramek 303x/311x/312x, `slup_new_1901`.
- [ ] ★ 1711 (beton, dziś tylko w grupie `no_switch`) – też pod przewód wzmacniający (49xx wyk. 2).
- [ ] ★ 1601, 1602 (49xx wyk. 1), 1604, 1605 (49xx wyk. 2) – słupy, na które „właściwie” idzie przewód wzmacniający.
- [ ] Rodziny stare 150x i 160x (stare nietypowe konstrukcje, + ŻK / STŻ) – razem z fundamentami wylewanymi.
- [ ] Słupy wirowane ETG (strunobetonowe) – pod nie są już 6130-04/05 i 6131-04/05; mocowania na ETG.
- [ ] Dwuteowniki 166x – poza 1665–1667 (jeśli są inne wymiary).
- [ ] 1617 na fundamencie wylewanym (pominięty w danych).
- [ ] Fundamenty wylewane (3 stopnie brudu) do słupów nie-palowych.

## C. Bramki i półbramki
- [ ] Nowe półbramki 190x (1905/1906/1907) i ESN-540 – czekają na opisy / wymiary.
- [ ] Zwisy ESN-544; dźwigary środkowe 3011–3014 i konsole regulacyjne (bramki > 34,9 m).
- [ ] Stare bramki (stary Kolprojekt / ESN) – brak rysunków; wtedy warianty bramkowe przewodu wzmacniającego.

## D. Wysięgniki – drobnica
- [ ] Zaciski linek ramion 106x/108x na linie nośnej: 6301 / 6311 / 5440 (dziś linki kończą się w osi liny).
- [ ] 4580 konsoleta regulowana – dwa modele: rama przy słupie skalowana wzdłuż wystawania (1 m, zero na licu) + przód stały.
- [ ] 4570 do nowych półbramek 190x (czeka na opisy).
- [ ] 4690-04 dla masztów bramek 30xx/31xx (czekało na decyzję).
- [ ] Mocowania wysięgników na słupach ETG (strunobeton).

## E. Kotwienia
- [ ] 5050-1 pręt prowadnicy ciężarów ×8 (stare zamocowanie 0454, tom 1 s. 111) – 5060-01 i 4550-01…09 są.
- [ ] Mocowania do słupa wg 33-0455 – przypisanie reszty wykonań do słupów (modele 5100 są).
- [ ] Kotwienie środkowe: zaciski 5560 / 5570 na linie nośnej, złączka 6410 na przewodzie, uchwyt 4590 (0416, do dźwigara bramki);
      obejma liny kotwienia na teowniku bez siodła (dziś drugi 4160-01).
- [ ] Uchwyty kotwień 4580 (wspólne z D).
- [ ] ★ Kotwienie słupów do ziemi (użytkownik 16.11 23:05): odciągi słupów kotwowych do gruntu – lina z kotwą gruntową / płytą
      kotwiącą, uchwyty na słupie, śruba rzymska; karty i części do ustalenia (zastrzał 4800 wyrzucony wcześniej). Kod: wpis przy
      słupie kotwowym, kierunek przedłużenia łańcucha.

## F. Odciągi sieci (0311–0326)
- [ ] 6303-7 końcówka liny odciągowej na linie nośnej; 5640-1 zacisk (odciągi pośrednie); 5270-3 zaciski (razem z kauszą, po 2–3,
      jak na gruncie); 6301-3 kausza; w 0326 dodatkowo 6301-1, 6303-1, 6311-1 ×2.
- [ ] Lina odciągowa Cu 35 (9811-3) – dziś 4490-00 pogrubiony; do decyzji osobny model / kolor miedzi. 9811-1 (0326) – co to jest.

## G. Przęsła: skrzyżowania, łączniki, izolowane przęsła naprężenia
- [ ] ★ 5380-1 zacisk ogranicznika 0561 (skrzyżowania nad rozjazdami, dziś prostopadłościany) – odcinek przewodu robi kod.
- [ ] Łącznik międzysekcyjny 33-0640: zaciski 6605 (liny nośne) i 6601 (przewód jezdny) – te same pójdą do połączeń przez odłącznik (K).
- [ ] Izolowane przęsło naprężenia (0060/0062/0078): izolatory w przewodzie jezdnym i w linie nośnej schodzących do kotwienia (bryły).
- [ ] 0331 / 0333 ograniczniki uniesienia na mostach i dziwnych konstrukcjach (karty nieczytane).

## H. Izolatory sekcyjne
Są: 7500-01, 7500-02, 7230-01, 7210-01, 7150-01, 5120-01 (użytkownik 16.11: „tyle wystarczy + linki”).
- [ ] 7510 (karta 0440) – pominięty, brak modelu.
- [ ] Izolator żywiczny w sieciach z dwiema linami nośnymi (32, 35) – dziś tylko w górnej linie (kod, nie model).

## I. Uszynienie
Są: 6120-01/02/04/07/08, 6130-00…05, 6131-01…05, 6132-01, 6840-01, 5180-07.
- [ ] ★ Iskiernik niskonapięciowy J-250 (karta 53-0831; „ogranicznik niskonapięciowy” na końcu sekcji): 7350-1 iskiernik +
      6333-2 uchwyt (wyk. 1 beton) / wprost na stali (wyk. 2), 5680-1 zacisk szynowy; linkę 5850 (Cu 50 w izolacji) zrobi kod.
      Słupek 6351 – gdy iskiernik nie na słupie.
- [ ] Stare uszynienie grupowe ESN 2130 (przykręcane do słupa, bez 6120 – osobna rodzina uchwytów), uchwyty na bednarkę – NA KONIEC.

## J. Przewód wzmacniający
Są: 4910–4950, ESN-2141/2510/2635A, 0301-05…091.
- [ ] 5180-04 zacisk końcowy dla AFL95 (kotwienie 23-0419 – dziś 5180-07 dla wszystkich).
- [ ] Końcówka 5630 i taśma (kotwienie 23-0419).
- [ ] Słupy 1601/1602/1604/1605/1711 (B).

## K. Zasilanie – odłączniki, odgromniki, połączenia (ZASILANIE.md)
Minimum na start (żeby odłącznik działał i był widoczny):
- [ ] ★ Odłącznik razem z izolatorami LWP 8-24 (7200) i wspornikami 4820 jako jeden model, noże osobnym submodelem (obrót przy
      otwieraniu): wersja 7420 (sekcyjny) i 7430 (ze stykiem uszyniającym).
- [ ] ★ 4750 konstrukcja pod odłącznik – na początek na słupy ceownikowe (wyk. 1), potem bramki (4/5), przestrzenne (6),
      dwuteowniki na palach (7…).
- [ ] ★ 7460 skrzynka napędu ręcznego z korbą / dźwignią jako submodelem (w nią się będzie klikać) + 4790 konstrukcja pod skrzynkę.
- [~] 6116 drążek napędowy – zrobi kod (rura), 4770 prowadnice drążka – na start bryły.
Później:
- [ ] Napęd silnikowy ONS-1: 7470 + 4811 łącznik + 4780 konstrukcja; 6861 korba ręczna.
- [ ] Odgromnik rożkowy 7310-1 (rożki w V na izolatorze, belka na słupie) + uchwyty równoległe na linie nośnej 5560-5 / 5500-6 /
      5570-6 / 5550-5 (wg liny L70…L150); linkę 9812-6 zrobi kod.
- [ ] Połączenia przez odłącznik (0641) i z przewodem rozdzielczym 3 kV (0650–0656): zaciski 6601-1/-2, 6605-6/-8, końcówki 6316-9,
      6306; linki i mostki zrobi kod.
- [ ] Podstacja (model w scenerii – może istniejący).
- [ ] Połączenie odłącznika z uszynieniem 5880 (linka L50 – kod).

## L. Poprawki w istniejących modelach
- [ ] 4330-05: drugi pusty `eye` w miejscu `hole_1`; 4340-01: dwa `slide_horizontal_min` (4340 i tak nieużywany).
- [ ] 6208-03: zdublowany submodel bez `_LOD`.
- [ ] 4490-01: oś liny przesunięta o 1,4 mm w Y (model nieużywany – lina proceduralna).
- [ ] 4550-05: tekstura na sztywno (`Map siec_trakcyjna\siec`) → `Map: -1`.
- [?] 6506: plik nazywał się 6505-01.t3d – na dysku jest 6506-01.t3d, sprawdzić, czy stary można usunąć.
- [?] 4270-01/02: zero w osi liny odciągowej, lina wzdłuż Y modelu (tak przyjęte w kodzie).

## M. Odłożone
- LPN-y (linie potrzeb nietrakcyjnych) – modele mocowań na słupy ETG może; LPN ze Strunobetu niepotrzebny.
- Linie WN/SN/nN obok sieci (kolejny temat po sieci): wydzielić izolatory od słupów w t3d i ustawić pivoty jak w sieci.
- Drugi typ peronowych (Bobry: dwie opaski na okrągłym słupie) – brak kart.
- Wysięgnik składany ręcznie „jak pizza” (sieć zlikwidowana / w budowie) – kod, modele te same.
