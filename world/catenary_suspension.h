// Sieć trakcyjna: wpisy podwieszeń (wysięgniki, zwisy, podwieszenia na bramkach) z pliku dołączanego dyrektywą "catenary"
// oraz rejestr konstrukcji wsporczych. Kod niezależny od reszty symulatora (C++17, tylko biblioteka standardowa).
#pragma once
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace catenary {

// jeden wpis "node <max> <min> <nazwa> suspension ... endsuspension"
struct suspension {
    std::string name;                       // nazwa wpisu
    double      range_max { 0.0 };          // zasięg widoczności jak w innych wpisach node
    double      range_min { 0.0 };
    std::string support;                    // nazwa wpisu node model z konstrukcją wsporczą
    int         card { 0 };                 // numer karty katalogowej schematu, np. 1110; 0 = dobór automatyczny
    int         execution { 0 };            // wykonanie z karty
    std::vector<std::string> messenger;     // przęsła, których linę nośną trzyma (0–2)
    std::vector<std::string> contact;       // przęsła, których przewód jezdny trzyma (dowolna liczba)
    std::string insulators;                 // zestaw podmian izolatorów; pusty = domyślny
    std::vector<std::pair<std::string, std::string>> overrides; // nadpisania pojedynczych ról: rola, część
    std::string skin;                       // wymienna tekstura całego podwieszenia; pusta = domyślna z pliku
    // punkty podane wprost, we współrzędnych scenerii: wpis bez konstrukcji wsporczej i bez przęseł.
    // "at" to oś przewodów jezdnych przy podwieszeniu, "attach" to pion przez zaczepy izolatorów (wysokość bez znaczenia),
    // "rope" to oś liny nośnej; bez niej lina jest nad przewodami na wysokości konstrukcyjnej schematu
    bool   has_at { false };
    bool   has_attach { false };
    bool   has_rope { false };
    double at[ 3 ] { 0.0, 0.0, 0.0 };
    double attach[ 3 ] { 0.0, 0.0, 0.0 };
    double rope[ 3 ] { 0.0, 0.0, 0.0 };
    // słup z katalogu wysięgników zamiast "attach": typ i punkt w osi słupa na wysokości główki szyny.
    // zaczepy izolatorów, mocowania i sam słup liczy wtedy exe
    bool   has_pole { false };
    std::string pole_type;
    double pole[ 3 ] { 0.0, 0.0, 0.0 };
    // parasol: dwa wpisy ze słupem i tą samą nazwą parasola to dwa wysięgniki na jednym słupie, po przeciwnych stronach.
    // słup stawia pierwszy z nich; mocowania regulowane (z połówką z nakrętkami) idą na wspólną wysokość
    std::string umbrella;
};

// zawartość pliku podwieszeń
struct suspension_file {
    std::string skin;                       // domyślna wymienna tekstura dla wpisów bez własnej
    std::vector<suspension> entries;
};

// rozbiera plik podwieszeń podany jako kolejne słowa (już bez komentarzy). Source: nazwa pliku do komunikatów.
// Błędne wpisy są pomijane, a opis każdego błędu trafia do Errors
void parse_suspensions( std::vector<std::string> const &Tokens, std::string const &Source, suspension_file &Output, std::vector<std::string> &Errors );

// wpis podwieszenia w postaci tekstu do pliku (jedna linia, bez znaku końca linii), w tej samej składni, którą czyta parse_suspensions.
// punkty z dokładnością do milimetra
std::string write_suspension( suspension const &Entry );

// dane konstrukcji wsporczej z rejestru
struct support_data {
    std::string type;                   // mast, gantry_mast, beam, drop
    double      height { 0.0 };         // m, wysokość nad punktem zerowym modelu (główka szyny)
    double      width_base { 0.0 };     // m, szerokość w kierunku toru u podstawy
    double      width_top { 0.0 };      // m, to samo na wysokości height
    // połowa szerokości na podanej wysokości: odległość czoła od osi konstrukcji
    double half_width( double Height ) const;
};

// rejestr konstrukcji wsporczych: dane czytane z pliku, szukane po ścieżce modelu
class support_registry {
public:
    // wczytuje plik; błędy trafiają do Errors. zwraca false, gdy pliku nie da się otworzyć
    bool load( std::string const &Filename, std::vector<std::string> &Errors );
    // dane konstrukcji o podanej ścieżce modelu (wielkość liter i rodzaj ukośników bez znaczenia); nullptr gdy brak
    support_data const *find( std::string const &Model ) const;
    std::size_t size() const { return m_supports.size(); }

private:
    std::map<std::string, support_data> m_supports;
};

} // namespace catenary
