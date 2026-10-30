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

// liczby iskrzenia: domyślne wpisane w symulator, nadpisywane plikiem data/traction_arcing.txt (czytany raz, przy pierwszym użyciu;
// brak pliku albo wpisu - domyślne). Opis kluczy w pliku
struct settings {
    // oblodzenie z pogody
    float icing_start { 1.f };              // °C, od tej temperatury w dół
    float icing_peak { -3.f };              // °C, najwięcej
    float icing_cold_span { 20.f };         // o tyle stopni poniżej szczytu oblodzenie spada do icing_cold_floor
    float icing_cold_floor { 0.25f };
    float icing_fog[ 2 ] { 2000.f, 300.f }; // widzialność (m), od której mgła się liczy, i przy której liczy się w pełni
    float icing_dry { 0.25f };              // wilgoć bez mgły i opadów (0-1)
    float icing_spread[ 2 ] { 0.4f, 1.6f }; // mnożnik oblodzenia przęseł, od - do (każde przęsło stałe)
    // łuki od lodu
    float ice_rate { 10.f };                // łuków na sekundę przy pełnym oblodzeniu, prędkości ice_speed i prądzie ice_current
    float ice_speed { 25.f };               // m/s
    float ice_current { 1000.f };           // A
    float ice_min_speed { 1.f };            // m/s, wolniej nie iskrzy
    float ice_duration[ 2 ] { 0.02f, 0.12f }; // s
    // ciągły słaby łuk przy mocnym oblodzeniu (Jan 2026-10-30: „napierdala ciągiem z dodatkowymi pierdolnięciami”): od oblodzenia
    // sustain_from w górę ślizg jedzie po lodzie w ciągłym łuku, na pełnym oblodzeniu o jasności sustain_level; iskier na sekundę
    float sustain_from { 0.45f };
    float sustain_level { 0.3f };
    float sustain_sparks { 60.f };
    // przejście sekcji
    float section_current { 20.f };         // A, mniej - nic
    float section_reference { 1500.f };     // A, pełna siła
    float section_duration[ 2 ] { 0.05f, 0.15f };
    // oderwanie pod obciążeniem
    float release_current { 15.f };
    float release_reference { 800.f };
    float release_duration[ 2 ] { 0.12f, 0.35f }; // stała + przyrost przy pełnej sile
    // pierwsze dotknięcie
    float touch_base { 0.08f };             // samo dotknięcie (odgromnik, woltomierz)
    float touch_filter { 0.35f };           // pojazd z falownikami (filtr wejściowy)
    float touch_ice { 0.45f };              // przy pełnym oblodzeniu
    float touch_current { 1500.f };         // A, prąd przy dotknięciu dodaje do 0,4
    float touch_duration[ 2 ] { 0.03f, 0.06f };
    // łuk: kolory (liniowe), migotanie, światło
    glm::vec3 arc_color[ 2 ] { { 0.25f, 0.4f, 1.f }, { 0.5f, 0.62f, 1.f } };
    float arc_flicker { 0.35f };            // najmniejsza jasność w migotaniu
    float arc_light { 10.f };               // jasność światła (razy gfx.lights.freespot.intensity)
    float arc_light_range { 40.f };         // m
    float arc_core_size[ 2 ] { 0.05f, 0.25f }; // m, rdzeń łuku: stała + przyrost przy pełnej sile
    // iskry
    float sparks_rate[ 2 ] { 40.f, 400.f }; // na sekundę: stała + przyrost przy pełnej sile
    float sparks_spray[ 2 ] { 2.f, 8.f };   // m/s rozprysku
    float sparks_carry[ 2 ] { 0.6f, 1.f };  // część prędkości pojazdu
    float sparks_life[ 3 ] { 0.2f, 0.5f, 0.7f }; // s: najkrócej, najdłużej + przyrost przy pełnej sile
    float sparks_drag { 3.5f };             // 1/s
    float sparks_max { 1500.f };
    // kolory smug (renderer): iskry od gorących do gasnących i rdzeń łuku; rgb jasność (ponad 1 - jaśniej niż biel), alfa
    glm::vec4 spark_color[ 4 ] { { 1.9f, 1.8f, 1.6f, 0.9f }, { 1.7f, 1.2f, 0.6f, 0.85f }, { 1.4f, 0.7f, 0.25f, 0.75f }, { 1.0f, 0.35f, 0.1f, 0.6f } };
    glm::vec4 core_color { 1.1f, 1.5f, 2.8f, 0.9f };
};
settings const &parameters();

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
// siła błysku przy dotknięciu z ustawień: Filter - pojazd z falownikami, Current - prąd w chwili dotknięcia
double touch_strength( bool Filter, double Current );
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
