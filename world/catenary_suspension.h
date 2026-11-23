// Sieć trakcyjna: wpisy podwieszeń (wysięgniki, zwisy, podwieszenia na bramkach) z pliku dołączanego dyrektywą "catenary"
// oraz rejestr konstrukcji wsporczych. Kod niezależny od reszty symulatora (C++17, tylko biblioteka standardowa).
#pragma once

#include <algorithm>
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
    std::string skin;                       // wymienna tekstura całego wysięgnika (wszystkie jego części); pusta = domyślna z pliku
    std::string pole_skin;                  // wymienna tekstura słupa; pusta = domyślna typu słupa (traction_supports.txt)
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
    // wysięgnik rozstawiony (konsola 4890; para wpisów z tą samą nazwą "umbrella" po tej samej stronie słupa albo jeden): punkt na osi
    // płaszczyzny wysięgnika przy słupie (oś słupa przesunięta wzdłuż toru do punktu mocowania na konsoli), "spread x y z"
    // słup stawia inny wpis w tym samym miejscu (trzeci wysięgnik przy rozstawionych, w osi słupa): ten tylko z niego korzysta
    bool   shared_pole { false };
    bool   above_pole { false };         // mocowania mogą wyjść ponad najwyższe położenie na słupie (uchwyt liny wyżej; słupy za niskie - tymczasowo)
    // górne mocowanie (odciągu) przesunięte w pionie względem karty, ±2,5 m (Jan 2026-10-26, ±0,5 → ±2,5 m 2026-11-18, wysięgniki rurowe): solver dobiera do niego
    // krótszą albo dłuższą rurę / pręt odciągu z tej samej rodziny części; wpis "stay_shift <m>"
    double stay_shift { 0.0 };
    // dolne mocowanie (ukośnika) przesunięte w pionie względem karty, ±2,5 m (Jan 2026-11-04, jak stay_shift): solver dobiera krótszą
    // albo dłuższą rurę ukośnika z tej samej rodziny części; wpis "strut_shift <m>"
    double strut_shift { 0.0 };
    // ręczny obrót wysięgnika na zawiasie wokół pionu, stopnie (Jan 2026-11-04), dodawany do obrotu od temperatury; dodatni - uchwyt
    // liny w stronę +X wysięgnika. Wpis "turn_by_hand <stopnie>" (do ±30°); bez zawiasu (wpis bez słupa) nic nie robi
    double turn_by_hand { 0.0 };
    // przechył słupa w poprzek toru (Jan 2026-10-19): słup razem z mocowaniami pochylony o tyle stopni w stronę toru (ujemny - od toru),
    // obrót wokół jego podstawy; wysięgnik składa się do przechylonych mocowań. Wpis "lean <stopnie>" (do ±10°, w edytorze ±1,5°, w trybie eksperta ±10°); parasole i rozstawione bez przechyłu
    double lean { 0.0 };
    // korekty wysięgnika z terenu (użytkownik 2026-11-18, tryb eksperta; karta zostaje, solver bierze je pod uwagę):
    // stay_gap - odstęp uchwytu odciągu 4070 od uchwytu głównego wzdłuż ukośnika wymuszony (zamiast doboru 0,10-0,30 m z kart),
    // metry, 0 = dobór solvera; wpis "stay_gap <m>"
    double stay_gap { 0.0 };
    // arms_spread - zestaw ramion złożony z członów (127x: dwa 6203-11 + 4110 na +-0,13 m wzdłuż toru): połowa rozstawu członów
    // wzdłuż toru, metry, 0 = z danych zestawu; wpis "arms_spread <m>"
    double arms_spread { 0.0 };
    // rope_shift - lina nośna (a za nią jej uchwyt na rurze) przesunięta poziomo wzdłuż wysięgnika, dodatnio w stronę konstrukcji,
    // metry; wpis "rope_shift <m>"
    double rope_shift { 0.0 };
    // cord_shift - obejma linki na wysięgu pomocniczym (6011, 6111) przesunięta wzdłuż niego, dodatnio w stronę konstrukcji, metry;
    // wpis "cord_shift <m>"
    double cord_shift { 0.0 };
    // cord_band - linka od obejmy na wysięgu pomocniczym idzie nie do uchwytu liny nośnej, tylko do opaski na ukośniku (zwis na
    // Brochowie, użytkownik 2026-11-18): część opaski i jej odległość od uchwytu głównego wzdłuż ukośnika (+ w stronę konstrukcji,
    // - za uchwyt), metry; pusta = jak w karcie. Wpis "cord_band <część> <m>"
    // stay_hole - otwór końcówki odciągu (4400), w który wchodzi sworzeń, liczony od ucha od 1; 0 = dobór solvera. Wpis "stay_hole <n>"
    int stay_hole { 0 };
    std::string cord_band;
    double cord_band_at { 0.0 };
    bool   has_spread { false };
    double spread[ 3 ] { 0.0, 0.0, 0.0 };
    // odciąg sieci wielotorowy (karty 0315/0316, 0325/0326 - pośrednie): ten odciąg nie ma słupa, jego lina idzie dalej od odciągu
    // <nazwa> z sąsiedniego toru (pojedynczy: od jego liny nośnej, podwójny: dolna od uchwytu ramion, górna od liny nośnej).
    // Wpis "pull_from <nazwa>"; "attach" liczy wtedy exe
    std::string pull_from;
    // odgromnik rożkowy na słupie (karty 0681/0683/0684, model 7310 wg słupu - wpis "arrester" słupa w traction_supports.txt):
    // przy szczycie słupa, od strony toru, linka 9812-6 łukiem do liny nośnej. Wpis "arrester <m>": o tyle niżej od położenia
    // domyślnego (0 = domyślne). Bez wpływu na prąd (sam model)
    bool   arrester { false };
    double arrester_drop { 0.0 };
    // odgromnik na drugiej stronie słupa (od toru), razem z linką i przyłączem obrócony wokół osi słupa jak wysięgnik uszynienia "away"
    // (użytkownik 2026-11-22). Wpis "arrester_away" za "arrester"
    bool   arrester_away { false };
    // rozbiórka sieci na tym słupie (użytkownik 2026-11-23): -1 - jak łańcuch, 0 - kompletny, 1 - bez drutów, 2 - rozkradziony,
    // 3 - sam słup. Wpis "dismantled intact|wires|looted|poles"; bez wpisu - jak łańcuch
    int    dismantled { -1 };
    // ustawienia rozbiórki na tym słupie (użytkownik 2026-11-24): odciąg pomocniczy -1 - jak wypadnie (rozkradziona - losowo), 0 - zwisa,
    // 1 - zdjęty ("dismantled_steady hang|off"); izolatory zwisające z mocowań, gdy wysięgnik zdjęty: 0 - żadne, 1 - ukośnika, 2 - odciągu,
    // 3 - oba ("dismantled_insulators strut|stay|both"); obrót luźnego wysięgnika na zawiasach w stopniach, ±75 ("dismantled_turn <st>");
    // bez wpisu - losowy
    int    dismantled_steady { -1 };
    int    dismantled_insulators { 0 };
    bool   has_dismantled_turn { false };
    double dismantled_turn { 0.0 };
};

