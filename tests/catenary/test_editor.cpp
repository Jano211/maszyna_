// Test części sieci trakcyjnej używanej przez edytor scenerii: tablice rozstawienia (traction_layout.txt), zapis wpisów
// podwieszeń, dodawanie / zmiana / usuwanie podwieszeń na żywo z atrapą klas symulatora
#include "stdafx.h"
#include "world/catenary_scene.h"
#include "world/catenary_overlap.h"
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
    check( std::abs( tables.messenger_outward( 600.0 ) - 0.07 ) < 1e-9 && std::abs( tables.messenger_outward( 800.0 ) - 0.06 ) < 1e-9 && tables.messenger_outward( 0.0 ) == 0.0
        && tables.messenger_outward( 5000.0 ) == 0.0 && std::abs( tables.messenger_outward( 200.0 ) - 0.10 ) < 1e-9, "lina nosna na zewnatrz luku: tablica curve_messenger (R 600 - 7 cm, R 800 - 6 cm, prosta 0)" );
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
    {
        // górne mocowanie przesunięte o ±0,3 m (stay_shift): składa się, odciąg dobrany z rodziny, zapis i odczyt
        auto const stayof = []( catenary::cantilever_result const &R ) { auto const *p { R.find( "stay" ) }; return p != nullptr ? p->part : std::string( "-" ); };
        catenary::cantilever_result plain, up, down;
        auto shifted { moved };
        auto const a { catenary_scene_probe( moved, plain ) };
        shifted.stay_shift = 0.30;
        auto const b { catenary_scene_probe( shifted, up ) };
        shifted.stay_shift = -0.30;
        auto const c { catenary_scene_probe( shifted, down ) };
        std::printf( "  odciag: karta %s (uwag %zu), +0,30 %s (uwag %zu), -0,30 %s (uwag %zu)\n", stayof( plain ).c_str(), plain.warnings.size(), stayof( up ).c_str(), up.warnings.size(), stayof( down ).c_str(), down.warnings.size() );
        for( auto const &w : up.warnings ) { std::printf( "    +: %s\n", w.c_str() ); }
        for( auto const &w : down.warnings ) { std::printf( "    -: %s\n", w.c_str() ); }
        auto const mountup { up.values.count( "mount.stay" ) ? up.values.at( "mount.stay" ) : 0.0 }, mountplain { plain.values.count( "mount.stay" ) ? plain.values.at( "mount.stay" ) : 0.0 };
        auto const text { catenary::write_suspension( shifted ) };
        std::vector<std::string> tokens; std::istringstream words { text }; for( std::string w; words >> w; ) { tokens.push_back( w ); }
        catenary::suspension_file parsed; std::vector<std::string> problems;
        catenary::parse_suspensions( tokens, "zapis", parsed, problems );
        check( a && b && c && std::abs( mountup - mountplain - 0.30 ) < 0.02 && text.find( " stay_shift -0.300" ) != std::string::npos
            && problems.empty() && parsed.entries.size() == 1 && parsed.entries.front().stay_shift == -0.30, "gorne mocowanie +-0,30 m (stay_shift): sklada sie, zapis i odczyt" );
    }
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
        {
            // półbramka 1901 (słup z dźwigarem w jednym modelu): jeden model, dźwigar od osi słupa na Hk, zwisy 4840; przy dźwigarze
            // skośnym do toru o 5° zwis równo z dźwigarem (pole.turn)
            auto const angle { 5.0 * 3.14159265358979323846 / 180.0 };
            catenary::gantry semi;
            semi.name = "pb1"; semi.beam = "polbramka_1901_10m"; semi.masts[ 0 ] = "polbramka_1901_10m";
            semi.points[ 0 ][ 0 ] = 0.0; semi.points[ 0 ][ 1 ] = 0.2; semi.points[ 0 ][ 2 ] = 80.0;
            semi.points[ 1 ][ 0 ] = 10.0 * std::cos( angle ); semi.points[ 1 ][ 1 ] = 0.2; semi.points[ 1 ][ 2 ] = 80.0 + 10.0 * std::sin( angle );
            errors.clear();
            auto const before { simulation::State.created.size() };
            auto const index { catenary_gantry_add( "td_bramka_test.scm", semi ) };
            auto good { catenary_gantry_parts( index ) == 1 && simulation::State.created.size() == before + 1 && errors.empty()
                && simulation::State.created[ before ].entry.find( "polbramki/polbramka_1901_10m.t3d" ) != std::string::npos };
            for( auto const &e : errors ) { std::printf( "  blad: %s\n", e.c_str() ); }
            catenary::suspension drop;
            drop.card = 1112; drop.execution = 1; drop.range_max = 1000;
            glm::dvec3 const pole { 6.0 * std::cos( angle ), 0.2 + 7.835, 80.0 + 6.0 * std::sin( angle ) };
            drop.has_at = true; drop.at[ 0 ] = pole.x + 2.0; drop.at[ 1 ] = 0.2 + 5.6; drop.at[ 2 ] = pole.z;
            drop.has_rope = true; drop.rope[ 0 ] = pole.x + 2.0; drop.rope[ 1 ] = 0.2 + 5.6 + 1.7; drop.rope[ 2 ] = pole.z;
            drop.has_pole = true; drop.pole_type = "4840-3"; drop.pole[ 0 ] = pole.x; drop.pole[ 1 ] = pole.y; drop.pole[ 2 ] = pole.z;
            catenary::cantilever_result probe;
            auto const assembled { catenary_scene_probe( drop, probe ) };
            for( auto const &warning : probe.warnings ) { std::printf( "  uwaga (zwis 4840): %s\n", warning.c_str() ); }
            auto const turn { probe.values.count( "pole.turn" ) ? probe.values.at( "pole.turn" ) : 0.0 };
            std::printf( "  polbramka: zwis 4840-3 obrocony o %.2f st.\n", turn );
            good = good && assembled && std::abs( std::abs( turn ) - 5.0 ) < 0.01;
            // ESN-544 też pasuje do 1901 (Jan 2026-10-30)
            if( auto const *catalog { catenary_scene_catalog() }; catalog != nullptr ) {
                auto const *beam { catalog->beam( "polbramka_1901_10m" ) };
                good = good && beam != nullptr && catenary::cantilever_catalog::drop_fits( *beam, "ESN-544" ) && catenary::cantilever_catalog::drop_fits( *beam, "4840" )
                    && false == catenary::cantilever_catalog::drop_fits( *beam, "4850" ) && catalog->drops_interchangeable( "4840", "ESN-544" );
            }
            catenary_gantry_remove( index );
            check( good, "polbramka 1901: jeden model, zwis 4840 na Hk, przy skosie 5 st. zwis rowno z dzwigarem" );
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
        // obrót wysięgników od temperatury: przy -20 °C lina 60 m od środkowego przesuwa się 60·17e-6·30 = 3,06 cm do niego,
        // m1 i m3 obracają się w przeciwne strony, m2 (środkowe) stoi; przy +10 °C wszystkie w swojej płaszczyźnie
        {
            auto const s1 { catenary_scene_swing( 0 ) }, s2 { catenary_scene_swing( 1 ) }, s3 { catenary_scene_swing( 2 ) };
            std::printf( "  obrot wysiegnikow przy -20 C: m1 %.3f, m2 %.3f, m3 %.3f st.\n", s1, s2, s3 );
            auto const spans { mock_span_bodies.size() };
            Global.AirTemperature = 40.f;
            catenary_scene_temperature();
            auto const w1 { catenary_scene_swing( 0 ) }, w3 { catenary_scene_swing( 2 ) };
            std::printf( "  przy +40 C: m1 %.3f, m3 %.3f st., przesla przebudowane %d\n", w1, w3, static_cast<int>( mock_span_bodies.size() - spans ) );
            Global.AirTemperature = 10.f;
            catenary_scene_temperature();
            auto const z1 { catenary_scene_swing( 0 ) }, z3 { catenary_scene_swing( 2 ) };
            check( s1 * s3 < 0.0 && std::abs( s2 ) < 1e-9 && std::abs( std::abs( s1 ) - std::abs( s3 ) ) < 0.05 && std::abs( s1 ) > 0.3 && std::abs( s1 ) < 1.5
                && w1 * s1 < 0.0 && w3 * s3 < 0.0 && std::abs( z1 ) < 1e-9 && std::abs( z3 ) < 1e-9 && mock_span_bodies.size() > spans,
                "obrot wysiegnikow od temperatury: od kotwienia srodkowego, przeciwnie po obu stronach, zero przy +10 C, przesla od nowa" );
        }
        Global.AirTemperature = 10.f;
    }

    // 6a''. przęsło naprężenia: dwa łańcuchy z jednym wspólnym przęsłem (słupy na z = 60 i 120, przewody 0,3 m obok siebie):
    // wczytane - bieżnia wspólna łączona raz po wczytaniu; zasilacze różne - izolowane; w edytorze nowe przęsło dostaje "parallel"
    {
        auto a { entry }, b { entry }, c { entry }, d { entry }, e { entry }, f { entry };
        a.name = "o1"; b.name = "o2"; c.name = "o3"; d.name = "o4"; e.name = "o5"; f.name = "o6";
        auto const place = [ & ]( catenary::suspension &S, double const Z, double const Shift ) {
            S.at[ 2 ] = S.rope[ 2 ] = S.pole[ 2 ] = Z;
            S.at[ 0 ] += Shift; S.rope[ 0 ] += Shift;
        };
        place( a, 0.0, 0.0 ); place( b, 60.0, 0.0 ); place( c, 120.0, 0.0 );
        place( d, 60.0, 0.3 ); place( e, 120.0, 0.3 ); place( f, 180.0, 0.3 );
        catenary::chain one, two;
        one.name = "na"; one.power = "pwr01"; one.type = "C120-2C"; one.suspensions = { "o1", "o2", "o3" };
        two.name = "nb"; two.power = "pwr02"; two.type = "C120-2C"; two.suspensions = { "o4", "o5", "o6" };
        std::string text;
        for( auto const *item : { &a, &b, &c, &d, &e, &f } ) { text += catenary::write_suspension( *item ) + "\n"; }
        text += catenary::write_chain( one ) + "\n" + catenary::write_chain( two ) + "\n";
        { std::ofstream out { "tests/catenary/td_naprezenie_test.scm", std::ios::binary }; out << text; }
        catenary_scene_clear(); errors.clear(); logs.clear(); mock_span_bodies.clear(); mock_span_names.clear();
        cParser directive( "td_naprezenie_test.scm endcatenary" );
        catenary_scene_load( directive );
        catenary_scene_resolve();
        auto const overlaps { catenary_chain_overlaps( 0 ) };
        // po wczytaniu (przed InitTraction) łączy catenary_scene_loaded() - wpis w logu; gdy sceneria już była zainicjowana, przęsło
        // drugiego łańcucha ma "parallel" od razu w treści
        auto const inbody { std::any_of( mock_span_bodies.begin(), mock_span_bodies.end(), []( std::string const &B ) { return B.find( " parallel na_2" ) != std::string::npos; } ) };
        auto const linked { inbody || std::any_of( logs.begin(), logs.end(), []( std::string const &L ) { return L.find( "tension overlaps" ) != std::string::npos; } ) };
        std::printf( "  naprezenie: nakladek %zu, log %d, bledy %zu\n", overlaps.size(), linked ? 1 : 0, errors.size() );
        for( auto const &o : overlaps ) { std::printf( "    z %zu: przesel %zu (pierwsze %zu), krzyzowe %zu, zwarte %d\n", o.other, o.spans.size(), o.spans.empty() ? 99 : o.spans.front(), o.crossing.size(), o.connected ? 1 : 0 ); }
        for( auto const &e : errors ) { std::printf( "    blad: %s\n", e.c_str() ); }
        auto good { overlaps.size() == 1 && overlaps.front().other == 1 && overlaps.front().spans == std::vector<std::size_t>{ 1 } && overlaps.front().other_spans == std::vector<std::size_t>{ 0 }
            && overlaps.front().crossing.empty() && false == overlaps.front().connected && linked && errors.empty() };
        // edytor: łańcuch nb od nowa - jego wspólne przęsło dostaje "parallel na_2"
        mock_span_bodies.clear(); mock_span_names.clear();
        auto changed { *catenary_chain_get( 1 ) };
        changed.power = "pwr01";
        catenary_chain_update( 1, changed );
        auto const index { std::find( mock_span_names.begin(), mock_span_names.end(), std::string( "nb_1" ) ) };
        good = good && index != mock_span_names.end() && mock_span_bodies[ static_cast<std::size_t>( index - mock_span_names.begin() ) ].find( " parallel na_2" ) != std::string::npos
            && catenary_chain_overlaps( 1 ).size() == 1 && catenary_chain_overlaps( 1 ).front().connected;
        // jedno wspólne przęsło (czterosłupowe): łączniki przy obu słupach krzyżowych, rysuje łańcuch o wyższym numerze
        auto const connectors { catenary_chain_connectors( 1 ) }, connectorsother { catenary_chain_connectors( 0 ) };
        std::printf( "  laczniki 0640 przy jednym wspolnym przesle: %zu trojkatow (drugi lancuch %zu)\n", connectors, connectorsother );
        good = good && connectors > 0 && connectorsother == 0;
        {
            auto isolated { *catenary_chain_get( 1 ) };
            isolated.isolated[ 0 ] = true;
            catenary_chain_update( 1, isolated );
            good = good && catenary_chain_connectors( 1 ) == 0 && false == catenary_chain_overlaps( 1 ).front().connected;
            isolated.isolated[ 0 ] = false;
            catenary_chain_update( 1, isolated );
        }
        // dwa wspólne przęsła (słup krzyżowy na z = 120): łańcuch nb przedłużony wstecz, zwarte - łącznik 0640 bez błędów
        auto g { entry }; g.name = "o7"; place( g, 0.0, 0.3 );
        catenary_scene_add( "td_naprezenie_test.scm", g );
        auto longer { *catenary_chain_get( 1 ) };
        longer.suspensions = { "o7", "o4", "o5", "o6" };
        catenary_chain_update( 1, longer );
        auto const twospans { catenary_chain_overlaps( 1 ) };
        if( false == twospans.empty() ) { std::printf( "  dwa wspolne: przesel %zu, krzyzowe %zu, zwarte %d\n", twospans.front().spans.size(), twospans.front().crossing.size(), twospans.front().connected ? 1 : 0 ); }
        good = good && twospans.size() == 1 && twospans.front().spans.size() == 2 && twospans.front().crossing.size() == 1 && twospans.front().connected && errors.empty();
        std::remove( "tests/catenary/td_naprezenie_test.scm" );
        check( good, "przeslo naprezenia: wspolne przeslo wykryte, biezna wspolna po wczytaniu i w edytorze, izolowane / zwarte wg zasilacza, slup krzyzowy" );
    }

    // 6a4. uszynienie grupowe: uchwyty 6120-01 na górze słupów 1611-11 (wykonanie z danych słupa), zapis i odczyt
    {
        auto a { entry }, b { entry }, c { entry };
        a.name = "e1"; b.name = "e2"; c.name = "e3";
        b.at[ 2 ] = b.rope[ 2 ] = b.pole[ 2 ] = 60.0; c.at[ 2 ] = c.rope[ 2 ] = c.pole[ 2 ] = 120.0;
        catenary::chain line;
        line.name = "ne"; line.power = "pwr01"; line.type = "C120-2C"; line.suspensions = { "e1", "e2", "e3" };
        catenary::earthing earth;
        earth.name = "u1"; earth.poles = { "e1", "e2", "e3" };
        auto const written { catenary::write_earthing( earth ) };
        std::string text;
        for( auto const *item : { &a, &b, &c } ) { text += catenary::write_suspension( *item ) + "\n"; }
        text += catenary::write_chain( line ) + "\n" + written + "\n";
        std::vector<std::string> tokens; std::istringstream words { text };
        for( std::string word; words >> word; ) { tokens.push_back( word ); }
        catenary::suspension_file parsed; std::vector<std::string> problems;
        catenary::parse_suspensions( tokens, "uszynienie", parsed, problems );
        auto good { problems.empty() && parsed.earthings.size() == 1 && catenary::write_earthing( parsed.earthings.front() ) == written };
        { std::ofstream out { "tests/catenary/td_uszynienie_test.scm", std::ios::binary }; out << text; }
        catenary_scene_clear(); errors.clear(); logs.clear();
        auto const before { simulation::State.created.size() };
        cParser directive( "td_uszynienie_test.scm endcatenary" );
        catenary_scene_load( directive );
        catenary_scene_resolve();
        auto holders { 0 };
        for( auto i { before }; i < simulation::State.created.size(); ++i ) { if( simulation::State.created[ i ].entry.find( "components/6120-01.t3d" ) != std::string::npos ) { ++holders; } }
        std::printf( "  uszynienie: uchwytow %d, czesci %zu\n", holders, catenary_earthing_parts( 0 ) );
        for( auto const &l : logs ) { if( l.find( "earthing" ) != std::string::npos ) { std::printf( "    %s\n", l.c_str() ); } }
        good = good && holders == 3 && catenary_earthing_parts( 0 ) == 3 && errors.empty();
        // edycja: uszynienie przez dwa słupy - uchwyty od nowa
        auto shorter { *catenary_earthing_get( 0 ) };
        shorter.poles = { "e1", "e2" };
        good = good && catenary_earthing_update( 0, shorter ) && catenary_earthing_parts( 0 ) == 2;
        std::remove( "tests/catenary/td_uszynienie_test.scm" );
        check( good, "uszynienie grupowe: zapis i odczyt, uchwyty 6120-01 na slupach 1611-11, edycja" );
    }

    // 6a5. uszynienie przez słup z kilkoma wysięgnikami (para rozstawionych + trzeci w osi słupa): jeden uchwyt na słup, obrócony
    // jak słup (płaszczyzna przez konsolę), nie w stronę rozstawionych wysięgników
    {
        auto a { entry }, b { entry }, c { entry }, d { entry };
        a.name = "r1"; b.name = "r2"; c.name = "r3"; d.name = "r4";
        for( auto *item : { &a, &b, &c } ) { item->pole[ 2 ] = 0.0; }
        a.at[ 2 ] = a.rope[ 2 ] = -1.2; b.at[ 2 ] = b.rope[ 2 ] = 1.2; c.at[ 2 ] = c.rope[ 2 ] = 0.0;
        a.umbrella = b.umbrella = "rp";
        a.has_spread = b.has_spread = true;
        a.spread[ 0 ] = b.spread[ 0 ] = -2.313; a.spread[ 1 ] = b.spread[ 1 ] = 0.2; a.spread[ 2 ] = -1.2; b.spread[ 2 ] = 1.2;
        c.shared_pole = true;
        d.at[ 2 ] = d.rope[ 2 ] = d.pole[ 2 ] = 60.0;
        catenary::chain line;
        line.name = "nr"; line.power = "pwr01"; line.type = "C120-2C"; line.suspensions = { "r1", "r4" };
        catenary::earthing earth;
        earth.name = "u1"; earth.poles = { "r1", "r2", "r3", "r4" };
        std::string text;
        for( auto const *item : { &a, &b, &c, &d } ) { text += catenary::write_suspension( *item ) + "\n"; }
        text += catenary::write_chain( line ) + "\n" + catenary::write_earthing( earth ) + "\n";
        { std::ofstream out { "tests/catenary/td_uszynienie2_test.scm", std::ios::binary }; out << text; }
        catenary_scene_clear(); errors.clear(); logs.clear(); mock_span_bodies.clear(); mock_span_names.clear();
        auto const before { simulation::State.created.size() };
        cParser directive( "td_uszynienie2_test.scm endcatenary" );
        catenary_scene_load( directive );
        catenary_scene_resolve();
        std::vector<std::string> angles;
        for( auto i { before }; i < simulation::State.created.size(); ++i ) {
            auto const &text { simulation::State.created[ i ].entry };
            if( text.find( "components/6120-01.t3d" ) == std::string::npos ) { continue; }
            auto const at { text.find( " angles " ) };
            angles.push_back( at == std::string::npos ? std::string() : text.substr( at, text.find( " endmodel" ) - at ) );
        }
        std::printf( "  uszynienie na slupie z kilkoma wysiegnikami: uchwytow %zu, czesci %zu\n", angles.size(), catenary_earthing_parts( 0 ) );
        for( auto const &item : angles ) { std::printf( "   %s\n", item.c_str() ); }
        // lina w przęśle r1-r4 (para rozstawionych to jeden słup)
        auto const rope { std::find( mock_span_names.begin(), mock_span_names.end(), std::string( "nr_1" ) ) != mock_span_names.end() };
        auto const good { angles.size() == 2 && catenary_earthing_parts( 0 ) == 2 && angles[ 0 ] == angles[ 1 ] && rope };
        std::remove( "tests/catenary/td_uszynienie2_test.scm" );
        check( good, "uszynienie: jeden uchwyt na slup z kilkoma wysiegnikami, obrot slupa" );
    }

    // 6a6. druga sieć wysięgnika 102x (dwie sieci w przęśle naprężenia): łańcuch "nb" idzie przez drugi przewód i drugą linę
    // wysięgnika s2, który składa łańcuch "na"; zapis i odczyt "second_net"
    {
        auto a { entry }, b { entry }, c { entry }, d { entry }, f { entry };
        a.name = "s1"; b.name = "s2"; c.name = "s3"; d.name = "s4"; f.name = "s5";
        b.card = 1024; b.execution = 1; b.has_rope = false;
        for( auto *item : { &a, &d } ) { item->at[ 2 ] = item->rope[ 2 ] = item->pole[ 2 ] = 40.0; }
        for( auto *item : { &c, &f } ) { item->at[ 2 ] = item->rope[ 2 ] = item->pole[ 2 ] = 160.0; }
        d.at[ 0 ] = d.rope[ 0 ] = f.at[ 0 ] = f.rope[ 0 ] = 0.5; d.pole[ 0 ] = f.pole[ 0 ] = -2.5;
        d.pole[ 2 ] += 3.0; f.pole[ 2 ] += 3.0;
        catenary::chain one, two;
        one.name = "na"; one.power = "pwr01"; one.type = "C120-2C"; one.suspensions = { "s1", "s2", "s3" };
        two.name = "nb"; two.power = "pwr01"; two.type = "C120-2C"; two.suspensions = { "s4", "s2", "s5" }; two.second_net = { "s2" };
        auto const written { catenary::write_chain( two ) };
        std::string text;
        for( auto const *item : { &a, &b, &c, &d, &f } ) { text += catenary::write_suspension( *item ) + "\n"; }
        text += catenary::write_chain( one ) + "\n" + written + "\n";
        std::vector<std::string> tokens; std::istringstream words { text };
        for( std::string word; words >> word; ) { tokens.push_back( word ); }
        catenary::suspension_file parsed; std::vector<std::string> problems;
        catenary::parse_suspensions( tokens, "druga_siec", parsed, problems );
        auto good { problems.empty() && parsed.chains.size() == 2 && catenary::write_chain( parsed.chains.back() ) == written && written.find( " second_net s2" ) != std::string::npos };
        { std::ofstream out { "tests/catenary/td_druga_siec_test.scm", std::ios::binary }; out << text; }
        catenary_scene_clear(); errors.clear(); logs.clear(); mock_span_bodies.clear(); mock_span_names.clear();
        cParser directive( "td_druga_siec_test.scm endcatenary" );
        catenary_scene_load( directive );
        catenary_scene_resolve();
        std::size_t s2 { catenary_scene_size() };
        for( std::size_t index = 0; index < catenary_scene_size(); ++index ) { if( catenary_scene_get( index ) != nullptr && catenary_scene_get( index )->name == "s2" ) { s2 = index; } }
        double wire[ 3 ], rope[ 3 ];
        bool hasrope { false };
        auto const net { catenary_scene_second_net( s2, wire, rope, hasrope ) };
        // przęsło nb_1 (s4 - s2) kończy się w drugim przewodzie, na_1 (s1 - s2) w pierwszym
        auto const body = [ & ]( std::string const &Name ) {
            for( std::size_t k = mock_span_names.size(); k-- > 0; ) { if( mock_span_names[ k ] == Name ) { return mock_span_bodies[ k ]; } }
            return std::string();
        };
        std::istringstream first { body( "na_1" ) }, second { body( "nb_1" ) };
        std::string skip;
        std::getline( first, skip ); std::getline( second, skip );
        double ax, ay, az, bx, by, bz, wa[ 3 ], wb[ 3 ];
        first >> ax >> ay >> az >> wa[ 0 ] >> wa[ 1 ] >> wa[ 2 ];
        second >> bx >> by >> bz >> wb[ 0 ] >> wb[ 1 ] >> wb[ 2 ];
        std::printf( "  druga siec: %d (lina %d), przewod B %.3f %.3f %.3f, na_1 konczy %.3f %.3f, nb_1 konczy %.3f %.3f, lancuch %d\n", net ? 1 : 0, hasrope ? 1 : 0,
            wire[ 0 ], wire[ 1 ], wire[ 2 ], wa[ 0 ], wa[ 1 ], wb[ 0 ], wb[ 1 ], catenary_scene_second_net_chain( s2 ) );
        for( auto const &e : errors ) { std::printf( "    blad: %s\n", e.c_str() ); }
        // 102x trzyma tylko drugą linę: przewód B idzie prosto od s4 do s5 (x 0,5), lina B przez drugi uchwyt liny
        std::string line;
        std::istringstream third { body( "nb_1" ) };
        std::getline( third, skip );
        double r[ 6 ];
        third >> r[ 0 ] >> r[ 0 ] >> r[ 0 ] >> r[ 0 ] >> r[ 0 ] >> r[ 0 ] >> r[ 0 ] >> r[ 0 ] >> r[ 0 ] >> r[ 3 ] >> r[ 4 ] >> r[ 5 ];
        std::printf( "  nb_1 lina konczy %.3f %.3f (uchwyt B %.3f %.3f)\n", r[ 3 ], r[ 4 ], rope[ 0 ], rope[ 1 ] );
        good = good && net && hasrope && std::abs( wb[ 0 ] - 0.5 ) < 0.002 && std::abs( wa[ 0 ] - 0.687 ) < 0.002 && std::abs( r[ 3 ] - rope[ 0 ] ) < 0.002 && std::abs( r[ 4 ] - rope[ 1 ] ) < 0.002
            && catenary_scene_second_net_chain( s2 ) == 1;
        check( good, "druga siec 102x: lina nb przez druga line wysiegnika, przewod nb prosto miedzy slupami B, zapis i odczyt" );
        // sieci z jednym przewodem (C95-C): każda ma swój uchwyt przewodu na 102x (A - wire_a 0,04 od słupa, B - wire_b)
        {
            auto onea { one }, twob { two };
            onea.type = twob.type = "C95-C";
            std::string single;
            for( auto const *item : { &a, &b, &c, &d, &f } ) { single += catenary::write_suspension( *item ) + "\n"; }
            single += catenary::write_chain( onea ) + "\n" + catenary::write_chain( twob ) + "\n";
            { std::ofstream out { "tests/catenary/td_druga_siec_test.scm", std::ios::binary }; out << single; }
            catenary_scene_clear(); errors.clear(); logs.clear(); mock_span_bodies.clear(); mock_span_names.clear();
            cParser again( "td_druga_siec_test.scm endcatenary" );
            catenary_scene_load( again );
            catenary_scene_resolve();
            std::istringstream ia { body( "na_1" ) }, ib { body( "nb_1" ) };
            std::getline( ia, skip ); std::getline( ib, skip );
            ia >> ax >> ay >> az >> wa[ 0 ] >> wa[ 1 ] >> wa[ 2 ];
            ib >> bx >> by >> bz >> wb[ 0 ] >> wb[ 1 ] >> wb[ 2 ];
            std::printf( "  jeden przewod: na_1 konczy %.3f, nb_1 konczy %.3f\n", wa[ 0 ], wb[ 0 ] );
            check( std::abs( wa[ 0 ] - 0.647 ) < 0.002 && std::abs( wb[ 0 ] - 0.727 ) < 0.002, "druga siec 102x przy sieciach z jednym przewodem: kazda w swoim uchwycie przewodu" );
        }
        std::remove( "tests/catenary/td_druga_siec_test.scm" );
    }

    // 6a3. izolator sekcyjny w połowie przęsła za i2: zapis i odczyt, przęsła za nim z jego zasilacza
    {
        auto a { entry }, b { entry }, c { entry }, d { entry };
        a.name = "i1"; b.name = "i2"; c.name = "i3"; d.name = "i4";
        b.at[ 2 ] = b.rope[ 2 ] = b.pole[ 2 ] = 60.0; c.at[ 2 ] = c.rope[ 2 ] = c.pole[ 2 ] = 120.0; d.at[ 2 ] = d.rope[ 2 ] = d.pole[ 2 ] = 180.0;
        catenary::chain line;
        line.name = "ni"; line.power = "pwr01"; line.type = "C120-2C"; line.suspensions = { "i1", "i2", "i3", "i4" };
        line.insulators.push_back( { "i2", "ceramic", "pwr09", 12.5 } );
        auto const written { catenary::write_chain( line ) };
        std::string text;
        for( auto const *item : { &a, &b, &c, &d } ) { text += catenary::write_suspension( *item ) + "\n"; }
        text += written + "\n";
        std::vector<std::string> tokens; std::istringstream words { text };
        for( std::string word; words >> word; ) { tokens.push_back( word ); }
        catenary::suspension_file parsed; std::vector<std::string> problems;
        catenary::parse_suspensions( tokens, "izolator", parsed, problems );
        auto good { problems.empty() && parsed.chains.size() == 1 && catenary::write_chain( parsed.chains.front() ) == written && written.find( " insulator i2 ceramic pwr09" ) != std::string::npos };
        { std::ofstream out { "tests/catenary/td_izolator_test.scm", std::ios::binary }; out << text; }
        catenary_scene_clear(); errors.clear(); mock_span_bodies.clear(); mock_span_names.clear();
        cParser directive( "td_izolator_test.scm endcatenary" );
        catenary_scene_load( directive );
        catenary_scene_resolve();
        good = good && mock_span_bodies.size() == 3 && mock_span_bodies[ 0 ].find( "pwr01" ) == 0 && mock_span_bodies[ 1 ].find( "pwr01" ) == 0 && mock_span_bodies[ 2 ].find( "pwr09" ) == 0 && errors.empty();
        std::remove( "tests/catenary/td_izolator_test.scm" );
        check( good, "izolator sekcyjny: zapis i odczyt, przesla za nim zasila jego zasilacz" );
    }

    // 6a'. kotwienia środkowe 0418 (jednostronna izolacja) i 0416 (do dźwigara bramki: środkowe podwieszenie na zwisie)
    for( auto const *card : { "0418", "0416", "tee" } ) {
        auto a { entry }, b { entry }, c { entry };
        a.name = "n1"; b.name = "n2"; c.name = "n3";
        // wysięgnik teowy (121) na słupie środkowym: lina kotwienia przez obejmę, bez zacisków 1 m
        auto const tee { std::string( card ) == "tee" };
        if( tee ) { b.card = 121; b.execution = 3; card = "0406"; }
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
        std::printf( "  kotwienie srodkowe %s%s: czesci %d, trojkaty %d\n", card, tee ? " (teownik)" : "", static_cast<int>( parts ), static_cast<int>( triangles ) );
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

    // 6b''. uchwyt liny wyżej (above_pole): wysięgnik samej liny 129x 0,60 m nad liną, mocowania ponad najwyższym położeniem słupa
    {
        catenary_scene_clear(); errors.clear(); logs.clear();
        catenary_scene_resolve();
        auto high { entry };
        high.name = "uh"; high.card = 1290; high.execution = 1; high.above_pole = true;
        high.rope[ 1 ] += 0.60;
        auto const text { catenary::write_suspension( high ) };
        std::vector<std::string> tokens; std::istringstream words { text }; for( std::string w; words >> w; ) { tokens.push_back( w ); }
        catenary::suspension_file parsed; std::vector<std::string> problems;
        catenary::parse_suspensions( tokens, "zapis", parsed, problems );
        auto const added { catenary_scene_add( "td_wyzej_test.scm", high ) };
        auto lowered { false };
        for( auto const &line : logs ) { if( line.find( "lowered" ) != std::string::npos ) { lowered = true; } }
        for( auto const &error : errors ) { std::printf( "    %s\n", error.c_str() ); }
        check( problems.empty() && text.find( " above_pole" ) != std::string::npos && parsed.entries.size() == 1 && parsed.entries.front().above_pole
            && catenary_scene_parts( added ) > 5 && false == lowered && errors.empty(),
            "uchwyt liny wyzej: zapis above_pole, wysiegnik samej liny sklada sie bez sciecia mocowan do slupa" );
        catenary_scene_clear();
    }

    // 6b'. dwa wysięgi pomocnicze z wysięgnikiem samej liny między nimi (oba messenger_from tego samego): lina w ich punktach
    // to lina trzymającego przesunięta wzdłuż łańcucha - przęsło między nimi prosto przez siodło, linki trafiają w końce lin
    {
        auto a { entry }, p { entry }, q { entry }, c { entry }, h { entry };
        a.name = "s1"; p.name = "sa"; q.name = "sb"; c.name = "s3"; h.name = "sh";
        p.at[ 2 ] = p.rope[ 2 ] = p.pole[ 2 ] = 159.462;
        q.at[ 2 ] = q.rope[ 2 ] = q.pole[ 2 ] = 161.032;
        h.at[ 2 ] = h.rope[ 2 ] = h.pole[ 2 ] = 160.247;
        h.rope[ 1 ] += 0.05;
        c.at[ 2 ] = c.rope[ 2 ] = c.pole[ 2 ] = 220.247;
        catenary::chain line;
        line.name = "lp"; line.power = "pwr01"; line.type = "C120-2C";
        line.messenger_from = { { "sa", "sh" }, { "sb", "sh" } };
        line.suspensions = { "s1", "sa", "sb", "s3" };
        std::string text { catenary::write_suspension( a ) + "\n" + catenary::write_suspension( p ) + "\n" + catenary::write_suspension( q ) + "\n"
            + catenary::write_suspension( c ) + "\n" + catenary::write_suspension( h ) + "\n" + catenary::write_chain( line ) + "\n" };
        { std::ofstream out { "tests/catenary/td_lina2_test.scm", std::ios::binary }; out << text; }
        catenary_scene_clear(); errors.clear(); logs.clear(); mock_span_bodies.clear(); mock_span_names.clear(); mock_deleted_spans = 0;
        cParser directive( "td_lina2_test.scm endcatenary" );
        catenary_scene_load( directive );
        catenary_scene_resolve();
        char ropey[ 32 ];
        std::snprintf( ropey, sizeof( ropey ), "%.3f", h.rope[ 1 ] );
        auto through { 0 }, holderz { 0 };
        for( auto const &body : mock_span_bodies ) {
            if( body.find( std::string( ropey ) + " 159.462" ) != std::string::npos || body.find( std::string( ropey ) + " 161.032" ) != std::string::npos ) { ++through; }
            if( body.find( "160.247" ) != std::string::npos ) { ++holderz; }
        }
        for( auto const &error : errors ) { std::printf( "    %s\n", error.c_str() ); }
        check( mock_span_bodies.size() == 3 && through == 3 && holderz == 0 && errors.empty(),
            "wysiegi pomocnicze z wysiegnikiem samej liny miedzy nimi: lina w ich punktach na wysokosci siodla, przewody prosto" );
        std::remove( "tests/catenary/td_lina2_test.scm" );
    }

    // 6d. lina i przewody zakotwione osobno: lina na 0405 (k1 na słupie 184 m), przewody na 0404 (k2 na słupie 190 m)
    {
        auto a { entry }, c { entry };
        a.name = "s0001"; c.name = "s0002";
        c.at[ 2 ] = c.rope[ 2 ] = c.pole[ 2 ] = 160.0;
        catenary::anchor rope, wires;
        rope.name = "k1"; rope.device = "0405"; rope.network = "C120-2C"; rope.pole_type = "1611-11";
        rope.pole[ 0 ] = -2.4; rope.pole[ 1 ] = 0.2; rope.pole[ 2 ] = 184.0;
        std::copy( std::begin( c.at ), std::end( c.at ), rope.toward );
        wires = rope;
        wires.name = "k2"; wires.device = "0404"; wires.pole[ 2 ] = 190.0;
        catenary::chain line;
        line.name = "l1"; line.power = "pwr01"; line.type = "C120-2C"; line.anchor_end = "k1"; line.anchor_end_contact = "k2";
        line.suspensions = { "s0001", "s0002" };
        auto const written { catenary::write_chain( line ) };
        std::string text { catenary::write_suspension( a ) + "\n" + catenary::write_suspension( c ) + "\n" + catenary::write_anchor( rope ) + "\n" + catenary::write_anchor( wires ) + "\n" + written + "\n" };
        { std::ofstream out { "tests/catenary/td_kotwa2_test.scm", std::ios::binary }; out << text; }
        catenary_scene_clear(); errors.clear(); logs.clear(); mock_span_bodies.clear(); mock_span_names.clear(); mock_deleted_spans = 0;
        cParser directive( "td_kotwa2_test.scm endcatenary" );
        catenary_scene_load( directive );
        catenary_scene_resolve();
        double wire[ 3 ] { 0.0, 0.0, 0.0 }, messenger[ 3 ] { 0.0, 0.0, 0.0 };
        auto good { mock_span_bodies.size() == 2 && catenary_anchor_size() == 2 };
        if( good ) {
            std::istringstream body { mock_span_bodies.back() };
            std::string skip;
            std::getline( body, skip );
            std::getline( body, skip );
            body >> wire[ 0 ] >> wire[ 1 ] >> wire[ 2 ];
            std::getline( body, skip );
            std::getline( body, skip );
            body >> messenger[ 0 ] >> messenger[ 1 ] >> messenger[ 2 ];
        }
        for( auto const &error : errors ) { std::printf( "    %s\n", error.c_str() ); }
        std::printf( "  osobno: przewody do %.2f, lina do %.2f\n", wire[ 2 ], messenger[ 2 ] );
        check( good && errors.empty() && written.find( " anchor_end_contact k2" ) != std::string::npos && wire[ 2 ] > 184.5 && wire[ 2 ] < 190.0 && messenger[ 2 ] < 184.0 && messenger[ 2 ] > 182.0,
            "lina na 0405 i przewody na 0404 na innym slupie: ostatnie przeslo konczy line na jednym, przewody na drugim" );
        std::remove( "tests/catenary/td_kotwa2_test.scm" );
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
            // przechylony słup (lean 3° w stronę toru): urządzenie z nim, czoło na wysokości rolek o h sin 3° w stronę drutów
            double flat[ 4 ][ 3 ], leaning[ 4 ][ 3 ];
            catenary_anchor_shape( index, flat );
            auto tilted { *catenary_scene_get( carrier ) };
            tilted.lean = 3.0;
            catenary_scene_update( carrier, tilted );
            catenary_anchor_shape( index, leaning );
            auto const h { flat[ 0 ][ 1 ] - tilted.pole[ 1 ] };
            auto const expected { h * std::sin( 3.0 * 3.14159265358979323846 / 180.0 ) };
            std::printf( "  kotwienie na przechylonym slupie: czolo x %.3f -> %.3f (oczekiwane +%.3f), y %.3f -> %.3f\n", flat[ 0 ][ 0 ], leaning[ 0 ][ 0 ], expected, flat[ 0 ][ 1 ], leaning[ 0 ][ 1 ] );
            check( std::abs( ( leaning[ 0 ][ 0 ] - flat[ 0 ][ 0 ] ) - expected ) < 0.02 && leaning[ 0 ][ 1 ] < flat[ 0 ][ 1 ], "kotwienie na istniejacym slupie idzie za jego przechylem" );
        }
        Global.AirTemperature = 15.f;
        std::remove( "tests/catenary/td_kotwa_test.scm" );
    }
    // szablony przęseł naprężenia (traction_overlaps.txt) i znacznik izolowanego końca łańcucha
    {
        auto const &cards { catenary_scene_overlaps() };
        catenary::overlap_card const *four { nullptr }, *six { nullptr };
        for( auto const &card : cards ) {
            if( card.name == "33-0060-3" ) { four = &card; }
            if( card.name == "13-0060-30" ) { six = &card; }
        }
        auto good { cards.size() == 19 && four != nullptr && six != nullptr };
        good = good && std::abs( four->offset( 0 ) + 22.0 ) < 1e-9 && std::abs( four->offset( 3 ) - 54.0 ) < 1e-9 && std::abs( four->length() - 76.0 ) < 1e-9 && four->fits( 3 ) && false == four->fits( 4 );
        good = good && six->poles[ six->first_crossing() ].name == "K1" && std::abs( six->length() - 183.0 ) < 1e-9 && std::abs( six->anchor_rope - 7.0 ) < 1e-9;
        catenary::chain line;
        line.name = "li"; line.power = "p1"; line.isolated[ 1 ] = true; line.suspensions = { "a", "b" };
        auto const written { catenary::write_chain( line ) };
        catenary::suspension a { entry }, b { entry };
        a.name = "a"; b.name = "b"; b.at[ 2 ] = b.rope[ 2 ] = b.pole[ 2 ] += 60.0;
        std::istringstream words { catenary::write_suspension( a ) + " " + catenary::write_suspension( b ) + " " + written };
        std::vector<std::string> tokens; for( std::string word; words >> word; ) { tokens.push_back( word ); }
        catenary::suspension_file parsed; std::vector<std::string> problems;
        catenary::parse_suspensions( tokens, "element", parsed, problems );
        good = good && written.find( " isolated end" ) != std::string::npos && problems.empty() && parsed.chains.size() == 1 && parsed.chains[ 0 ].isolated[ 1 ] && false == parsed.chains[ 0 ].isolated[ 0 ];
        check( good, "szablony przesel naprezenia: 19 kart, 0060-3 K1 w zerze, kotwowe -22 / +54 m; lancuch \"isolated end\" zapis i odczyt" );
    }
    // dyrektywa "catenary" w bloku origin: punkty pliku względem niego, zapis z edytora z powrotem względny
    {
        catenary_scene_clear(); errors.clear();
        { std::ofstream out { "tests/catenary/td_origin_test.scm", std::ios::binary };
          out << "node 1000 0 o1 suspension scheme 1124 1 at 0.687 5.800 0.247 rope 0.687 7.500 0.247 pole 1611-11 -2.313 0.200 0.247 endsuspension\n"; }
        double const offset[ 3 ] { 1000.0, 10.0, -500.0 };
        cParser directive( "td_origin_test.scm endcatenary" );
        catenary_scene_load( directive, offset );
        catenary_scene_resolve();
        auto const *loaded { catenary_scene_get( 0 ) };
        auto good { loaded != nullptr && std::abs( loaded->at[ 0 ] - 1000.687 ) < 1e-6 && std::abs( loaded->at[ 1 ] - 15.8 ) < 1e-6 && std::abs( loaded->pole[ 2 ] + 499.753 ) < 1e-6 };
        if( good ) {
            auto moved { *loaded };
            moved.at[ 0 ] += 0.1; moved.rope[ 0 ] += 0.1;
            catenary_scene_update( 0, moved );
            std::string message;
            catenary_scene_save( message );
            std::ifstream in { "tests/catenary/td_origin_test.scm", std::ios::binary };
            std::string text { std::istreambuf_iterator<char>( in ), std::istreambuf_iterator<char>() };
            good = text.find( "at 0.787 5.800 0.247" ) != std::string::npos && text.find( "pole 1611-11 -2.313 0.200 0.247" ) != std::string::npos;
            if( false == good ) { std::printf( "  %s\n", text.c_str() ); }
        }
        check( good, "origin: dyrektywa catenary w bloku origin przesuwa punkty, zapis z edytora wzgledny" );
        std::remove( "tests/catenary/td_origin_test.scm" );
    }
    for( auto const &line : errors ) { std::printf( "  %s\n", line.c_str() ); }
    std::printf( failures == 0 ? "wszystko OK\n" : "BLEDY: %d\n", failures );
    return failures;
}
