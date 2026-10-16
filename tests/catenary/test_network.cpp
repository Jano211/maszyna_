// Test danych i geometrii: typy czytane z plików jak w symulatorze, porównanie z tablicami długości wieszaków z katalogu.
#include "world/catenary_network.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

int main( int argc, char *argv[] ) {
    auto &registry { catenary::types() };
    registry.directory( argc > 1 ? argv[ 1 ] : "tests/catenary/data" );
    int errors { 0 };
    registry.on_error = [ & ]( std::string const &Message ) { std::printf( "  LOG: %s\n", Message.c_str() ); ++errors; };

    std::map<int, std::map<int, std::vector<double>>> lengths;
    {
        std::ifstream file { argc > 2 ? argv[ 2 ] : "tests/catenary/katalog_dlugosci.txt" };
        if( !file ) { std::cerr << "brak pliku z tablicami dlugosci\n"; return 2; }
        std::string line;
        while( std::getline( file, line ) ) {
            if( line.empty() || line[ 0 ] == '#' ) { continue; }
            std::istringstream stream { line };
            std::string kind; int code, span; stream >> kind >> code >> span;
            std::vector<double> values; double value;
            while( stream >> value ) { values.push_back( value ); }
            lengths[ code ][ span ] = values;
        }
    }
    struct entry { char const *symbol; int code; double tolerance; };
    entry const entries[] { { "C120-2C", 3, 1.5 }, { "C95-C", 10, 1.5 }, { "YC120-2C150", 20, 1.5 }, { "YwsC120-2C", 26, 1.5 }, { "YwsC120-2C-M", 30, 1.5 },
                            { "2C120-2C-3", 32, 3.0 }, { "2C120-2C-4", 35, 1.5 }, { "YC150-2CS150", 36, 1.5 }, { "YC120-2CS150", 37, 1.5 } };
    bool failed { false };
    long total { 0 };
    for( auto const &item : entries ) {
        auto const *definition { registry.find( item.symbol ) };
        if( definition == nullptr ) { std::printf( "%s: typ nie wczytany\n", item.symbol ); failed = true; continue; }
        if( definition->type.code != item.code ) { std::printf( "%s: zly kod\n", item.symbol ); failed = true; }
        double sumsquares { 0.0 }, worst { 0.0 }; long count { 0 };
        for( auto const &row : lengths[ item.code ] ) {
            auto const span { static_cast<double>( row.first ) };
            auto const gaps { registry.hanger_gaps( definition->hanger_table, span ) };
            // przęsło poziome wzdłuż osi X, lina na wysokości konstrukcyjnej typu
            auto const hk { definition->type.structure_height };
            catenary::span_points points { { 0, 5, 0 }, { span, 5, 0 }, { 0, 5 + hk, 0 }, { span, 5 + hk, 0 } };
            std::vector<catenary::vec3> lines; std::string error;
            if( false == catenary::build_span_lines( *definition, gaps, points, 0.04, lines, error ) ) { std::printf( "%s %d m: %s\n", item.symbol, row.first, error.c_str() ); failed = true; continue; }
            // wieszaki to odcinki pionowe kończące się na przewodzie jezdnym: górny punkt, potem punkt na przewodzie
            auto const hangers { row.second.size() };
            std::vector<catenary::vec3> hangerlines;
            for( std::size_t index = 0; index + 1 < lines.size(); index += 2 ) {
                if( std::abs( lines[ index + 1 ].y - 5.0 ) < 1e-9 && lines[ index ].y > 5.001 && std::abs( lines[ index ].x - lines[ index + 1 ].x ) < 1e-9 ) {
                    hangerlines.push_back( lines[ index ] ); hangerlines.push_back( lines[ index + 1 ] );
                }
            }
            if( gaps.size() != hangers + 1 || hangerlines.size() != 2 * hangers ) { std::printf( "%s %d m: inna liczba wieszakow\n", item.symbol, row.first ); failed = true; continue; }
            for( std::size_t index = 0; index < hangers; ++index ) {
                if( row.second[ index ] <= 0.0 ) { continue; }
                auto const &top { hangerlines[ 2 * index ] };
                auto const &bottom { hangerlines[ 2 * index + 1 ] };
                // w tablicach długość wieszaka linki Y jest pomniejszona o korektę typu
                auto const iny { definition->type.y_hanger_drop > 0.0 && ( top.x < 0.5 * definition->type.y_length( span ) || top.x > span - 0.5 * definition->type.y_length( span ) ) };
                auto const length { top.y - bottom.y - ( iny ? definition->type.y_hanger_drop : 0.0 ) };
                auto const difference { std::round( length * 100.0 ) - row.second[ index ] };
                sumsquares += difference * difference; ++count;
                if( std::abs( difference ) > std::abs( worst ) ) { worst = difference; }
                if( std::abs( bottom.y - 5.0 ) > 1e-9 || std::abs( std::abs( bottom.z ) - ( definition->type.contact_count > 1 ? 0.04 : 0.0 ) ) > 1e-9 ) { std::printf( "%s %d m: wieszak nie konczy sie na przewodzie\n", item.symbol, row.first ); failed = true; }
            }
        }
        auto const rms { std::sqrt( sumsquares / count ) };
        std::printf( "kod %2d %-13s wartosci %4ld  blad sr.kw. %.2f cm  najwiekszy %+.0f cm  %s\n", item.code, item.symbol, count, rms, worst, rms <= item.tolerance ? "OK" : "ZA DUZO" );
        failed = failed || rms > item.tolerance;
        total += count;
    }
    std::printf( "razem porownano %ld wartosci, komunikatow o bledach danych: %d\n", total, errors );
    failed = failed || errors > 0;

    // rozpiętości niecałkowite i spoza tablic; przęsło obrócone w poziomie, ze spadkiem i z liną przesuniętą w bok
    {
        auto const *definition { registry.find( "YwsC120-2C" ) };
        for( double const span : { 3.0, 8.0, 17.3, 23.4, 24.5, 41.6, 42.4, 63.4, 69.5, 70.49, 73.0, 95.0 } ) {
            auto const gaps { registry.hanger_gaps( definition->hanger_table, span ) };
            double sum { 0.0 }, largest { 0.0 };
            for( auto const gap : gaps ) { sum += gap; largest = std::max( largest, gap ); }
            auto const dx { span * 0.6 }, dz { span * 0.8 };
            catenary::span_points points { { 100, 5.6, 200 }, { 100 + dx, 5.2, 200 + dz }, { 100.2, 7.3, 200 }, { 100 + dx - 0.2, 6.6, 200 + dz } };
            std::vector<catenary::vec3> lines; std::string error;
            auto const built { catenary::build_span_lines( *definition, gaps, points, 0.04, lines, error ) };
            bool finite { true };
            for( auto const &point : lines ) { finite = finite && std::isfinite( point.x + point.y + point.z ); }
            // 95 m przy tej wysokości liny to przęsło niewykonalne: ma zostać odrzucone z opisem, a nie narysowane
            auto const expected { span < 90.0 };
            auto const good { built == expected && finite && std::abs( sum - span ) < 1e-6 && largest < 4.6 && ( built || false == error.empty() ) };
            std::printf( "przeslo %6.2f m: wieszakow %2zu, najwiekszy odstep %.2f m, odcinkow %3zu  %s %s\n",
                span, gaps.size() - 1, largest, built ? lines.size() / 2 : 0, good ? "OK" : "BLAD", error.c_str() );
            failed = failed || !good;
        }
    }
    // linka Y na styku przęseł: połówki z dwóch przęseł mają się spotkać w osi podwieszenia
    {
        auto const *definition { registry.find( "YwsC120-2C" ) };
        auto const solve = [ & ]( double const Span, bool const Ystart, bool const Yend ) {
            catenary::span_points points { { 0, 5, 0 }, { Span, 5, 0 }, { 0, 6.7, 0 }, { Span, 6.7, 0 } };
            catenary::span_solution solution; std::string error;
            if( false == catenary::solve_span_points( *definition, registry.hanger_gaps( definition->hanger_table, Span ), points, Ystart, Yend, solution, error ) ) { std::printf( "blad: %s\n", error.c_str() ); }
            return solution;
        };
        // dwa jednakowe przęsła: uzgodnienie nie może niczego zmienić
        {
            auto left { solve( 61, true, true ) }; auto const right { solve( 61, true, true ) };
            auto const before { left.result };
            catenary::join_y( *definition, left, true, right.result.y_half_start );
            double change { 0.0 };
            for( std::size_t index = 0; index < before.hangers.size(); ++index ) { change = std::max( change, std::abs( before.hangers[ index ].length - left.result.hangers[ index ].length ) ); }
            change = std::max( change, std::abs( before.y_end.back().y - left.result.y_end.back().y ) );
            std::printf( "Y, przesla 61+61 m: zmiana po uzgodnieniu %.6f m  %s\n", change, change < 1e-9 ? "OK" : "BLAD" );
            failed = failed || change >= 1e-9;
        }
        // różne przęsła: osobno liczone połówki się rozjeżdżają, po uzgodnieniu spotykają się w osi
        {
            auto left { solve( 61, true, true ) }; auto right { solve( 70, true, true ) };
            auto const gapbefore { left.result.y_end.back().y - right.result.y_start.front().y };
            auto const lefthalf { left.result.y_half_end }, righthalf { right.result.y_half_start };
            catenary::join_y( *definition, left, true, righthalf );
            catenary::join_y( *definition, right, false, lefthalf );
            auto const gapafter { left.result.y_end.back().y - right.result.y_start.front().y };
            auto const ends { std::abs( left.result.y_end.front().y - lefthalf.junction ) + std::abs( right.result.y_start.back().y - righthalf.junction ) };
            auto const good { std::abs( gapafter ) < 1e-9 && ends < 1e-9 && std::abs( left.result.y_end.back().x - 61 ) < 1e-9 && std::abs( right.result.y_start.front().x ) < 1e-9 };
            std::printf( "Y, przesla 61+70 m: rozjazd w osi przed %.3f m, po %.6f m, konce na linie nosnej  %s\n", gapbefore, gapafter, good ? "OK" : "BLAD" );
            failed = failed || !good;
        }
        // przęsło bez Y przy jednym podwieszeniu: tam wieszaki wiszą na linie nośnej, a linki nie ma
        {
            auto const span { solve( 61, false, true ) };
            auto const &result { span.result };
            bool good { result.y_start.empty() && false == result.y_end.empty() && result.y_half_start.reach == 0.0 && result.y_half_end.reach > 0.0 };
            for( auto const &hanger : result.hangers ) {
                if( hanger.x < 8.0 ) { good = good && false == hanger.on_y && hanger.length > 1.2; }
                if( hanger.x > 53.0 ) { good = good && hanger.on_y; }
            }
            catenary::span_points points { { 0, 5, 0 }, { 61, 5, 0 }, { 0, 6.7, 0 }, { 61, 6.7, 0 } };
            std::vector<catenary::vec3> lines;
            catenary::build_lines( *definition, span, points, 0.04, lines );
            std::printf( "Y tylko przy jednym podwieszeniu: %s (odcinkow %zu)\n", good ? "OK" : "BLAD", lines.size() / 2 );
            failed = failed || !good;
        }
        // sieć z dwiema linami, bez strefy przy podwieszeniach: dolna lina na całej długości pod górną
        {
            auto const *twin { registry.find( "2C120-2C-4" ) };
            catenary::span_points points { { 0, 5, 0 }, { 65, 5, 0 }, { 0, 6.7, 0 }, { 65, 6.7, 0 } };
            catenary::span_solution solution; std::string error;
            auto good { catenary::solve_span_points( *twin, registry.hanger_gaps( twin->hanger_table, 65 ), points, false, false, solution, error ) };
            std::vector<catenary::vec3> lines;
            if( good ) { catenary::build_lines( *twin, solution, points, 0.04, lines ); }
            // odcinki: 2 przewody jezdne, górna lina, dolna lina o tej samej liczbie odcinków, wieszaki i tyle samo łączników między linami
            auto const ropes { solution.result.messenger.size() - 1 };
            good = good && lines.size() / 2 == 2 + 2 * ropes + 2 * solution.result.hangers.size();
            std::printf( "dwie liny bez strefy Y: %s\n", good ? "OK" : "BLAD" );
            failed = failed || !good;
        }
    }
    // linka Y w planie: na prostej zostaje pod liną, na łuku przechodzi łagodnym łukiem obok załomu; z obu przęseł wychodzi to samo
    {
        auto const *definition { registry.find( "YwsC120-2C" ) };
        for( double const angle : { 0.0, 0.1 } ) { // załom osi sieci przy podwieszeniu [rad]; 0,1 rad to przęsła 61 m na łuku ok. 600 m
            auto const lx { -61.0 }, rx { 61.0 * std::cos( angle ) }, rz { 61.0 * std::sin( angle ) };
            catenary::span_points const leftpoints { { lx, 5, 0 }, { 0, 5, 0 }, { lx, 6.7, 0 }, { 0, 6.7, 0 } };
            catenary::span_points const rightpoints { { 0, 5, 0 }, { rx, 5, rz }, { 0, 6.7, 0 }, { rx, 6.7, rz } };
            catenary::span_solution left, right; std::string error;
            auto const gaps { registry.hanger_gaps( definition->hanger_table, 61 ) };
            catenary::solve_span_points( *definition, gaps, leftpoints, true, true, left, error );
            catenary::solve_span_points( *definition, gaps, rightpoints, true, true, right, error );
            auto const leftside { catenary::make_y_side( left, leftpoints, true ) };
            auto const rightside { catenary::make_y_side( right, rightpoints, false ) };
            auto const leftplan { catenary::solve_y_plan( *definition, leftside, rightside ) };
            auto const rightplan { catenary::solve_y_plan( *definition, rightside, leftside ) };
            double mismatch { 0.0 };
            for( double const distance : { 0.0, 1.0, 5.0, 8.0 } ) {
                // ten sam punkt linki widziany z obu przęseł: po lewej "distance" w głąb, po prawej to ujemna odległość
                auto const a { leftplan.point( -distance ) }, b { rightplan.point( distance ) };
                mismatch = std::max( mismatch, std::hypot( a.x - b.x, a.z - b.z ) );
            }
            auto const axis { leftplan.point( 0.0 ) };
            auto const shift { std::hypot( axis.x, axis.z ) };
            auto const chord { std::hypot( 0.5 * ( leftside.junction.x + rightside.junction.x ), 0.5 * ( leftside.junction.z + rightside.junction.z ) ) };
            auto const clamp { leftplan.point( leftside.reach ) };
            auto const atclamp { std::hypot( clamp.x - leftside.junction.x, clamp.z - leftside.junction.z ) };
            std::vector<catenary::vec3> lines;
            catenary::build_lines( *definition, left, leftpoints, 0.04, lines, {}, leftplan );
            bool finite { true };
            for( auto const &point : lines ) { finite = finite && std::isfinite( point.x + point.y + point.z ); }
            auto const good { leftplan.valid && rightplan.valid && finite && mismatch < 1e-9 && atclamp < 1e-9
                           && ( angle == 0.0 ? shift < 1e-9 : std::abs( shift - 0.5 * chord ) < 1e-6 ) };
            std::printf( "Y w planie, zalom %.2f rad: linka %.3f m od osi podwieszenia (cieciwa %.3f m), roznica miedzy przeslami %.9f m, odcinkow %zu  %s\n", angle, shift, chord, mismatch, lines.size() / 2, good ? "OK" : "BLAD" );
            failed = failed || !good;
        }
    }
    // odcinek do kotwienia: bez wieszaków, liny i przewody jezdne wiszą pod własnym ciężarem
    {
        auto const *definition { registry.find( "2C120-2C-4" ) };
        catenary::span_points points { { 0, 5, 0 }, { 40, 5, 0 }, { 0, 6.7, 0 }, { 40, 5.82, 0 } };
        catenary::span_solution solution; std::string error;
        auto good { catenary::solve_span_points( *definition, {}, points, false, false, solution, error ) };
        catenary::span_geometry geometry;
        if( good ) { catenary::build_geometry( *definition, solution, points, 0.04, geometry ); }
        double lowest { 5.0 }; int hangers { 0 }, ropes { 0 };
        for( auto const &element : geometry.elements ) {
            if( element.kind == catenary::element_kind::contact ) { lowest = std::min( { lowest, element.from.y, element.to.y } ); }
            if( element.kind == catenary::element_kind::hanger ) { ++hangers; }
            if( element.kind == catenary::element_kind::messenger ) { ++ropes; }
        }
        // zwis przewodu jezdnego: w L^2 / 8 T, naciąg 1906 daN na dwa przewody Djp100
        auto const expected { 2 * 0.890 * catenary::gravity * 40 * 40 / ( 8 * 19060.0 ) };
        good = good && hangers == 0 && geometry.fittings.empty() && ropes > 20 && std::abs( ( 5.0 - lowest ) - expected ) < 0.002;
        std::printf( "odcinek do kotwienia 40 m: zwis przewodu jezdnego %.3f m (wzor %.3f m), zwis lin %.3f m  %s\n", 5.0 - lowest, expected, solution.result.sag, good ? "OK" : "BLAD" );
        failed = failed || !good;
    }
    // bryły: ściany graniastosłupów i prostopadłościanów zwrócone na zewnątrz, średnice z tabeli przewodów
    {
        auto const *definition { registry.find( "YwsC120-2C" ) };
        bool good { true };
        {
            catenary::span_geometry geometry;
            geometry.elements.push_back( { { 1, 2, 3 }, { 4, 2.5, 7 }, catenary::element_kind::messenger } );
            std::vector<catenary::solid_vertex> solid, none;
            catenary::build_solid( *definition, geometry, solid, none );
            std::vector<catenary::vec3> triangles;
            for( auto const &vertex : solid ) {
                triangles.push_back( vertex.position );
                // normalna jednostkowa, prostopadła do osi i skierowana od osi
                auto const &n { vertex.normal };
                double const px { vertex.position.x - 1 }, py { vertex.position.y - 2 }, pz { vertex.position.z - 3 };
                good = good && std::abs( n.x * n.x + n.y * n.y + n.z * n.z - 1.0 ) < 1e-9 && std::abs( n.x * 3 + n.y * 0.5 + n.z * 4 ) < 1e-9 && ( n.x * px + n.y * py + n.z * pz ) > 0.0;
            }
            good = good && none.empty() && triangles.size() == 6 * 2 * 3;
            double const ax { 3 }, ay { 0.5 }, az { 4 }, al { std::sqrt( ax * ax + ay * ay + az * az ) };
            double radius { 0.0 };
            for( std::size_t index = 0; index + 2 < triangles.size(); index += 3 ) {
                auto const &a { triangles[ index ] }, &b { triangles[ index + 1 ] }, &c { triangles[ index + 2 ] };
                double const ux { b.x - a.x }, uy { b.y - a.y }, uz { b.z - a.z }, vx { c.x - a.x }, vy { c.y - a.y }, vz { c.z - a.z };
                double const nx { uy * vz - uz * vy }, ny { uz * vx - ux * vz }, nz { ux * vy - uy * vx };
                // wektor od osi do środka trójkąta
                double const cx { ( a.x + b.x + c.x ) / 3 - 1 }, cy { ( a.y + b.y + c.y ) / 3 - 2 }, cz { ( a.z + b.z + c.z ) / 3 - 3 };
                auto const along { ( cx * ax + cy * ay + cz * az ) / al };
                double const rx { cx - along * ax / al }, ry { cy - along * ay / al }, rz { cz - along * az / al };
                good = good && ( nx * rx + ny * ry + nz * rz ) > 0.0;
                double const px { a.x - 1 }, py { a.y - 2 }, pz { a.z - 3 }; auto const pa { ( px * ax + py * ay + pz * az ) / al };
                radius = std::max( radius, std::sqrt( std::max( 0.0, px * px + py * py + pz * pz - pa * pa ) ) );
            }
            good = good && std::abs( radius - 0.007 ) < 1e-6; // lina L120: średnica 14,0 mm
        }
        {
            catenary::span_geometry geometry;
            geometry.fittings.push_back( { { 1, 2, 3 }, { 0.6, 0, 0.8 }, catenary::fitting_kind::y_clamp } );
            std::vector<catenary::solid_vertex> solid, none;
            catenary::build_solid( *definition, geometry, none, solid );
            std::vector<catenary::vec3> triangles;
            for( auto const &vertex : solid ) {
                triangles.push_back( vertex.position );
                auto const &n { vertex.normal };
                good = good && std::abs( n.x * n.x + n.y * n.y + n.z * n.z - 1.0 ) < 1e-9
                            && ( n.x * ( vertex.position.x - 1 ) + n.y * ( vertex.position.y - 2 ) + n.z * ( vertex.position.z - 3 ) ) > 0.0;
            }
            good = good && none.empty() && triangles.size() == 12 * 3;
            for( std::size_t index = 0; index + 2 < triangles.size(); index += 3 ) {
                auto const &a { triangles[ index ] }, &b { triangles[ index + 1 ] }, &c { triangles[ index + 2 ] };
                double const ux { b.x - a.x }, uy { b.y - a.y }, uz { b.z - a.z }, vx { c.x - a.x }, vy { c.y - a.y }, vz { c.z - a.z };
                double const nx { uy * vz - uz * vy }, ny { uz * vx - ux * vz }, nz { ux * vy - uy * vx };
                double const cx { ( a.x + b.x + c.x ) / 3 - 1 }, cy { ( a.y + b.y + c.y ) / 3 - 2 }, cz { ( a.z + b.z + c.z ) / 3 - 3 };
                good = good && ( nx * cx + ny * cy + nz * cz ) > 0.0;
            }
        }
        auto const hanger { catenary::wire_diameter( definition->hanger_wire ) };
        good = good && hanger > 0.003 && hanger < 0.005;
        std::printf( "bryly: sciany i normalne na zewnatrz, lina 14 mm, linka wieszaka %.1f mm (oszacowana z masy)  %s\n", hanger * 1000, good ? "OK" : "BLAD" );
        failed = failed || !good;
    }
    // błędne dane: brak pliku typu i plik w starym formacie mają dać nullptr i komunikat, każdy tylko raz
    {
        auto const before { errors };
        auto const *missing { registry.find( "nie-ma-takiego" ) };
        auto const afterfirst { errors };
        registry.find( "nie-ma-takiego" );
        auto const repeated { errors != afterfirst };
        registry.directory( argc > 3 ? argv[ 3 ] : "tests/catenary" );
        auto const *old { registry.find( "stary-format" ) };
        auto const good { missing == nullptr && old == nullptr && afterfirst == before + 1 && false == repeated && errors > afterfirst };
        std::printf( "bledne typy odrzucone i zgloszone raz: %s\n", good ? "OK" : "BLAD" );
        failed = failed || !good;
    }
    return failed ? 1 : 0;
}
