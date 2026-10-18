#include "world/catenary_cantilever.h"
#include <cstdio>
#include <map>
using namespace catenary;
int main( int, char *argv[] ) {
    cantilever_catalog cat; std::vector<std::string> err;
    cat.load( std::string( argv[ 1 ] ) + "/traction_cantilevers.txt", err );
    std::map<int,double> raise { {103,0.76},{104,0.76},{101,1.40},{11,0.10},{10,0.10},{12,0.10},{13,0.10},{14,0.10} };
    auto const *pole { cat.pole( "1701" ) };
    for( auto const &e : cat.schemes() ) {
        auto const &s { e.second };
        int g = s.card / 10;
        double rail = 0.2 + ( raise.count( g ) ? raise[ g ] : 0.0 );
        double a = 0.5 * ( s.reach_min + s.reach_max );
        cantilever_result r;
        solve_cantilever_on_pole( cat, s, *pole, a + 0.112, -( 5.8 - rail ), { 0.0, s.height }, {}, r );
        std::string w;
        for( auto const &x : r.warnings ) { auto p = x.find( ": " ); std::string t = ( p == std::string::npos ? x : x.substr( p + 2 ) ); if( !w.empty() ) w += " | "; w += t; }
        std::printf( "%d;%d;;;;;;;;;;%s\n", s.card, s.execution, w.c_str() );
    }
}
