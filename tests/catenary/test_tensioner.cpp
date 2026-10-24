// Test urządzenia naprężającego: dane z tests/catenary/data/traction_tensioners.txt, naciągi z plików traction_<symbol>.txt.
// Uruchamiać z katalogu repozytorium: tests/catenary/tt tests/catenary/data/
#include "world/catenary_tensioner.h"

#include <cmath>
#include <cstdio>
#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

namespace {

int failures { 0 };

void check( bool const Condition, std::string const &Text ) {
    std::printf( "%s  %s\n", Condition ? "OK   " : "BLAD ", Text.c_str() );
    if( false == Condition ) { ++failures; }
}

// naciągi z pliku typu sieci: traction_code -> (lina, przewody)
struct network { std::string symbol; int contacts { 0 }; double messenger { 0.0 }, contact { 0.0 }; };

bool read_network( std::string const &File, int &Code, network &Network ) {
    std::ifstream file { File };
    if( false == file.is_open() ) { return false; }
    std::string line;
    Code = -1;
    while( std::getline( file, line ) ) {
        if( auto const comment { line.find( "//" ) }; comment != std::string::npos ) { line.erase( comment ); }
        std::istringstream words { line };
        std::string key, value;
        if( !( words >> key >> value ) ) { continue; }
        if( key == "traction_code:" ) { Code = std::stoi( value ); }
        else if( key == "symbol:" ) { Network.symbol = value; }
        else if( key == "contact_wire_num:" ) { Network.contacts = std::stoi( value ); }
        else if( key == "catenary_tension:" ) { Network.messenger = std::stod( value ); }
        else if( key == "contact_tension:" ) { Network.contact = std::stod( value ); }
    }
    return Code >= 0;
}

double rope_length( catenary::tensioner_result const &Result ) {
    double total { 0.0 };
    for( std::size_t k = 1; k < Result.rope.size(); ++k ) {
        double sum { 0.0 };
        for( int a = 0; a < 3; ++a ) { auto const d { Result.rope[ k ][ a ] - Result.rope[ k - 1 ][ a ] }; sum += d * d; }
        total += std::sqrt( sum );
    }
    return total;
}

} // namespace