// rozbiórka: nazwy stanów w pliku (0..3) i odczyt; -1 - nie ma takiej nazwy
char const *dismantled_name( int State );
int dismantled_of( std::string const &Name );

// łańcuch sieci: kolejne przęsła między podwieszeniami z listy (od kotwy do kotwy), wpis
// "node <max> <min> <nazwa> chain ... suspensions <nazwy podwieszeń> endchain". Exe robi z niego zwykłe przęsła (TTraction):
// przęsło k idzie od podwieszenia k do k+1, przewód jezdny przez punkty "at", lina nośna przez punkty "rope"
struct chain {
    std::string name;                       // nazwa łańcucha; przęsła dostają nazwy <nazwa>_<numer od 1>
    double      range_max { -1.0 };
    double      range_min { 0.0 };
    std::string power;                      // zasilacz (jak w node traction)
    double      voltage { 3000.0 };         // napięcie nominalne, V
    double      current { 4500.0 };         // prąd maksymalny, A
    double      resistivity { 0.01 };       // jak w node traction (0,01 = wartość domyślna scenerii)
    std::string material { "cu" };          // cu, al
    double      thickness { 3.0 };          // grubość rysowanego drutu (rysowanie bez typu sieci)
    int         state { 0 };                // stan drutów: 0 nowe, 2 przejściowe, 1 zaśniedziałe
    std::string type;                       // symbol typu sieci, np. YwsC120-2C
    double      hangers[ 4 ] { 0.4, 4.0, 3.0, 0.04 }; // jak w node traction: najniższy wieszak, odstęp, liczba drutów, odstęp drutów
    std::string parallel;                   // przęsło bieżni wspólnej (jak "parallel" w node traction); puste = brak
    std::string anchor_start, anchor_end;   // kotwienia (wpisy "anchor") przed pierwszym i za ostatnim podwieszeniem; puste = brak
    // przewody jezdne zakotwione osobno (inne kotwienie, zwykle na innym słupie; 0404 albo przewody rozdzielonego): wtedy
    // anchor_start/anchor_end trzyma samą linę nośną (0405, lina rozdzielonego). Puste = całość na anchor_start/anchor_end
    std::string anchor_start_contact, anchor_end_contact;
    // kotwienie środkowe (karty 63-0406 / 0408): podwieszenie, przy którym lina kotwienia środkowego jest zaciśnięta na linie nośnej,
    // i karta (0406 z izolacją, 0408 bez); lina idzie do słupów sąsiednich podwieszeń. Puste = brak
    std::string middle;
    std::string middle_device { "0406" };
    double      middle_heights[ 2 ] { 0.0, 0.0 };  // uchwyty liny kotwienia na słupach przed i za (nad główką); 0 = lina nośna sąsiada
    // lina nośna z innego podwieszenia niż przewody jezdne (wysięgniki rozstawione na konsoli 4890, wysięgnik samej liny przy
    // odciągu przewodów): w punkcie łańcucha <podwieszenie z listy> przewody idą przez jego "at", lina przez "rope" podwieszenia
    // <trzymające linę>. Wpis "messenger_from <z listy> <trzymające linę>", przed "suspensions"
    std::vector<std::pair<std::string, std::string>> messenger_from;
    // izolatory sekcyjne: w przęśle za podwieszeniem "after", rodzaj "resin" (żywiczny; na jeden albo dwa przewody wg typu sieci) albo
    // "ceramic"; przęsła za nim zasila "power" (puste - zasilacz łańcucha). Domyślnie w połowie przęsła, "at" - odległość od podwieszenia
    // "after" wzdłuż przęsła w metrach (Jan 2026-10-30: do przestawiania). Wpis "insulator <podwieszenie> <rodzaj> <zasilacz|-> [at <m>]"
    // przed "suspensions"
    struct section_insulator {
        std::string after;
        std::string kind { "resin" };
        std::string power;
        double at { -1.0 };     // m od podwieszenia "after"; ujemne - w połowie przęsła
    };
    std::vector<section_insulator> insulators;
    // druga sieć wysięgnika (Jan 2026-10-30, przęsła naprężenia: 102x - druga lina i drugi przewód, 117x/118x, 0115/0116, 0135/0136,
    // 0155/0156 - drugi przewód, 0120, 120x - dwa przewody): w punkcie łańcucha <podwieszenie z listy> przewód idzie przez drugi
    // uchwyt przewodu wysięgnika (wire_b albo second_wire), lina przez drugi uchwyt liny (messenger_clamp_b); bez niego - z wysięgnika
    // samej liny (messenger_from) albo swobodnie (prosto między sąsiednimi punktami liny). Wysięgnik składa łańcuch, który ma to
    // podwieszenie jako zwykłe. Wpis "second_net <z listy>", przed "suspensions"
    std::vector<std::string> second_net;
    // izolowane przęsło naprężenia (karty 0060/0062/0078: „w przęśle izolowanym - izolatory”): izolatory w przewodach i linie
    // schodzących do kotwienia na początku / końcu łańcucha, przy słupie krzyżowym. Wpis "isolated start|end [at <m>]", przed
    // "suspensions"; "at" - odległość izolatorów od słupa krzyżowego wzdłuż przęsła (użytkownik 2026-11-22: przesuwane jak izolator
    // sekcyjny), bez niego domyślna
    bool isolated[ 2 ] { false, false };
    double isolated_at[ 2 ] { -1.0, -1.0 };   // m od słupa krzyżowego; ujemne - domyślnie
    // bieżnia wspólna ustawiona ręcznie dla jednego przęsła (Jan 2026-11-04, rozjazdy: pantograf szuka wtedy co chwilę najniższego
    // przewodu nad sobą zamiast trzymać się przewodu uciekającego do kotwienia): przęsło za podwieszeniem <nazwa> albo "start"
    // (przęsło od kotwienia początkowego), on = zawsze, off = nigdy; bez wpisu - wykrywane samo. Wpis "shared <nazwa|start> on|off",
    // przed "suspensions"
    std::vector<std::pair<std::string, bool>> shared;
    // połączenia elektryczne bezpośrednie z innym łańcuchem tego pliku (karty 33-0642 równoległe i 33-0648 rozjazdowe; użytkownik
    // 2026-11-22: „przy rozjazdach też są takie łączki”): w przęśle za podwieszeniem "after", "at" m od niego wzdłuż przęsła (ujemne -
    // 1,5 m), linka z przewodu tego łańcucha przez jego linę łukiem na linę drugiego i "S" do jego przewodu. Wpis
    // "connection <podwieszenie> 0642|0648 <łańcuch> [at <m>]", przed "suspensions"; 0650 / 0652 - z przewodem rozdzielczym 3 kV
    // (wpis uszynienia z przewodem "feeder" tego pliku, karta 0639): <łańcuch> to nazwa tego wpisu
    struct connection {
        std::string after;
        std::string card { "0648" };
        std::string other;
        double at { -1.0 };
    };
    std::vector<connection> connections;
    // rozbiórka (użytkownik 2026-11-23, linie z rozebraną siecią): 0 - sieć jest, 1 - bez drutów (wysięgniki zostały), 2 - rozkradziona
    // (bez drutów, losowo bez odciągów pomocniczych albo całych wysięgników), 3 - same słupy. Bez drutów łańcuch nie ma przęseł: ani
    // drutów, ani styku z pantografem, ani prądu; kotwienia i odciągi sieci też nie. Wpis "dismantled wires|looted|poles", przed
    // "suspensions"; słup może mieć swój wyjątek
    int dismantled { 0 };
    std::vector<std::string> suspensions;   // nazwy podwieszeń z tego samego pliku, po kolei
};

