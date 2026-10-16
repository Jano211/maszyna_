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
g++ $F -I$T/mock -I. $T/test_scene.cpp world/catenary_scene.cpp world/catenary_suspension.cpp world/catenary_cantilever.cpp world/catenary_span.cpp world/catenary_network.cpp world/catenary_solver.cpp -o $T/tsc
# 5. solver wysiegnika: katalog z data/traction_cantilevers.txt i porownanie ze zlozeniami z Blendera (cantilever_ref.txt)
g++ $F -I. $T/test_cantilever.cpp world/catenary_cantilever.cpp -o $T/tc
echo "zbudowane: t ts sim tsc tc"