int main( int argc, char **argv ) {
    std::string const data { argc > 1 ? argv[ 1 ] : "tests/catenary/data/" };
    catenary::tensioner_catalog catalog;
    std::vector<std::string> errors;
    check( catalog.load( data + "traction_tensioners.txt", errors ), "wczytanie traction_tensioners.txt" );
    for( auto const &error : errors ) { std::printf( "      %s\n", error.c_str() ); }
    check( errors.empty(), "plik bez bledow" );
    auto const *device { catalog.device( "0461" ) };
    check( device != nullptr, "urzadzenie 0461" );
    if( device == nullptr ) { return 1; }

    // tablica: wzor ze starej karty 13-0011 wobec wierszy nowej karty 0011-1
    {
        auto const *table { catalog.table( "0011-1" ) };
        double b, s;
        table->lookup( 10.0, 400.0, b, s );
        check( table->formula && std::abs( b - 1.75 ) < 1e-9 && std::abs( s - 2.25 ) < 1e-9, "wzor: +10 °C -> b 1,75, s 2,25" );
        int odd { 0 };
        double worst { 0.0 };
        for( std::size_t r = 0; r < table->distances.size(); ++r ) {
            for( std::size_t c = 0; c < table->temperatures.size(); ++c ) {
                table->formula_values( table->temperatures[ c ], table->distances[ r ], b, s );
                auto const miss { std::max( std::abs( b - table->b[ r ][ c ] ), std::abs( s - table->s[ r ][ c ] ) ) };
                if( miss > 0.025 ) {
                    ++odd;
                    std::printf( "      inaczej niz karta: %g m, %+g °C: wzor b %.3f s %.3f, karta b %.2f s %.2f\n", table->distances[ r ], table->temperatures[ c ], b, s, table->b[ r ][ c ], table->s[ r ][ c ] );
                }
                else { worst = std::max( worst, miss ); }
            }
        }
        std::printf( "      najwieksza roznica wzor - karta (bez literowek): %.3f m\n", worst );
        check( odd <= 3, "wzor zgodny z tablica 0011-1 do 2,5 cm (poza literowkami karty)" );
        double b7, s7;
        catalog.table( "0011-7" )->lookup( -25.0, 200.0, b7, s7 );
        check( std::abs( b7 - 3.02 ) < 0.01 && std::abs( s7 - 2.38 ) < 0.01, "tablica 0011-7 z przesuniecia 0011-1: 200 m, -25 °C -> b 3,02, s 2,38 (karta)" );
    }

    // naciagi z dzwigni i ciezarow = naciagi z kart 0002 w plikach typow sieci
    std::map<int, network> networks;
    for( auto const *symbol : { "C120-2C", "C95-2C", "C95-C", "YwsC120-2C" } ) {
        int code;
        network net;
        if( read_network( data + "traction_" + symbol + ".txt", code, net ) ) { networks[ code ] = net; }
    }
    for( auto const &type : device->types ) {
        auto const found { networks.find( type.code ) };
        if( found == networks.end() ) { check( false, "typ " + std::to_string( type.code ) + ": brak pliku typu sieci" ); continue; }
        auto const &net { found->second };
        catenary::tensioner_input input;
        input.type = type.code;
        input.contacts = net.contacts;
        catenary::tensioner_result result;
        auto const solved { catenary::solve_tensioner( catalog, *device, input, result ) };
        for( auto const &warning : result.warnings ) { std::printf( "      %s\n", warning.c_str() ); }
        char text[ 256 ];
        std::snprintf( text, sizeof( text ), "typ %d %s: %d ciezarow, lina %.0f daN (karta %.0f), przewody %.0f daN (karta %.0f)",
            type.code, net.symbol.c_str(), result.weights, result.messenger_tension, net.messenger, result.contact_tension, net.contact );
        check( solved && std::abs( result.messenger_tension - net.messenger ) < 0.006 * net.messenger && std::abs( result.contact_tension - net.contact ) < 0.006 * net.contact, text );
        check( result.contact_ends.size() == static_cast<std::size_t>( net.contacts ), "typ " + std::to_string( type.code ) + ": konce przewodow jezdnych" );
        check( result.warnings.empty(), "typ " + std::to_string( type.code ) + ": bez uwag przy +10 °C, 700 m" );
    }

    // lina: stala dlugosc przy zmianie temperatury (tablica spojna z 1:4), konce na swoich miejscach
    {
        catenary::tensioner_input input;
        catenary::tensioner_result result;
        double lengths[ 3 ];
        double const temperatures[ 3 ] { -25.0, 10.0, 40.0 };
        for( int k = 0; k < 3; ++k ) {
            input.temperature = temperatures[ k ];
            input.distance = 700.0;
            catenary::solve_tensioner( catalog, *device, input, result );
            lengths[ k ] = rope_length( result );
            std::printf( "      %+.0f °C: b %.2f s %.2f, lina %.2f m, stos %.2f m, gora preta %.2f m\n", temperatures[ k ], result.b, result.s, lengths[ k ], result.stack_height, result.rope.back()[ 2 ] );
        }
        check( std::abs( lengths[ 0 ] - lengths[ 1 ] ) < 0.05 && std::abs( lengths[ 2 ] - lengths[ 1 ] ) < 0.05, "lina: ta sama dlugosc w -25, +10 i +40 °C" );
        check( std::abs( lengths[ 1 ] - 8.70 ) < 1.5, "lina: dlugosc blisko ciegla 4490-1 (8,70 m)" );
    }

    // stan sieci: brakujace ciezary, wydluzenie, ciezary na gruncie
    {
        catenary::tensioner_input input;
        catenary::tensioner_result full, thin, stretched, grounded;
        catenary::solve_tensioner( catalog, *device, input, full );
        input.missing = 4;
        catenary::solve_tensioner( catalog, *device, input, thin );
        check( thin.weights == full.weights - 4 && std::abs( thin.tension - full.tension * thin.weights / full.weights ) < 1e-6, "4 brakujace ciezary: naciag mniejszy proporcjonalnie" );
        input.missing = 0;
        input.elongation = 0.10;
        catenary::solve_tensioner( catalog, *device, input, stretched );
        check( std::abs( stretched.b - ( full.b - 0.10 ) ) < 1e-9 && std::abs( stretched.s - ( full.s - 0.40 ) ) < 1e-9, "wydluzenie 10 cm: rolka 10 cm, ciezary 40 cm nizej" );
        input.temperature = 40.0;
        input.distance = 750.0;
        input.elongation = 0.30;
        catenary::solve_tensioner( catalog, *device, input, grounded );
        check( grounded.grounded && grounded.tension == 0.0 && std::abs( grounded.s - input.ground ) < 1e-9, "+40 °C, 750 m, wydluzenie 30 cm: ciezary na gruncie, siec bez naciagu" );
    }

    // kolory i obrot betonowych
    {
        catenary::tensioner_input input;
        input.color = "szary";
        input.top_color = "zolty";
        catenary::tensioner_result result;
        catenary::solve_tensioner( catalog, *device, input, result );
        std::vector<catenary::tensioner_placement const *> weights;
        for( auto const &part : result.parts ) { if( part.role.rfind( "weight.", 0 ) == 0 ) { weights.push_back( &part ); } }
        check( weights.size() == 26 && weights.front()->part == "6321-02" && weights.back()->part == "6323-01", "kolory: stos szary (wyk. 2), najwyzszy zolty (wyk. 1)" );
        // n-ty ciezar w stosie (od 0) obrocony o n * 90 stopni: 0 -> X wzdluz +X, 1 -> X wzdluz +Y, 2 -> X wzdluz -X
        check( std::abs( weights[ 0 ]->rotation[ 0 ] - 1.0 ) < 1e-9 && std::abs( weights[ 1 ]->rotation[ 3 ] - 1.0 ) < 1e-9 && std::abs( weights[ 2 ]->rotation[ 0 ] + 1.0 ) < 1e-9
            && std::abs( weights[ 9 ]->rotation[ 3 ] - 1.0 ) < 1e-9, "kazdy kolejny ciezar obrocony o 90 stopni (zeliwne i betonowe)" );
    }

    // stare złożenie: uchwyty 5110 na przewodach
    {
        catenary::tensioner_input input;
        input.old_fittings = true;
        catenary::tensioner_result result;
        catenary::solve_tensioner( catalog, *device, input, result );
        int old { 0 };
        for( auto const &part : result.parts ) { if( part.role.rfind( "contact_clamp", 0 ) == 0 && part.part == "5110-01" ) { ++old; } }
        check( old == 2, "stare zlozenie: uchwyty krancowe 5110 na obu przewodach" );
    }
    // izolator rolkowy ESN-2025 zamiast 7150 (bez łączników 4720): osobno na linie i na przewodach; łańcuch krótszy o 0,432 m
    {
        auto const insulator = []( catenary::tensioner_result const &Result, std::string const &Role ) {
            std::string found;
            for( auto const &part : Result.parts ) {
                if( part.role.rfind( Role + ".", 0 ) == 0 && ( part.part == "esn_2025" || part.part == "7150-01" ) ) { found = part.part; }
            }
            return found;
        };
        catenary::tensioner_input input;
        catenary::tensioner_result fresh, both, line, wires;
        catenary::solve_tensioner( catalog, *device, input, fresh );
        input.old_messenger = input.old_contact = true;
        catenary::solve_tensioner( catalog, *device, input, both );
        input.old_contact = false;
        catenary::solve_tensioner( catalog, *device, input, line );
        input.old_messenger = false; input.old_contact = true;
        catenary::solve_tensioner( catalog, *device, input, wires );
        auto const shorter { fresh.messenger_end[ 0 ] - both.messenger_end[ 0 ] };
        int links { 0 };
        for( auto const &part : both.parts ) { if( part.role.rfind( "contact.", 0 ) == 0 ) { ++links; } }
        check( insulator( fresh, "messenger" ) == "7150-01" && insulator( both, "messenger" ) == "esn_2025" && insulator( both, "contact" ) == "esn_2025"
            && insulator( line, "messenger" ) == "esn_2025" && insulator( line, "contact" ) == "7150-01"
            && insulator( wires, "messenger" ) == "7150-01" && insulator( wires, "contact" ) == "esn_2025"
            && links == 2 && std::abs( shorter - 0.432 ) < 1e-6,
            "izolator rolkowy ESN-2025: na linie, na przewodach albo na obu; ESN-2025 + 4420, lancuch krotszy o "
            + std::to_string( shorter ) + " m" );
    }

    // dzwignia: lancuchy zaczynaja sie w otworach modelu (otwor w modelu: X = -odsuniecie, Z = wzdluz osi od zera)
    {
        auto const *lever { catalog.lever( "6406-01" ) };
        for( auto const &type : device->types ) {
            catenary::tensioner_input input;
            input.type = type.code;
            catenary::tensioner_result result;
            catenary::solve_tensioner( catalog, *device, input, result );
            catenary::tensioner_placement const *body { nullptr }, *messenger { nullptr }, *contact { nullptr };
            for( auto const &part : result.parts ) {
                if( part.role == "lever" ) { body = &part; }
                if( part.role == "messenger.1" ) { messenger = &part; }
                if( part.role == "contact.1" ) { contact = &part; }
            }
            auto const fits = [ & ]( catenary::tensioner_placement const *Link, std::string const &Hole ) {
                auto const &h { lever->holes.at( Hole ) };
                double const model[ 3 ] { -h.second, 0.0, h.first };
                for( int a = 0; a < 3; ++a ) {
                    auto const at { body->position[ a ] + body->rotation[ 3 * a ] * model[ 0 ] + body->rotation[ 3 * a + 1 ] * model[ 1 ] + body->rotation[ 3 * a + 2 ] * model[ 2 ] };
                    if( std::abs( at - Link->position[ a ] ) > 1e-6 ) { return false; }
                }
                return true;
            };
            check( body && messenger && contact && fits( messenger, type.messenger_hole ) && fits( contact, type.contact_hole ),
                "typ " + std::to_string( type.code ) + ": lacznik 4720 liny i przewodow w otworach dzwigni " + type.messenger_hole + "/" + type.contact_hole );
        }
    }

    // prowadnica i pierścień tylko na uchwytach: słup bez uchwytów w danych - bez nich (nie ma ich gdzie przykręcić), bez uwag
    {
        catenary::tensioner_input input;
        catenary::tensioner_result result;
        catenary::solve_tensioner( catalog, *device, input, result );
        auto none { true };
        for( auto const &part : result.parts ) { if( part.role == "guide" || part.role == "ring" ) { none = false; } }
        check( none && false == result.has_guide && result.warnings.empty(), "bez uchwytow (slup spoza danych): bez prowadnicy i pierscienia, bez uwag" );
    }

    // uchwyty 5100 na słupie 1611: górny trzyma 4450 za mount_top, dolny (odwrócony) za mount_bottom; tyły po drugiej stronie słupa
    {
        // uchwyty jeszcze zakomentowane w danych: ustawione tutaj
        auto withbrackets { *device };
        catenary::tensioner_device::bracket_set set;
        set.poles = { "1611" }; set.hole_out = 0.04; set.hole_drop = 0.04; set.bolt = 0.217;
        set.top_front = "5100-01"; set.top_back = "5100-05"; set.bottom_front = "5100-03"; set.bottom_back = "5100-06";
        withbrackets.brackets = { set };
        withbrackets.pivot_top = 0.03436; withbrackets.pivot_bottom = 0.02561;
        catenary::tensioner_input input;
        input.pole = "1611-11";
        input.depth = []( double ) { return 0.20; };
        catenary::tensioner_result result;
        catenary::solve_tensioner( catalog, withbrackets, input, result );
        std::map<std::string, catenary::tensioner_placement const *> by;
        for( auto const &part : result.parts ) { by[ part.role ] = &part; }
        auto const hole = []( catenary::tensioner_placement const *P, int const A ) {
            double const local[ 3 ] { -0.04, 0.0, -0.04 };
            return P->position[ A ] + P->rotation[ 3 * A ] * local[ 0 ] + P->rotation[ 3 * A + 1 ] * local[ 1 ] + P->rotation[ 3 * A + 2 ] * local[ 2 ];
        };
        auto good { by.count( "bracket_top" ) && by.count( "bracket_bottom" ) && by.count( "bracket_top.back" ) && by.count( "bracket_bottom.back" ) && by.count( "mount.1" ) };
        if( good ) {
            auto const *pivot { by[ "mount.1" ] };
            auto const *top { by[ "bracket_top" ] }, *bottom { by[ "bracket_bottom" ] };
            good = std::abs( pivot->position[ 0 ] - 0.04 ) < 1e-9
                && std::abs( hole( top, 0 ) - 0.04 ) < 1e-9 && std::abs( hole( top, 2 ) - ( pivot->position[ 2 ] + 0.03436 ) ) < 1e-9
                && std::abs( hole( bottom, 0 ) - 0.04 ) < 1e-9 && std::abs( hole( bottom, 2 ) - ( pivot->position[ 2 ] - 0.02561 ) ) < 1e-9
                && std::abs( by[ "bracket_top.back" ]->position[ 0 ] + 0.20 ) < 1e-9 && top->part == "5100-01" && by[ "bracket_top.back" ]->part == "5100-05"
                && result.warnings.empty();
        }
        check( good, "uchwyty 5100 na slupie 1611: otwory w mount_top / mount_bottom 4450 (0,04 przed czolem), tyly za slupem" );
        input.depth = []( double ) { return 0.25; };
        catenary::solve_tensioner( catalog, withbrackets, input, result );
        check( std::find( result.warnings.begin(), result.warnings.end(), std::string( "the bracket bolts are shorter than the structure is deep" ) ) != result.warnings.end(),
            "uchwyty 5100: uwaga, gdy sruby krotsze niz slup gleboki" );
        // prowadnica na uchwytach: mount 5070 w hole_1 uchwytu, pierścień po osi prowadnicy, stos 0,04 + 0,07 + 0,20 od czoła
        {
            auto full { withbrackets };
            full.guide = "5070-02"; full.guide_length = 4.50; full.guide_mount = 0.07; full.guide_bottom = 0.80;
            full.ring = "5080-01"; full.ring_arm = 0.200; full.ring_at = 1.00;
            catenary::tensioner_input in;
            in.pole = "1611-11";
            in.depth = []( double ) { return 0.12; };
            catenary::tensioner_result r;
            catenary::solve_tensioner( catalog, full, in, r );
            std::map<std::string, catenary::tensioner_placement const *> at;
            for( auto const &part : r.parts ) { at[ part.role ] = &part; }
            auto ok { at.count( "guide" ) && at.count( "ring" ) && at.count( "rod" ) && at.count( "bracket_guide_low" ) && at.count( "bracket_guide_high" ) && r.warnings.empty() };
            if( ok ) {
                ok = std::abs( at[ "guide" ]->position[ 0 ] - 0.11 ) < 1e-9 && std::abs( at[ "ring" ]->position[ 0 ] - 0.31 ) < 1e-9 && std::abs( at[ "rod" ]->position[ 0 ] - 0.31 ) < 1e-9
                    && std::abs( hole( at[ "bracket_guide_low" ], 0 ) - r.guide_mount[ 0 ] ) < 1e-9 && std::abs( hole( at[ "bracket_guide_low" ], 2 ) - 0.80 ) < 1e-9
                    && std::abs( hole( at[ "bracket_guide_high" ], 2 ) - 5.30 ) < 1e-9 && at[ "bracket_guide_low" ]->part == "5100-03" && at[ "bracket_guide_high" ]->part == "5100-01"
                    && std::abs( r.rope.back()[ 0 ] - 0.31 ) < 1e-9;
            }
            std::printf( "      prowadnica na uchwytach: os 0.110, stos %.3f m od czola, lina z rolki schodzi o %.3f m ukosnie\n",
                at.count( "rod" ) ? at[ "rod" ]->position[ 0 ] : 0.0, at.count( "rod" ) ? at[ "rod" ]->position[ 0 ] - ( 0.04 + 0.0747 + 0.27 - 0.116 ) : 0.0 );
            for( auto const &w : r.warnings ) { std::printf( "      %s\n", w.c_str() ); }
            check( ok, "prowadnica 5070 w hole_1 uchwytow 5100 (0,80 i 5,30), pierscien po osi prowadnicy, stos 0,31 od czola" );
            // pierścień 1,00 m nad spodem stosu zostaje na prowadnicy od -25 do +40 °C (karta 23-0018-2)
            auto stays { true };
            for( double const t : { -25.0, 40.0 } ) {
                in.temperature = t;
                catenary::solve_tensioner( catalog, full, in, r );
                stays = stays && r.warnings.empty();
                std::printf( "      %+.0f °C: pierscien %.2f m, prowadnica %.2f..%.2f m\n", t, r.s + 1.0, r.guide_mount[ 2 ], r.guide_end[ 2 ] );
            }
            check( stays, "pierscien na prowadnicy od -25 do +40 °C, bez uwag" );
        }
        input.pole = "1701";
        catenary::solve_tensioner( catalog, withbrackets, input, result );
        auto none { true };
        for( auto const &part : result.parts ) { if( part.role.rfind( "bracket", 0 ) == 0 ) { none = false; } }
        check( none, "slup bez uchwytow w danych: bez uchwytow, 4450 na czole" );
    }

    // wyzej na slupie: rolki i ciezary o tyle samo wyzej, lina ta sama
    {
        catenary::tensioner_input input;
        catenary::tensioner_result low, high;
        catenary::solve_tensioner( catalog, *device, input, low );
        input.height = 7.20;
        catenary::solve_tensioner( catalog, *device, input, high );
        check( std::abs( high.s - low.s - 0.50 ) < 1e-9 && std::abs( high.height - 7.20 ) < 1e-9 && std::abs( rope_length( high ) - rope_length( low ) ) < 1e-9, "rolki 0,5 m wyzej: ciezary 0,5 m wyzej, lina ta sama" );
    }

    // łańcuchy wycelowane w ostatnie podwieszenie: konce drutow na prostej od otworu dzwigni do podwieszenia
    {
        catenary::tensioner_input input;
        catenary::tensioner_result level, aimed;
        catenary::solve_tensioner( catalog, *device, input, level );
        input.aimed = true;
        double const messenger[ 3 ] { 60.0, 1.5, 8.2 }, contact[ 3 ] { 60.0, -0.3, 5.0 };
        std::copy( messenger, messenger + 3, input.messenger_target );
        std::copy( contact, contact + 3, input.contact_target );
        catenary::solve_tensioner( catalog, *device, input, aimed );
        auto const along = [ & ]( std::string const &Role, double const *Target ) {
            catenary::tensioner_placement const *first { nullptr };
            for( auto const &part : aimed.parts ) { if( part.role == Role ) { first = &part; } }
            if( first == nullptr ) { return false; }
            // model biegnie w -X: kolumna X rotacji = -kierunek łańcucha
            double d[ 3 ], l { 0.0 };
            for( int a = 0; a < 3; ++a ) { d[ a ] = Target[ a ] - first->position[ a ]; l += d[ a ] * d[ a ]; }
            l = std::sqrt( l );
            double dot { 0.0 };
            for( int a = 0; a < 3; ++a ) { dot += -first->rotation[ 3 * a ] * d[ a ] / l; }
            return dot > 0.99999;
        };
        check( along( "messenger.1", messenger ) && along( "contact.1", contact ), "lancuchy do drutow skierowane na ostatnie podwieszenie" );
        check( aimed.messenger_end[ 2 ] > level.messenger_end[ 2 ] && aimed.messenger_end[ 1 ] > level.messenger_end[ 1 ] && aimed.warnings.empty(), "lina nosna podchodzi w gore i w bok, bez uwag" );
        // dźwignia w równowadze: moment sił łańcuchów względem osi (wokół Y urządzenia) zero, uchwyt wzdłuż wypadkowej
        {
            catenary::tensioner_placement const *lever { nullptr }, *holder { nullptr }, *m { nullptr }, *c { nullptr };
            for( auto const &part : aimed.parts ) {
                if( part.role == "lever" ) { lever = &part; }
                if( part.role == "holder.1" ) { holder = &part; }
                if( part.role == "messenger.1" ) { m = &part; }
                if( part.role == "contact.1" ) { c = &part; }
            }
            // oś dźwigni = zero modelu przesunięte o otwór osi (model: X = -odsunięcie, Z = wzdłuż)
            auto const *lv { catalog.lever( "6406-01" ) };
            auto const *type { device->type( input.type ) };
            auto const &ah { lv->holes.at( type->axis_hole ) };
            double const local[ 3 ] { -ah.second, 0.0, ah.first };
            double axis[ 3 ], fm[ 3 ], fc[ 3 ], fsum[ 3 ];
            for( int a = 0; a < 3; ++a ) {
                axis[ a ] = lever->position[ a ] + lever->rotation[ 3 * a ] * local[ 0 ] + lever->rotation[ 3 * a + 2 ] * local[ 2 ];
                fm[ a ] = -m->rotation[ 3 * a ] * aimed.messenger_tension;
                fc[ a ] = -c->rotation[ 3 * a ] * aimed.contact_tension;
                fsum[ a ] = fm[ a ] + fc[ a ];
            }
            auto const moment_y = [ & ]( catenary::tensioner_placement const *At, double const *F ) {
                double const r[ 3 ] { At->position[ 0 ] - axis[ 0 ], 0.0, At->position[ 2 ] - axis[ 2 ] };
                return r[ 2 ] * F[ 0 ] - r[ 0 ] * F[ 2 ];
            };
            auto const moment { moment_y( m, fm ) + moment_y( c, fc ) };
            auto const scale { aimed.tension * 0.4 };
            // uchwyt biegnie w -X części: -kolumna X = kierunek od rolki do dźwigni
            double const hx { -holder->rotation[ 0 ] }, hz { -holder->rotation[ 6 ] };
            auto const fl { std::sqrt( fsum[ 0 ] * fsum[ 0 ] + fsum[ 2 ] * fsum[ 2 ] ) };
            auto const inline_ { ( hx * fsum[ 0 ] + hz * fsum[ 2 ] ) / fl };
            check( std::abs( moment ) < 1e-3 * scale && std::abs( inline_ ) > 0.99999, // uchwyt "~4440": odwrocony, znak bez znaczenia
                "dzwignia przechylona do drutow: moment " + std::to_string( moment ) + " daNm, uchwyt wzdluz wypadkowej, odchylenie "
                + std::to_string( aimed.lever_tilt * 180.0 / 3.14159265358979 ) + " st., wzdluz " + std::to_string( inline_ ) + ", skala " + std::to_string( scale ) );
            check( std::abs( level.lever_tilt ) < 1e-12, "bez celu dzwignia pionowo" );
        }
        // typowo: podwieszenie 50 m dalej, lina 7,20, przewód 5,60 nad główką - druty ciągną w dół, dźwignia u góry do konstrukcji
        for( auto const &type : device->types ) {
            catenary::tensioner_input typical { input };
            typical.type = type.code;
            double const tm[ 3 ] { 50.0, 0.0, 7.20 }, tc[ 3 ] { 50.0, 0.0, 5.60 };
            std::copy( tm, tm + 3, typical.messenger_target );
            std::copy( tc, tc + 3, typical.contact_target );
            catenary::tensioner_result result;
            catenary::solve_tensioner( catalog, *device, typical, result );
            std::printf( "      typ %d: dzwignia odchylona o %.2f st.\n", type.code, result.lever_tilt * 180.0 / 3.14159265358979 );
            check( result.lever_tilt > 0.0 && result.lever_tilt < 0.2 && result.warnings.empty(),
                "typ " + std::to_string( type.code ) + ": druty nizej niz dzwignia - dzwignia pochylona, mniej niz 11 st." );
        }
    }

    // lina jako bryla proceduralna z uchami: ucha w koncach liny, rura od ucha do ucha bez lacznikow, scianki na zewnatrz
    {
        catenary::tensioner_input input;
        catenary::tensioner_result result;
        catenary::solve_tensioner( catalog, *device, input, result );
        auto const first { result.rope.front() }, last { result.rope.back() };
        std::vector<catenary::tensioner_vertex> tube;
        catenary::tensioner_rope_solid( result, 0.006, 8, "4490-02", 0.205, tube );
        std::vector<catenary::tensioner_placement const *> eyes;
        for( auto const &part : result.parts ) { if( part.role.rfind( "rope_eye.", 0 ) == 0 ) { eyes.push_back( &part ); } }
        auto const tip = []( catenary::tensioner_placement const *Eye, int const A ) { return Eye->position[ A ] + 0.205 * Eye->rotation[ 3 * A + 2 ]; };
        auto const at = [ & ]( catenary::tensioner_placement const *Eye, std::vector<double> const &P ) {
            return std::abs( tip( Eye, 0 ) - P[ 0 ] ) < 1e-6 && std::abs( tip( Eye, 1 ) - P[ 1 ] ) < 1e-6 && std::abs( tip( Eye, 2 ) - P[ 2 ] ) < 1e-6; };
        check( eyes.size() == 2 && at( eyes[ 0 ], first ) && at( eyes[ 1 ], last ), "lina: uszy 4490-02 zaczepione w obu koncach liny" );
        // kazdy wierzcholek 6 mm od osi liny (punkt w polowie trojkata po normalnej do srodka trafia w poblize osi),
        // normalne trojkatow na zewnatrz: iloczyn krawedzi zgodny z normalnymi wierzcholkow
        int outward { 0 }, triangles { static_cast<int>( tube.size() / 3 ) };
        for( std::size_t k = 0; k + 2 < tube.size(); k += 3 ) {
            double e1[ 3 ], e2[ 3 ], n[ 3 ];
            for( int a = 0; a < 3; ++a ) { e1[ a ] = tube[ k + 1 ].position[ a ] - tube[ k ].position[ a ]; e2[ a ] = tube[ k + 2 ].position[ a ] - tube[ k ].position[ a ]; }
            n[ 0 ] = e1[ 1 ] * e2[ 2 ] - e1[ 2 ] * e2[ 1 ]; n[ 1 ] = e1[ 2 ] * e2[ 0 ] - e1[ 0 ] * e2[ 2 ]; n[ 2 ] = e1[ 0 ] * e2[ 1 ] - e1[ 1 ] * e2[ 0 ];
            double d { 0.0 };
            for( int a = 0; a < 3; ++a ) { d += n[ a ] * ( tube[ k ].normal[ a ] + tube[ k + 1 ].normal[ a ] + tube[ k + 2 ].normal[ a ] ); }
            if( d > 0.0 ) { ++outward; }
        }
        std::printf( "      rura liny: %d trojkatow\n", triangles );
        check( triangles > 0 && outward == triangles, "lina: rura proceduralna, wszystkie scianki na zewnatrz" );
        // pierscienie: na luku rolki co 5 stopni (polokrag = 36 odcinkow)
        check( result.rope.size() > 3 * 36, "lina: luki na rolkach co 5 stopni" );
    }

    // krazki rolek: osobno rowek A (model rolki) i B (4460-11), obrot o przewinieta line: 1, 2, 3, 4 razy przesuw rolki
    {
        catenary::tensioner_input input;
        catenary::tensioner_result warm, cold;
        catenary::solve_tensioner( catalog, *device, input, warm );
        input.temperature = -25.0;
        catenary::solve_tensioner( catalog, *device, input, cold );
        int sheaves { 0 };
        for( auto const &part : cold.parts ) { if( part.part == "4460-11" ) { ++sheaves; } }
        auto const radius { 0.5 * 0.2148 + 0.5 * 0.012 };
        auto const travel { cold.b - warm.b };
        char text[ 200 ];
        std::snprintf( text, sizeof( text ), "krazki: przy +10 °C bez obrotu, przy -25 °C rolka ruchoma %.2f m, krazek staly B %.2f obrotu", travel, cold.turn_fixed_b / ( 2.0 * 3.14159265358979 ) );
        check( sheaves == 2 && std::abs( warm.turn_fixed_b ) < 1e-9 && std::abs( cold.turn_moving_a - travel / radius ) < 1e-9
            && std::abs( cold.turn_fixed_a - 2.0 * cold.turn_moving_a ) < 1e-9 && std::abs( cold.turn_moving_b - 3.0 * cold.turn_moving_a ) < 1e-9
            && std::abs( cold.turn_fixed_b - 4.0 * cold.turn_moving_a ) < 1e-9, text );
    }

    // zabezpieczenie 4600: cztery ramiona, dolna śruba pod rolkami
    {
        catenary::tensioner_input input;
        input.temperature = -25.0;
        catenary::tensioner_result result;
        catenary::solve_tensioner( catalog, *device, input, result );
        int arms { 0 };
        for( auto const &part : result.parts ) { if( part.role.rfind( "safety", 0 ) == 0 ) { ++arms; } }
        check( arms == 4 && result.warnings.empty(), "zabezpieczenie 4600: dwie pary ramion, tez przy -25 °C (rolki najdalej)" );
    }

    std::printf( failures == 0 ? "wszystko OK\n" : "BLEDOW: %d\n", failures );
    return failures == 0 ? 0 : 1;
}
