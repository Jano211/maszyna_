// katalog konstrukcji dla edytora (data/traction_catalog.txt): składnia, pokrycie kart, dobór karty z grupy
#include "world/catenary_catalog.h"
#include "world/catenary_cantilever.h"
#include <cstdio>
#include <set>
using namespace catenary;
int failures { 0 };
void check( bool Good, char const *What ) { std::printf( "%s  %s\n", Good ? "OK  " : "BLAD", What ); if( !Good ) { ++failures; } }
int main( int, char *argv[] ) {
    std::string const data { argv[ 1 ] };
    cantilever_catalog cantilevers; std::vector<std::string> errors;
    cantilevers.load( data + "/traction_cantilevers.txt", errors );
    cantilevers.load( data + "/traction_supports.txt", errors );
    structure_catalog catalog;
    auto const loaded { catalog.load( data + "/traction_catalog.txt", errors ) };
    for( auto const &error : errors ) { std::printf( "  %s\n", error.c_str() ); }
    check( loaded && errors.empty(), "traction_catalog.txt wczytany bez bledow" );
    std::printf( "  kategorii %zu, grup %zu, rodzin %zu, grup slupow %zu\n", catalog.categories().size(), catalog.groups().size(), catalog.families().size(), catalog.polegroups().size() );
    // każda karta z katalogu wysięgników w jakiejś rodzinie, każda karta rodziny w katalogu wysięgników
    std::set<int> cards, listed;
    for( auto const &scheme : cantilevers.schemes() ) { cards.insert( scheme.first.first ); }
    std::size_t missing { 0 }, unknown { 0 };
    for( auto const &family : catalog.families() ) { for( auto card : family.cards ) { listed.insert( card ); } }
    for( auto card : cards ) { if( !listed.count( card ) ) { std::printf( "  karta %04d bez rodziny\n", card ); ++missing; } }
    for( auto const &family : catalog.families() ) {
        auto any { false };
        for( auto card : family.cards ) { if( cards.count( card ) ) { any = true; } }
        if( !any ) { std::printf( "  rodzina %s bez zadnej karty w danych\n", family.id.c_str() ); ++unknown; }
    }
    check( missing == 0 && unknown == 0, "kazda karta w rodzinie, kazda rodzina ma karty" );
    // słupy w grupach
    std::size_t loose { 0 };
    for( auto const &pole : cantilevers.poles() ) {
        if( pole.second.gantry || false == pole.second.drop.empty() ) { continue; }
        if( catalog.polegroup_of( pole.first ) == nullptr ) { std::printf( "  slup %s bez grupy\n", pole.first.c_str() ); ++loose; }
    }
    check( loose == 0, "kazdy slup w grupie" );
    // dobór
    auto const p1 { pick_card( catalog, cantilevers, "t_tube_swing", "", 0, false, true, 3, 2.0 ) };
    check( p1.scheme != nullptr && p1.scheme->card == 1111 && p1.scheme->execution == 3, "rurowe przechylne, prosta, do slupa, A 2,00, wyk. 3 -> 1111-3" );
    auto const p2 { pick_card( catalog, cantilevers, "t_tube_swing", "", 0, true, false, 1, 2.0 ) };
    check( p2.scheme != nullptr && p2.scheme->card / 10 == 114, "rurowe przechylne, luk, od slupa -> 114x" );
    auto const p3 { pick_card( catalog, cantilevers, "t_tee12_swing", "", 0, false, true, 1, 2.0 ) };
    check( p3.scheme != nullptr && p3.scheme->card == 111 && p3.scheme->execution == 2, "teowniki przechylne siec 1,2, prosta, do slupa, A 2,00 -> 0111 wyk. 2 (z pasma)" );
    auto const p4 { pick_card( catalog, cantilevers, "t_tee12_fixed", "", 0, false, false, 1, 4.0 ) };
    check( p4.scheme != nullptr && p4.scheme->card == 122 && p4.scheme->execution == 5, "teowniki nieprzechylne, od slupa, A 4,00 -> 0122 wyk. 5" );
    auto const p5 { pick_card( catalog, cantilevers, "s_platform", "", 0, false, true, 1, 5.0 ) };
    check( p5.scheme != nullptr && p5.scheme->card == 1032, "peronowe, A 5,00 -> 1032" );
    auto const p6 { pick_card( catalog, cantilevers, "t_tube_swing", "", 0, false, true, 1, 9.0 ) };
    check( p6.scheme == nullptr && false == p6.problem.empty(), "A poza pasmami: brak karty z powodem" );
    std::printf( failures == 0 ? "wszystko OK\n" : "BLEDY: %d\n", failures );
    return failures;
}
