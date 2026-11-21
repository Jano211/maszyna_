/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

// Sieć trakcyjna: przęsła nowej sieci (łańcuchy, TTraction z m_catenary) i zasilacze scenerii jako obwód prądu stałego
// (world/catenary_power.h). Stara sieć (node traction poza łańcuchem) liczy się dalej po staremu.
// Zasady przyłączenia (etap B planu, tests/catenary/ZASILANIE.md):
// - przęsła połączone końcami (hvNext) to jeden węzeł, jeśli mają ten sam zasilacz albo jedno z nich ma nazwę sekcji (kabina
//   sekcyjna, wpis "section") - jak w starej sieci; różne zasilacze = przerwa (izolator sekcyjny, izolowane przęsło naprężenia);
// - przęsła bieżni wspólnej (przęsło naprężenia) z tym samym zasilaczem połączone końcami leżącymi obok siebie (łączniki 0640);
// - zasilacz przyłączony kablem do najbliższego końca przęsła ze swoją nazwą w każdej części sieci, kabina sekcyjna (wpis
//   "section") to szyna zbiorcza bez źródła, przyłączona tak samo - łączy sieci obu torów;
// - części sieci bez żadnego zasilacza zostają po staremu (napięcie z wpisu przęsła), żeby nie wyłączyć prądu w sceneriach,
//   w których nazwa zasilacza nie ma swojego wpisu.
#pragma once

#include <string>

class TTraction;

// coś się zmieniło w sieci albo w zasilaczach: obwód do przebudowania przed następnym krokiem
void catenary_power_changed();
// krok obwodu: rozwiązanie z poborem pojazdów zgłoszonym od poprzedniego kroku, wyłączniki szybkie
void catenary_power_update( double Dt );
// napięcie dla pantografu na przęśle (pobór Current [A] zgłaszany do następnego kroku); false - przęsło nie jest w obwodzie
// (stara sieć albo część bez zasilacza) i liczy się po staremu
bool catenary_power_voltage( TTraction const &Span, double Current, double &Voltage );

// stan zasilacza w obwodzie (do panelu i podglądu); false, gdy zasilacz nie jest w obwodzie nowej sieci
struct catenary_power_feeder {
    double current { 0.0 };        // A
    double voltage { 0.0 };        // V na szynie zasilacza
    bool tripped { false };
    bool locked { false };
    bool conducting { true };
    double timer { 0.0 };          // s, od wyłączenia (przy otwartym wyłączniku)
    int trips { 0 };               // wyłączenia z rzędu
};
bool catenary_power_feeder_state( std::string const &Name, catenary_power_feeder &State );
