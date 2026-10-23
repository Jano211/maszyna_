// Test urządzenia naprężającego: dane z tests/catenary/data/traction_tensioners.txt, naciągi z plików traction_<symbol>.txt.
// Uruchamiać z katalogu repozytorium: tests/catenary/tt tests/catenary/data/
#include "world/catenary_tensioner.h"

#include <cmath>
#include <cstdio>
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

    // lina z uchami: ucho na obu końcach z zaczepem w skrajnym punkcie liny, odcinki liny bez uch, ta sama długość razem
    {
        catenary::tensioner_input input;
        catenary::tensioner_result result;
        catenary::solve_tensioner( catalog, *device, input, result );
        auto const first { result.rope.front() }, last { result.rope.back() };
        catenary::tensioner_rope_parts( result, "4490-01", 1.0, 1.0, "4490-02", 0.15 );
        std::vector<catenary::tensioner_placement const *> eyes;
        double stretched { 0.0 };
        for( auto const &part : result.parts ) {
            if( part.role.rfind( "rope_eye.", 0 ) == 0 ) { eyes.push_back( &part ); }
            else if( part.role.rfind( "rope.", 0 ) == 0 ) { stretched += part.scale[ 2 ]; }
        }
        auto const tip = []( catenary::tensioner_placement const *Eye, int const A ) { return Eye->position[ A ] + 0.15 * Eye->rotation[ 3 * A + 2 ]; };
        auto const at = [ & ]( catenary::tensioner_placement const *Eye, std::vector<double> const &P ) {
            return std::abs( tip( Eye, 0 ) - P[ 0 ] ) < 1e-6 && std::abs( tip( Eye, 1 ) - P[ 1 ] ) < 1e-6 && std::abs( tip( Eye, 2 ) - P[ 2 ] ) < 1e-6; };
        check( eyes.size() == 2 && at( eyes[ 0 ], first ) && at( eyes[ 1 ], last ), "lina: uszy 4490-02 zaczepione w obu koncach liny" );
        check( std::abs( stretched + 0.30 - rope_length( result ) ) < 1e-6, "lina: odcinki 4490-01 krotsze o dwa uszy" );
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
