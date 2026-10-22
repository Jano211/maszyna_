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

## Ustalenia (2026-10-21, odpowiedzi użytkownika)
1. Dźwigar siada na słupie na wysokości: główka szyny (0) + prześwit (`Mount_low` / `Mount_high`).
   `Offset_x` = przesunięcie dźwigara w Y względem środka słupa, żeby trafił w mocowanie (słup końcowy ma mocowania
   w środku, przelotowy – możliwość dźwigarów z obu stron). Nic więcej.
2. Zwis przesuwa się po dźwigarze; zero zwisu ustawione tak, że chodzi na wysokości prześwitu. Typ zwisu musi pasować
   do konstrukcji (`Drop_type`). Na zwisie montuje się wysięgnik – zwykły albo parasol: ZWIS = „pływający słup” na bramce
   (solver jak na słupie). Mocowania: te same opaski co na słupach (4610/4620/4630); numer wykonania NIEZNANY –
   któreś z niskich, pod profil zwisu 120x200 mm (stare zwisy 200x120, odwrotnie) – dobrać przy kodzie po wymiarach opasek.
   Dźwigar jest kratownicą, zwis przechodzi przez jego środek, więc w rzeczywistości nie każde miejsce jest możliwe
   (prawdopodobnie schodki co ok. 60 cm, w każdym ±10–20 cm ruchu) – NIE robimy (za duży wysiłek, mało widoczne).
   Ewentualnie kiedyś po sprawdzeniu na modelach.
3. Fundamentu osobno nie ma – słupy bramek mają po prostu więcej wykonań (-1…-4). Można je kiedyś rozdzielić;
   malowane osobno, więc mogą służyć za dodatkowy wariant tekstury.
4. Półbramkom bez tagów trzeba dopisać brakujące parametry (W, L, H, Hk, Beam_out, Drop_type).
5. Zapis docelowo w `traction_supports.txt` jako nowe bloki (np. `mast`, `beam`, `drop`, `semi_gantry`) – tak.

## Zrobione 2026-10-22 – bramki jednoprzęsłowe z dźwigarem E i zwisy 4850

- `traction_supports.txt`: `mast` (38 słupów 30xx/31xx z tagów `$e_`), `beam` (261 dźwigarów E90…E349, E385; rozstaw osi słupów
  = numer/10 m, zero w osi pierwszego słupa – użytkownik), zwisy `pole 4850-1…5` jako „pływające słupy”: zero w spodzie
  dźwigara (`top 0`), mocowania wyk. 05 (karta 53-0501 – użytkownik), w pełnym zakresie długości (`mount_max -0.001`,
  `mount_min -długość`), `turn 90` (profil 0,12 m w stronę toru), `drop 4850`.
- Solver: `mount_max` może być ujemne (warunek `!= 0`), nowe `mount_min` (uwaga, gdy mocowanie wypada pod końcem zwisu),
  `turn` obraca model słupa wokół pionu.
- Plik sieci: wpis `gantry` (beam, level low|high, side ±1, dwa `mast <typ> x y z`); exe wstawia słupy i dźwigar jako modele.
  Zwisy z wysięgnikami to zwykłe podwieszenia z `pole 4850-N x y z` (y = spód dźwigara) – działają z nimi łańcuchy, parasole itd.
- Edytor (Sieć trakcyjna): „Postaw bramkę” – LPM pierwszy słup, LPM drugi (dźwigar z katalogu najbliższy odległości,
  drugi słup dosunięty na jego rozstaw), dolne/górne mocowanie, strona dźwigara. Klik na dźwigar = bramka zaznaczona:
  LPM nad torem wiesza zwis z wysięgnikiem (najkrótszy zwis, przy którym mocowania mieszczą się na zwisie; zwis
  przesunięty wzdłuż toru na linię dźwigara). „Wróć do słupów” kończy.
- Solver na zwisach (drut 5,6, środek pasm, karty 11xx/12xx): dźwigar 7,35 – większość z uwagą „odciąg ścięty do spodu
  dźwigara” (przy hk 1,70 odciąg chce być nad dźwigarem); 7,75 – zwis 4850-4/-5 prawie wszystko bez uwag.
- Jeszcze nie ma: półbramki, dźwigary środkowe 30xx (wiele przęseł), zwisy 4840 i ESN-544, przesuwanie zwisu po dźwigarze
  przeciąganiem.

### Poprawki 2026-10-22 (po obejrzeniu przez użytkownika)
- Słupy bramek obrócone o 90°: X modelu (szerokość W) wzdłuż dźwigara. Offset działa wzdłuż dźwigara: dźwigar biegnie
  między ścianami słupów, osie słupów są o offset każdego słupa dalej niż końce dźwigara (rozstaw osi = numer/10 + offsetA
  + offsetB). Każdy koniec dźwigara bierze swój słup (offset, prześwit) – przy różnych słupach dźwigar leży lekko skośnie.
  `side` we wpisie zostaje (zapisywane 1), na razie bez znaczenia.
- Tekstura wymienna bramki (pole „Tekstura”): słupy, dźwigar i zwisy nowej bramki; „Nadaj teksturę” dla zaznaczonej
  (razem ze zwisami na dźwigarze).
- „Prostopadle do toru”: drugi słup prostopadle do toru, przy którym postawiono pierwszy.
- Wysokość słupów zaznaczonej bramki: pola „Słup 1/2 na wysokości” (zero słupa w główce szyny; teren bierze głowica
  fundamentu wykonania -1…-4). Gizmo edytora nie działa na części sieci – są poza warstwami scenerii.
- Przeciąganie: słup (zwykły) wzdłuż toru, bramka (za dźwigar) razem ze zwisami.
- Poprawka (użytkownik 2026-10-22 23:48): między słupami jest o 0,20 m mniej niż numer dźwigara (E101 = 9,9 m), drugiego słupa
  nie obracamy. Rozstaw osi = numer/10 − 0,20 + offsetA + offsetB (`catenary::gantry_spacing`). Edycja postawionej bramki:
  dźwigar z listy / ±0,1 m, obrót wokół środka, „Prostopadle do toru”, „Weź słupy”. Zwisy idą za bramką tylko przy przeciąganiu.

## Bramki wieloprzęsłowe (2026-10-22 wieczór)
- Słupy końcowe (`end`) kończą bramkę, przelotowe (`through`) mogą kończyć albo stać w środku z dźwigarami z obu stron.
- Każde przęsło to osobny wpis `gantry`; przęsła na wspólnym słupie (ten sam punkt) tworzą jedną bramkę; słup stawia przęsło o najniższym numerze.
- Dźwigary 30xx (pojedyncze elementy na jednym słupie przelotowym albo 1901) i 1910 (zewnętrzne półbramek) – jeszcze nie, pytania w PRZEKAZANIE.md.
