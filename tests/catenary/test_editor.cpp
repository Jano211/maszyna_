// Test części sieci trakcyjnej używanej przez edytor scenerii: tablice rozstawienia (traction_layout.txt), zapis wpisów
// podwieszeń, dodawanie / zmiana / usuwanie podwieszeń na żywo z atrapą klas symulatora
#include "stdafx.h"
#include "world/catenary_scene.h"
#include "world/catenary_network.h"
#include "world/catenary_layout.h"
#include "world/catenary_suspension.h"
#include "simulation/simulation.h"
#include "rendering/renderer.h"
#include "utilities/Globals.h"
#include "utilities/Logs.h"
#include "utilities/parser.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

mock_renderer Renderer, *GfxRenderer { &Renderer };
mock_globals Global;
namespace simulation { mock_table<TTraction> Traction; mock_table<TAnimModel> Instances; mock_state State; }
std::vector<std::string> errors, logs;
void ErrorLog( std::string const &Message ) { errors.push_back( Message ); }
void WriteLog( const char *Message ) { logs.emplace_back( Message ); }
extern int mock_deleted_parts;
extern int mock_deleted_spans;
extern std::vector<std::string> mock_span_bodies, mock_span_names;

int failures { 0 };
void check( bool const Good, char const *What ) { std::printf( "%s  %s\n", Good ? "OK  " : "BLAD", What ); if( false == Good ) { ++failures; } }

