/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

// Sieć trakcyjna: obwód prądu stałego nowej sieci (plan „od A do Z”, tests/catenary/ZASILANIE.md).
// Węzły to końce przęseł, gałęzie to przęsła (opór przewodów jezdnych i lin nośnych równolegle, z szynami jako przewodem
// powrotnym dopisanymi szeregowo). Zasilacze (odpływy podstacji) to źródła napięcia z oporem wewnętrznym i diodą prostownika:
// bez prądu wstecznego, chyba że przyjmują rekuperację. Pojazdy to odbiorniki prądu z poprzedniego kroku (pobór dodatni,
// rekuperacja ujemna). Obwód dzielony jest na wyspy (części połączone gałęziami); każda wyspa liczona osobno, rozkładem LDLᵀ
// w wąskiej obwiedni (sieć to głównie łańcuchy), liczonym od nowa tylko przy zmianie układu albo stanu zasilaczy - co krok
// samo podstawienie, więc koszt rośnie liniowo z liczbą przęseł.
// Moduł nie zależy od symulatora - dane podaje mu world/catenary_power_scene.cpp, testy tests/catenary/test_power.cpp.
// Stara sieć (node traction bez łańcucha) liczy się po staremu, w TTraction / TTractionPowerSource.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace catenary_power {

// ustawienia z data/traction_power.txt (bez pliku - te wartości)
struct settings {
    double rail { 0.015 };              // Ω/km, opór toru (dwie szyny równolegle ze złączami) - przewód powrotny, szeregowo z siecią
    double feeder { 0.005 };            // Ω, opór połączenia zasilacza z siecią (kabel zasilacza, odłącznik)
    double cabin { 0.002 };             // Ω, opór połączenia sieci z szyną zbiorczą kabiny sekcyjnej
    double leakage { 1.0e-9 };          // S, upływność węzła do szyn (żeby wyspa bez zasilacza miała napięcie 0, a nie nieokreślone)
    double regen_limit { 3900.0 };      // V, powyżej pojazdy nie oddają już energii (sieć nie przyjmuje rekuperacji)
    // opór właściwy przewodów (Ω·mm²/m): przewód jezdny Cu twardy (Djp100 0,182 Ω/km - Głowacki), liny Cu, aluminium, brąz
    double copper_contact { 0.0182 };
    double copper_rope { 0.0179 };
    double aluminium { 0.0287 };
    double bronze { 0.0280 };           // lina brązowa (CuSi) - ZAŁOŻENIE, do sprawdzenia
};
settings const &current_settings();
// wczytuje ustawienia z pliku (wołane raz, przy pierwszym użyciu; Path pusty = data/traction_power.txt)
void load_settings( std::string const &Path = std::string() );

// zasilacz: źródło napięcia z oporem wewnętrznym, wyłącznikiem szybkim i diodą prostownika
struct source {
    std::string name;
    int node { -1 };                    // węzeł obwodu, do którego przyłączony jest zasilacz (jego szyna)
    double voltage { 3300.0 };          // V, napięcie źródłowe (bieg jałowy)
    double internal { 0.2 };            // Ω, opór wewnętrzny (podstacja + kabel)
    double trip_current { 0.0 };        // A, nastawa wyłącznika szybkiego; 0 = bez wyłącznika
    double fast_timeout { 1.0 };        // s, ponowne załączenie po wyłączeniu
    int repetition { 3 };               // tyle ponownych załączeń, potem długa przerwa
    double slow_timeout { 60.0 };       // s, długa przerwa
    bool recuperation { false };        // przyjmuje rekuperację (prąd wsteczny)
    // stan
    bool tripped { false };             // wyłącznik szybki otwarty
    bool locked { false };              // długa przerwa po wyczerpaniu ponownych załączeń
    bool conducting { true };           // prostownik przewodzi (napięcie sieci niższe od źródłowego albo rekuperacja)
    double timer { 0.0 };
    int trips { 0 };
    double current { 0.0 };             // A, prąd oddawany do sieci (ujemny - przyjmowany z rekuperacji)
    double output { 0.0 };              // V, napięcie na szynie zasilacza
};

// gałąź: przęsło albo połączenie (łącznik, kabel), z oporem
struct branch {
    int a { -1 };
    int b { -1 };
    double resistance { 0.0 };          // Ω
    double load { 0.0 };                // A, pobór pojazdów w tym kroku (w połowie do każdego końca); ujemny - rekuperacja
};

class circuit {
public:
    // budowa: węzły numerowane od 0
    int add_node();
    int add_branch( int A, int B, double Resistance );
    int add_source( source const &Source );
    void clear();
    std::size_t nodes() const { return m_voltage.size(); }
    std::vector<branch> &branches() { return m_branches; }
    std::vector<source> &sources() { return m_sources; }
    std::vector<branch> const &branches() const { return m_branches; }
    std::vector<source> const &sources() const { return m_sources; }

