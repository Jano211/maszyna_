# Konwersja starej sieci (Rainsted, szablony siec_trakcyjna_ph) na nową sieć

Pierwsza próba: `scenery\l61\siec\cze.scm` (Częstochowa) → `cze_nowa.scm` + plik sieci `cze_nowa_siec.scm` (2026-10-22).

1. `sh tests/catenary/build.sh` (buduje też `conv_pick` – dobór karty, wykonania, hk i złożenie solverem, na atrapach exe).
2. Pliki z G: (`scenery\l61\siec\cze.scm`, `scenery\l61\tory\cze.scm`, użyte szablony `scenery\siec_trakcyjna_ph\*.inc`) do jednego katalogu.
3. `python3 tests/catenary/konwersja/convert.py <cze.scm> <tory/cze.scm> <katalog szablonów> <wyjście .scm> <wyjście pliku sieci> <ścieżka pliku sieci w scenerii> <repo>`

Co robi:
- Słupy z wysięgnikiem `sts1|sts2|stb1+<A>_<2|3><k|d>.inc` → podwieszenie `pole` (sts → 1613-11, stb → 1701), punkt drutu i liny z komentarza `//rainsted,h0,v0,h1,v1`, oś słupa z przesunięcia modelu słupa w szablonie; główka szyny = najbliższy tor pod drutem + 0,18.
- Karta: grupa „rurowe przechylne 11xx”, rodzina z łuku (promień toru < 4000) i odsuwu, wykonanie: 3 przewody → wyk. 1 (potem 2), 2 przewody → wyk. 5 (potem 3, 4); hk z karty, gdy stara różni się najwyżej o 0,15 m. Gdy wysięgnik ma uwagi, słup odsuwa się od toru do 0,15 m (co 2,5 cm).
- Przęsła z hk > 1,0: `type YC120-2C150` (Wires 3/4) albo `C95-C` (Wires 2); końce lin przy przekonwertowanych podwieszeniach na wysokość liny z karty.
- Reszta (bramki, zwisy, kotwy, odciągi, izolatory, wysięgniki „z”, „n”, „nn”, same wysięgniki na nogach bramek) zostaje po staremu; przekonwertowane includy są zakomentowane `//conv`.
- Od 31.10 dyrektywa `catenary` (i wpisy sieci w scenerii) biorą `origin`, więc plik sieci ma współrzędne jak przekonwertowana sceneria. Pliki z pierwszej konwersji (cze_nowa_siec.scm, z dodanym originem) trzeba przekonwertować jeszcze raz albo dać dyrektywę poza blokiem origin.
