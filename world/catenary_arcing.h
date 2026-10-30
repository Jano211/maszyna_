/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

// Sieć trakcyjna: iskrzenie pantografu. Gdy ślizg traci styk z przewodem pod prądem, powstaje łuk: jasny, niebieskawy błysk
// w miejscu styku i iskry (krople stopionej miedzi), które lecą z prędkością pojazdu, hamują w powietrzu i spadają.
// Najczęściej przy oblodzonym przewodzie (szadź, marznąca mgła i mżawka, temperatura kilka stopni poniżej zera), tym częściej,
// im większy prąd i prędkość; zawsze przy oderwaniu ślizgu pod obciążeniem (opuszczenie pantografu, koniec przewodu) i przy
// przejeździe na przęsło innej sekcji zasilania (izolator sekcyjny, izolowane przęsło naprężenia).
// Moduł liczy tylko stan (łuki, iskry); rysują go renderery (światło łuku w lokalnych światłach, iskry jako smugi).
#pragma once

#include <cstddef>
#include <vector>
#include <glm/glm.hpp>

class TTraction;

namespace catenary_arcing {

// łuk w tej chwili: punkt styku, jasność 0-1 (z migotaniem), kolor (liniowy, bez jasności)
struct arc {
    glm::dvec3 position { 0.0 };
    glm::vec3 color { 0.55f, 0.65f, 1.f };
    float intensity { 0.f };
};

// iskra: kropla rozżarzonej miedzi
struct spark {
    glm::dvec3 position { 0.0 };
    glm::dvec3 previous { 0.0 };    // położenie w poprzednim kroku: smuga od niego do position
    glm::vec3 velocity { 0.f };     // m/s
    float age { 0.f };              // s
    float life { 0.5f };            // s
    float heat { 1.f };             // 1 biało-żółta, 0 wygasła (czerwona)
};

// oblodzenie przewodów 0-1 z pogody: temperatura (szadź od ok. +1 °C w dół, najwięcej ok. -3 °C, mniej przy silnym mrozie),
// mgła i opady (wilgoć, marznąca mżawka)
float icing();

// pantograf w styku z przewodem (wołane co krok pojazdu). Key - pantograf (wskaźnik jego danych); Point - punkt styku
// we współrzędnych scenerii; Velocity - prędkość pojazdu (m/s, świat); Current - prąd przez pantograf (A); Span - przęsło
// pod ślizgiem. Zwraca siłę łuku, który się w tym kroku zaczął (0 - żaden): dla dźwięku
float contact( void const *Key, glm::dvec3 const &Point, glm::dvec3 const &Velocity, double Current, TTraction const *Span, double Dt );
// pierwsze dotknięcie przewodu (podniesienie pantografu): krótki błysk siły Strength 0-1 (ładowanie filtra wejściowego, lód
// na przewodzie albo na ślizgu). Zwraca siłę (0 - żaden)
float touch( void const *Key, glm::dvec3 const &Point, double Strength );
// ślizg stracił przewód (opuszczony, koniec przęsła, za wysoko): łuk, gdy płynął prąd. Zwraca jego siłę (0 - żaden)
float release( void const *Key, glm::dvec3 const &Point, glm::dvec3 const &Velocity, double Current );

// krok czasu: łuki gasną, iskry lecą
void update( double Dt );
std::vector<arc> const &arcs();
std::vector<spark> const &sparks();
// zmienia się po każdym update (renderer wie, że ma przepisać smugi)
std::size_t revision();
// nowa sceneria
void clear();

} // namespace catenary_arcing
