/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

// Podwieszenia sieci trakcyjnej w scenerii: plik dołączany dyrektywą "catenary <plik> endcatenary".
// Starsze wersje symulatora nie znają tej dyrektywy, zgłaszają trzy nieznane słowa i pliku nie otwierają

#pragma once

#include <string>

class cParser;

// obsługa dyrektywy "catenary" ze scenerii: wczytuje wskazany plik podwieszeń
void catenary_scene_load( cParser &Input );
// po wczytaniu scenerii: wiąże podwieszenia z konstrukcjami wsporczymi i przęsłami po nazwach, zgłasza braki
void catenary_scene_resolve();
// przed wczytaniem nowej scenerii: usuwa podwieszenia poprzedniej
void catenary_scene_clear();
// zmienia katalog, z którego czytany jest katalog wysięgników (traction_cantilevers.txt); domyślnie "data/"
void catenary_scene_data( std::string const &Directory );
