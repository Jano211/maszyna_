// Test world/catenary_scene.cpp z atrapą klas symulatora: dyrektywa catenary, wczytanie pliku, powiązanie po nazwach
#include "stdafx.h"
#include "world/catenary_scene.h"
#include "world/catenary_span.h"
#include "world/catenary_network.h"
#include "simulation/simulation.h"
#include "rendering/renderer.h"
#include "utilities/Globals.h"
#include "utilities/Logs.h"
#include "utilities/parser.h"
#include <cstdio>

mock_renderer Renderer, *GfxRenderer { &Renderer };
mock_globals Global;
namespace simulation { mock_table<TTraction> Traction; mock_table<TAnimModel> Instances; mock_state State; }
std::vector<std::string> errors, logs;
void ErrorLog( std::string const &Message ) { errors.push_back( Message ); }
void WriteLog( const char *Message ) { logs.emplace_back( Message ); }

int main( int, char *argv[] ) {
    catenary::types().directory( argv[ 1 ] );
    Global.asCurrentSceneryPath = "tests/catenary/";
    { std::ofstream out { "tests/catenary/Siec_Test.scm" }; // wielkość liter w nazwie pliku ma zostać zachowana
      out << "skin wysiegniki/ocynk\n"
             "node 1000 0 wys_1 suspension support slup_1 scheme 1110 2 messenger p1 p2 contact p1 p2 endsuspension\n"
             "node 1000 0 wys_2 suspension support slup_brak contact p2 p3 endsuspension\n"
             "node 1000 0 wys_3 suspension support slup_1 contact bez_typu endsuspension\n"; }
    TTraction p1, p2, bez; TAnimModel slup;
    for( auto *span : { &p1, &p2, &bez } ) { span->pPoint1 = { 0, 5, 0 }; span->pPoint2 = { 60, 5, 0 }; span->pPoint3 = { 0, 6.7, 0 }; span->pPoint4 = { 60, 6.7, 0 }; }
    catenary_span_assign( p1, "YwsC120-2C" ); catenary_span_assign( p2, "YwsC120-2C" );
    simulation::Traction.items = { { "p1", &p1 }, { "p2", &p2 }, { "bez_typu", &bez } };
    simulation::Instances.items = { { "slup_1", &slup } };

    // sceneria: dyrektywa, a po niej zwykły ciąg dalszy, który ma zostać nietknięty
    cParser scenery( "Siec_Test.scm endcatenary node 1 2 3" );
    catenary_scene_load( scenery );
    auto const rest { scenery.getToken<std::string>() };
    auto const loaded { errors.empty() && logs.size() == 1 && rest == "node" };
    catenary_scene_resolve();
    // wys_2: brak słupa i brak przęsła p3; wys_3: przęsło bez typu
    auto const resolved { errors.size() == 3 && logs.size() == 2 && logs.back().find( "1 of 3" ) != std::string::npos };
    for( auto const &line : logs ) { std::printf( "  %s\n", line.c_str() ); }
    for( auto const &line : errors ) { std::printf( "  %s\n", line.c_str() ); }
    // brak pliku i brak nazwy pliku
    errors.clear();
    cParser missing( "nie_ma.scm endcatenary" ); catenary_scene_load( missing );
    cParser noname( "endcatenary node" ); catenary_scene_load( noname );
    auto const rest2 { noname.getToken<std::string>() };
    auto const bad { errors.size() == 2 && rest2 == "node" };
    catenary_scene_clear(); logs.clear(); catenary_scene_resolve();
    auto const cleared { logs.empty() };

    // wysięgniki z punktów podanych wprost: części mają trafić do scenerii jako egzemplarze modeli
    errors.clear(); logs.clear();
    catenary_scene_data( argv[ 1 ] );
    { std::ofstream out { "tests/catenary/Siec_Test.scm" };
      out << "node 1000 0 none suspension scheme 1113 1 at 0.087 5.8 170.247 attach -2.263 5.8 170.247 endsuspension\n"      // konstrukcja po stronie -X
             "node 800 0 w2 suspension scheme 1124 1 at 10 5.8 100 rope 10 7.5 100 attach 10 0 102.7 endsuspension\n"        // konstrukcja po stronie +Z
             "node 1000 0 w3 suspension scheme 9999 1 at 0 5.8 0 attach -2.3 0 0 endsuspension\n"; }                         // nie ma takiej karty
    cParser points( "Siec_Test.scm endcatenary" );
    catenary_scene_load( points );
    catenary_scene_resolve();
    auto const &created { simulation::State.created };
    auto assembled { errors.size() == 1 && created.size() == 32 && logs.size() >= 3 && logs.back().find( "2 cantilever(s) assembled from 32 part(s)" ) != std::string::npos };
    if( created.size() == 32 ) { // 2 x (13 wysięgnika + 3 ramion)
        // pierwszy wysięgnik: izolator ukośnika 2,35 m od przewodów po stronie -X, 10 cm pod przewodem;
        // drugi: to samo po stronie +Z. uchwyt liny nośnej stoi w osi liny, 1,7 m nad przewodem
        auto const find = [ & ]( std::size_t const From, std::string const &Part ) -> mock_state::created_model const * {
            for( auto index { From }; index < From + 16; ++index ) { if( created[ index ].entry.find( Part ) != std::string::npos ) { return &created[ index ]; } }
            return nullptr; };
        auto const *first { find( 0, "7100-01.t3d" ) };
        auto const *second { find( 16, "7100-01.t3d" ) };
        auto const *clamp { find( 16, "4140-01.t3d" ) };
        assembled = assembled && first != nullptr && second != nullptr && clamp != nullptr
            && std::abs( first->position.x - ( 0.087 - 2.35 ) ) < 1e-6 && std::abs( first->position.y - 5.7 ) < 1e-6 && std::abs( first->position.z - 170.247 ) < 1e-6
            && std::abs( second->position.x - 10.0 ) < 1e-6 && std::abs( second->position.z - 102.7 ) < 1e-6
            && std::abs( clamp->position.x - 10.0 ) < 1e-6 && std::abs( clamp->position.y - 7.5 ) < 1e-6 && std::abs( clamp->position.z - 100.0 ) < 1e-6
            && first->entry.find( "node 1000.0 0.0 none model 0 0 0 0 siec_trakcyjna/components/7100-01.t3d none angles " ) == 0
            && second->entry.find( "node 800.0 " ) == 0;
        std::printf( "  %s\n  %s\n", first ? first->entry.c_str() : "?", created[ 12 ].entry.c_str() );
    }
    for( auto const &line : logs ) { std::printf( "  %s\n", line.c_str() ); }
    for( auto const &line : errors ) { std::printf( "  %s\n", line.c_str() ); }
    std::printf( "wysiegniki z punktow: czesci %zu (ma byc 32), bledow %zu (1)  %s\n", created.size(), errors.size(), assembled ? "OK" : "BLAD" );
    catenary_scene_clear();

    // dyrektywa po FirstInit: sceneria jest już zainicjowana, więc wysięgniki mają powstać od razu przy wczytaniu pliku,
    // a ponowne wiązanie nie może ich zdublować (64 = 52 wysięgnika + 12 ramion)
    errors.clear(); logs.clear();
    catenary_scene_resolve();
    cParser late( "Siec_Test.scm endcatenary" );
    catenary_scene_load( late );
    auto const afterload { created.size() };
    catenary_scene_resolve();
    auto const latedone { afterload == 64 && created.size() == 64 };
    std::printf( "dyrektywa po FirstInit: czesci %zu po wczytaniu, %zu po ponownym wiazaniu (ma byc 64 i 64)  %s\n", afterload, created.size(), latedone ? "OK" : "BLAD" );
    catenary_scene_clear();

    // wysięgnik na słupie 1701: oś słupa 2,4 m od przewodów po stronie -X, główka szyny 5,6 m pod przewodem.
    // ma powstać 23 części: 13 wysięgnika, 3 ramion (zestaw 6201-02, zwis 4240-02, głowica 4240-22) i 7 słupa z mocowaniami
    errors.clear(); logs.clear();
    auto const before { created.size() };
    { std::ofstream out { "tests/catenary/Siec_Test.scm" };
      out << "node 1000 0 s1 suspension scheme 1112 1 at 0 5.8 50 rope 0 7.5 50 pole 1701 -2.4 0.2 50 endsuspension\n"
             "node 1000 0 s2 suspension scheme 1112 1 at 0 5.8 60 pole 9999 -2.4 0.2 60 endsuspension\n"; }   // nie ma takiego słupa
    cParser onpole( "Siec_Test.scm endcatenary" );
    catenary_scene_load( onpole );
    catenary_scene_resolve();
    auto const poleparts { created.size() - before };
    auto polegood { poleparts == 23 && errors.size() == 1 };
    for( auto index { before }; index < created.size(); ++index ) {
        auto const &item { created[ index ] };
        if( item.entry.find( "siec_trakcyjna/slupy/1701.t3d" ) != std::string::npos ) {
            polegood = polegood && std::abs( item.position.x + 2.4 ) < 1e-6 && std::abs( item.position.y - 0.2 ) < 1e-6 && std::abs( item.position.z - 50.0 ) < 1e-6;
        }
        if( item.entry.find( "4610-00.t3d" ) != std::string::npos ) {
            // oś obrotu zawiasu: czoło słupa na wysokości śrub (5,4413 m nad główką) plus 148,7 mm
            auto const depth { 0.175 + 0.21 * ( 7.46 - 5.4413 ) / 8.75 };
            polegood = polegood && std::abs( item.position.x - ( -2.4 + 0.5 * depth + 0.1487 ) ) < 1e-3;
        }
    }
    for( auto const &line : errors ) { std::printf( "  %s\n", line.c_str() ); }
    std::printf( "wysiegnik na slupie: czesci %zu (ma byc 23), bledow %zu (1)  %s\n", poleparts, errors.size(), polegood ? "OK" : "BLAD" );
    catenary_scene_clear();

    // parasol na słupie 1701: dwa wysięgniki 1112-1 po obu stronach słupa (oś słupa x = -2,4, przewody w x = 0 i x = -4,8).
    // mocowania 03 mają połówki z nakrętkami, więc obie strony idą na wspólne mocowania bez tyłów: strona pierwsza 13 + 3 ramion +
    // słup + 4 mocowania (ukośnik z zawiasem, odciąg z prętem) = 21, druga bez słupu i z połówkami 4640-05 / 4650-05 = 20
    errors.clear(); logs.clear();
    auto const umbrellabefore { created.size() };
    { std::ofstream out { "tests/catenary/Siec_Test.scm" };
      out << "node 1000 0 p1 suspension scheme 1112 1 at 0 5.8 80 pole 1701 -2.4 0.2 80 umbrella u1 endsuspension\n"
             "node 1000 0 p2 suspension scheme 1112 1 at -4.8 5.8 80 pole 1701 -2.4 0.2 80 umbrella u1 endsuspension\n"
             "node 1000 0 p3 suspension scheme 1112 1 at 0 5.8 90 pole 1701 -2.4 0.2 90 umbrella u2 endsuspension\n"; }   // parasol bez pary
    cParser umbrellafile( "Siec_Test.scm endcatenary" );
    catenary_scene_load( umbrellafile );
    catenary_scene_resolve();
    std::size_t poles { 0 }, backs { 0 }, nuts { 0 };
    for( auto index { umbrellabefore }; index < created.size(); ++index ) {
        auto const &entry { created[ index ].entry };
        if( entry.find( "1701.t3d" ) != std::string::npos ) { ++poles; }
        if( entry.find( "4610-20.t3d" ) != std::string::npos ) { ++backs; }
        if( entry.find( "4640-05.t3d" ) != std::string::npos || entry.find( "4650-05.t3d" ) != std::string::npos ) { ++nuts; }
    }
    auto const umbrellaparts { created.size() - umbrellabefore };
    // p3 bez pary składa się sam: 23 części, w tym słup i 2 tyły
    auto const umbrellagood { umbrellaparts == 21 + 20 + 23 && poles == 2 && backs == 2 && nuts == 2 && errors.size() == 1 };
    for( auto const &line : errors ) { std::printf( "  %s\n", line.c_str() ); }
    std::printf( "parasol: czesci %zu (ma byc 64), slupow %zu (2), tylow %zu (2), polowek z nakretkami %zu (2), bledow %zu (1)  %s\n", umbrellaparts, poles, backs, nuts, errors.size(), umbrellagood ? "OK" : "BLAD" );
    catenary_scene_clear();

    // zestaw izolatorów 25 kV: 1116-1 z krótszymi rurami (jak w td_katalog); izolatory 7020-25 i 7100-25, zestaw "99" nie istnieje
    errors.clear(); logs.clear();
    auto const kvbefore { created.size() };
    { std::ofstream out { "tests/catenary/Siec_Test.scm" };
      out << "node 1000 0 k1 suspension scheme 1116 1 at 0 5.8 100 pole 1701 -3.512 0.2 100 insulators 25 override strut 6001-06 override stay 6007-06 endsuspension\n"
             "node 1000 0 k2 suspension scheme 1116 1 at 0 5.8 110 pole 1701 -3.512 0.2 110 insulators 99 endsuspension\n"; }
    cParser kvfile( "Siec_Test.scm endcatenary" );
    catenary_scene_load( kvfile );
    catenary_scene_resolve();
    std::size_t kv { 0 }, kvold { 0 };
    for( auto index { kvbefore }; index < created.size(); ++index ) {
        auto const &entry { created[ index ].entry };
        if( entry.find( "7020-25.t3d" ) != std::string::npos || entry.find( "7100-25.t3d" ) != std::string::npos ) { ++kv; }
        if( entry.find( "7020-01.t3d" ) != std::string::npos || entry.find( "7100-01.t3d" ) != std::string::npos ) { ++kvold; }
    }
    std::size_t kvmissing { 0 };
    for( auto const &line : errors ) { if( line.find( "insulator set \"99\"" ) != std::string::npos ) { ++kvmissing; } }
    auto kvgood { kv == 2 && kvold == 2 && kvmissing == 2 };
    std::printf( "izolatory 25 kV: podmienionych %zu (2), starych w zestawie bez czesci %zu (2), komunikatow o braku %zu (2)  %s\n", kv, kvold, kvmissing, kvgood ? "OK" : "BLAD" );
    catenary_scene_clear();

    // zestaw 25 kV bez podanych rur: exe dobiera ukośnik, odciąg i wysięg z izolatorem (jak w td_katalog)
    std::size_t kvauto { 0 };
    for( auto const &trial : { "1116 1 -3.512", "1114 2 -2.912", "1112 1 -2.312", "1212 1 -2.312", "1110 1 -1.712", "1232 1 -2.312" } ) {   // oś słupa: środek pasma A + pół słupa
        errors.clear();
        auto const before { created.size() };
        { std::ofstream out { "tests/catenary/Siec_Test.scm" };
          std::string const text { trial };
          auto const space { text.rfind( ' ' ) };
          out << "node 1000 0 a1 suspension scheme " << text.substr( 0, space ) << " at 0 5.8 100 pole 1701 " << text.substr( space + 1 ) << " 0.2 100 insulators 25 endsuspension\n"; }
        cParser autofile( "Siec_Test.scm endcatenary" );
        catenary_scene_load( autofile );
        catenary_scene_resolve();
        std::string tubes;
        for( auto index { before }; index < created.size(); ++index ) {
            auto const &entry { created[ index ].entry };
            for( auto const *family : { "/6001-", "/6002-", "/6007-", "/6011-", "/6111-" } ) {
                auto const at { entry.find( family ) };
                if( at != std::string::npos ) { tubes += " " + entry.substr( at + 1, 7 ); }
            }
        }
        if( created.size() > before && errors.empty() ) { ++kvauto; }
        std::printf( "  25 kV %s: czesci %zu, rury%s\n", trial, created.size() - before, tubes.c_str() );
        catenary_scene_clear();
    }
    kvgood = kvgood && kvauto == 6;
    std::printf( "izolatory 25 kV z doborem rur: zlozonych %zu (6)  %s\n", kvauto, kvauto == 6 ? "OK" : "BLAD" );

    auto const good { loaded && resolved && bad && cleared && assembled && latedone && polegood && umbrellagood && kvgood };
    std::printf( "podwieszenia w scenerii: wczytanie %s, powiazanie %s, bledne dyrektywy %s, czyszczenie %s\n", loaded ? "OK" : "BLAD", resolved ? "OK" : "BLAD", bad ? "OK" : "BLAD", cleared ? "OK" : "BLAD" );
    return good ? 0 : 1;
}
