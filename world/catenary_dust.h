/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

// Sieć trakcyjna: pył na słupach i osprzęcie (brązowo-miedziany pył z przewodów i z toru, ogólny brud). Nakłada go shader
// materiału mat_normalmap_specgloss: na wszystkie modele wstawione przez sieć (słupy, fundamenty, wysięgniki, osprzęt, bramki -
// znają wysokość szyny swojego słupa), na inne tylko z "param_dust: <mnożnik>" w .mat; "param_dust: <mnożnik>" zmienia też
// ilość na częściach sieci, ujemny wyłącza. Według położenia w świecie, nie według UV: plamy biorą szum 3D ze współrzędnych świata, więc słup i przykręcone do niego mocowanie
// mają jedną, ciągłą warstwę. Ile pyłu: ułożenie powierzchni (góra najwięcej, pion mniej, spód prawie nic) i wysokość nad
// główką szyny słupa, na którym część siedzi (fundament i dół słupa najbardziej). Rdza i inne lokalne zabrudzenia zostają
// w teksturach.
// Wysokość szyny: TAnimModel::dust_rail() - wpisuje ją kod sieci przy stawianiu części (zero słupa), renderer przekazuje ją
// shaderowi w dolnym wierszu macierzy instancji (scene::basic_cell::instance_record, effective_modelview() w common.glsl).
#pragma once

#include <glm/glm.hpp>

namespace gl {
struct scene_ubs;
}

namespace catenary_dust {

// liczba punktów profilu wysokości
int constexpr kProfilePoints { 8 };
// okres szumu pyłu w świecie, m: położenie kamery przekazywane shaderowi jest zawinięte do niego (szum ma ten sam okres,
// więc zawinięcie nie robi skoku). Ta sama liczba w shaders/ogl46/dust.glsl i shaders/vk14/dust.glsl (DUST_PERIOD)
double constexpr kPeriod { 4096.0 };

// liczby pyłu: domyślne wpisane w symulator, nadpisywane plikiem data/traction_dust.txt (czytany raz, przy pierwszym użyciu;
// brak pliku albo wpisu - domyślne). Opis kluczy w pliku
struct settings {
    glm::vec3 color { 0.148f, 0.066f, 0.033f }; // kolor pyłu, liniowy (w pliku sRGB, #RRGGBB; domyślny #6B4A36)
    float strength { 1.f };                     // mnożnik całości (0 - wyłączony)
    float side { 0.45f };                       // ściany pionowe względem poziomych od góry
    float bottom { 0.08f };                     // spody (powierzchnie zwrócone w dół)
    float contrast { 0.7f };                    // plamistość: 0 - równa warstwa, 1 - od zera do podwójnej
    float gloss { 0.6f };                       // o tyle (część) pełny pył zmniejsza połysk
    float metal { 0.9f };                       // o tyle zmniejsza metaliczność
    float reflection { 0.8f };                  // o tyle zmniejsza odbicie otoczenia
    float no_rail { 2.f };                      // wysokość nad szyną przyjmowana dla modeli bez wysokości szyny, m
    // profil: wysokość nad główką szyny (m, rosnąco) i ilość pyłu; między punktami liniowo, poza nimi jak w skrajnym
    glm::vec2 profile[ kProfilePoints ] {
        { -3.f, 1.f }, { 0.f, 1.f }, { 0.6f, 0.75f }, { 2.f, 0.4f },
        { 4.f, 0.3f }, { 5.5f, 0.5f }, { 7.f, 0.55f }, { 10.f, 0.35f } };
};
settings const &parameters();

// ustawienia pyłu w bloku sceny przebiegu: kolor, liczby, profil i położenie kamery Camera (współrzędne scenerii) zawinięte
// do okresu szumu
void setup_scene( gl::scene_ubs &Scene, glm::dvec3 const &Camera );

} // namespace catenary_dust
