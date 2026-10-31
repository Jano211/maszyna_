#!/bin/sh
# Budowanie testow sieci trakcyjnej poza exe (Linux, g++ >= 11, naglowki glm).
# Uruchamiac z katalogu, w ktorym leza world/ i tests/catenary/ (czyli z C:\Maszyna albo jego kopii).
# T = katalog testow, domyslnie tests/catenary
T=${T:-tests/catenary}
F="-std=c++17 -O1 -w"
set -e
# 1. katalog i geometria: 5059 dlugosci wieszakow z katalogu
g++ $F -I. $T/test_network.cpp world/catenary_network.cpp world/catenary_solver.cpp -o $T/t
# 2. parser pliku podwieszen i rejestr konstrukcji wsporczych
g++ $F -I. $T/test_suspension.cpp world/catenary_suspension.cpp -o $T/ts
# 3. cala sceneria przez atrapy naglowkow exe (atrapy musza byc PRZED -I.)
g++ $F -I$T/mock -I. $T/sim_traction.cpp world/catenary_span.cpp world/catenary_network.cpp world/catenary_solver.cpp -o $T/sim
# 4. wczytanie pliku podwieszen i wiazanie nazw
g++ $F -I$T/mock -I. $T/test_scene.cpp $T/mock_objects.cpp world/catenary_scene.cpp world/catenary_tensioner.cpp world/catenary_layout.cpp world/catenary_overlap.cpp world/catenary_catalog.cpp world/catenary_suspension.cpp world/catenary_cantilever.cpp world/catenary_span.cpp world/catenary_network.cpp world/catenary_solver.cpp -o $T/tsc
# 5. solver wysiegnika: katalog z data/traction_cantilevers.txt i porownanie ze zlozeniami z Blendera (cantilever_ref.txt)
g++ $F -I. $T/test_cantilever.cpp world/catenary_cantilever.cpp -o $T/tc
# 6. edytor: zasady rozstawiania (traction_layout.txt), zapis wpisow, dodawanie/zmiana/usuwanie podwieszen na zywo
g++ $F -I$T/mock -I. $T/test_editor.cpp $T/mock_objects.cpp world/catenary_scene.cpp world/catenary_tensioner.cpp world/catenary_layout.cpp world/catenary_overlap.cpp world/catenary_catalog.cpp world/catenary_suspension.cpp world/catenary_cantilever.cpp world/catenary_span.cpp world/catenary_network.cpp world/catenary_solver.cpp -o $T/te
# 7. katalog konstrukcji dla edytora (traction_catalog.txt): pokrycie kart, dobor karty z grupy
g++ $F -I. $T/test_catalog.cpp world/catenary_catalog.cpp world/catenary_cantilever.cpp -o $T/tk
# 8. konwersja starej sieci: dobor karty i zlozenie solverem (tests/catenary/konwersja)
g++ $F -I$T/mock -I. $T/conv_pick.cpp $T/mock_objects.cpp world/catenary_scene.cpp world/catenary_tensioner.cpp world/catenary_layout.cpp world/catenary_overlap.cpp world/catenary_catalog.cpp world/catenary_suspension.cpp world/catenary_cantilever.cpp world/catenary_span.cpp world/catenary_network.cpp world/catenary_solver.cpp -o $T/conv_pick
# 9. urzadzenie naprezajace (kotwienie ciezarowe): tablica 0011-1, naciagi z dzwigni = karty 0002, lina, stan sieci
g++ $F -I. $T/test_tensioner.cpp world/catenary_tensioner.cpp -o $T/tt
# 10. iskrzenie pantografu: oblodzenie z pogody, łuki, iskry
g++ $F -I$T/mock_arcing -I. -Iref/glm $T/test_arcing.cpp world/catenary_arcing.cpp -o $T/ta
echo "zbudowane: t ts sim tsc tc te tk conv_pick tt ta"
