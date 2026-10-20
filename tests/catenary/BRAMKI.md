# Bramki, półbramki, dźwigary, zwisy – stan danych (przeczytane 2026-10-21)

Źródło: `scenery/siec_trakcyjna/` w katalogu symulatora (pliki `.inc`). Katalog `bramki/` – NIEAKTUALNY, nie brać.
Cel: składanie bramek z klocków w edytorze exe i ogólnie nowe typy konstrukcji wsporczych (obok `pole` w `traction_supports.txt`).

## Co jest w plikach

Każdy `.inc` to zwykły include wstawiający jeden model (`origin p2 p3 p4`, `rotate 0 p5 0`, `node … model … (p6)`),
P1 = nazwa, P2–P4 = współrzędne, P5 = obrót, P6 = tekstura wymienna. Wstawiać na wysokości główki szyny.
Parametry do składania są w komentarzach w nagłówku jako `//$e_<klucz>=<wartość>; opis` (tagi dla edytora).

### Słupy bramek – `slupy/slup_30xx-N.inc`, `slup_311x/312x.inc` (`$e_Type=gantry_mast`)
- `e_Subtype`: `intermediate` (przelotowy – dźwigary z obu stron) / `terminal` (końcowy – z jednej strony)
- `e_W` szerokość, `e_L` długość, `e_H` wysokość słupa
- `e_Mount_low` / `e_Mount_high` – prześwit bramki w dolnym / górnym mocowaniu dźwigara (do przylepiania zwisów)
- `e_Offset_x` – odsunięcie dźwigara względem słupa (na jedną albo drugą stronę)
- 3031–3038: dopisek `-1…-4` = wykonanie fundamentu (głowica 25 cm … 170 cm), prześwit 7,75/7,35
- 3111–3113 (perony), 3121–3123: na jednym fundamencie palowym, prześwit 8,05/7,65
- `slup_new_1901.inc` – bez tagów

### Dźwigary – `dzwigary/*.inc` (`$e_Type=gantry_beam`)
- `e_W` = długość dźwigara (rozpiętość), `e_L` 0,6, `e_H` 0,9, `e_Drop_type` – typ pasujących zwisów (4850)
- 1910-1/-2 (półbramka 6,5 m …), 3011–3025 (środkowe, np. 3011 = 25,6 m), E90…E349, E385 = gotowe o długości
  E/10 m co 0,1 m (np. E101 = 10,1 m; model `dzwigary/10.1m.t3d`)

### Zwisy – `zwisy/<typ>/<typ>-N.inc` (`$e_Type=drop_bracket`)
- `e_Subtype` 4840 (stare półbramki 1901, 2,2 m), 4850 (bramki i nowe półbramki, 1,8 m), ESN-544 (półbramki ESN-540, 1,65 m)
- `e_H` długość zwisu, `e_L`, `e_W`

### Półbramki – `polbramki/*.inc`
- Tylko `polbramka_1901_2x7-3m.inc` ma tagi (bez `$e_`): `Type=semi_gantry`, W 0,6, L 0,4, H 9,8,
  `Hk=7.835` (wysokość konstrukcyjna do przylepiania zwisów), `Beam_out=7.30` (długość dźwigara), `Drop_type=4840`
- Pozostałe (1901, ESN-540, new_1901/1905/1906/1907) – bez parametrów, stary układ parametrów (P6/P7 = obroty, P1 = tekstura)

## Do ustalenia z użytkownikiem (zanim powstanie kod)
1. Słup + dźwigar: gdzie jest zero dźwigara i jak siada na słupie (`Mount_low`/`high` = spód/góra dźwigara? oś?),
   co dokładnie odsuwa `Offset_x` (oś dźwigara od osi słupa w poprzek?).
2. Zwis na dźwigarze: dowolne miejsce wzdłuż dźwigara czy stałe punkty? Zero zwisu (góra – przylega do spodu dźwigara?),
   a na dole – co do niego mocujemy (wysięgnik z rur przez mocowania jak na słupie? osobne karty?).
3. Fundament słupa bramki: model fundamentu osobno (jak `foundation` przy słupie) czy w modelu słupa (3031-1…-4)?
4. Półbramki bez tagów – uzupełnić (W, L, H, Hk, Beam_out, Drop_type) czy wyliczyć z nazwy?
5. Docelowy zapis: nowe bloki w `traction_supports.txt` (np. `mast`, `beam`, `drop`, `semi_gantry`) zamiast tagów w `.inc`.
