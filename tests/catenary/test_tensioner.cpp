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

    // tablica: wartosci nominalne i z rogow
    {
        auto const *table { catalog.table( "0011-1" ) };
        double b, s;
        table->lookup( 10.0, 400.0, b, s );
        check( std::abs( b - 1.75 ) < 1e-9 && std::abs( s - 2.25 ) < 1e-9, "tablica: +10 °C -> b 1,75, s 2,25" );
        table->lookup( -25.0, 750.0, b, s );
        check( std::abs( b - 2.20 ) < 1e-9 && std::abs( s - 4.05 ) < 1e-9, "tablica: -25 °C, 750 m -> b 2,20, s 4,05" );
        table->lookup( 12.5, 725.0, b, s );
        check( std::abs( b - ( 1.75 + 1.69 + 1.75 + 1.69 ) / 4.0 ) < 1e-9, "tablica: interpolacja dwuliniowa" );
        check( false == table->lookup( 45.0, 800.0, b, s ) && std::abs( s - 0.73 ) < 1e-9, "tablica: poza zakresem przyciete do brzegu" );
        // spojnosc b i s z przekladnia 1:4 (s - 2,25 = 4 (b - 1,75), zaokraglenia do 1 cm)
        int odd { 0 };
        for( std::size_t r = 0; r < table->distances.size(); ++r ) {
            for( std::size_t c = 0; c < table->temperatures.size(); ++c ) {
                auto const miss { ( table->s[ r ][ c ] - 2.25 ) - 4.0 * ( table->b[ r ][ c ] - 1.75 ) };
                if( std::abs( miss ) > 0.045 ) {
                    ++odd;
                    std::printf( "      niespojne: %g m, %+g °C: b %.2f s %.2f (roznica %.3f)\n", table->distances[ r ], table->temperatures[ c ], table->b[ r ][ c ], table->s[ r ][ c ], miss );
                }
            }
        }
        check( odd <= 3, "tablica: b i s zgodne z przekladnia 1:4 (trzy literowki karty)" );
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

    std::printf( failures == 0 ? "wszystko OK\n" : "BLEDOW: %d\n", failures );
    return failures == 0 ? 0 : 1;
}
