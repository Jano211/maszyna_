# Ręczne składanie wysięgników i konstrukcji nietypowych – plan (2026-11-20)

Stan: PLAN, nic niezaczęte. Ustalenia z użytkownikiem 2026-11-20 00:24 (odpowiedzi na trzy pytania) + lista „nietypowe z terenu”
z ZADANIA.md + korekty z Brochowa (PRZEKAZANIE 17–18.11). Nie nazywać tego „pizzą” (użytkownik).

## Po co

W terenie stoi mnóstwo rzeczy, których nie ma na żadnej karcie:
- sieć zlikwidowana / w budowie: same ukośniki z odciągami, wysięgniki bez łapek, bez wysięgów pomocniczych;
- „trzymacze powietrza”: zwisy bez niczego, zwisy i słupy z samymi mocowaniami, z wiszącym izolatorem (izolatorami), konsolety bez
  wysięgników, zwisy luzem przyspawane pod mostem / w hali peronowej;
- rzadkie złożenia, które trzymają druty (Brochów: dwa wysięgniki w jednym zwisie, rura krótsza o parę wykonań, przesunięte mocowania).
Większość nic nie trzyma (użytkownik: „w większości gówna do trzymania powietrza”), ale część musi trzymać linę i przewody.

## Ustalenia

1. Dwa wejścia (użytkownik: „i tak, i tak”):
   - **z karty**: bierzemy złożony wysięgnik (karta + wykonanie, ze wszystkimi korektami) i przerabiamy – szybciej usunąć łapki albo
     wysięg pomocniczy niż składać ukośnik z odciągiem od zera;
   - **od pustej konstrukcji**: słup, zwis (także luzem), konsoleta – i dokładamy klocek po klocku.
2. Dokładanie klocka: klik w punkt łączenia postawionej części → lista części, które **wolno** tam przypiąć. Co z czym wolno kleić –
   wyznaczone automatycznie z istniejących złożeń (niżej). Bez wolnego stawiania i przesuwania myszką.
3. Na koniec złożenie można zapisać jako **szablon** w osobnej kategorii („specjalne”) i wstawiać jak kartę.
4. Złożenie może trzymać druty (linę nośną, przewody jezdne) – wtedy jest normalnym podwieszeniem łańcucha – albo nic nie trzymać.
5. Wiszące izolatory: swobodnie w pionie z ucha mocowania; do wyboru wszystkie izolatory (ZADANIA.md, 2026-11-19).

## Model danych

Złożenie ręczne = drzewo części. Każda część:
- `part` – numer katalogowy (model z components/ albo mocowanie / konsoleta / zwis z traction_supports.txt);
- `on <rodzic> <punkt rodzica>` – do którego punktu którego rodzica przypięta (korzeń: konstrukcja – słup, zwis, konsoleta);
- `by <punkt własny>` – którym swoim punktem (domyślnie zero modelu);
- ułożenie: `as parent` (jak rodzic – z obrotem z reguły połączenia), `hang` (wisi pionowo, grawitacja), `toward <część> <punkt>`
  (oś części w stronę punktu innej – odciągi, linki), `stretch <część> <punkt>` (rozciągnięta do punktu – linki, liny);
- parametry z reguły połączenia: `slide <m>` (przesuw po rurze/osi rodzica w zakresie z reguły), `turn <stopnie>` (obrót wokół osi
  połączenia, np. ćwierć obrotu izolatora na szekli), `hole <n>` (otwór skokowy – 4400, łańcuchy z otworami), `length` (rura z
  rodziny długości – „o parę wykonań krótsza”).
Usunięcie części usuwa całe jej poddrzewo (zdejmujesz łapki – idą też uchwyty drutu na nich).

Współrzędne jak w solverze wysięgnika (zero w osi przewodów przy podwieszeniu, Y do konstrukcji, Z w górę) albo – dla złożeń bez
drutów – od konstrukcji (zero na czole, jak mocowania). Nic nie jest liczone „na żywo” z zygzaka: złożenie ręczne jest **zamrożone**
– zmiana odsuwu / wysokości drutów nie przestawia części (DO POTWIERDZENIA, pytanie 1).

## Reguły „co z czym” – z istniejących złożeń