// podwieszenie, które trzyma linę nośną łańcucha w punkcie Listed (samo Listed, gdy nie ma wpisu messenger_from)
std::string const &messenger_holder( chain const &Chain, std::string const &Listed );

// kotwienie ciężarowe (urządzenie naprężające z pliku traction_tensioners.txt) na słupie kotwowym, wpis
// "node <max> <min> <nazwa> anchor device <karta> network <symbol typu> pole <słup> x y z [on <podwieszenie>] toward x y z distance <m>
//  [color <kolor>] [top_color <kolor>] [elongation <m>] [missing <n>] [ground <m>] [height <m>] [rope_height <m>] [insulators <rodzaj>] [guide new|old] [skin <tekstura>]
//  endanchor".
// pole: oś słupa na wysokości główki szyny; toward: punkt, z którego przychodzą druty (urządzenie leży w tym kierunku od
// słupa); distance: odległość od kotwienia środkowego albo stałego. Łańcuch przypina się do niego "anchor_start/anchor_end",
// a ostatnie przęsło (bez wieszaków) kończy się na uchwytach krańcowych urządzenia
struct anchor {
    std::string name;
    double      range_max { 1000.0 };
    double      range_min { 0.0 };
    std::string device { "0461" };
    std::string network;                    // symbol typu sieci (traction_<symbol>.txt): numer typu i liczba przewodów jezdnych
    std::string pole_type;
    double      pole[ 3 ] { 0.0, 0.0, 0.0 };
    std::string on;                         // kotwienie na istniejącym słupie: nazwa podwieszenia (z tego pliku), którego słup to
                                            // jest; słupa nie stawia, typ i punkt bierze z tamtego wpisu (pole tylko zapasowo)
    std::string gantry;                     // kotwienie na nodze bramki (wpis "gantry <bramka> <1|2>"): bramka z tego pliku i jej słup
    int         mast { 0 };                 // (0, 1); słupa nie stawia, typ i punkt z bramki (pole tylko zapasowo)
    double      toward[ 3 ] { 0.0, 0.0, 0.0 };
    double      distance { 700.0 };
    std::string color, top_color;
    double      elongation { 0.0 };
    int         missing { 0 };
    double      ground { -0.50 };
    double      height { 0.0 };              // oś rolek nad główką szyny; 0 = z tablicy montażowej (6,70). Kotwienie stałe: przewody
    double      rope_height { 0.0 };         // kotwienie stałe: uchwyt liny nośnej nad główką szyny; 0 = jak przy ostatnim słupie
    // izolatory w łańcuchach do drutów: puste = auto (rolkowe ESN-2025 na obu przy drutach zaśniedziałych, inaczej żywiczne 7150),
    // "new" żywiczne, "old" rolkowe na obu, "messenger" rolkowy tylko na linie nośnej, "contact" rolkowe tylko na przewodach
    std::string insulators;
    // prowadnica ciężarów: false = rurowa 5070 z pierścieniem na stosie, true ("guide old") = stara 0454 (pierścienie 5060, pręty 5050)
    bool        old_guide { false };
    std::string skin;
};

