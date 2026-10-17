// Test solvera wysięgnika: porównanie ze złożeniami 11xx z Blendera (cantilever_ref.txt) i kontrola mechaniki.
// Uruchamiać z katalogu zawierającego world/ i tests/catenary/:  tests/catenary/tc tests/catenary/data
#include "world/catenary_cantilever.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>

struct stat_t {
    double sum { 0.0 }, max { 0.0 }; int count { 0 }; std::string worst;
    void add( double const Value, std::string const &Name ) { sum += Value * Value; ++count; if( Value > max ) { max = Value; worst = Name; } }
    double rms() const { return ( count > 0 ? std::sqrt( sum / count ) : 0.0 ); }
};

double distance( catenary::cantilever_placement const *Placement, double const Y, double const Z ) {
    return ( Placement == nullptr ? 99.0 : std::hypot( Placement->position[ 1 ] - Y, Placement->position[ 2 ] - Z ) );
}

std::vector<std::string> words( std::string const &Text ) { std::istringstream s { Text }; std::vector<std::string> list; std::string w; while( s >> w ) { list.push_back( w ); } return list; }

int main( int argc, char *argv[] ) {
    std::string const data { argc > 1 ? argv[ 1 ] : "tests/catenary/data" };
    bool failed { false };

    catenary::cantilever_catalog catalog;
    std::vector<std::string> errors;
    auto const loaded { catalog.load( data + "/traction_cantilevers.txt", errors ) };
    {
        int swing { 0 }; for( auto const &entry : catalog.schemes() ) { swing += ( entry.first.first / 100 == 11 ); }
        auto const good { loaded && errors.empty() && swing == 174 && /* 111x-114x: 163, 117x/118x: 11 */  catalog.registration_count() >= 20 && catalog.part_count() > 50 && catalog.models() == "siec_trakcyjna/components/" };
        std::printf( "katalog wysiegnikow: czesci %zu, zestawow ramion %zu, schematow %zu, bledow %zu  %s\n",
            catalog.part_count(), catalog.registration_count(), catalog.scheme_count(), errors.size(), good ? "OK" : "BLAD" );
        for( auto const &e : errors ) { std::printf( "  %s\n", e.c_str() ); }
        failed = failed || !good;
        if( false == loaded ) { return 1; }
    }
    {
        // zepsuty plik: każdy błąd ma dać komunikat, a poprawne wpisy po nim mają się wczytać
        catenary::cantilever_catalog broken; std::vector<std::string> messages;
        broken.parse( words(
            "smiec "
            "part a kind strut length x endpart "
            "part b kind strut length 1.0 "
            "part c kind strut length 2.0 pin 0.03 endpart "
            "part c kind strut length 3.0 endpart "
            "registration r tube 0.2 clamp 0.1 item c 0 0 0 1 0 0 0 1 endregistration "
            "registration s tube 0.2 clamp 0.1 endregistration "
            "scheme 1110 1 reach 1.45 1.75 registration brak role strut nieznana endscheme "
            "scheme 1111 1 reach 1.75 2.05 registration s mount gdzies 1 endscheme "
            "scheme 1112 1 reach 2.05 2.35 registration s role strut c endscheme "
            "rules stay_gap 0.1 endrules" ), "zly.txt", messages );
        auto const good { broken.part_count() == 1 && broken.part( "c" ) != nullptr && broken.part( "c" )->length == 2.0
            && broken.registration_count() == 1 && broken.scheme_count() == 2 && broken.scheme( 1112, 1 ) != nullptr && messages.size() >= 9
            && broken.rules.stay_gap_min == 0.10 && broken.scheme_for_reach( 1110, 1, 2.2 ) == broken.scheme( 1112, 1 ) };
        std::printf( "zepsuty plik: czesci %zu (ma byc 1), zestawow %zu (1), schematow %zu (2), komunikatow %zu  %s\n",
            broken.part_count(), broken.registration_count(), broken.scheme_count(), messages.size(), good ? "OK" : "BLAD" );
        if( false == good ) { for( auto const &m : messages ) { std::printf( "  %s\n", m.c_str() ); } }
        failed = failed || !good;
    }

    // --- porównanie ze złożeniami z Blendera
    std::ifstream reference { "tests/catenary/cantilever_ref.txt" };
    if( false == reference.is_open() ) { std::printf( "brak tests/catenary/cantilever_ref.txt\n" ); return 1; }
    // osobno karty z odsuwem do słupa (111x, 113x) i od słupa (112x, 114x): w drugich linka idzie mocno skosem,
    // a w złożeniach z Blendera uchwyt liny wisi pionowo, więc różnice są tam z założenia większe
    std::map<std::string, std::map<std::string, stat_t>> stats;
    std::map<std::string, int> warnings;
    int count { 0 }, solved { 0 }, inexact { 0 };
    std::string line;
    while( std::getline( reference, line ) ) {
        if( line.empty() || line[ 0 ] == '#' ) { continue; }
        auto const w { words( line ) };
        if( w.size() != 25 ) { std::printf( "zly wiersz wzorca: %s\n", line.c_str() ); failed = true; continue; }
        auto const value = [ & ]( std::size_t const Index ) { return ( w[ Index ] == "-" ? 0.0 : std::stod( w[ Index ] ) ); };
        auto const name { w[ 0 ] };
        auto const *scheme { catalog.scheme( std::stoi( w[ 1 ] ), std::stoi( w[ 2 ] ) ) };
        ++count;
        if( scheme == nullptr ) { std::printf( "brak schematu dla %s\n", name.c_str() ); failed = true; continue; }
        catenary::cantilever_input input;
        input.stay_mount = { value( 3 ), value( 4 ) };
        input.strut_mount = { value( 5 ), value( 6 ) };
        input.steady_mount = { value( 7 ), value( 8 ) };
        input.messenger = { value( 9 ), value( 10 ) };
        catenary::cantilever_result result;
        if( false == catenary::solve_cantilever( catalog, *scheme, input, result ) ) {
            std::printf( "%s: nie zlozony: %s\n", name.c_str(), result.warnings.empty() ? "?" : result.warnings.front().c_str() );
            failed = true; continue;
        }
        ++solved;
        for( auto const &warning : result.warnings ) {
            // do zliczania: komunikat bez nazwy wysięgnika i liczb
            auto text { warning.substr( warning.find( ": " ) + 2 ) };
            for( auto &c : text ) { if( c >= '0' && c <= '9' ) { c = '#'; } }
            ++warnings[ text ];
        }
        auto const away { ( scheme->card / 10 ) % 2 == 0 };  // 112x, 114x
        auto &group { stats[ away ? "odsuw od slupa" : "odsuw do slupa" ] };
        auto const angle { std::atan2( value( 12 ), -value( 11 ) ) };
        group[ "1 kat ukosnika [st]" ].add( std::abs( result.strut_angle - angle ) * 180.0 / 3.14159265358979, name );
        group[ "2 uchwyt glowny 4050 [cm]" ].add( 100.0 * distance( result.find( "head_clamp" ), value( 13 ), value( 14 ) ), name );
        group[ "3 uchwyt odciagu 4070 [cm]" ].add( 100.0 * distance( result.find( "stay_clamp" ), value( 15 ), value( 16 ) ), name );
        group[ "4 koncowka odciagu 4400 [cm]" ].add( 100.0 * distance( result.find( "stay_end" ), value( 21 ), value( 22 ) ), name );
        if( false == scheme->steady_mounted ) {
            group[ "5 dolny uchwyt 4050 [cm]" ].add( 100.0 * distance( result.find( "steady_clamp" ), value( 17 ), value( 18 ) ), name );
            group[ "6 hak wysiegu pomocniczego [cm]" ].add( 100.0 * distance( result.find( "steady" ), value( 19 ), value( 20 ) ), name );
        }
        group[ "7 obejma linki 4210 [cm]" ].add( 100.0 * distance( result.find( "cord_clamp" ), value( 23 ), value( 24 ) ), name );

        // mechanika ma się zamykać co do milimetra: odciąg w jednej linii i o długości z katalogu,
        // lina nośna w uchwycie, ucho dolnego uchwytu w haku wysięgu pomocniczego
        auto const *stayinsulator { result.find( "stay_insulator" ) };
        auto const *stayend { result.find( "stay_end" ) };
        auto const *hook { result.find( "messenger_clamp" ) };
        auto const *part { catalog.part( scheme->roles.at( "stay" ) ) };
        auto const *end { catalog.part( scheme->roles.at( "stay_end" ) ) };
        auto const *clamp { catalog.part( scheme->roles.at( "messenger_clamp" ) ) };
        auto const chain { catalog.part( scheme->roles.at( "stay_insulator" ) )->length + part->length + end->holes[ result.stay_hole ] };
        auto const span { std::hypot( stayend->position[ 1 ] - stayinsulator->position[ 1 ], stayend->position[ 2 ] - stayinsulator->position[ 2 ] ) };
        // uchwyt liny stoi zerem w osi liny nośnej, a jego hak wypada w uchu uchwytu głównego: 7 cm od osi ukośnika
        auto const hang { std::hypot( hook->position[ 1 ] - input.messenger.y, hook->position[ 2 ] - input.messenger.z ) };
        auto const *head { result.find( "head_clamp" ) };
        auto const tilt { std::atan2( -hook->rotation[ 5 ], hook->rotation[ 4 ] ) };
        auto const hooky { hook->position[ 1 ] + clamp->hook[ 0 ] * std::cos( tilt ) - clamp->hook[ 1 ] * std::sin( tilt ) };
        auto const hookz { hook->position[ 2 ] + clamp->hook[ 0 ] * std::sin( tilt ) + clamp->hook[ 1 ] * std::cos( tilt ) };
        auto exact { std::abs( chain - span ) < 1e-4 && hang < 1e-9
            && std::abs( std::hypot( head->position[ 1 ] - hooky, head->position[ 2 ] - hookz ) - catalog.part( scheme->roles.at( "head_clamp" ) )->eye ) < 1e-6 };
        if( false == scheme->steady_mounted ) {
            // ucho dolnego uchwytu: w odległości "eye" od osi ukośnika, w haku
            auto const *low { result.find( "steady_clamp" ) };
            auto const *steady { result.find( "steady" ) };
            // na uchu leży punkt "hook" wysięgu pomocniczego (rura pozioma, Y części w stronę konstrukcji)
            auto const contact { steady->position[ 1 ] + catalog.part( scheme->roles.at( "steady" ) )->hook[ 0 ] };
            exact = exact && std::abs( std::hypot( low->position[ 1 ] - contact, low->position[ 2 ] - steady->position[ 2 ] ) - catalog.part( scheme->roles.at( "steady_clamp" ) )->eye ) < 1e-6;
        }
        if( false == exact ) { ++inexact; std::printf( "%s: mechanika sie nie zamyka (odciag %.4f / %.4f, lina %.4f)\n", name.c_str(), chain, span, hang ); }
    }
    std::printf( "zlozenia z Blendera: %d, zlozonych %d, z niedomknieta mechanika %d  %s\n", count, solved, inexact, ( count == 163 && solved == count && inexact == 0 ) ? "OK" : "BLAD" );
    failed = failed || count != 163 || solved != count || inexact != 0;
    for( auto const &group : stats ) {
        std::printf( "  %s:\n", group.first.c_str() );
        for( auto const &stat : group.second ) {
            std::printf( "    %-34s sr.kw. %6.2f  najw. %6.2f (%s)\n", stat.first.c_str() + 2, stat.second.rms(), stat.second.max, stat.second.worst.c_str() );
        }
    }
    std::printf( "  ostrzezenia solvera przy wymiarach ze zlozen:\n" );
    if( warnings.empty() ) { std::printf( "    brak\n" ); }
    for( auto const &warning : warnings ) { std::printf( "    %3d x %s\n", warning.second, warning.first.c_str() ); }
    {
        // progi: do słupa solver ma trafiać w ręczne złożenia co do paru centymetrów; odciąg jest w złożeniach
        // dopasowany na oko (końcówka 4400 nie trafia w sworzeń uchwytu), więc tam próg jest luźniejszy
        auto &to { stats[ "odsuw do slupa" ] }; auto &away { stats[ "odsuw od slupa" ] };
        auto const good { to[ "1 kat ukosnika [st]" ].max < 1.0 && to[ "2 uchwyt glowny 4050 [cm]" ].max < 3.0 && to[ "5 dolny uchwyt 4050 [cm]" ].max < 3.0
            && to[ "6 hak wysiegu pomocniczego [cm]" ].max < 5.5 /* hak leży krawędzią na krawędzi ucha, w złożeniach osią w osi: 3 cm różnicy */ && to[ "3 uchwyt odciagu 4070 [cm]" ].max < 15.0
            && away[ "1 kat ukosnika [st]" ].max < 2.0 && away[ "2 uchwyt glowny 4050 [cm]" ].max < 9.0 && away[ "3 uchwyt odciagu 4070 [cm]" ].max < 20.0 };
        std::printf( "zgodnosc ze zlozeniami  %s\n", good ? "OK" : "BLAD" );
        failed = failed || !good;
    }

    // --- całe pasmo A każdej karty: zaczepy izolatorów od 20 cm przed dolną granicą pasma do 10 cm za nią
    // (mocowanie do słupa zabiera ok. 20 cm), co 5 cm. Wynik jest informacją, nie warunkiem testu
    {
        std::map<std::string, int> sweep; int cases { 0 }, clean { 0 };
        for( auto const &entry : catalog.schemes() ) {
            auto const &scheme { entry.second };
            for( int step = -4; step <= 2; ++step ) {
                catenary::cantilever_result result;
                ++cases;
                if( false == catenary::solve_cantilever( catalog, scheme, catenary::cantilever_nominal_input( scheme, scheme.reach_min + 0.05 * step ), result ) ) { ++sweep[ "nie zlozony" ]; continue; }
                if( result.warnings.empty() ) { ++clean; }
                for( auto const &warning : result.warnings ) {
                    auto text { warning.substr( warning.find( ": " ) + 2 ) };
                    for( auto &c : text ) { if( c >= '0' && c <= '9' ) { c = '#'; } }
                    auto const quote { text.find( '"' ) };
                    if( quote != std::string::npos ) { auto const close { text.find( '"', quote + 1 ) }; if( close != std::string::npos ) { text.erase( quote, close - quote + 1 ); } }
                    ++sweep[ text ];
                }
            }
        }
        std::printf( "pasmo A (zaczepy od -20 do +10 cm wokol dolnej granicy): przypadkow %d, bez ostrzezen %d\n", cases, clean );
        for( auto const &warning : sweep ) { std::printf( "    %4d x %s\n", warning.second, warning.first.c_str() ); }
    }

    // --- przeliczenie na układ scenerii: to, co symulator zrobi z kątami i skalą wpisu node model, ma dać
    // te same punkty, co macierz części w układzie wysięgnika obróconym o kierunek do konstrukcji
    {
        auto worst { 0.0 }; int checked { 0 };
        double const samples[ 4 ][ 3 ] { { 0.3, -1.1, 0.2 }, { -0.05, 0.4, -0.07 }, { 0.0, 0.0, 1.0 }, { 1.0, 0.0, 0.0 } };
        for( auto const heading : { 0.0, 0.7, -2.4, 3.1 } ) {
            for( auto const card : { 1110, 1113, 1124, 1141, 1203 } ) {
                auto const *scheme { catalog.scheme( card, 1 ) };
                catenary::cantilever_result result;
                if( scheme == nullptr || false == catenary::solve_cantilever( catalog, *scheme, catenary::cantilever_nominal_input( *scheme, scheme->reach_min ), result ) ) { failed = true; continue; }
                for( auto const &placement : result.parts ) {
                    auto const part { catenary::cantilever_to_scene( placement, heading ) };
                    auto const rad { 3.14159265358979323846 / 180.0 };
                    auto const cy { std::cos( part.angles[ 1 ] * rad ) }, sy { std::sin( part.angles[ 1 ] * rad ) };
                    auto const cx { std::cos( part.angles[ 0 ] * rad ) }, sx { std::sin( part.angles[ 0 ] * rad ) };
                    auto const cz { std::cos( part.angles[ 2 ] * rad ) }, sz { std::sin( part.angles[ 2 ] * rad ) };
                    for( auto const &sample : samples ) {
                        // symulator: wierzchołek pliku ( x, y, z ) staje się ( -x, z, y ), potem skala, obroty Z, X, Y i przesunięcie
                        double v[ 3 ] { -sample[ 0 ] * part.scale[ 0 ], sample[ 2 ] * part.scale[ 1 ], sample[ 1 ] * part.scale[ 2 ] };
                        double a[ 3 ] { v[ 0 ] * cz - v[ 1 ] * sz, v[ 0 ] * sz + v[ 1 ] * cz, v[ 2 ] };
                        double b[ 3 ] { a[ 0 ], a[ 1 ] * cx - a[ 2 ] * sx, a[ 1 ] * sx + a[ 2 ] * cx };
                        double const simulator[ 3 ] { b[ 0 ] * cy + b[ 2 ] * sy + part.offset[ 0 ], b[ 1 ] + part.offset[ 1 ], -b[ 0 ] * sy + b[ 2 ] * cy + part.offset[ 2 ] };
                        // solver: punkt w układzie wysięgnika, potem zamiana osi i obrót o kierunek do konstrukcji
                        auto const &r { placement.rotation };
                        double const s[ 3 ] { sample[ 0 ] * placement.scale[ 0 ], sample[ 1 ] * placement.scale[ 1 ], sample[ 2 ] * placement.scale[ 2 ] };
                        double const c[ 3 ] {
                            r[ 0 ] * s[ 0 ] + r[ 1 ] * s[ 1 ] + r[ 2 ] * s[ 2 ] + placement.position[ 0 ],
                            r[ 3 ] * s[ 0 ] + r[ 4 ] * s[ 1 ] + r[ 5 ] * s[ 2 ] + placement.position[ 1 ],
                            r[ 6 ] * s[ 0 ] + r[ 7 ] * s[ 1 ] + r[ 8 ] * s[ 2 ] + placement.position[ 2 ] };
                        auto const ch { std::cos( heading ) }, sh { std::sin( heading ) };
                        double const expected[ 3 ] { -c[ 0 ] * ch + c[ 1 ] * sh, c[ 2 ], c[ 0 ] * sh + c[ 1 ] * ch };
                        for( int axis = 0; axis < 3; ++axis ) { worst = std::max( worst, std::abs( simulator[ axis ] - expected[ axis ] ) ); }
                        ++checked;
                    }
                }
            }
        }
        auto const good { checked > 500 && worst < 1e-9 };
        std::printf( "przeliczenie na uklad scenerii: punktow %d, najwieksza roznica %.1e m  %s\n", checked, worst, good ? "OK" : "BLAD" );
        failed = failed || !good;
    }

    std::printf( failed ? "TEST NIE PRZESZEDL\n" : "wszystko OK\n" );
    return failed ? 1 : 0;
}