    // pobór pojazdu z gałęzi w tym kroku (dodawany do już zgłoszonych); A, ujemny - rekuperacja
    void add_load( int Branch, double Current );
    void clear_loads();
    // rozwiązanie obwodu z bieżącymi odbiornikami i stan wyłączników po kroku Dt [s]
    void solve( double Dt );
    // napięcie węzła względem szyn
    double voltage( int Node ) const;
    // napięcie w środku gałęzi (tam przyjmowany jest pojazd): średnia końców minus spadek na połówkach od jej własnego poboru
    double branch_voltage( int Branch ) const;
    // prąd w gałęzi od a do b (bez jej własnego poboru)
    double branch_current( int Branch ) const;
    // wyspa węzła (numer części obwodu połączonej gałęziami)
    int island( int Node ) const;
    // jaka część rekuperacji pojazdów wyspy węzła została przyjęta przez sieć w ostatnim kroku (1 - całość)
    double regen_scale( int Node ) const;
    // podział na wyspy po zmianie układu (solve robi to sam); czy wyspa węzła ma jakikolwiek zasilacz
    void prepare();
    bool island_powered( int Node ) const;

private:
    // rozkład wyspy: numeracja, obwiednia, wartości L i D
    struct island_cache {
        struct link { std::size_t row, column; double conductance; };
        std::vector<int> node;                  // pozycja w rozkładzie -> węzeł obwodu
        std::vector<std::size_t> position;      // numer lokalny w wyspie -> pozycja w rozkładzie
        std::vector<std::size_t> first;         // pierwsza kolumna obwiedni wiersza
        std::vector<std::size_t> start;         // początek wiersza w value
        std::vector<link> links;                // gałęzie (wiersz > kolumna)
        std::vector<double> value;
        std::vector<char> pattern;              // stan zasilaczy, z którym liczony był rozkład
        bool factored { false };
    };

    void build_islands();
    void factor_island( std::size_t Island );
    void substitute( std::size_t Island, std::vector<double> &Values ) const;
    void solve_island( std::size_t Island );

    std::vector<branch> m_branches;
    std::vector<source> m_sources;
    std::vector<double> m_voltage;
    // wyspy: węzły i zasilacze każdej, gałęzie przyłączone do węzła
    bool m_islandsvalid { false };
    std::vector<int> m_island;                      // węzeł -> wyspa
    std::vector<std::vector<int>> m_islandnodes;
    std::vector<std::vector<int>> m_islandsources;
    std::vector<std::vector<int>> m_nodebranches;
    std::vector<std::size_t> m_local;               // węzeł -> numer lokalny w wyspie
    std::vector<island_cache> m_cache;
    std::vector<double> m_regenscale;
};

// sieć do obwodu: przęsła i zasilacze w postaci niezależnej od symulatora (world/catenary_power_scene.cpp tłumaczy na nią scenerię)
struct span_input {
    std::string power;                  // nazwa zasilacza przęsła
    double ends[ 2 ][ 3 ] {};           // końce przęsła (przewód jezdny), m
    int next[ 2 ] { -1, -1 };           // przęsło połączone z końcem (numer na liście), -1 brak
    int nextend[ 2 ] { 0, 0 };          // którym swoim końcem
    std::vector<int> parallel;          // przęsła bieżni wspólnej (przęsło naprężenia)
    double resistivity { 0.075e-3 };    // Ω/m, przewody jezdne i liny nośne razem (bez szyn)
};
struct source_input {
    source parameters;                  // nazwa, napięcie, opór, wyłącznik (node nieistotny)
    double location[ 3 ] {};
    bool section { false };             // kabina sekcyjna (wpis "section"): szyna zbiorcza bez źródła
};
struct mapping {
    circuit network;
    std::vector<int> branch;            // przęsło -> gałąź obwodu; -1 = przęsło w części bez zasilacza (liczone po staremu)
    std::vector<std::string> sources;   // źródło obwodu -> nazwa zasilacza
};
// buduje obwód. Zasady (tests/catenary/ZASILANIE.md, etap B):
// - końce połączonych przęseł to jeden węzeł, gdy przęsła mają ten sam zasilacz albo jedno z nich ma nazwę kabiny sekcyjnej
//   (jak w starej sieci); różne zasilacze - przerwa (izolator sekcyjny, izolowane przęsło naprężenia);
// - przęsła bieżni wspólnej z tym samym zasilaczem - połączone końcami leżącymi bliżej niż 3 m (łączniki 0640 przy słupach krzyżowych);
// - zasilacz przyłączony kablem do najbliższego końca przęsła ze swoją nazwą, w każdej części sieci, w której takie są;
//   kabina sekcyjna tak samo, ale jej szyna nie ma źródła - łączy części (sieci obu torów);
// - opór przęsła: przewody + tor (szyny) na długości przęsła.
// Previous: stan wyłączników ze starego obwodu (po nazwie), żeby przebudowa w czasie jazdy go nie kasowała
void build( std::vector<span_input> const &Spans, std::vector<source_input> const &Sources, mapping &Result,
    std::vector<source> const &Previous = {} );

} // namespace catenary_power