// bramka: dwa słupy i dźwigar między nimi, wpis
// "node <max> <min> <nazwa> gantry beam <dźwigar> level low|high side 1|-1 mast <słup> x y z mast <słup> x y z endgantry".
// Punkty to osie słupów na wysokości główki szyny; dźwigar biegnie od pierwszego do drugiego, na prześwicie dolnego albo górnego
// mocowania pierwszego słupa, przesunięty wzdłuż toru o offset słupa na stronę "side". Zwisy z wysięgnikami to zwykłe podwieszenia
// ze "pole <zwis> x y z" (punkt w spodzie dźwigara).
// Pojedynczy element dźwigara (wspornik na jednym słupie przelotowym, np. 3017-3025): "mast <słup> x y z tip x y z" -
// masts[1] puste, points[1] to punkt w kierunku końca wspornika
struct gantry {
    std::string name;
    double      range_max { 1000.0 };
    double      range_min { 0.0 };
    std::string beam;
    bool        high { false };             // górne mocowanie dźwigara
    int         side { 1 };
    std::string masts[ 2 ];
    double      points[ 2 ][ 3 ] { { 0.0, 0.0, 0.0 }, { 0.0, 0.0, 0.0 } };
    std::string skin;
};

// uszynienie grupowe (karta 0805, Jan 2026-10-25): lina uszynienia po konstrukcjach wsporczych, niezależna od sieci. Wpis
// "node <max> <min> <nazwa> earthing poles <podwieszenia> endearthing": na słupach tych podwieszeń (po kolei) uchwyty 6120 na górze
// słupa (wykonanie z danych słupa - "earthing" w traction_supports.txt), lina przez ich punkty "wire"
// ustawienia uchwytu na jednym słupie (wpis "holder <podwieszenie> [arm] [away] [raise <m>]" za listą słupów, tylko dla słupów,
// które coś zmieniają; Jan 2026-11-08): arm - wysięgnik przewodu uszyniającego 6130 (karta 0806) zamiast uchwytu 6120, away - uchwyt
// albo wysięgnik obrócony od toru (na tylnej ścianie słupa), raise - przesunięcie w pionie od położenia domyślnego (ujemne w dół)
// shift - zwis bramki: przesunięcie uchwytu 6840 wzdłuż dźwigara od osi zwisu (Jan 2026-11-09); nojumper - bez przyłącza 13-0885
// (linki od liny do konstrukcji; domyślnie na każdym słupie)
// longer - wysięgnik dłuższy (6131 zamiast 6130; wpis "long" wysięgnika w traction_supports.txt; Jan 2026-11-16); na zwisie bramki
// arm - wysięgnik 6132 na dźwigarze zamiast uchwytu 6840
// przewód wzmacniający (Jan 2026-11-16): feeder_arm - wysięgnik inny niż domyślny linii (np. 4920 zamiast 4910; wpis "feeder_arm
// <nazwa>"), fresh - uchwyt na nowym izolatorze (0301-051 zamiast -05 itd.; "new"); away - tu: do toru (domyślnie przewód od toru)
struct earthing_holder {
    std::string pole;
    std::string feeder_arm;
    bool        fresh { false };
    bool        arm { false };
    bool        longer { false };
    bool        away { false };
    double      raise { 0.0 };
    double      shift { 0.0 };
    bool        nojumper { false };
    bool plain() const { return feeder_arm.empty() && false == fresh && false == arm && false == longer && false == away && raise == 0.0 && shift == 0.0 && false == nojumper; }
};

