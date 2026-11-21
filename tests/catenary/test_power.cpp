// Test obwodu prądu stałego nowej sieci (world/catenary_power.h): przypadki policzone ręcznie.
// Budowanie: g++ -std=c++17 -O2 -I. tests/catenary/test_power.cpp world/catenary_power.cpp -o tests/catenary/tp

#include "world/catenary_power.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>

using namespace catenary_power;

namespace {

int Failures { 0 };

void check( bool const Good, std::string const &What, double const Value = 0.0, double const Expected = 0.0 ) {
    std::printf( "%s %s: %.3f (oczekiwane %.3f)\n", Good ? "OK  " : "BLAD", What.c_str(), Value, Expected );
    if( false == Good ) { ++Failures; }
}

void near( double const Value, double const Expected, double const Tolerance, std::string const &What ) {
    check( std::abs( Value - Expected ) <= Tolerance, What, Value, Expected );
}

// łańcuch Count gałęzi o oporze R; zwraca węzeł początkowy, numery gałęzi w Branches
int chain( circuit &Circuit, int const Count, double const R, std::vector<int> &Branches, int First = -1 ) {
    auto const start { First >= 0 ? First : Circuit.add_node() };
    auto previous { start };
    for( int index = 0; index < Count; ++index ) {
        auto const node { Circuit.add_node() };
        Branches.push_back( Circuit.add_branch( previous, node, R ) );
        previous = node;
    }
    return start;
}

source supply( std::string const &Name, int const Node, double const Voltage = 3300.0, double const Internal = 0.2 ) {
    source result;
    result.name = Name;
    result.node = Node;
    result.voltage = Voltage;
    result.internal = Internal;
    return result;
}

// tor prosty wzdłuż X od X0, Count przęseł po 50 m; nazwy zasilaczy wg Names(indeks); przęsła łączone końcami
void track( std::vector<span_input> &Spans, int const Count, double const X0, double const Y, std::string ( *Names )( int ) ) {
    auto const first { static_cast<int>( Spans.size() ) };
    for( int index = 0; index < Count; ++index ) {
        span_input span;
        span.power = Names( index );
        span.ends[ 0 ][ 0 ] = X0 + index * 50.0;
        span.ends[ 1 ][ 0 ] = X0 + ( index + 1 ) * 50.0;
        span.ends[ 0 ][ 1 ] = span.ends[ 1 ][ 1 ] = Y;
        span.resistivity = 0.058e-3;
        if( index > 0 ) { span.next[ 0 ] = first + index - 1; span.nextend[ 0 ] = 1; }
        if( index + 1 < Count ) { span.next[ 1 ] = first + index + 1; span.nextend[ 1 ] = 0; }
        Spans.push_back( span );
    }
}

source_input feeder( std::string const &Name, double const X, double const Y, bool const Section = false ) {
    source_input result;
    result.parameters.name = Name;
    result.parameters.voltage = 3300.0;
    result.parameters.internal = 0.2;
    result.location[ 0 ] = X;
    result.location[ 1 ] = Y;
    result.section = Section;
    return result;
}

double feeder_current( mapping const &Map, std::string const &Name ) {
    for( std::size_t index = 0; index < Map.sources.size(); ++index ) {
        if( Map.sources[ index ] == Name ) { return Map.network.sources()[ index ].current; }
    }
    return -1.0e9;
}

} // namespace

