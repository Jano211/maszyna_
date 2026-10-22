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
                }
            }
            check( good, "wysiegnik na slupie bramki 3031-1: bez modelu slupa, mocowania z wyk. 04 pod dzwigarem" );
        }
    }
    for( auto const &line : errors ) { std::printf( "  %s\n", line.c_str() ); }
    std::printf( failures == 0 ? "wszystko OK\n" : "BLEDY: %d\n", failures );
    return failures;
}