Narzędzie (rozszerzenie gen_cantilevers.py albo test w C++ na solverze): składa **wszystkie** karty × wykonania z katalogu
i dla każdej pary części zapisuje, który nazwany punkt jednej (hole_1, eye, end, mount_*, slide_*, holder, hook…) wypada w zerze albo
punkcie drugiej (tolerancja ~1 mm), z obrotem względnym i – gdy część siedzi na rurze – zakresem przesuwu (slide_min..slide_max).
Wynik: plik `traction_joints.txt`, np.
```
joint 4070 eye  -> 4400 zero   turn 0      // końcówka odciągu na sworzniu uchwytu
joint 6001 axis -> 4050 zero   slide 0.20 2.80   // uchwyt na ukośniku, przesuw po rurze
joint 4690 mount_1 -> 4630-06 zero
joint mount eye -> 7020 zero   hang        // izolator wiszący na mocowaniu (z wpisu ręcznego, nie ze złożeń)
```
Ręczne dopiski w osobnej sekcji (rzeczy, których żadna karta nie składa: wiszący izolator na mocowaniu, izolator bezpośrednio na
konsoli). Generowane i ręczne w jednym pliku, generator nie rusza sekcji ręcznej. Rodziny zamienników (7010↔7110, 7020↔7120,
-01/-02/-25) już są – lista podpowiada też zamiennik w tym samym miejscu.

## Zapis w pliku sieci

Podwieszenie z ręcznym złożeniem (to samo słowo `suspension`, żeby łańcuch, słup, zwis, przeciąganie działały jak dotąd):
```
node ... suspension support <słup|zwis> ... manual
  part a1 4630-03 on pole steady
  part a2 7020-01 on a1 eye hang
  part a3 6011-05 on a2 end as parent length 6011-03
endmanual ... endsuspension
```
Z karty: `scheme 1271 3` + `manual from_scheme` + lista różnic (`remove <rola>`, `override`, `slide` …) – złożenie liczy się kartą,
potem nakłada różnice. Dzięki temu „wysięgnik bez łapek” to jedna linijka `remove arms`, a karta dalej się dopasowuje do drutów.
Szablony: `traction_custom.txt` (kategoria „specjalne” w katalogu edytora), ten sam format bez położenia; w scenerii `manual
template <nazwa>`.
Stare exe: nowe słowa w środku `suspension` → jeden błąd na wpis (jak przy innych nowych dopiskach) – zgodność wstecz niewymagana.

## Trzymanie drutów

Złożenie trzyma drut, gdy ma część z rolą drutu: uchwyt liny (4140, 4160, siodła teowników) – lina nośna w jego zerze; zestaw ramion
/ uchwyt przewodu (62xx, 4120) – przewody w punktach drutów zestawu. Wtedy wpis podaje łańcuchowi `rope` i `at` z tych punktów.
W trybie „z karty” – jak dziś (karta liczy się do drutów, różnice na wierzchu). W trybie „od zera” – części zamrożone, druty idą tam,
gdzie stoją uchwyty (zygzak ustawia się uchwytem: `slide` po wysięgu).

## Edytor (tryb ekspert)

- Panel wysięgnika: przycisk „Złóż ręcznie” (bierze bieżącą kartę) i na stronie słupów / bramek „Pusta konstrukcja” (słup, zwis,
  zwis luzem, konsoleta).
- Drzewo części (lista z wcięciami): klik zaznacza część w widoku; Del – usuwa z poddrzewem; przy zaznaczonej – suwaki jej parametrów
  (slide, turn, hole, długość z rodziny).
- W widoku: punkty łączenia zaznaczonej części jako kropki; klik w kropkę → lista pasujących części (z traction_joints.txt).
- „Zapisz jako szablon…” → nazwa, opis → traction_custom.txt; szablony w katalogu w kategorii „Specjalne”.
- Zwis luzem: wstawiany LPM na dowolnej wysokości (podgląd), obrót, bez bramki; dalej jak zwis.

## Kolejność (propozycja)

1. traction_joints.txt z generatora + test, że każde istniejące złożenie da się odtworzyć z reguł (to sprawdza reguły).
2. Tryb „z karty” z `remove` / `override` / `slide` – od razu daje sieć bez łapek, bez wysięgów pomocniczych, Brochów (część korekt
   już jest w „Korektach z terenu”).
3. Puste konstrukcje: słup / zwis / konsoleta + mocowania + wiszące izolatory (lista GOP z ZADANIA.md), zwisy luzem.
4. Dokładanie klocków po punktach w edytorze.
5. Szablony (traction_custom.txt, kategoria „Specjalne”).
6. Złożenia od zera trzymające druty.

## Pytania do użytkownika

1. Złożenie od zera trzymające druty: zamrożone (druty idą tam, gdzie stoi uchwyt) czy ma się dopasowywać jak karta (to bardzo dużo
   pracy – trzeba by zgadywać, które części mają się ruszać)?
2. Szablony tylko twoje lokalne (`traction_custom.txt` w data/), czy też do rozsyłania razem ze scenerią (jak pliki sieci)?
3. Czy kolejność wyżej pasuje (najpierw „z karty” z usuwaniem, bo daje najwięcej od razu)?