int main( int, char *argv[] ) {
    Global.asCurrentSceneryPath = "tests/catenary/";
    catenary::types().directory( argv[ 1 ] );
    catenary_scene_data( argv[ 1 ] );

    // 1. tablice rozstawienia
    auto const &tables { catenary_scene_layout() };
    check( tables.nominal_span( "YwsC120-2C", 1 ) == 70.0 && tables.nominal_span( "YwsC120-2C", 2 ) == 66.0, "rozpietosc nominalna YwsC120-2C 70/66 (43-0002)" );
    check( tables.nominal_span( "SKB70-C", 1 ) == 80.0 && tables.nominal_span( "nie ma", 1 ) == 0.0, "stara siec 5: 80 m, nieznana: 0" );
    check( std::abs( catenary::curve_span( 500.0, 0.40 ) - 40.0 ) < 1e-9, "luk R 500, z 0,40: a = 40 m (wykres 0008)" );
    check( catenary::span_limit( tables, "YwsC120-2C", 1, 2000.0, 0.40 ) == 70.0, "luk R 2000: ograniczenie nominalna 70 m" );
    check( std::abs( catenary::span_limit( tables, "YwsC120-2C", 1, 600.0, 0.40 ) - 2.0 * std::sqrt( 480.0 ) ) < 1e-9, "luk R 600: z wykresu 0008" );
    double a[ 2 ] { 0.0, 0.0 }, b[ 2 ] { 100.0, 0.0 }, c[ 2 ] { 200.0, 0.0 }, d[ 2 ] { 500.0, 500.0 }, e[ 2 ] { 0.0, 1000.0 };
    check( catenary::plan_radius( a, b, c ) == 0.0 && std::abs( catenary::plan_radius( a, d, e ) - 500.0 ) < 1e-6, "promien przez trzy punkty (prosta = 0)" );
    check( catenary::reach_digit( 1.50 ) == 0 && catenary::reach_digit( 2.50 ) == 3 && catenary::reach_digit( 4.20 ) == 9 && catenary::reach_digit( 1.40 ) == -1, "pasmo A -> ostatnia cyfra karty" );
    catenary::suspension_family family;
    check( catenary::family_decade( family ) == 1110, "przechylne, prosta, do slupa: 111x" );
    family.toward_pole = false; family.curve = true; family.swinging = false;
    check( catenary::family_decade( family ) == 1240, "nieprzechylne, luk, od slupa: 124x" );

    // 2. zapis wpisu i ponowne odczytanie
    catenary::suspension entry;
    entry.range_max = 1000; entry.card = 1124; entry.execution = 1;
    entry.has_at = true; entry.at[ 0 ] = 0.687; entry.at[ 1 ] = 5.8; entry.at[ 2 ] = 100.247;
    entry.has_rope = true; entry.rope[ 0 ] = 0.687; entry.rope[ 1 ] = 7.5; entry.rope[ 2 ] = 100.247;
    entry.has_pole = true; entry.pole_type = "1611-11"; entry.pole[ 0 ] = -2.313; entry.pole[ 1 ] = 0.2; entry.pole[ 2 ] = 100.247;
    auto const text { catenary::write_suspension( entry ) };
    check( text == "node 1000 0 none suspension scheme 1124 1 at 0.687 5.800 100.247 rope 0.687 7.500 100.247 pole 1611-11 -2.313 0.200 100.247 endsuspension", "zapis wpisu w skladni pliku sieci" );
    {
        std::vector<std::string> tokens; std::istringstream words { text }; for( std::string w; words >> w; ) { tokens.push_back( w ); }
        catenary::suspension_file parsed; std::vector<std::string> problems;
        catenary::parse_suspensions( tokens, "zapis", parsed, problems );
        check( problems.empty() && parsed.entries.size() == 1 && catenary::write_suspension( parsed.entries.front() ) == text, "odczyt zapisanego wpisu daje to samo" );
    }

    // 3. edytor: dodanie, zmiana, próba, usunięcie, zapis
    catenary_scene_clear(); errors.clear(); logs.clear();
    catenary_scene_resolve();   // sceneria wczytana, edytor dokłada wpisy
    auto const before { simulation::State.created.size() };
    auto const index { catenary_scene_add( "td_edytor_test.scm", entry ) };
    auto const first { simulation::State.created.size() - before };
    check( catenary_scene_get( index ) != nullptr && first > 10 && catenary_scene_parts( index ) == first && errors.empty(), "dodane podwieszenie: wysiegnik zlozony od razu" );
    check( catenary_scene_changed() && catenary_scene_new_files().size() == 1, "nowy plik sieci do zapisania i do dolaczenia" );
    auto moved { entry };
    moved.pole[ 0 ] = -2.613; moved.card = 1125;   // słup 0,3 m dalej: A o 0,3 m większe, następna karta
    mock_deleted_parts = 0;
    check( catenary_scene_update( index, moved ) && mock_deleted_parts == static_cast<int>( first ) && catenary_scene_parts( index ) > 10, "zmiana: stare czesci usuniete, nowe wstawione" );
    catenary::cantilever_result probe;
    check( catenary_scene_probe( moved, probe ), "proba zlozenia bez wstawiania" );
    auto wrong { moved }; wrong.card = 9999;
    check( false == catenary_scene_probe( wrong, probe ) && false == probe.warnings.empty() && errors.empty(), "proba z karta spoza katalogu: przyczyna w wyniku, nie w logu" );
    std::string message;
    check( catenary_scene_save( message ) && false == catenary_scene_changed(), "zapis pliku sieci" );
    {
        std::ifstream saved { "tests/catenary/td_edytor_test.scm", std::ios::binary };
        std::string content { std::istreambuf_iterator<char>( saved ), std::istreambuf_iterator<char>() };
        check( content.find( "scheme 1125 1" ) != std::string::npos && content.find( "pole 1611-11 -2.613" ) != std::string::npos && content.find( "\r\n" ) != std::string::npos, "w pliku zmieniony wpis, CRLF" );
    }
    mock_deleted_parts = 0;
    auto const parts { catenary_scene_parts( index ) };
    catenary_scene_remove( index );
    check( catenary_scene_get( index ) == nullptr && mock_deleted_parts == static_cast<int>( parts ) && catenary_scene_changed(), "usuniecie: czesci znikaja" );
    catenary_scene_save( message );
    {
        std::ifstream saved { "tests/catenary/td_edytor_test.scm", std::ios::binary };
        std::string content { std::istreambuf_iterator<char>( saved ), std::istreambuf_iterator<char>() };
        check( content.find( "suspension" ) == std::string::npos && content.find( "zalozony w edytorze" ) != std::string::npos, "po usunieciu plik bez wpisow, naglowek zostaje" );
    }
    std::remove( "tests/catenary/td_edytor_test.scm" );

    // 4. łańcuch: zapis i odczyt wpisu
    {
        catenary::chain line;
        line.name = "l1"; line.power = "pwr01"; line.voltage = 3500; line.type = "YwsC120-2C"; line.state = 2;
        line.suspensions = { "s0001", "s0002", "s0003" };
        auto const written { catenary::write_chain( line ) };
        std::vector<std::string> tokens;
        for( auto const &item : { entry } ) { (void)item; }
        auto a { entry }, b { entry }, c { entry };
        a.name = "s0001"; b.name = "s0002"; c.name = "s0003";
        b.at[ 2 ] = b.rope[ 2 ] = b.pole[ 2 ] = 160.0; b.at[ 0 ] = b.rope[ 0 ] = 0.087;
        c.at[ 2 ] = c.rope[ 2 ] = c.pole[ 2 ] = 220.0;
        std::string text { catenary::write_suspension( a ) + "\n" + catenary::write_suspension( b ) + "\n" + catenary::write_suspension( c ) + "\n" + written + "\n" };
        std::istringstream words { text };
        for( std::string word; words >> word; ) { tokens.push_back( word ); }
        catenary::suspension_file parsed; std::vector<std::string> problems;
        catenary::parse_suspensions( tokens, "lancuch", parsed, problems );
        check( problems.empty() && parsed.chains.size() == 1 && catenary::write_chain( parsed.chains.front() ) == written, "lancuch: zapis i odczyt daja to samo" );
        auto broken { line }; broken.suspensions = { "s0001", "nie_ma" };
        std::vector<std::string> tokens2;
        std::istringstream words2 { catenary::write_suspension( a ) + "\n" + catenary::write_chain( broken ) };
        for( std::string word; words2 >> word; ) { tokens2.push_back( word ); }
        catenary::suspension_file parsed2; problems.clear();
        catenary::parse_suspensions( tokens2, "lancuch", parsed2, problems );
        check( parsed2.chains.empty() && problems.size() == 1, "lancuch z brakujacym podwieszeniem odrzucony z komunikatem" );

        // wczytanie pliku z łańcuchem: przęsła powstają od razu
        { std::ofstream out { "tests/catenary/td_lancuch_test.scm", std::ios::binary }; out << text; }
        catenary_scene_clear(); errors.clear(); logs.clear(); mock_span_bodies.clear(); mock_span_names.clear(); mock_deleted_spans = 0;
        cParser directive( "td_lancuch_test.scm endcatenary" );
        catenary_scene_load( directive );
        catenary_scene_resolve();
        check( catenary_chain_size() == 1 && mock_span_bodies.size() == 2 && mock_span_names.front() == "l1_1" && mock_span_names.back() == "l1_2", "wczytany lancuch: dwa przesla l1_1, l1_2" );
        check( mock_span_bodies.front().find( "pwr01 3500.0 4500.0 0.0100 cu 3.00 2" ) == 0 && mock_span_bodies.front().find( "type YwsC120-2C endtraction" ) != std::string::npos
            && mock_span_bodies.front().find( "0.687 5.800 100.247\n0.087 5.800 160.000\n0.687 7.500 100.247\n0.087 7.500 160.000" ) != std::string::npos, "przeslo: zasilanie, stan, punkty drutu i liny, typ" );
        // przesunięcie środkowego słupa: dwa przęsła przy nim od nowa
        mock_span_bodies.clear();
        auto moved2 { *catenary_scene_get( 1 ) };
        moved2.at[ 0 ] = moved2.rope[ 0 ] = -0.3;
        catenary_scene_update( 1, moved2 );
        check( mock_deleted_spans == 2 && mock_span_bodies.size() == 2 && mock_span_bodies.front().find( "-0.300 5.800 160.000" ) != std::string::npos, "przesuniety slup: oba przesla przy nim od nowa" );
        std::size_t position { 0 };
        check( catenary_chain_of( 1, position ) == 0 && position == 1 && catenary_chain_members( 0 ).size() == 3, "slup w lancuchu: miejsce 2 z 3" );
        // zmiana typu całego łańcucha jednym wpisem
        mock_span_bodies.clear();
        auto changed { *catenary_chain_get( 0 ) };
        changed.type = "C120-2C"; changed.material = "al"; changed.state = 0;
        catenary_chain_update( 0, changed );
        check( mock_span_bodies.size() == 2 && mock_span_bodies.back().find( " al 3.00 0" ) != std::string::npos && mock_span_bodies.back().find( "type C120-2C" ) != std::string::npos, "typ, material i stan calego lancucha jednym wpisem" );
        // usunięcie środkowego: łańcuch łączy sąsiednie
        mock_span_bodies.clear();
        catenary_scene_remove( 1 );
        check( catenary_chain_get( 0 ) != nullptr && catenary_chain_get( 0 )->suspensions.size() == 2 && mock_span_bodies.size() == 1 && mock_span_bodies.front().find( "0.687 5.800 220.000" ) != std::string::npos, "usuniety slup: lancuch przez sasiednie" );
        check( catenary_scene_new_name( "td_lancuch_test.scm" ) == "s0002" && catenary_scene_new_chain_name( "td_lancuch_test.scm" ) == "td_lancuch_test_l1", "nowe nazwy: pierwsza wolna" );
        catenary_scene_save( message );
        std::ifstream saved { "tests/catenary/td_lancuch_test.scm", std::ios::binary };
        std::string content { std::istreambuf_iterator<char>( saved ), std::istreambuf_iterator<char>() };
        check( content.find( "chain power pwr01" ) != std::string::npos && content.find( "suspensions s0001 s0003 endchain" ) != std::string::npos, "zapis: lancuch z nowa lista" );
        std::remove( "tests/catenary/td_lancuch_test.scm" );
    }
    // 5. bramka: zapis, odczyt, słupy i dźwigar w scenerii
    {
        catenary::gantry line;
        line.name = "b1"; line.beam = "E100"; line.side = -1; line.high = true;
        line.masts[ 0 ] = "3031-1"; line.masts[ 1 ] = "3032-1";
        line.points[ 0 ][ 0 ] = 0.0; line.points[ 0 ][ 1 ] = 0.2; line.points[ 0 ][ 2 ] = 50.0;
        line.points[ 1 ][ 0 ] = 9.992; line.points[ 1 ][ 1 ] = 0.2; line.points[ 1 ][ 2 ] = 50.0;
        auto const written { catenary::write_gantry( line ) };
        std::vector<std::string> tokens; std::istringstream words { written };
        for( std::string word; words >> word; ) { tokens.push_back( word ); }
        catenary::suspension_file parsed; std::vector<std::string> problems;
        catenary::parse_suspensions( tokens, "bramka", parsed, problems );
        check( problems.empty() && parsed.gantries.size() == 1 && catenary::write_gantry( parsed.gantries.front() ) == written, "bramka: zapis i odczyt daja to samo" );
        catenary_scene_clear(); catenary_scene_resolve(); errors.clear();
        auto const before { simulation::State.created.size() };
        auto const index { catenary_gantry_add( "td_bramka_test.scm", line ) };
        auto const &created { simulation::State.created };
        auto good { catenary_gantry_parts( index ) == 3 && created.size() == before + 3 && errors.empty() };
        if( good ) {
            auto const &mastb { created[ before + 1 ] };
            auto const &beam { created[ before + 2 ] };
            good = std::abs( mastb.position.x - 9.992 ) < 1e-6 && std::abs( mastb.position.z - 50.0 ) < 1e-6 && mastb.entry.find( "slupy/3032-1.t3d" ) != std::string::npos
                && std::abs( beam.position.y - ( 0.2 + 7.75 ) ) < 1e-6 && std::abs( beam.position.x - 0.121 ) < 1e-6 && std::abs( beam.position.z - 50.0 ) < 1e-6 && beam.entry.find( "dzwigary/10m.t3d" ) != std::string::npos;
            std::printf( "  %s @ %.3f %.3f %.3f\n", beam.entry.c_str(), beam.position.x, beam.position.y, beam.position.z );
        }
        check( good, "bramka: slupy w punktach, dzwigar od sciany pierwszego slupa (offset wzdluz dzwigara), na przeswicie gornym" );
        // zwis 4850 na dźwigarze jako "pływający słup"
        catenary::suspension drop;
        drop.card = 1112; drop.execution = 1; drop.range_max = 1000;
        drop.has_at = true; drop.at[ 0 ] = 4.0; drop.at[ 1 ] = 5.8; drop.at[ 2 ] = 50.1;
        drop.has_rope = true; drop.rope[ 0 ] = 4.0; drop.rope[ 1 ] = 7.5; drop.rope[ 2 ] = 50.1;
        drop.has_pole = true; drop.pole_type = "4850-3"; drop.pole[ 0 ] = 2.0; drop.pole[ 1 ] = 0.2 + 7.75; drop.pole[ 2 ] = 50.1;
        catenary::cantilever_result probe;
        auto const assembled { catenary_scene_probe( drop, probe ) };
        for( auto const &warning : probe.warnings ) { std::printf( "  uwaga: %s\n", warning.c_str() ); }
        check( assembled, "wysiegnik na zwisie 4850-3 sklada sie" );
        // wysięgnik wprost na słupie bramki (mocowania wyk. 04): słupa nie stawia (stawia go bramka), mocowania pod dźwigarem
        // na poziomie tej bramki (górny: 7,75 - 0,15 nad główką szyny)
        {
            auto const *catalog { catenary_scene_catalog() };
            auto const *mast { catalog != nullptr ? catalog->pole( "3031-1" ) : nullptr };
            auto good { mast != nullptr && mast->gantry && mast->strut == "4610-04" };
            if( good ) {
                catenary::suspension onmast;
                double const reach { 2.0 };
                auto const *scheme { catalog->scheme_for_reach( 1110, 1, reach ) };
                good = scheme != nullptr;
                if( good ) {
                    onmast.card = scheme->card; onmast.execution = 1; onmast.range_max = 1000;
                    auto const wirex { 0.5 * mast->depth( 0.0 ) + reach };
                    onmast.has_at = true; onmast.at[ 0 ] = wirex; onmast.at[ 1 ] = 0.2 + 5.6; onmast.at[ 2 ] = 50.0;
                    onmast.has_rope = true; onmast.rope[ 0 ] = wirex; onmast.rope[ 1 ] = 0.2 + 5.6 + scheme->height; onmast.rope[ 2 ] = 50.0;
                    onmast.has_pole = true; onmast.pole_type = "3031-1"; onmast.pole[ 0 ] = 0.0; onmast.pole[ 1 ] = 0.2; onmast.pole[ 2 ] = 50.0;
                    catenary::cantilever_result probe;
                    good = catenary_scene_probe( onmast, probe );
                    for( auto const &warning : probe.warnings ) { std::printf( "  uwaga (slup bramki): %s\n", warning.c_str() ); }
                    good = good && probe.find( "pole" ) == nullptr && probe.find( "strut_mount" ) != nullptr;
                    auto const stay { probe.values.find( "mount.stay" ) };
                    if( stay != probe.values.end() ) {
                        std::printf( "  karta %d, os srub mocowania odciagu %.3f m nad glowka szyny (dzwigar gorny 7,75)\n", scheme->card, stay->second );
                        good = good && stay->second <= 7.75 - 0.15 + 1e-6;
                    }
                    for( auto const &warning : probe.warnings ) { if( warning.find( "above the highest mount" ) != std::string::npos ) { good = false; } }
                    // edycja postawionego: inny odsuw - podwieszenie składa się od nowa (zgłoszenie 2026-10-22: "nic się nie dzieje")
                    auto const placed { catenary_scene_add( "td_edytor_test.scm", onmast ) };
                    auto const partsbefore { catenary_scene_parts( placed ) };
                    auto edited { onmast };
                    edited.at[ 0 ] -= 0.2; edited.rope[ 0 ] -= 0.2;
                    auto const deletedbefore { mock_deleted_parts };
                    auto const updated { catenary_scene_update( placed, edited ) };
                    std::printf( "  edycja na slupie bramki: czesci %zu -> %zu, usuniete %d, at %.2f\n", partsbefore, catenary_scene_parts( placed ), mock_deleted_parts - deletedbefore, catenary_scene_get( placed )->at[ 0 ] );
                    good = good && updated && partsbefore > 0 && catenary_scene_parts( placed ) > 0 && mock_deleted_parts - deletedbefore == static_cast<int>( partsbefore );
                    catenary_scene_remove( placed );
                }
            }
            check( good, "wysiegnik na slupie bramki 3031-1: bez modelu slupa, mocowania z wyk. 04 pod dzwigarem" );
        }
        // bramka wieloprzęsłowa: dwa przęsła na wspólnym słupie przelotowym - słup stawia tylko pierwsze; po usunięciu
        // pierwszego przęsła stawia go drugie
        {
            catenary::gantry a;
            a.name = "w1"; a.beam = "E100"; a.side = 1; a.high = false;
            a.masts[ 0 ] = "3031-1"; a.masts[ 1 ] = "3031-1";
            a.points[ 0 ][ 0 ] = 0.0; a.points[ 0 ][ 1 ] = 0.2; a.points[ 0 ][ 2 ] = 80.0;
            a.points[ 1 ][ 0 ] = 10.042; a.points[ 1 ][ 1 ] = 0.2; a.points[ 1 ][ 2 ] = 80.0;
            auto b { a };
            b.name = "w2"; b.masts[ 1 ] = "3032-1";
            b.points[ 0 ][ 0 ] = 10.042; b.points[ 1 ][ 0 ] = 20.034;
            errors.clear();
            auto const ia { catenary_gantry_add( "td_bramka_test.scm", a ) };
            auto const ib { catenary_gantry_add( "td_bramka_test.scm", b ) };
            auto good { catenary_gantry_parts( ia ) == 3 && catenary_gantry_parts( ib ) == 2 && errors.empty() };
            catenary_gantry_remove( ia );
            good = good && catenary_gantry_parts( ia ) == 0 && catenary_gantry_parts( ib ) == 3;
            catenary_gantry_update( ia, a );
            good = good && catenary_gantry_parts( ia ) == 3 && catenary_gantry_parts( ib ) == 2;
            // słup końcowy z dźwigarami z obu stron: błąd w logu
            auto c { b };
            c.name = "w3"; c.masts[ 0 ] = "3032-1"; c.points[ 0 ][ 0 ] = 20.034; c.points[ 1 ][ 0 ] = 29.976;
            errors.clear();
            catenary_gantry_add( "td_bramka_test.scm", c );
            good = good && false == errors.empty();
            for( auto const &line : errors ) { std::printf( "  (oczekiwany) %s\n", line.c_str() ); }
            errors.clear();
            check( good, "bramka wieloprzeslowa: wspolny slup przelotowy stawiany raz, po usunieciu przesla przejmuje go sasiednie" );
            // pojedynczy element dźwigara (3021) na wolnej stronie słupa przelotowego pierwszego przęsła: tylko dźwigar
            catenary::gantry single { a };
            single.name = "w4"; single.beam = "3021"; single.masts[ 1 ].clear();
            single.points[ 1 ][ 0 ] = -5.0; single.points[ 1 ][ 1 ] = 0.2; single.points[ 1 ][ 2 ] = 80.0;
            auto const written { catenary::write_gantry( single ) };
            std::vector<std::string> tokens; std::istringstream words { written };
            for( std::string word; words >> word; ) { tokens.push_back( word ); }
            catenary::suspension_file parsed; std::vector<std::string> problems;
            catenary::parse_suspensions( tokens, "element", parsed, problems );
            auto singlegood { problems.empty() && parsed.gantries.size() == 1 && parsed.gantries.front().masts[ 1 ].empty() && written.find( " tip " ) != std::string::npos };
            errors.clear();
            auto const is { catenary_gantry_add( "td_bramka_test.scm", single ) };
            singlegood = singlegood && catenary_gantry_parts( is ) == 1 && errors.empty();
            double from[ 3 ], to[ 3 ];
            auto const *catalog { catenary_scene_catalog() };
            gantry_beam_ends( single, *catalog->mast( "3031-1" ), nullptr, catalog->beam( "3021" )->span, from, to );
            singlegood = singlegood && std::abs( from[ 0 ] + 0.121 ) < 1e-6 && std::abs( to[ 0 ] + 0.121 + 7.1 ) < 1e-6 && std::abs( from[ 1 ] - ( 0.2 + 7.35 ) ) < 1e-6;
            for( auto const &line : errors ) { std::printf( "  %s\n", line.c_str() ); }
            check( singlegood, "pojedynczy element 3021 na slupie przelotowym: zapis z tip, sam dzwigar (slup stawia przeslo), 7,1 m od sciany slupa" );
        }
    }
    // 6a. kotwienie środkowe przy środkowym z trzech podwieszeń: części na słupach sąsiednich, lina z V w obu przęsłach,
    // kotwienie ciężarowe na końcu liczy odległość od niego
    {
        auto a { entry }, b { entry }, c { entry };
        a.name = "m1"; b.name = "m2"; c.name = "m3";
        b.at[ 2 ] = b.rope[ 2 ] = b.pole[ 2 ] = 160.247;
        c.at[ 2 ] = c.rope[ 2 ] = c.pole[ 2 ] = 220.247;
        catenary::anchor anchor;
        anchor.name = "km"; anchor.network = "C120-2C"; anchor.pole_type = "1611-11";
        anchor.pole[ 0 ] = -2.4; anchor.pole[ 1 ] = 0.2; anchor.pole[ 2 ] = 244.0;
        anchor.toward[ 0 ] = c.at[ 0 ]; anchor.toward[ 1 ] = c.at[ 1 ]; anchor.toward[ 2 ] = c.at[ 2 ];
        anchor.distance = 700.0;
        catenary::chain line;
        line.name = "lm"; line.power = "pwr01"; line.type = "C120-2C"; line.anchor_end = "km"; line.middle = "m2";
        line.suspensions = { "m1", "m2", "m3" };
        auto const written { catenary::write_chain( line ) };
        std::string text { catenary::write_suspension( a ) + "\n" + catenary::write_suspension( b ) + "\n" + catenary::write_suspension( c ) + "\n"
            + catenary::write_anchor( anchor ) + "\n" + written + "\n" };
        { std::ofstream out { "tests/catenary/td_srodek_test.scm", std::ios::binary }; out << text; }
        catenary_scene_clear(); errors.clear(); logs.clear(); mock_span_bodies.clear(); mock_span_names.clear(); mock_deleted_spans = 0;
        Global.AirTemperature = 10.f;
        cParser directive( "td_srodek_test.scm endcatenary" );
        catenary_scene_load( directive );
        catenary_scene_resolve();
        std::size_t parts { 0 }, triangles { 0 };
        auto const built { catenary_chain_middle( 0, parts, triangles ) };
        auto const *anchored { catenary_anchor_result( 0 ) };
        double b0, s0;
        std::printf( "  kotwienie srodkowe: czesci %d, trojkaty %d, bledy %d; kotwienie ciezarowe b %.2f\n", static_cast<int>( parts ), static_cast<int>( triangles ), static_cast<int>( errors.size() ), anchored != nullptr ? anchored->b : 0.0 );
        for( auto const &error : errors ) { std::printf( "    %s\n", error.c_str() ); }
        (void)b0; (void)s0;
        check( written.find( " middle m2" ) != std::string::npos && built && parts >= 2 * 6 && triangles > 200 && errors.empty(),
            "kotwienie srodkowe: zapis w lancuchu, na obu slupach 4430 z uchwytami i izolatorem, lina z V w obu przeslach" );
        // odległość kotwienia ciężarowego od środkowego: 60 + 23,8 m zamiast 700 - rolka bliżej tablicy +10 °C (bez zmian), przy mrozie mniej
        Global.AirTemperature = -20.f;
        catenary_scene_temperature();
        auto const *cold { catenary_anchor_result( 0 ) };
        if( cold != nullptr ) { std::printf( "  przy -20 °C: b %.3f, uwagi %d\n", cold->b, static_cast<int>( cold->warnings.size() ) ); for( auto const &w : cold->warnings ) { std::printf( "    %s\n", w.c_str() ); } }
        check( cold != nullptr && cold->b > 1.75 && cold->b < 1.75 + 84.0 * 17e-6 * 30.0 + 0.01, "kotwienie ciezarowe liczy odleglosc od kotwienia srodkowego (84 m, nie 700)" );
        Global.AirTemperature = 10.f;
    }

    // 6a'. kotwienia środkowe 0418 (jednostronna izolacja) i 0416 (do dźwigara bramki: środkowe podwieszenie na zwisie)
    for( auto const *card : { "0418", "0416" } ) {
        auto a { entry }, b { entry }, c { entry };
        a.name = "n1"; b.name = "n2"; c.name = "n3";
        b.at[ 2 ] = b.rope[ 2 ] = b.pole[ 2 ] = 160.247;
        c.at[ 2 ] = c.rope[ 2 ] = c.pole[ 2 ] = 220.247;
        if( std::string( card ) == "0416" ) { b.pole_type = "4850-3"; b.pole[ 0 ] = 1.2; b.pole[ 1 ] = 8.6; }
        catenary::chain line;
        line.name = "ln"; line.power = "pwr01"; line.type = "C120-2C"; line.middle = "n2"; line.middle_device = card;
        line.suspensions = { "n1", "n2", "n3" };
        std::string text { catenary::write_suspension( a ) + "\n" + catenary::write_suspension( b ) + "\n" + catenary::write_suspension( c ) + "\n" + catenary::write_chain( line ) + "\n" };
        { std::ofstream out { "tests/catenary/td_srodek2_test.scm", std::ios::binary }; out << text; }
        catenary_scene_clear(); errors.clear(); logs.clear(); mock_span_bodies.clear(); mock_span_names.clear();
        cParser directive( "td_srodek2_test.scm endcatenary" );
        catenary_scene_load( directive );
        catenary_scene_resolve();
        std::size_t parts { 0 }, triangles { 0 };
        auto const built { catenary_chain_middle( 0, parts, triangles ) };
        auto middleerrors { 0 };
        for( auto const &error : errors ) { if( error.find( "middle" ) != std::string::npos ) { ++middleerrors; std::printf( "    %s\n", error.c_str() ); } }
        std::printf( "  kotwienie srodkowe %s: czesci %d, trojkaty %d\n", card, static_cast<int>( parts ), static_cast<int>( triangles ) );
        check( built && middleerrors == 0 && triangles > 200 && ( std::string( card ) == "0416" ? parts == 0 : parts >= 2 * 5 ),
            ( std::string( "kotwienie srodkowe " ) + card + ( std::string( card ) == "0416" ? ": liny od dzwigara, bez urzadzen na slupach" : ": urzadzenia na obu slupach (z izolatorem i ze sruba rzymska)" ) ).c_str() );
    }
    std::remove( "tests/catenary/td_srodek2_test.scm" );

    // 6c. wysięgniki rozstawione na konsoli 4890-02 słupa 1611-11: para "umbrella" z punktami "spread" po 0,594 m wzdłuż toru
    {
        catenary_scene_clear(); errors.clear(); logs.clear();
        auto a { entry }, b { entry };
        a.name = "ra"; b.name = "rb"; a.umbrella = b.umbrella = "rz";
        a.has_spread = b.has_spread = true;
        a.at[ 2 ] = a.rope[ 2 ] = 100.247 + 0.594; b.at[ 2 ] = b.rope[ 2 ] = 100.247 - 0.594;
        a.spread[ 0 ] = b.spread[ 0 ] = -2.313; a.spread[ 1 ] = b.spread[ 1 ] = 0.2;
        a.spread[ 2 ] = 100.247 + 0.594; b.spread[ 2 ] = 100.247 - 0.594;
        auto single { entry }; single.name = "rs"; single.pole[ 2 ] = single.at[ 2 ] = single.rope[ 2 ] = 300.0;
        auto const alone { catenary_scene_add( "td_rozstaw.scm", single ) };
        auto const first { catenary_scene_add( "td_rozstaw.scm", a ) };
        auto const second { catenary_scene_add( "td_rozstaw.scm", b ) };
        auto const na { catenary_scene_parts( alone ) }, n1 { catenary_scene_parts( first ) }, n2 { catenary_scene_parts( second ) };
        std::printf( "  rozstawione: osobno %zu czesci, pierwszy %zu (slup, konsole), drugi %zu\n", na, n1, n2 );
        for( auto const &error : errors ) { std::printf( "    %s\n", error.c_str() ); }
        check( errors.empty() && na > 10 && n1 > n2 && n2 > 5 && n1 >= na, "wysiegniki rozstawione: para na konsoli 4890 sklada sie bez bledow (slup i konsole stawia pierwszy)" );
        catenary_scene_remove( second );
        check( errors.empty() && catenary_scene_parts( first ) == n1, "wysiegnik rozstawiony sam na konsoli: po usunieciu drugiego dalej na konsoli ze slupem" );
    }

    // 6b. lina nośna z innego wysięgnika (rozstawione na konsoli): w punkcie m2 przewody przez m2, lina przez h2 1,2 m dalej
    {
        auto a { entry }, b { entry }, c { entry }, h { entry };
        a.name = "m1"; b.name = "m2"; c.name = "m3"; h.name = "h2";
        b.at[ 2 ] = b.rope[ 2 ] = b.pole[ 2 ] = 160.247;
        h.at[ 2 ] = h.rope[ 2 ] = h.pole[ 2 ] = 161.447;
        c.at[ 2 ] = c.rope[ 2 ] = c.pole[ 2 ] = 220.247;
        catenary::chain line;
        line.name = "lr"; line.power = "pwr01"; line.type = "C120-2C";
        line.messenger_from = { { "m2", "h2" } };
        line.suspensions = { "m1", "m2", "m3" };
        auto const written { catenary::write_chain( line ) };
        std::string text { catenary::write_suspension( a ) + "\n" + catenary::write_suspension( b ) + "\n" + catenary::write_suspension( c ) + "\n"
            + catenary::write_suspension( h ) + "\n" + written + "\n" };
        { std::ofstream out { "tests/catenary/td_lina_test.scm", std::ios::binary }; out << text; }
        catenary_scene_clear(); errors.clear(); logs.clear(); mock_span_bodies.clear(); mock_span_names.clear(); mock_deleted_spans = 0;
        cParser directive( "td_lina_test.scm endcatenary" );
        catenary_scene_load( directive );
        catenary_scene_resolve();
        // przęsło 1 kończy przewód w m2 (160.247), linę w h2 (161.447); przęsło 2 odwrotnie
        auto ropes { 0 }, wires { 0 };
        for( auto const &body : mock_span_bodies ) {
            if( body.find( "161.447" ) != std::string::npos ) { ++ropes; }
            if( body.find( "160.247" ) != std::string::npos ) { ++wires; }
        }
        for( auto const &error : errors ) { std::printf( "    %s\n", error.c_str() ); }
        check( written.find( " messenger_from m2 h2 suspensions" ) != std::string::npos && mock_span_bodies.size() == 2 && ropes == 2 && wires == 2 && errors.empty(),
            "lina nosna z innego wysiegnika (messenger_from): oba przesla przy m2 z lina przez h2, przewodem przez m2" );
    }

    // 6. kotwienie ciężarowe na końcu łańcucha: słup i urządzenie, ostatnie przęsło bez wieszaków do uchwytów krańcowych,
    // temperatura przestawia ciężary i przęsło
    {
        auto a { entry }, c { entry };
        a.name = "s0001"; c.name = "s0002";
        c.at[ 2 ] = c.rope[ 2 ] = c.pole[ 2 ] = 160.0;
        catenary::anchor anchor;
        anchor.name = "k1"; anchor.network = "C120-2C"; anchor.pole_type = "1611-11";
        anchor.pole[ 0 ] = -2.4; anchor.pole[ 1 ] = 0.2; anchor.pole[ 2 ] = 184.0;
        anchor.toward[ 0 ] = c.at[ 0 ]; anchor.toward[ 1 ] = c.at[ 1 ]; anchor.toward[ 2 ] = c.at[ 2 ];
        anchor.distance = 700.0; anchor.color = "szary"; anchor.top_color = "zolty";
        catenary::chain line;
        line.name = "l1"; line.power = "pwr01"; line.type = "C120-2C"; line.anchor_end = "k1";
        line.suspensions = { "s0001", "s0002" };
        std::string text { catenary::write_suspension( a ) + "\n" + catenary::write_suspension( c ) + "\n" + catenary::write_anchor( anchor ) + "\n" + catenary::write_chain( line ) + "\n" };
        { std::ofstream out { "tests/catenary/td_kotwa_test.scm", std::ios::binary }; out << text; }
        catenary_scene_clear(); errors.clear(); logs.clear(); mock_span_bodies.clear(); mock_span_names.clear(); mock_deleted_spans = 0;
        Global.AirTemperature = 10.f;
        auto const before { simulation::State.created.size() };
        cParser directive( "td_kotwa_test.scm endcatenary" );
        catenary_scene_load( directive );
        catenary_scene_resolve();
        auto const *result { catenary_anchor_result( 0 ) };
        check( catenary_anchor_size() == 1 && result != nullptr && result->warnings.empty() && catenary_anchor_parts( 0 ) > 30 && simulation::State.created.size() > before + 30,
            ( "kotwienie: slup i urzadzenie w scenerii (" + std::to_string( catenary_anchor_parts( 0 ) ) + " czesci)" ).c_str() );
        // przęsła: s0001-s0002 z wieszakami, s0002-k1 bez (odstęp 0)
        auto good { mock_span_bodies.size() == 2 };
        double end[ 3 ] { 0.0, 0.0, 0.0 };
        if( good ) {
            std::istringstream body { mock_span_bodies.back() };
            std::string skip;
            std::getline( body, skip );
            std::getline( body, skip );
            body >> end[ 0 ] >> end[ 1 ] >> end[ 2 ];
            good = mock_span_bodies.back().find( "0.400 0.000 3 0.040 vis" ) != std::string::npos && mock_span_bodies.front().find( "0.400 4.000 3 0.040 vis" ) != std::string::npos;
            std::printf( "  koniec przewodow przy kotwieniu: %.3f %.3f %.3f\n", end[ 0 ], end[ 1 ], end[ 2 ] );
        }
        // urządzenie od czoła słupa w stronę s0002: rolka 1,75 m, dźwignia, łańcuchy ~1,4 m, więc koniec ok. 3,5-4,5 m od słupa
        good = good && end[ 2 ] < 184.0 - 3.0 && end[ 2 ] > 184.0 - 5.5 && end[ 1 ] > 0.2 + 6.0 && end[ 1 ] < 0.2 + 6.8;
        check( good, "kotwienie: ostatnie przeslo bez wieszakow konczy sie na uchwytach krancowych urzadzenia" );
        auto const warm { result->b };
        mock_span_bodies.clear();
        Global.AirTemperature = -20.f;
        catenary_scene_temperature();
        result = catenary_anchor_result( 0 );
        check( result->b > warm + 0.3 && mock_span_bodies.size() == 2, "mroz: rolka dalej od slupa, przesla lancucha z kotwieniem od nowa" );
        // brakujące ciężary: sieć krótsza, przęsła łańcucha z mniejszym naciągiem
        {
            auto data { *catenary_anchor_get( 0 ) };
            auto const frost { *result };
            data.missing = 4;
            mock_span_bodies.clear();
            catenary_anchor_update( 0, data );
            auto const *thin { catenary_anchor_result( 0 ) };
            auto const tension { std::count_if( mock_span_bodies.begin(), mock_span_bodies.end(), []( std::string const &Body ) { return Body.find( " tension " ) != std::string::npos; } ) };
            std::printf( "  4 brakujace ciezary: naciag %.0f -> %.0f daN, siec krotsza o %.3f m, przesel z naciagiem %d\n", frost.tension, thin->tension, thin->shortening, static_cast<int>( tension ) );
            check( thin->shortening > 0.02 && thin->b > frost.b && tension == 2, "brakujace ciezary: siec krotsza (przekroje z tablicy przewodow), przesla z mniejszym naciagiem" );
            data.missing = 0;
            mock_span_bodies.clear();
            catenary_anchor_update( 0, data );
            result = catenary_anchor_result( 0 );
            check( std::none_of( mock_span_bodies.begin(), mock_span_bodies.end(), []( std::string const &Body ) { return Body.find( " tension " ) != std::string::npos; } ),
                "pelny stos: przesla z naciagiem typu sieci" );
        }
        // kotwienie stale 0403 zamiast ciezarowego: bez ciezarow, przeslo do niego od nowa, z powrotem 0461
        {
            auto data { *catenary_anchor_get( 0 ) };
            data.device = "0403";
            mock_span_bodies.clear();
            catenary_anchor_update( 0, data );
            auto const *fixed { catenary_anchor_result( 0 ) };
            auto const devices { catenary_anchor_devices() };
            auto const listed { std::count_if( devices.begin(), devices.end(), []( catenary_anchor_card const &Item ) { return Item.card == "0403" && Item.fixed; } ) == 1
                && std::count_if( devices.begin(), devices.end(), []( catenary_anchor_card const &Item ) { return Item.card == "0471" && Item.series && false == Item.fixed; } ) == 1
                && std::none_of( devices.begin(), devices.end(), []( catenary_anchor_card const &Item ) { return Item.card.find( '-' ) != std::string::npos && Item.title.empty(); } )
                && std::count_if( devices.begin(), devices.end(), []( catenary_anchor_card const &Item ) { return Item.card == "0461-2x2" && false == Item.title.empty(); } ) == 1 };
            std::printf( "  kotwienie stale: czesci %d, uwagi %d, przesel od nowa %d\n", static_cast<int>( catenary_anchor_parts( 0 ) ), fixed != nullptr ? static_cast<int>( fixed->warnings.size() ) : -1, static_cast<int>( mock_span_bodies.size() ) );
            check( listed && fixed != nullptr && fixed->fixed && fixed->weights == 0 && catenary_anchor_parts( 0 ) > 10 && mock_span_bodies.size() == 2,
                "kotwienie stale 0403 w scenerii: lista kart, bez ciezarow, przesla od nowa" );
            data.device = "0471";
            catenary_anchor_update( 0, data );
            auto const *series { catenary_anchor_result( 0 ) };
            check( series != nullptr && false == series->fixed && series->weights > 0 && series->warnings.empty() && std::abs( series->b - 2.50 ) < 0.6,
                "rolki szeregowe 0471 w scenerii: bez uwag" );
            // rozdzielone 0482 nie ma sieci tego typu: bierze inne ciezarowe (0461), bez bledu
            data.device = "0482";
            catenary_anchor_update( 0, data );
            auto const *fallback { catenary_anchor_result( 0 ) };
            check( fallback != nullptr && fallback->weights > 0 && fallback->rope2.empty() && fallback->warnings.empty(), "rozdzielone 0482 dla sieci z jedna lina: zastepcze urzadzenie ciezarowe" );
            data.device = "0461";
            catenary_anchor_update( 0, data );
            result = catenary_anchor_result( 0 );
        }
        // obrys urządzenia dla edytora: czoło słupa w stronę s0002 na wysokości rolek, rolka ruchoma dalej, kolumna ciężarów pionowo;
        // kotwienie kończy łańcuch l1 (koniec)
        {
            double shape[ 4 ][ 3 ];
            bool start { true };
            auto const has { catenary_anchor_shape( 0, shape ) };
            auto const chain { catenary_anchor_chain( 0, start ) };
            auto const height { result->height + 0.2 };
            auto good { has && chain == 0 && false == start };
            good = good && std::abs( shape[ 0 ][ 1 ] - height ) < 1e-6 && shape[ 0 ][ 2 ] < 184.0 && shape[ 0 ][ 2 ] > 183.5;
            good = good && std::abs( ( shape[ 0 ][ 2 ] - shape[ 1 ][ 2 ] ) - result->b ) < 0.05;
            good = good && std::abs( shape[ 2 ][ 0 ] - shape[ 3 ][ 0 ] ) < 1e-9 && std::abs( shape[ 2 ][ 2 ] - shape[ 3 ][ 2 ] ) < 1e-9 && std::abs( shape[ 3 ][ 1 ] - ( result->s + 0.2 ) ) < 1e-6;
            std::printf( "  obrys kotwienia: czolo %.2f %.2f %.2f, rolka ruchoma %.2f %.2f %.2f, ciezary %.2f..%.2f\n", shape[ 0 ][ 0 ], shape[ 0 ][ 1 ], shape[ 0 ][ 2 ], shape[ 1 ][ 0 ], shape[ 1 ][ 1 ], shape[ 1 ][ 2 ], shape[ 3 ][ 1 ], shape[ 2 ][ 1 ] );
            check( good, "kotwienie: obrys dla edytora (czolo slupa, rolka ruchoma, kolumna ciezarow) i lancuch, ktory konczy" );
        }
        // kotwienie na istniejącym słupie (podwieszenie s0003 za s0002): bez własnego słupa, za słupem idzie po przesunięciu
        {
            auto pole3 { entry };
            pole3.name = "s0003"; pole3.at[ 2 ] = pole3.rope[ 2 ] = pole3.pole[ 2 ] = 184.0;
            auto const carrier { catenary_scene_add( "td_kotwa_test.scm", pole3 ) };
            catenary::anchor on { anchor };
            on.name = "k2"; on.on = "s0003";
            auto const partsown { catenary_anchor_parts( 0 ) };
            Global.AirTemperature = 10.f;
            catenary_scene_temperature();
            auto const index { catenary_anchor_add( "td_kotwa_test.scm", on ) };
            auto const partson { catenary_anchor_parts( index ) };
            auto moved { *catenary_scene_get( carrier ) };
            moved.pole[ 2 ] = moved.at[ 2 ] = moved.rope[ 2 ] = 186.0;
            auto const before { mock_deleted_parts };
            catenary_scene_update( carrier, moved );
            auto const written { catenary::write_anchor( *catenary_anchor_get( index ) ) };
            check( partson > 0 && partson + 2 == partsown && mock_deleted_parts >= before + static_cast<int>( partson ) && written.find( " on s0003 " ) != std::string::npos,
                ( "kotwienie na istniejacym slupie: bez slupa i fundamentu (" + std::to_string( partson ) + " czesci), idzie za slupem, zapis z \"on\"" ).c_str() );
        }
        Global.AirTemperature = 15.f;
        std::remove( "tests/catenary/td_kotwa_test.scm" );
    }
    for( auto const &line : errors ) { std::printf( "  %s\n", line.c_str() ); }
    std::printf( failures == 0 ? "wszystko OK\n" : "BLEDY: %d\n", failures );
    return failures;
}