// sekcja uszynienia grupowego (Jan 2026-11-09): "from <podwieszenie> to <podwieszenie>" - słupy pomiędzy wyznacza exe po przęsłach
// łańcuchów (wstawiony / usunięty słup łańcucha sam wchodzi do sekcji); "start" / "end" - rodzaj końca: anchor (kotwienie z izolacją
// 13-0491), plain (bez izolacji 13-0492), split (punkt izolujący 13-0493 w przęśle do następnej sekcji); brak - lina kończy się
// na uchwycie. Stary zapis "poles <lista>" (słupy wprost) dalej czytany
struct earthing {
    std::string name;
    double      range_max { 1000.0 };
    double      range_min { 0.0 };
    std::string from, to;                   // sekcja: pierwszy i ostatni słup (puste - lista "poles")
    std::string start, end;                 // rodzaje końców: anchor, plain, split albo puste
    std::string line;                       // przewód (wpis earthing_line, "line <nazwa>"); pusty - pierwszy (AFL120 uszynienia)
    bool section() const { return false == from.empty() && false == to.empty(); }
    std::vector<std::string> poles;
    std::vector<earthing_holder> holders;   // tylko słupy z ustawieniami innymi niż domyślne
    std::string skin;
    // ustawienia uchwytu na słupie (podwieszeniu) o tej nazwie; domyślne, gdy nie ma wpisu
    earthing_holder holder( std::string const &Pole ) const {
        for( auto const &item : holders ) { if( item.pole == Pole ) { return item; } }
        earthing_holder plain; plain.pole = Pole; return plain;
    }
    // zapisuje ustawienia słupa (domyślne usuwają wpis)
    void set_holder( earthing_holder const &Holder ) {
        holders.erase( std::remove_if( holders.begin(), holders.end(), [ & ]( earthing_holder const &H ) { return H.pole == Holder.pole; } ), holders.end() );
        if( false == Holder.plain() ) { holders.push_back( Holder ); }
    }
};

