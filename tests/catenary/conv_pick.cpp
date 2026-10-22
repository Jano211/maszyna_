// Konwersja starej sieci (Rainsted): dobór karty, wykonania i hk dla podwieszeń ze słupem, złożenie solverem.
// Wejście (stdin, jedna linia na podwieszenie):
//   <nazwa> <słup> <px> <py> <pz> <ax> <ay> <az> <hk_stare> <łuk 0|1> <do_słupa 0|1> <przewody 1|2> <grupa> <przesunięcie_max>
//   p = oś słupa na wysokości główki szyny, a = oś przewodów jezdnych przy podwieszeniu
// Wyjście (stdout): <nazwa> <karta> <wykonanie> <lina_y> <A> <liczba uwag> <px> <pz> | uwagi  albo  <nazwa> 0 0 0 <A> -1 0 0 | powód
// Słup może odsunąć się od toru do <przesunięcie_max> (co 2,5 cm), gdy w miejscu ze starej scenerii wysięgnik ma uwagi
#include "stdafx.h"
#include "world/catenary_scene.h"
#include "world/catenary_catalog.h"
#include "world/catenary_network.h"
#include "simulation/simulation.h"
#include "rendering/renderer.h"
#include "utilities/Globals.h"
#include "utilities/Logs.h"
#include <cmath>
#include <cstdio>
#include <iostream>
#include <sstream>

mock_renderer Renderer, *GfxRenderer { &Renderer };
mock_globals Global;
namespace simulation { mock_table<TTraction> Traction; mock_table<TAnimModel> Instances; mock_state State; }
std::vector<std::string> errors, logs;
void ErrorLog( std::string const &Message ) { errors.push_back( Message ); }
void WriteLog( const char *Message ) { logs.emplace_back( Message ); }

int main( int, char *argv[] ) {
    Global.asCurrentSceneryPath = "tests/catenary/";
    catenary_scene_data( argv[ 1 ] );
    catenary::types().directory( argv[ 1 ] );
    catenary_scene_clear(); catenary_scene_resolve();
    auto const *catalog { catenary_scene_catalog() };
    auto const &structures { catenary_scene_structures() };
    std::string line;
    while( std::getline( std::cin, line ) ) {
        std::istringstream in { line };
        std::string name, pole, group; double p0[ 3 ], a[ 3 ], hkold, shiftmax; int curve, toward, wires;
        if( !( in >> name >> pole >> p0[ 0 ] >> p0[ 1 ] >> p0[ 2 ] >> a[ 0 ] >> a[ 1 ] >> a[ 2 ] >> hkold >> curve >> toward >> wires >> group >> shiftmax ) ) { continue; }
        auto const *polet { catalog->pole( pole ) };
        if( polet == nullptr ) { std::printf( "%s 0 0 0 0 -1 0 0 | no pole\n", name.c_str() ); continue; }
        // odsunięcie słupa od drutu w planie
        auto const dx0 { p0[ 0 ] - a[ 0 ] }, dz0 { p0[ 2 ] - a[ 2 ] };
        auto const len0 { std::max( 1e-6, std::hypot( dx0, dz0 ) ) };
        std::string best; int bestcount { 1 << 20 };
        for( double shift = 0.0; shift <= shiftmax + 1e-9; shift += 0.025 ) {
        double p[ 3 ] { p0[ 0 ] + dx0 / len0 * shift, p0[ 1 ], p0[ 2 ] + dz0 / len0 * shift };
        auto const height { a[ 1 ] - p[ 1 ] };
        auto const reach { std::hypot( a[ 0 ] - p[ 0 ], a[ 2 ] - p[ 2 ] ) - 0.5 * polet->depth( height ) };
        // łapki lekkie, gdzie się da: dwa przewody wyk. 1 (potem 2), jeden przewód wyk. 5 (potem 3, 4)
        std::vector<int> preferred { wires == 2 ? std::vector<int> { 1, 2 } : std::vector<int> { 5, 3, 4 } };
        catenary::catalog_pick chosen;
        for( auto const execution : preferred ) {
            auto const pick { catenary::pick_card( structures, *catalog, group, "", 0, curve == 1, toward == 1, execution, reach ) };
            if( pick.scheme != nullptr && false == pick.execution_changed ) { chosen = pick; break; }
            if( pick.scheme != nullptr && chosen.scheme == nullptr ) { chosen = pick; }
        }
        if( chosen.scheme == nullptr ) { if( best.empty() ) { char t[ 128 ]; std::snprintf( t, sizeof( t ), "%s 0 0 0 %.3f -1 0 0 | no card for A", name.c_str(), reach ); best = t; } continue; }
        // hk z karty, chyba że stara bardzo odbiega (> 0,15 m)
        auto const hk { std::abs( hkold - chosen.scheme->height ) > 0.15 ? hkold : chosen.scheme->height };
        catenary::suspension data;
        data.card = chosen.scheme->card; data.execution = chosen.scheme->execution;
        data.has_at = true; for( int k = 0; k < 3; ++k ) { data.at[ k ] = a[ k ]; }
        data.has_rope = true; data.rope[ 0 ] = a[ 0 ]; data.rope[ 1 ] = a[ 1 ] + hk; data.rope[ 2 ] = a[ 2 ];
        data.has_pole = true; data.pole_type = pole; for( int k = 0; k < 3; ++k ) { data.pole[ k ] = p[ k ]; }
        catenary::cantilever_result result;
        auto const ok { catenary_scene_probe( data, result ) };
        std::string notes;
        for( auto const &warning : result.warnings ) { notes += ( notes.empty() ? "" : " ;; " ) + warning; }
        auto const count { ok ? static_cast<int>( result.warnings.size() ) : 1000 };
        if( count < bestcount ) {
            bestcount = count;
            char t[ 64 ];
            std::snprintf( t, sizeof( t ), " %.3f %.3f", p[ 0 ], p[ 2 ] );
            best = name + " " + std::to_string( data.card ) + " " + std::to_string( data.execution ) + " " + std::to_string( data.rope[ 1 ] ) + " " + std::to_string( reach ) + " " + std::to_string( ok ? count : -1 ) + t + " | " + notes;
        }
        if( count == 0 ) { break; }
        }
        std::printf( "%s\n", best.c_str() );
    }
}
