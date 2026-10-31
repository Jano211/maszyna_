/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

// Sieć trakcyjna: szablony przęseł naprężenia (karty Katalogu ESN tom 1: 0060, 0062, 0068, 0078; Kolprojekt 2010: 0060-36/37),
// plik data/traction_overlaps.txt. Edytor stawia z nich całe przęsło naprężenia: słupy w odległościach z karty, na każdym role obu
// sieci (sieć A dochodzi do przęsła i kończy się kotwieniem, sieć B zaczyna się kotwieniem i idzie dalej).
// Kod niezależny od reszty symulatora (C++17, tylko biblioteka standardowa).
#pragma once
#include <string>
#include <vector>

namespace catenary {

// rola sieci na słupie przęsła naprężenia
enum class overlap_role { none, anchor, work, lift };

struct overlap_pole {
    std::string name;                   // K1, P1, A0... (z karty)
    double distance { 0.0 };            // m, od poprzedniego słupa wzdłuż toru
    overlap_role net[ 2 ] { overlap_role::none, overlap_role::none };  // sieć A, sieć B
    bool same { false };                // na tej samej konstrukcji co poprzedni (wysięgniki rozstawione na konsoli 4890)
};

struct overlap_card {
    std::string name;                   // numer karty, np. 33-0060-3
    std::string title;
    std::vector<int> networks;          // kody typów sieci (traction_code), dla których jest karta
    bool curve { false };               // na łuku (0078)
    double stagger { 0.20 };            // z: odsuw każdej sieci od osi toru na słupach krzyżowych
    double lift { 0.15 };               // poderwanie przewodu schodzącego do kotwienia
    double common { 0.0 };              // wspólna bieżnia c (opis)
    double height { 1.70 };             // hk sieci pracującej
    double anchor_rope { 6.70 }, anchor_wire { 6.70 };   // kotwienie liny i przewodów nad główką szyny
    std::vector<overlap_pole> poles;
    // pierwszy słup, na którym pracuje albo jest poderwana któraś sieć (tam klika użytkownik); poles.size() gdy żadnego
    std::size_t first_crossing() const;
    // odległość wzdłuż toru od pierwszego słupa krzyżowego do słupa Index (ujemna przed nim)
    double offset( std::size_t Index ) const;
    // długość całego przęsła (od kotwowego do kotwowego)
    double length() const;
    bool fits( int Network ) const;
};

// czyta plik szablonów podany liniami (komentarze // zdejmuje sam). Błędne wpisy pomijane, opis trafia do Errors
void parse_overlaps( std::vector<std::string> const &Lines, std::string const &Source, std::vector<overlap_card> &Output, std::vector<std::string> &Errors );
// wczytuje plik; false, gdy nie da się go otworzyć
bool load_overlaps( std::string const &Filename, std::vector<overlap_card> &Output, std::vector<std::string> &Errors );

} // namespace catenary