// zawartość pliku podwieszeń
// odłącznik sieci (plan zasilania, etap E; na razie bez modelu - sam stan w obwodzie prądu): łączy dwa punkty łańcuchów
// (punkt łańcucha = podwieszenie z jego listy), np. obie strony izolatora sekcyjnego (podwieszenie przed nim i następne) albo
// dwie sieci granicy zasilania (przęsło naprężenia). Stan: closed - połączone, open - przerwa, earthed - przerwa, a strona "to"
// zwarta z szynami (odłącznik ze stykiem uszyniającym). Wpis
// "node <max> <min> <nazwa> disconnector number <nr> state closed|open|earthed from <łańcuch> <podwieszenie> to <łańcuch> <podwieszenie> enddisconnector"
struct disconnector {
    std::string name;
    double range_max { -1.0 };
    double range_min { 0.0 };
    std::string number;                     // numer z planu sekcjonowania (np. 19, 101, 10 - zasilaczowy); pusty - bez numeru
    int state { 0 };                        // 0 - zamknięty, 1 - otwarty, 2 - uszyniony
    std::string from_chain, from_point;
    std::string to_chain, to_point;         // "to cable -": kabel zasilacza (karty 0655/0656, stara 0617) - linka w dół słupa
    // zespół odłącznika na słupie (karty 83-0661...0677, użytkownik 2026-11-22: na razie zaślepki zamiast modeli): karta, słup (podwieszenie,
    // na którego słupie stoi; puste - from_point), wysokość zacisków nad główką szyny (ujemna - nad szczytem słupa). Bez karty - jak dotąd
    // sam znak w edytorze. Połączenia linek wg kart 0641 (między sekcjami), 0643/0644 (po bramce), 0655 (kabel)
    std::string card;
    std::string on;
    double height { -1.0 };
};
char const *disconnector_state_name( int State );

