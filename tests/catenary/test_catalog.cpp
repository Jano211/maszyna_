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
        if( false == pole.second.drop.empty() ) { continue; }
        if( catalog.polegroup_of( pole.first ) == nullptr ) { std::printf( "  slup %s bez grupy\n", pole.first.c_str() ); ++loose; }
    }
    check( loose == 0, "kazdy slup (takze slupy bramek) w grupie" );
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
    // liczba przewodów jezdnych (wpisy "arms"): wyk. 1 (6201-02, dwa przewody) przy sieci jednoprzewodowej pomijane
    {
        auto const *s1 { cantilevers.scheme( 1111, 1 ) }, *s5 { cantilevers.scheme( 1111, 5 ) };
        check( s1 != nullptr && s5 != nullptr && catalog.contacts_of( *s1 ) == 2 && catalog.contacts_of( *s5 ) == 1, "1111-1 ramiona na dwa przewody, 1111-5 na jeden" );
        auto const one { pick_card( catalog, cantilevers, "t_tube_swing", "", 0, false, true, 1, 2.0, 1 ) };
        check( one.scheme != nullptr && one.scheme->card == 1111 && catalog.contacts_of( *one.scheme ) == 1 && one.execution_changed, "siec 1 przewod, wybrane wyk. 1 -> inne wykonanie 1111 na jeden przewod" );
        auto const two { pick_card( catalog, cantilevers, "t_tube_swing", "", 0, false, true, 1, 2.0, 2 ) };
        check( two.scheme != nullptr && two.scheme->execution == 1 && false == two.execution_changed, "siec 2 przewody, wyk. 1 zostaje" );
        // teowniki sieci 1, 2 (YpC120-2C) maja ramiona 6201-02: przy sieci dwuprzewodowej dobor jak dotad, przy jednoprzewodowej brak
        auto const tee2 { pick_card( catalog, cantilevers, "t_tee12_swing", "", 0, false, true, 1, 2.0, 2 ) };
        check( tee2.scheme != nullptr && tee2.scheme->card == 111, "teowniki siec 1,2 przy dwoch przewodach -> 0111" );
        auto const tee1 { pick_card( catalog, cantilevers, "t_tee12_swing", "", 0, false, true, 1, 2.0, 1 ) };
        std::printf( "  teowniki siec 1,2 przy jednym przewodzie: %s\n", tee1.problem.c_str() );
        check( tee1.scheme == nullptr && tee1.problem.find( "one contact wire" ) != std::string::npos, "teowniki siec 1,2 przy jednym przewodzie: brak, z powodem (liczba przewodow)" );
        std::size_t described { 0 }, missing { 0 };
        for( auto const &entry : cantilevers.schemes() ) {
            auto const arms { entry.second.roles.find( "arms" ) };
            if( arms == entry.second.roles.end() ) { continue; }
            if( catalog.arms_contacts( arms->second ) > 0 ) { ++described; } else { ++missing; }
        }
        std::printf( "  karty z ramionami: %zu opisanych, %zu bez opisu liczby przewodow\n", described, missing );
        check( missing == 0 && catalog.arms_contacts( "6206-1111" ) == 1, "kazdy zestaw ramion kart ma liczbe przewodow, pary rozjazdowe - jeden" );
    }
    std::printf( failures == 0 ? "wszystko OK\n" : "BLEDY: %d\n", failures );
    return failures;
}