int main() {
    // przęsło 50 m sieci YC120-2C (ok. 0,058 Ω/km) z torem (0,015 Ω/km)
    double const R { 50.0 * ( 0.058 + 0.015 ) / 1000.0 };

    // 1. jedna podstacja, pociąg 1000 A na ostatnim przęśle (100 przęseł = 5 km)
    {
        circuit c;
        std::vector<int> spans;
        auto const start { chain( c, 100, R, spans ) };
        c.add_source( supply( "A", start ) );
        c.add_load( spans.back(), 1000.0 );
        c.solve( 0.02 );
        near( c.branch_voltage( spans.back() ), 3300.0 - 1000.0 * ( 0.2 + 99.5 * R ), 0.05, "jedna podstacja, pociag na koncu 5 km" );
        near( c.sources()[ 0 ].current, 1000.0, 0.05, "prad podstacji = prad pociagu" );
    }
    // 2. dwie podstacje na końcach (zasilanie dwustronne), pociąg w środku: po połowie prądu z każdej
    double drop_two { 0.0 };
    {
        circuit c;
        std::vector<int> spans;
        auto const start { chain( c, 200, R, spans ) };
        c.add_source( supply( "A", start ) );
        c.add_source( supply( "B", c.branches().back().b ) );
        // pociąg w samym środku: na granicy przęseł 99 i 100, po połowie poboru z każdego
        c.add_load( spans[ 99 ], 500.0 );
        c.add_load( spans[ 100 ], 500.0 );
        c.solve( 0.02 );
        near( c.sources()[ 0 ].current, 500.0, 0.05, "dwustronnie: podstacja A polowa" );
        near( c.sources()[ 1 ].current, 500.0, 0.05, "dwustronnie: podstacja B polowa" );
        near( c.voltage( c.branches()[ spans[ 99 ] ].b ), 3300.0 - 500.0 * 0.2 - ( 99.0 * 500.0 + 250.0 ) * R, 0.05, "dwustronnie: napiecie w srodku" );
        drop_two = 3300.0 - c.voltage( c.branches()[ spans[ 99 ] ].b );
    }
    // 3. kabina sekcyjna: dwa tory między tymi samymi podstacjami, szyna zbiorcza w połowie łączy sieci obu torów.
    // Głowacki s. 209: przy jednym pociągu w połowie odcinka spadek napięcia mniejszy o ok. 50 %
    {
        circuit c;
        std::vector<int> one, two;
        auto const start1 { chain( c, 200, R, one ) };
        auto const start2 { chain( c, 200, R, two ) };
        // podstacja: jedna szyna zbiorcza, zasilacze na oba tory (opór kabla pomijalny wobec wewnętrznego)
        auto const busa { c.add_node() }, busb { c.add_node() };
        c.add_branch( busa, start1, 0.005 );
        c.add_branch( busa, start2, 0.005 );
        c.add_branch( busb, c.branches()[ one.back() ].b, 0.005 );
        c.add_branch( busb, c.branches()[ two.back() ].b, 0.005 );
        c.add_source( supply( "A", busa ) );
        c.add_source( supply( "B", busb ) );
        // kabina (bez źródła) w połowie: węzeł 100 obu torów
        auto const cabin { c.add_node() };
        c.add_branch( cabin, c.branches()[ one[ 99 ] ].b, 0.002 );
        c.add_branch( cabin, c.branches()[ two[ 99 ] ].b, 0.002 );
        c.add_load( one[ 99 ], 500.0 );
        c.add_load( one[ 100 ], 500.0 );
        c.solve( 0.02 );
        auto const drop { 3300.0 - c.voltage( c.branches()[ one[ 99 ] ].b ) };
        // spadek w samej sieci (bez oporu wewnętrznego podstacji, który jest taki sam w obu układach)
        auto const line_two { drop_two - 500.0 * 0.2 }, line_cabin { drop - 500.0 * 0.2 };
        check( line_cabin < line_two * 0.6 && line_cabin > line_two * 0.4, "kabina: spadek w sieci ok. polowa (Glowacki s. 209)", line_cabin, line_two * 0.5 );
    }
    // 4. dioda: podstacje o różnym napięciu bez obciążenia - słabsza nie przyjmuje prądu z mocniejszej
    {
        circuit c;
        std::vector<int> spans;
        auto const start { chain( c, 50, R, spans ) };
        c.add_source( supply( "A", start, 3300.0 ) );
        c.add_source( supply( "B", c.branches().back().b, 3400.0 ) );
        c.solve( 0.02 );
        check( false == c.sources()[ 0 ].conducting, "dioda: slabsza podstacja nie przewodzi" );
        near( c.sources()[ 1 ].current, 0.0, 0.01, "dioda: brak pradu wyrownawczego" );
        near( c.voltage( start ), 3400.0, 0.1, "dioda: napiecie sieci = mocniejsza podstacja" );
    }
    // 5. rekuperacja bez odbiorcy: podstacja bez rekuperacji, pojazd oddaje 1000 A - napięcie nie ucieka ponad próg
    {
        circuit c;
        std::vector<int> spans;
        auto const start { chain( c, 50, R, spans ) };
        c.add_source( supply( "A", start ) );
        c.add_load( spans.back(), -1000.0 );
        c.solve( 0.02 );
        auto const v { c.branch_voltage( spans.back() ) };
        check( v <= current_settings().regen_limit + 1.0 && v >= 3300.0, "rekuperacja bez odbiorcy: napiecie ograniczone", v, current_settings().regen_limit );
        near( c.sources()[ 0 ].current, 0.0, 0.01, "rekuperacja bez odbiorcy: prostownik nie przyjmuje" );
    }
    // 6. rekuperacja z odbiorcą: drugi pociąg pobiera 1000 A, pierwszy oddaje 600 A - podstacja daje resztę
    {
        circuit c;
        std::vector<int> spans;
        auto const start { chain( c, 100, R, spans ) };
        c.add_source( supply( "A", start ) );
        c.add_load( spans[ 40 ], -600.0 );
        c.add_load( spans[ 60 ], 1000.0 );
        c.solve( 0.02 );
        near( c.sources()[ 0 ].current, 400.0, 0.5, "rekuperacja z odbiorca: podstacja daje reszte" );
    }
    // 7. podstacja przyjmująca rekuperację (falownik): oddaje się w nią prąd wsteczny
    {
        circuit c;
        std::vector<int> spans;
        auto const start { chain( c, 50, R, spans ) };
        auto item { supply( "A", start ) };
        item.recuperation = true;
        c.add_source( item );
        c.add_load( spans.back(), -500.0 );
        c.solve( 0.02 );
        near( c.sources()[ 0 ].current, -500.0, 0.5, "falownik: przyjmuje 500 A" );
    }
    // 8. wyłącznik szybki: nastawa 2000 A, pociąg 3000 A - wyłącza, po czasie załącza, po 3 próbach długa przerwa
    {
        circuit c;
        std::vector<int> spans;
        auto const start { chain( c, 20, R, spans ) };
        auto item { supply( "A", start ) };
        item.trip_current = 2000.0;
        item.fast_timeout = 1.0;
        item.repetition = 3;
        item.slow_timeout = 60.0;
        c.add_source( item );
        int trips { 0 };
        bool locked { false };
        for( int step = 0; step < 300; ++step ) {
            c.clear_loads();
            c.add_load( spans.back(), 3000.0 );
            auto const was { c.sources()[ 0 ].tripped };
            c.solve( 0.1 );
            if( false == was && c.sources()[ 0 ].tripped ) { ++trips; }
            locked = locked || c.sources()[ 0 ].locked;
        }
        check( trips == 4 && locked, "wylacznik: 1 + 3 proby, potem dluga przerwa", trips, 4 );
        c.clear_loads();
        c.add_load( spans.back(), 3000.0 );
        c.solve( 0.1 );
        near( c.branch_voltage( spans.back() ), 0.0, 0.001, "wylacznik otwarty: brak napiecia" );
    }
    // 9. wyspa bez zasilacza (np. za otwartym odłącznikiem): zero woltów, bez względu na pobór
    {
        circuit c;
        std::vector<int> fed, dead;
        auto const start { chain( c, 10, R, fed ) };
        chain( c, 10, R, dead );
        c.add_source( supply( "A", start ) );
        c.add_load( dead[ 5 ], 800.0 );
        c.solve( 0.02 );
        near( c.branch_voltage( dead[ 5 ] ), 0.0, 0.001, "wyspa bez zasilacza: 0 V" );
        near( c.voltage( start ), 3300.0, 0.01, "druga wyspa nietknieta" );
    }
    // 10. wydajność: 20 000 przęseł (1000 km toru), pociągi co 5 km
    {
        circuit c;
        std::vector<int> spans;
        auto const start { chain( c, 20000, R, spans ) };
        for( int index = 0; index <= 20000; index += 300 ) {
            c.add_source( supply( "P" + std::to_string( index ), index == 0 ? start : c.branches()[ spans[ index - 1 ] ].b ) );
        }
        for( int index = 50; index < 20000; index += 100 ) { c.add_load( spans[ index ], 800.0 ); }
        c.solve( 0.02 );
        auto const begin { std::chrono::steady_clock::now() };
        int const repeats { 20 };
        for( int step = 0; step < repeats; ++step ) {
            // pociągi się ruszają: pobór trochę inny co krok
            c.clear_loads();
            for( int index = 50 + step; index < 20000; index += 100 ) { c.add_load( spans[ index ], 800.0 + step ); }
            c.solve( 0.02 );
        }
        auto const ms { std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - begin ).count() / repeats };
        check( ms < 5.0, "wydajnosc: 20 000 przesel, ms na krok", ms, 5.0 );
    }

    // --- budowa obwodu z przęseł (etap B) ---
    // 11. izolator sekcyjny: przęsła 0-19 zasilacz A, 20-39 zasilacz B - przerwa między nimi
    {
        std::vector<span_input> spans;
        track( spans, 40, 0.0, 0.0, []( int Index ) { return std::string( Index < 20 ? "A" : "B" ); } );
        mapping map;
        build( spans, { feeder( "A", 0.0, 10.0 ), feeder( "B", 2000.0, 10.0 ) }, map );
        map.network.add_load( map.branch[ 21 ], 800.0 );
        map.network.solve( 0.02 );
        near( feeder_current( map, "A" ), 0.0, 0.01, "izolator sekcyjny: A nie zasila za izolatorem" );
        near( feeder_current( map, "B" ), 800.0, 0.01, "izolator sekcyjny: B zasila swoje" );
        check( map.network.island( map.network.branches()[ map.branch[ 19 ] ].b ) != map.network.island( map.network.branches()[ map.branch[ 20 ] ].a ), "izolator sekcyjny: dwie wyspy" );
    }
    // 11a. izolator w środku przęsła 19 (przerwa 0,45-0,55): granica elektryczna w przerwie, nie na końcu przęsła (użytkownik
    // 2026-11-21: „napięcie zniknęło dopiero od następnego przęsła”). Tylko zasilacz A: za przerwą 0 V już w przęśle 19
    {
        std::vector<span_input> spans;
        track( spans, 40, 0.0, 0.0, []( int Index ) { return std::string( Index < 20 ? "A" : "B" ); } );
        spans[ 19 ].gap[ 0 ] = 0.45;
        spans[ 19 ].gap[ 1 ] = 0.55;
        mapping map;
        build( spans, { feeder( "A", 0.0, 10.0 ) }, map );
        int branch { -1 };
        double position { 0.0 };
        auto const before { map.locate( 19, 0.3, branch, position ) };
        map.network.solve( 0.02 );
        auto const vbefore { before ? map.network.branch_voltage_at( branch, position, 0.0 ) : -1.0 };
        auto const after { map.locate( 19, 0.7, branch, position ) };
        check( before && vbefore > 3000.0 && false == after, "izolator w przesle: przed przerwa napiecie, za nia przeslo poza zasilaniem", vbefore, 3300.0 );
        // z zasilaczem B: część za przerwą z B
        mapping both;
        build( spans, { feeder( "A", 0.0, 10.0 ), feeder( "B", 2000.0, 10.0 ) }, both );
        auto const located { both.locate( 19, 0.7, branch, position ) };
        both.network.add_load( branch, 500.0, position );
        both.network.solve( 0.02 );
        near( located ? feeder_current( both, "B" ) : -1.0, 500.0, 0.01, "izolator w przesle: za przerwa zasila B" );
        near( feeder_current( both, "A" ), 0.0, 0.01, "izolator w przesle: A nie zasila za przerwa" );
    }
    // 12. przęsło naprężenia zwarte (ten sam zasilacz, bieżnia wspólna): łańcuch 2 równolegle obok łańcucha 1 na końcówce,
    // połączony tylko przez bieżnię; izolowane (inny zasilacz) - bez połączenia
    for( auto const isolated : { false, true } ) {
        std::vector<span_input> spans;
        track( spans, 10, 0.0, 0.0, []( int ) { return std::string( "A" ); } );
        track( spans, 10, 400.0, 0.25, isolated ? +[]( int ) { return std::string( "A2" ); } : +[]( int ) { return std::string( "A" ); } );
        // bieżnia wspólna: przęsła 8, 9 łańcucha 1 i 0, 1 łańcucha 2 (końce co 50 m, 0,25 m obok)
        for( int a : { 8, 9 } ) {
            for( int b : { 10, 11 } ) { spans[ a ].parallel.push_back( b ); spans[ b ].parallel.push_back( a ); }
        }
        mapping map;
        build( spans, { feeder( "A", 0.0, 10.0 ), feeder( "A2", 900.0, 10.0 ) }, map );
        map.network.add_load( map.branch[ 19 ], 500.0 );
        map.network.solve( 0.02 );
        if( isolated ) {
            near( feeder_current( map, "A2" ), 500.0, 0.01, "izolowane przeslo naprezenia: druga siec z wlasnego zasilacza" );
        }
        else {
            near( feeder_current( map, "A" ), 500.0, 0.01, "zwarte przeslo naprezenia: prad przez lacznik bieżni" );
        }
    }
    // 13. nazwa zasilacza bez wpisu w scenerii: przęsła poza obwodem (liczone po staremu)
    {
        std::vector<span_input> spans;
        track( spans, 5, 0.0, 0.0, []( int ) { return std::string( "nieznany" ); } );
        mapping map;
        build( spans, { feeder( "A", 0.0, 10.0 ) }, map );
        check( map.branch[ 0 ] < 0 && map.branch[ 4 ] < 0, "zasilacz bez wpisu: przesla poza obwodem" );
    }
    // 14. kabina sekcyjna: dwa tory po 8 km między podstacjami A (x=0) i B (x=8000), w połowie przęsła z nazwą kabiny KS;
    // pociąg 1000 A w połowie toru 1 - prąd płynie też siecią toru 2 przez szynę zbiorczą kabiny
    {
        auto const names = []( int Index ) { return std::string( Index < 78 ? "A" : Index < 82 ? "KS" : "B" ); };
        std::vector<span_input> spans;
        track( spans, 160, 0.0, 0.0, names );
        track( spans, 160, 0.0, 4.5, names );
        mapping map;
        build( spans, { feeder( "A", 0.0, 10.0 ), feeder( "B", 8000.0, 10.0 ), feeder( "KS", 4000.0, 10.0, true ) }, map );
        // zasilacze A i B przyłączone do obu torów (każdy tor to osobna część aż do kabiny, która je łączy)
        map.network.add_load( map.branch[ 80 ], 1000.0 );
        map.network.solve( 0.02 );
        auto const track2 { std::abs( map.network.branch_current( map.branch[ 160 + 40 ] ) ) };
        check( track2 > 100.0, "kabina: prad plynie tez siecia toru 2", track2, 250.0 );
        near( feeder_current( map, "A" ) + feeder_current( map, "B" ), 1000.0, 0.05, "kabina: podstacje razem = pociag" );
    }
    std::printf( Failures == 0 ? "wszystko OK\n" : "BLEDY: %d\n", Failures );
    return Failures == 0 ? 0 : 1;
}
