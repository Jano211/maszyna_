// Test world/catenary_span.cpp na przęsłach całej scenerii, z atrapą klas symulatora (test/mock).
// wejście: wiersze "typ|- odstęp_wieszaków x1 y1 z1 x2 y2 z2 x3 y3 z3 x4 y4 z4"; argument: katalog danych
#include "stdafx.h"
#include "world/Traction.h"
#include "world/catenary_span.h"
#include "world/catenary_network.h"
#include "rendering/renderer.h"
#include "utilities/Globals.h"
#include "utilities/Logs.h"

#include <cstdio>
#include <iostream>
#include <map>
#include <tuple>

mock_renderer Renderer, *GfxRenderer { &Renderer };
mock_globals Global;
int errorcount { 0 };
void ErrorLog( std::string const &Message ) { ++errorcount; std::printf( "LOG %s\n", Message.c_str() ); }

int main( int argc, char *argv[] ) {
    if( argc < 2 ) { return 2; }
    catenary::types().directory( argv[ 1 ] );
    std::vector<TTraction> spans;
    std::vector<std::string> symbols;
    std::string type; double hangerstep, p[ 12 ];
    while( std::cin >> type >> hangerstep ) {
        for( auto &value : p ) { std::cin >> value; }
        TTraction span;
        span.pPoint1 = { p[0], p[1], p[2] }; span.pPoint2 = { p[3], p[4], p[5] }; span.pPoint3 = { p[6], p[7], p[8] }; span.pPoint4 = { p[9], p[10], p[11] };
        span.iNumSections = ( hangerstep > 0.0 ? 10 : 0 );
        spans.push_back( span );
        symbols.push_back( type == "-" ? "" : type );
    }
    for( std::size_t index = 0; index < spans.size(); ++index ) { catenary_span_assign( spans[ index ], symbols[ index ] ); }
    // łączenie końców jak w InitTraction: pierwszy wolny koniec innego przęsła w tym samym punkcie
    auto const near = []( glm::dvec3 const &a, glm::dvec3 const &b ) { return std::abs( a.x - b.x ) < 0.025 && std::abs( a.y - b.y ) < 0.025 && std::abs( a.z - b.z ) < 0.025; };
    for( auto &span : spans ) {
        for( int end = 0; end < 2; ++end ) {
            if( span.hvNext[ end ] ) { continue; }
            auto const &point { end == 0 ? span.pPoint1 : span.pPoint2 };
            for( auto &other : spans ) {
                if( &other == &span || span.hvNext[ end ] ) { continue; }
                for( int otherend = 0; otherend < 2; ++otherend ) {
                    if( other.hvNext[ otherend ] == nullptr && near( point, otherend == 0 ? other.pPoint1 : other.pPoint2 ) ) {
                        span.hvNext[ end ] = &other; span.iNext[ end ] = otherend; other.hvNext[ otherend ] = &span; other.iNext[ otherend ] = end; break;
                    }
                }
            }
        }
    }
    // linie: koniec odcinka występujący raz w całej sieci, wysoko nad przewodem jezdnym i poza końcem liny nośnej, wisi w powietrzu
    std::map<std::tuple<long, long, long>, int> degree;
    auto const key = []( glm::vec3 const &v ) { return std::make_tuple( std::lround( v.x * 200 ), std::lround( v.y * 200 ), std::lround( v.z * 200 ) ); };
    std::vector<gfx::vertex_array> lines( spans.size() );
    std::size_t typed { 0 }, segments { 0 };
    for( std::size_t index = 0; index < spans.size(); ++index ) {
        if( false == catenary_span_lines( spans[ index ], lines[ index ] ) ) { continue; }
        ++typed;
        for( auto const &vertex : lines[ index ] ) { ++degree[ key( vertex.position ) ]; ++segments; }
    }
    int dangling { 0 };
    for( std::size_t index = 0; index < spans.size(); ++index ) {
        auto const &span { spans[ index ] };
        for( auto const &vertex : lines[ index ] ) {
            auto const &v { vertex.position };
            if( degree[ key( v ) ] != 1 ) { continue; }
            // koniec liny nośnej albo dolnej liny tuż pod nim (do 6 cm) to koniec sieci, nie błąd
            auto const atropeend = [ & ]( glm::dvec3 const &Point ) { return std::hypot( v.x - Point.x, v.z - Point.z ) < 0.01 && v.y <= Point.y + 0.01 && v.y >= Point.y - 0.06; };
            if( v.y - span.pPoint1.y > 0.2 && !atropeend( span.pPoint3 ) && !atropeend( span.pPoint4 ) ) { ++dangling; std::printf( "  koniec w powietrzu: %.2f %.2f %.2f\n", v.x, v.y, v.z ); }
        }
    }
    std::printf( "przesel %zu, z typem policzonych %zu, odcinkow %zu, koncow w powietrzu %d, bledow w logu %d\n", spans.size(), typed, segments / 2, dangling, errorcount );

    // bryły: kamera jedzie wzdłuż sieci 3 m pod przewodem; bryły mają powstawać blisko i być zwalniane daleko
    CatenarySettings.solid_range = 30.f;
    bool solidok { true };
    std::size_t peak { 0 }, peakspans { 0 }, largest { 0 };
    for( std::size_t step = 0; step < spans.size(); ++step ) {
        auto const &at { spans[ step ] };
        glm::dvec3 const camera { 0.5 * ( at.pPoint1.x + at.pPoint2.x ), at.pPoint1.y - 3.0, 0.5 * ( at.pPoint1.z + at.pPoint2.z ) };
        std::size_t loaded { 0 };
        for( auto &span : spans ) {
            auto const solid { catenary_span_solid( span, camera ) };
            if( solid.wires == null_handle ) { continue; }
            ++loaded;
            std::size_t count { 0 };
            for( auto const &handle : { solid.wires, solid.fittings } ) {
                if( handle == null_handle ) { continue; }
                auto const &vertices { Renderer.Vertices( handle ) };
                count += vertices.size();
                if( vertices.empty() || vertices.size() % 3 != 0 ) { solidok = false; }
                for( auto const &vertex : vertices ) {
                    auto const &n { vertex.normal };
                    solidok = solidok && std::isfinite( vertex.position.x + vertex.position.y + vertex.position.z ) && std::abs( n.x * n.x + n.y * n.y + n.z * n.z - 1.f ) < 1e-3f;
                }
            }
            largest = std::max( largest, count );
        }
        peak = std::max( peak, Renderer.stored() );
        peakspans = std::max( peakspans, loaded );
    }
    solidok = solidok && Renderer.inserts > 0 && Renderer.releases > 0 && peakspans <= 8;
    // przybliżenie kamery wydłuża zasięg, zasięg 0 wyłącza bryły i zwalnia już utworzone
    {
        auto &probe { spans[ spans.size() / 2 ] };
        glm::dvec3 const far { probe.pPoint1.x + 80.0, probe.pPoint1.y, probe.pPoint1.z };
        auto const plain { catenary_span_solid( probe, far ).wires != null_handle };
        Global.ZoomFactor = 4.f;
        auto const zoomed { catenary_span_solid( probe, far ).wires != null_handle };
        Global.ZoomFactor = 1.f;
        CatenarySettings.solid_range = 0.f;
        auto const disabled { catenary_span_solid( probe, far ).wires != null_handle };
        solidok = solidok && false == plain && zoomed && false == disabled;
        std::printf( "bryly z 80 m: bez przyblizenia %s, z przyblizeniem 4x %s, przy zasiegu 0 %s\n", plain ? "sa" : "brak", zoomed ? "sa" : "brak", disabled ? "sa" : "brak" );
    }
    std::printf( "bryly: utworzen %d, zwolnien %d, naraz najwyzej %zu przesel i %zu wierzcholkow, najwieksze przeslo %zu trojkatow  %s\n",
        Renderer.inserts, Renderer.releases, peakspans, peak, largest / 3, solidok ? "OK" : "BLAD" );
    // wygląd metalu: wartości z pliku data/traction_materials.txt (katalog bieżący), stan i materiał z wpisu przęsła
    bool metalok { true };
    {
        TTraction span;
        auto const &newcopper { catenary_span_metal( span, false ) };
        span.DamageFlag = 2; auto const &worncopper { catenary_span_metal( span, false ) };
        span.DamageFlag = 1; auto const &tarnishedbrass { catenary_span_metal( span, true ) };
        span.DamageFlag = 3; auto const &both { catenary_span_metal( span, false ) };
        span.Material = 2; span.DamageFlag = 0; auto const &aluminium { catenary_span_metal( span, false ) };
        metalok = std::abs( newcopper.color[ 0 ] - 0.80f ) < 1e-6f && newcopper.color[ 0 ] > worncopper.color[ 0 ] && worncopper.gloss < newcopper.gloss
               && tarnishedbrass.color[ 1 ] > tarnishedbrass.color[ 0 ] && both.color[ 1 ] > both.color[ 0 ] && std::abs( aluminium.color[ 0 ] - aluminium.color[ 1 ] ) < 0.02f;
        std::printf( "wyglad metalu wg materialu i stanu: %s\n", metalok ? "OK" : "BLAD" );
    }
    return ( dangling == 0 && errorcount == 0 && solidok && metalok ) ? 0 : 1;
}