struct suspension_file {
    std::string skin;                       // domyślna wymienna tekstura dla wpisów bez własnej
    std::vector<suspension> entries;
    std::vector<chain> chains;
    std::vector<gantry> gantries;
    std::vector<anchor> anchors;
    std::vector<earthing> earthings;
    std::vector<disconnector> disconnectors;
};

// rozbiera plik podwieszeń podany jako kolejne słowa (już bez komentarzy). Source: nazwa pliku do komunikatów.
// Błędne wpisy są pomijane, a opis każdego błędu trafia do Errors
// References: łańcuchy, których podwieszeń nie ma wśród rozebranych wpisów, są odrzucane (cały plik naraz). Wpisy scenerii rozbierane
// po jednym (node ... chain w pliku warstwy) - false; sprawdza je wtedy check_chain_references dla wszystkich wpisów pliku
void parse_suspensions( std::vector<std::string> const &Tokens, std::string const &Source, suspension_file &Output, std::vector<std::string> &Errors, bool References = true );
// odrzuca łańcuchy, które wskazują podwieszenia spoza pliku (każdy z błędem w Errors)
void check_chain_references( suspension_file &File, std::string const &Source, std::vector<std::string> &Errors );

// przesuwa wszystkie punkty pliku (podwieszenia, kotwienia, bramki) o Offset - blok "origin" scenerii, w którym stoi dyrektywa
// "catenary" albo wpis sieci; przy zapisie pliku z edytora przesunięcie odejmowane (Sign -1)
void shift_suspensions( suspension_file &File, double const *Offset, double Sign = 1.0 );

// wpis podwieszenia w postaci tekstu do pliku (jedna linia, bez znaku końca linii), w tej samej składni, którą czyta parse_suspensions.
// punkty z dokładnością do milimetra
std::string write_suspension( suspension const &Entry );
// wpis łańcucha w postaci tekstu do pliku (jedna linia)
std::string write_chain( chain const &Entry );
// wpis bramki w postaci tekstu do pliku (jedna linia)
std::string write_gantry( gantry const &Entry );
// wpis kotwienia w postaci tekstu do pliku (jedna linia)
std::string write_anchor( anchor const &Entry );
// wpis uszynienia w postaci tekstu do pliku (jedna linia)
std::string write_earthing( earthing const &Entry );
// wpis odłącznika w postaci tekstu do pliku (jedna linia)
std::string write_disconnector( disconnector const &Entry );

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
