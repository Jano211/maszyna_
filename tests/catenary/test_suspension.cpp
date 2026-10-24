// Test rozbioru pliku podwieszeń i rejestru konstrukcji wsporczych
#include "world/catenary_suspension.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

std::vector<std::string> words( std::string const &Text ) { std::istringstream s { Text }; std::vector<std::string> list; std::string w; while( s >> w ) { list.push_back( w ); } return list; }

int main() {
    bool failed { false };
    {
        catenary::suspension_file file; std::vector<std::string> errors;
        catenary::parse_suspensions( words(
            "skin wysiegniki/ocynk "
            "node 1000 0 wys_17 suspension support slup_17 scheme 1110 2 messenger td_t2_016 td_t2_017 contact td_t2_016 td_t2_017 insulators plastic override strut_insulator 7100-1p endsuspension "
            "node 1000 0 none suspension support slup_18 contact td_t2_017 skin wysiegniki/stary endsuspension "
            "node 1000 0 wys_19 suspension support bramka_3 messenger a b contact a b c d endsuspension" ), "test.scm", file, errors );
        auto const good { errors.empty() && file.skin == "wysiegniki/ocynk" && file.entries.size() == 3
            && file.entries[ 0 ].name == "wys_17" && file.entries[ 0 ].support == "slup_17" && file.entries[ 0 ].card == 1110 && file.entries[ 0 ].execution == 2
            && file.entries[ 0 ].messenger.size() == 2 && file.entries[ 0 ].contact.size() == 2 && file.entries[ 0 ].insulators == "plastic"
            && file.entries[ 0 ].overrides.size() == 1 && file.entries[ 0 ].overrides[ 0 ].second == "7100-1p" && file.entries[ 0 ].range_max == 1000
            && file.entries[ 1 ].name.empty() && file.entries[ 1 ].messenger.empty() && file.entries[ 1 ].skin == "wysiegniki/stary" && file.entries[ 1 ].card == 0
            && file.entries[ 2 ].contact.size() == 4 };
        std::printf( "poprawny plik podwieszen: wpisow %zu, bledow %zu  %s\n", file.entries.size(), errors.size(), good ? "OK" : "BLAD" );
        for( auto const &e : errors ) { std::printf( "  %s\n", e.c_str() ); }
        failed = failed || !good;
    }
    {
        // każdy błędny wpis ma dać komunikat i zostać pominięty, a poprawne po nim mają się wczytać
        catenary::suspension_file file; std::vector<std::string> errors;
        catenary::parse_suspensions( words(
            "smiec "
            "node 1000 0 a suspension contact p1 endsuspension "                        // brak support
            "node 1000 0 b suspension support s messenger p1 p2 p3 endsuspension "      // trzy liny
            "node 1000 0 c suspension support s endsuspension "                         // bez przęseł
            "node 1000 0 d suspension support s contact p1 scheme abc 2 endsuspension " // zły schemat
            "node 1000 0 e suspension support s contact p1 "                            // brak endsuspension
            "node 1000 0 f model 0 0 0 0 slup.t3d none endmodel "                       // inny typ wpisu
            "node 1000 0 g suspension support s contact p1 endsuspension "
            "node 1000 0 g suspension support s contact p2 endsuspension "              // powtórzona nazwa
            "node 1000 0 h suspension support s contact p1 endsuspension" ), "zly.scm", file, errors );
        auto const good { file.entries.size() == 2 && file.entries[ 0 ].name == "g" && file.entries[ 1 ].name == "h" && errors.size() >= 8 };
        std::printf( "bledny plik podwieszen: wpisow %zu (maja byc 2), komunikatow %zu  %s\n", file.entries.size(), errors.size(), good ? "OK" : "BLAD" );
        if( false == good ) { for( auto const &e : errors ) { std::printf( "  %s\n", e.c_str() ); } }
        failed = failed || !good;
    }
    {
        // wpisy z punktami podanymi wprost: bez konstrukcji i przęseł, za to ze schematem i kompletem punktów
        catenary::suspension_file file; std::vector<std::string> errors;
        catenary::parse_suspensions( words(
            "node 1000 0 none suspension scheme 1113 1 at 0.087 5.8 170.247 attach -2.013 5.8 170.247 endsuspension "
            "node 800 0 w2 suspension scheme 1124 1 at 0.687 5.8 100.2 rope 0.687 7.5 100.2 attach -2.0 0 100.2 skin wysiegniki/stary endsuspension "
            "node 1000 0 w3 suspension scheme 1113 1 at 0 5.8 0 endsuspension "              // brak attach
            "node 1000 0 w4 suspension at 0 5.8 0 attach -2 0 0 endsuspension "             // brak schematu
            "node 1000 0 w5 suspension scheme 1113 1 at 0 5.8 attach -2 0 0 endsuspension"  // dwie liczby zamiast trzech
            ), "punkty.scm", file, errors );
        auto const good { file.entries.size() == 2 && errors.size() == 3
            && file.entries[ 0 ].has_at && file.entries[ 0 ].has_attach && false == file.entries[ 0 ].has_rope && file.entries[ 0 ].support.empty()
            && std::abs( file.entries[ 0 ].at[ 2 ] - 170.247 ) < 1e-9 && std::abs( file.entries[ 0 ].attach[ 0 ] + 2.013 ) < 1e-9 && file.entries[ 0 ].card == 1113
            && file.entries[ 1 ].has_rope && std::abs( file.entries[ 1 ].rope[ 1 ] - 7.5 ) < 1e-9 && file.entries[ 1 ].range_max == 800 };
        std::printf( "wpisy z punktami: wpisow %zu (maja byc 2), komunikatow %zu (3)  %s\n", file.entries.size(), errors.size(), good ? "OK" : "BLAD" );
        if( false == good ) { for( auto const &e : errors ) { std::printf( "  %s\n", e.c_str() ); } }
        failed = failed || !good;
    }
    {
        { std::ofstream out { "tests/catenary/supports_test.txt" };
          out << "// rejestr testowy\nsupport Siec_Trakcyjna\\Slupy\\1611-3.t3d {\n  type: mast\n  height: 8.2   // m\n  width_base: 0.385\n  width_top: 0.160\n}\n"
                 "support zly.t3d {\n  type: mast\n  height: x\n}\nsupport niedomkniety.t3d {\n  type: mast\n"; }
        catenary::support_registry registry; std::vector<std::string> errors;
        auto const loaded { registry.load( "tests/catenary/supports_test.txt", errors ) };
        auto const *data { registry.find( "siec_trakcyjna/slupy/1611-3.t3d" ) };
        std::vector<std::string> none;
        auto const good { loaded && registry.size() == 1 && data != nullptr && data->type == "mast" && errors.size() == 3
            && std::abs( data->half_width( 0.0 ) - 0.1925 ) < 1e-9 && std::abs( data->half_width( 8.2 ) - 0.08 ) < 1e-9 && std::abs( data->half_width( 4.1 ) - 0.13625 ) < 1e-9
            && registry.find( "inny.t3d" ) == nullptr && false == catenary::support_registry().load( "tests/catenary/nie_ma.txt", none ) };
        std::printf( "rejestr konstrukcji: wpisow %zu, komunikatow %zu, polowa szerokosci na 4,1 m %.4f m  %s\n", registry.size(), errors.size(), data ? data->half_width( 4.1 ) : -1.0, good ? "OK" : "BLAD" );
        failed = failed || !good;
    }
    {
        // kotwienie i łańcuch z kotwieniami: odczyt, zapis i odczyt zapisanego
        catenary::suspension_file file; std::vector<std::string> errors;
        catenary::parse_suspensions( words(
            "node 1000 0 k1 anchor device 0461 network C120-2C pole 1611-11 10 0 20 toward 10 5.6 60 distance 650 color szary top_color zolty missing 3 rope_height 7.85 insulators messenger endanchor "
            "node 1000 0 k2 anchor device 0461 pole 1611-11 10 0 endanchor "    // za malo liczb, brak toward
            "node 1000 0 a suspension scheme 1113 1 at 0 5.6 0 attach -2 0 0 endsuspension node 1000 0 b suspension scheme 1113 1 at 0 5.6 60 attach -2 0 60 endsuspension "
            "node 1000 0 l1 chain power z voltage 3000 current 4500 resistivity 0.01 material cu thickness 3 state 0 anchor_end k1 suspensions a b endchain"
            ), "kotwy.scm", file, errors );
        auto good { file.anchors.size() == 1 && errors.size() >= 1 && file.chains.size() == 1 && file.chains[ 0 ].anchor_end == "k1"
            && file.chains[ 0 ].suspensions.size() == 2 && file.anchors[ 0 ].missing == 3 && file.anchors[ 0 ].top_color == "zolty" && file.anchors[ 0 ].insulators == "messenger" && file.anchors[ 0 ].rope_height == 7.85
            && std::abs( file.anchors[ 0 ].toward[ 2 ] - 60.0 ) < 1e-9 && std::abs( file.anchors[ 0 ].distance - 650.0 ) < 1e-9 };
        if( good ) {
            catenary::suspension_file again; std::vector<std::string> none;
            catenary::parse_suspensions( words( catenary::write_suspension( file.entries[ 0 ] ) + " " + catenary::write_suspension( file.entries[ 1 ] ) + " " + catenary::write_anchor( file.anchors[ 0 ] ) + " " + catenary::write_chain( file.chains[ 0 ] ) ), "zapis.scm", again, none );
            for( auto const &e : none ) { std::printf( "  %s\n", e.c_str() ); }
            good = none.empty() && again.anchors.size() == 1 && again.anchors[ 0 ].network == "C120-2C" && again.anchors[ 0 ].pole_type == "1611-11"
                && again.anchors[ 0 ].missing == 3 && again.anchors[ 0 ].insulators == "messenger" && again.anchors[ 0 ].rope_height == 7.85 && again.chains.size() == 1 && again.chains[ 0 ].anchor_end == "k1";
        }
        std::printf( "kotwienie: wpisow %zu, komunikatow %zu, zapis i odczyt  %s\n", file.anchors.size(), errors.size(), good ? "OK" : "BLAD" );
        if( false == good ) { for( auto const &e : errors ) { std::printf( "  %s\n", e.c_str() ); } }
        failed = failed || !good;
    }
    return failed ? 1 : 0;
}
