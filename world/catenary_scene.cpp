/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "stdafx.h"
#include "world/catenary_scene.h"

#include "world/catenary_suspension.h"
#include "world/catenary_cantilever.h"
#include "world/catenary_span.h"
#include "world/Traction.h"
#include "model/AnimModel.h"
#include "simulation/simulation.h"
#include "utilities/Globals.h"

#include <algorithm>
#include <limits>
#include "utilities/Logs.h"
#include "utilities/parser.h"

#include <cmath>
#include <locale>
#include <set>
#include <sstream>

namespace {

// katalog z plikami danych sieci, z ukośnikiem na końcu
std::string Datadirectory { "data/" };

// podwieszenie powiązane z obiektami scenerii
struct scene_suspension {
    catenary::suspension data;
    std::string skin;                   // wymienna tekstura: własna wpisu albo domyślna z pliku
    TAnimModel *support { nullptr };    // konstrukcja wsporcza
    std::vector<TTraction *> messenger; // przęsła, których linę nośną trzyma
    std::vector<TTraction *> contact;   // przęsła, których przewód jezdny trzyma
};

std::vector<scene_suspension> Suspensions;
// ile podwieszeń z początku listy jest już powiązanych i złożonych
std::size_t Resolved { 0 };
// czy sceneria przeszła już inicjalizację (FirstInit); plik podwieszeń dołączony później jest wiązany od razu
bool Initialized { false };
// uwagi solvera już wypisane do logu; ta sama uwaga dla tej samej karty idzie tylko raz, żeby nie zalać logu
std::set<std::string> Notes;

// katalog wysięgników: części, zestawy ramion i schematy kart (traction_cantilevers.txt) oraz słupy i mocowania
// (traction_supports.txt); czytany przy pierwszym użyciu
catenary::cantilever_catalog const *
cantilevers() {

    static catenary::cantilever_catalog catalog;
    static bool loaded { false };
    static bool good { false };
    if( false == loaded ) {
        loaded = true;
        std::vector<std::string> errors;
        auto const filename { Datadirectory + "traction_cantilevers.txt" };
        good = catalog.load( filename, errors );
        if( false == good ) {
            ErrorLog( "Catenary: file \"" + filename + "\" is missing, cantilevers won't be assembled" );
        }
        // słupy i mocowania do słupów: osobny plik pisany ręcznie (generator go nie rusza), dopisywany do tego samego katalogu
        if( good ) {
            auto const supports { Datadirectory + "traction_supports.txt" };
            if( false == catalog.load( supports, errors ) ) {
                ErrorLog( "Catenary: file \"" + supports + "\" is missing, cantilevers on poles won't be assembled" );
            }
        }
        for( auto const &error : errors ) {
            ErrorLog( error );
        }
    }
    return ( good ? &catalog : nullptr );
}

std::string
text_number( double const Value, int const Precision = 4 ) {

    std::ostringstream stream;
    stream.imbue( std::locale::classic() );
    stream.setf( std::ios::fixed );
    stream.precision( Precision );
    stream << Value;
    return stream.str();
}

// składa wysięgnik podwieszenia z części katalogowych i wstawia części do scenerii jako egzemplarze modeli.
// zwraca liczbę wstawionych części; 0 gdy wysięgnika nie dało się złożyć
// Wire: oś przewodów jezdnych przy podwieszeniu, Rope: oś liny nośnej, Attach: pion przez zaczepy izolatorów,
// a przy wpisie ze słupem oś słupa na wysokości główki szyny. Umbrella: ustawienia strony parasola (tylko przy słupie);
// Probe: tylko złożenie bez wstawiania części, wynik solvera trafia tam (zwraca 1, gdy się złożył)
std::size_t
assemble( scene_suspension const &Suspension, catenary::cantilever_catalog const &Catalog, glm::dvec3 const &Wire, glm::dvec3 const *Rope, glm::dvec3 const &Attach,
    catenary::cantilever_umbrella const *Umbrella = nullptr, catenary::cantilever_result *Probe = nullptr ) {

    auto const &data { Suspension.data };
    auto const label { "Bad scenario: suspension \"" + ( data.name.empty() ? std::string( "none" ) : data.name ) + "\" " };
    auto const *scheme { Catalog.scheme( data.card, data.execution ) };
    if( scheme == nullptr ) {
        ErrorLog( label + "uses scheme " + std::to_string( data.card ) + " " + std::to_string( data.execution ) + " which isn't in the cantilever catalogue" );
        return 0;
    }
    // kierunek poziomy od przewodów do konstrukcji wyznacza płaszczyznę wysięgnika
    auto const towardx { Attach.x - Wire.x };
    auto const towardz { Attach.z - Wire.z };
    auto const reach { std::sqrt( towardx * towardx + towardz * towardz ) };
    if( reach < 0.5 ) {
        ErrorLog( label + "has its attachment point less than 0.5 m from the contact wires" );
        return 0;
    }
    auto const heading { std::atan2( towardx, towardz ) };
    // lina nośna w układzie wysięgnika; bez podanego punktu jest nad przewodami na wysokości konstrukcyjnej schematu
    catenary::cantilever_input input;
    if( Rope != nullptr ) {
        input.messenger.y = ( ( Rope->x - Wire.x ) * towardx + ( Rope->z - Wire.z ) * towardz ) / reach;
        input.messenger.z = Rope->y - Wire.y;
    }
    else {
        input.messenger.z = scheme->height;
    }
    // zaczepy izolatorów na wysokościach ze schematu; zaczep odciągu idzie za liną nośną, gdy ta wisi inaczej niż w schemacie,
    // a zaczep ukośnika razem z nim tam, gdzie schemat trzyma je w stałym odstępie (karty z trzema zaczepami)
    auto const raise { input.messenger.z - scheme->height };
    input.stay_mount = { reach, scheme->mount_stay + raise };
    input.strut_mount = { reach, scheme->mount_strut + ( scheme->steady_mounted ? raise : 0.0 ) };
    input.steady_mount = { reach, scheme->mount_steady };
    // zestaw izolatorów (np. "25" - 25 kV): każdy izolator schematu NNNN-xx podmieniony na NNNN-<zestaw>, jeśli taka część jest
    if( false == data.insulators.empty() ) {
        for( auto const &role : scheme->roles ) {
            auto const *part { Catalog.part( role.second ) };
            if( part == nullptr || part->kind != "insulator" ) { continue; }
            auto const dash { role.second.find( '-' ) };
            auto const variant { role.second.substr( 0, dash == std::string::npos ? role.second.size() : dash ) + "-" + data.insulators };
            if( Catalog.part( variant ) != nullptr ) { input.overrides[ role.first ] = variant; }
            else { ErrorLog( label + "asks for insulator set \"" + data.insulators + "\", but part \"" + variant + "\" isn't in the cantilever catalogue; \"" + role.second + "\" kept" ); }
        }
    }
    for( auto const &replacement : data.overrides ) {
        input.overrides[ replacement.first ] = replacement.second;
    }

    catenary::cantilever_pole const *pole { nullptr };
    if( data.has_pole ) {
        pole = Catalog.pole( data.pole_type );
        if( pole == nullptr ) {
            ErrorLog( label + "uses pole \"" + data.pole_type + "\" which isn't in the cantilever catalogue" );
            return 0;
        }
    }
    // Drop: obniżenie dolnego zaczepu ukośnika (dobór przy innym zestawie izolatorów, niżej)
    auto const solve = [ & ]( std::map<std::string, std::string> const &Overrides, double const Drop, catenary::cantilever_result &Result ) {
        if( pole != nullptr ) {
            // słup z katalogu: zaczepy, mocowania i słup liczy solver; Attach to oś słupa na wysokości główki szyny
            auto lowered { *scheme };
            lowered.mount_strut -= Drop;
            return catenary::solve_cantilever_on_pole( Catalog, lowered, *pole, reach, Attach.y - Wire.y, input.messenger, Overrides,
                ( Umbrella != nullptr ? *Umbrella : catenary::cantilever_umbrella() ), Result );
        }
        auto attempt { input };
        attempt.overrides = Overrides;
        attempt.strut_mount.z -= Drop;
        return catenary::solve_cantilever( Catalog, *scheme, attempt, Result );
    };
    // inny zestaw izolatorów (25 kV) bez kart katalogowych (Jan 2026-10-17: „lepić na oko”): rury ukośnika, odciągu i wysięgu
    // z izolatorem dobierane z tej samej rodziny części - długość bliska pierwotnej minus przyrost izolatora i po jednym
    // kroku w obie strony; wygrywa złożenie z najmniejszą liczbą uwag solvera, przy równej - najbliższe tej długości.
    // Rury podane w scenerii przez override nie są ruszane. Gdy mimo to zostają uwagi (zwykle uchwyt wysięgu 4050 wchodzi na
    // dłuższy izolator ukośnika - Jan 2026-10-17, 1232), dolny zaczep ukośnika schodzi co 0,1 m (najwyżej 0,5 m), aż uwag
    // nie będzie; zostaje najmniejsze obniżenie z najmniejszą liczbą uwag.
    std::vector<std::pair<std::string, std::vector<std::string>>> fits;
    if( false == data.insulators.empty() ) {
        for( auto const *tube : { "strut", "stay", "steady" } ) {
            auto const role { scheme->roles.find( tube ) }, insulator { scheme->roles.find( std::string( tube ) + "_insulator" ) };
            auto const replaced { input.overrides.find( std::string( tube ) + "_insulator" ) };
            if( role == scheme->roles.end() || insulator == scheme->roles.end() || replaced == input.overrides.end() ) { continue; }
            if( std::any_of( data.overrides.begin(), data.overrides.end(), [ & ]( std::pair<std::string, std::string> const &O ) { return O.first == tube; } ) ) { continue; }
            auto const *original { Catalog.part( role->second ) };
            auto const *oldins { Catalog.part( insulator->second ) }, *newins { Catalog.part( replaced->second ) };
            auto const dash { role->second.find( '-' ) };
            if( original == nullptr || oldins == nullptr || newins == nullptr || dash == std::string::npos ) { continue; }
            auto const family { role->second.substr( 0, dash + 1 ) };
            auto const target { original->length - ( newins->length - oldins->length ) };
            std::vector<std::pair<double, std::string>> sorted;
            for( auto const &part : Catalog.parts() ) {
                if( part.first.compare( 0, family.size(), family ) == 0 && part.second.kind == original->kind ) { sorted.emplace_back( part.second.length, part.first ); }
            }
            std::sort( sorted.begin(), sorted.end() );
            if( sorted.empty() ) { continue; }
            std::size_t nearest { 0 };
            for( std::size_t index = 1; index < sorted.size(); ++index ) {
                if( std::abs( sorted[ index ].first - target ) < std::abs( sorted[ nearest ].first - target ) ) { nearest = index; }
            }
            std::vector<std::string> candidates { sorted[ nearest ].second };   // pierwszy = najbliższy
            if( nearest > 0 ) { candidates.push_back( sorted[ nearest - 1 ].second ); }
            if( nearest + 1 < sorted.size() ) { candidates.push_back( sorted[ nearest + 1 ].second ); }
            fits.emplace_back( tube, candidates );
        }
    }
    catenary::cantilever_result result;
    auto solved { false };
    if( data.insulators.empty() ) {
        solved = solve( input.overrides, 0.0, result );
    }
    else {
        auto best { std::numeric_limits<std::size_t>::max() };
        std::size_t baseline { 0 };     // uwagi niezależne od doboru (np. mocowanie odciągu ścięte do mount_max)
        { catenary::cantilever_result plain; solve( input.overrides, 0.0, plain );
          for( auto const &warning : plain.warnings ) { if( warning.find( "highest mount position" ) != std::string::npos ) { ++baseline; } } }
        for( int step = 0; step <= 5 && best > baseline; ++step ) {
            auto const drop { 0.1 * step };
            std::vector<std::size_t> choice( fits.size(), 0 );
            while( true ) {
                auto overrides { input.overrides };
                for( std::size_t index = 0; index < fits.size(); ++index ) { overrides[ fits[ index ].first ] = fits[ index ].second[ choice[ index ] ]; }
                catenary::cantilever_result attempt;
                if( solve( overrides, drop, attempt ) && attempt.warnings.size() < best ) {
                    best = attempt.warnings.size();
                    result = attempt;
                    result.values[ "strut_mount.drop" ] = drop;
                    solved = true;
                }
                // następna kombinacja (kolejność: najbliższe długości najpierw, więc przy równej liczbie uwag zostaje pierwsza)
                std::size_t index { 0 };
                while( index < fits.size() && ++choice[ index ] == fits[ index ].second.size() ) { choice[ index ] = 0; ++index; }
                if( index == fits.size() ) { break; }
            }
        }
        if( false == solved ) { solved = solve( input.overrides, 0.0, result ); }
    }
    if( Probe != nullptr ) { *Probe = result; return ( solved ? 1 : 0 ); }
    // uwagi solvera to informacja o dopasowaniu części, nie błąd scenerii; błędem jest dopiero wysięgnik, którego nie da się złożyć
    for( auto const &warning : result.warnings ) {
        if( solved ) {
            if( Notes.insert( warning ).second ) { WriteLog( ( "Catenary: " + warning + " (first seen in suspension \"" + ( data.name.empty() ? std::string( "none" ) : data.name ) + "\")" ).c_str() ); }
        }
        else { ErrorLog( label + "can't be assembled: " + warning ); }
    }
    if( false == solved ) { return 0; }

    // każda część to zwykły egzemplarz modelu, taki sam jak z wpisu node model
    std::size_t count { 0 };
    auto const skin { Suspension.skin.empty() ? std::string( "none" ) : Suspension.skin };
    for( auto const &placement : result.parts ) {
        auto const part { catenary::cantilever_to_scene( placement, heading ) };
        auto entry {
            "node " + text_number( data.range_max, 1 ) + " " + text_number( data.range_min, 1 ) + " none model 0 0 0 0 "
            + ( placement.model.empty() ? Catalog.models() + placement.part : placement.model ) + ".t3d " + skin
            + " angles " + text_number( part.angles[ 0 ] ) + " " + text_number( part.angles[ 1 ] ) + " " + text_number( part.angles[ 2 ] ) };
        if( part.scale[ 0 ] != 1.0 || part.scale[ 1 ] != 1.0 || part.scale[ 2 ] != 1.0 ) {
            entry += " scale " + text_number( part.scale[ 0 ], 5 ) + " " + text_number( part.scale[ 1 ], 5 ) + " " + text_number( part.scale[ 2 ], 5 );
        }
        entry += " endmodel";
        glm::dvec3 const position { Wire.x + part.offset[ 0 ], Wire.y + part.offset[ 1 ], Wire.z + part.offset[ 2 ] };
        if( simulation::State.create_model( entry, "none", position ) != nullptr ) { ++count; }
    }
    return count;
}

} // namespace

// obsługa dyrektywy "catenary" ze scenerii: wczytuje wskazany plik podwieszeń
void
catenary_scene_load( cParser &Input ) {

    // nazwa pliku z zachowaniem wielkości liter, potem reszta dyrektywy do endcatenary
    auto const filename { Input.getToken<std::string>( false ) };
    if( filename.empty() || filename == "endcatenary" ) {
        ErrorLog( "Bad scenario: \"catenary\" without a file name in file \"" + Input.Name() + "\" (line " + std::to_string( Input.Line() ) + ")" );
        return;
    }
    {
        std::string token;
        do {
            token = Input.getToken<std::string>();
        } while( false == token.empty() && token != "endcatenary" );
    }
    if( false == Global.bLoadTraction ) { return; }

    std::vector<std::string> tokens;
    {
        cParser parser( filename, cParser::buffer_FILE, Global.asCurrentSceneryPath, Global.bLoadTraction );
        auto token { parser.getToken<std::string>() };
        while( false == token.empty() ) {
            tokens.emplace_back( token );
            token = parser.getToken<std::string>();
        }
    }
    if( tokens.empty() ) {
        ErrorLog( "Bad scenario: catenary file \"" + filename + "\" is missing or empty" );
        return;
    }
    catenary::suspension_file file;
    std::vector<std::string> errors;
    catenary::parse_suspensions( tokens, filename, file, errors );
    for( auto const &error : errors ) {
        ErrorLog( error );
    }
    for( auto const &entry : file.entries ) {
        scene_suspension suspension;
        suspension.data = entry;
        suspension.skin = ( entry.skin.empty() ? file.skin : entry.skin );
        Suspensions.emplace_back( suspension );
    }
    WriteLog( ( "Catenary: " + std::to_string( file.entries.size() ) + " suspension(s) loaded from file \"" + filename + "\"" ).c_str() );
    if( Initialized ) {
        // dyrektywa po FirstInit: tory, przęsła i modele już są, więc nie ma na co czekać
        catenary_scene_resolve();
    }
}

// po wczytaniu scenerii: wiąże podwieszenia z konstrukcjami wsporczymi i przęsłami po nazwach, zgłasza braki
void
catenary_scene_resolve() {

    Initialized = true;
    if( Resolved >= Suspensions.size() ) { return; }

    auto const first { Resolved };
    std::size_t complete { 0 };
    std::size_t assembled { 0 };
    std::size_t parts { 0 };
    std::map<std::string, std::vector<std::size_t>> umbrellas;     // parasole: nazwa -> wpisy
    for( ; Resolved < Suspensions.size(); ++Resolved ) {
        auto &suspension { Suspensions[ Resolved ] };
        auto const &data { suspension.data };
        auto const label { "Bad scenario: suspension \"" + ( data.name.empty() ? std::string( "none" ) : data.name ) + "\" " };
        if( data.has_at ) {
            // wpis z punktami podanymi wprost: nie ma czego wiązać, od razu składanie wysięgnika
            ++complete;
            auto const *catalog { cantilevers() };
            if( catalog == nullptr ) { continue; }
            glm::dvec3 const wire { data.at[ 0 ], data.at[ 1 ], data.at[ 2 ] };
            glm::dvec3 const attach { data.has_pole ? glm::dvec3 { data.pole[ 0 ], data.pole[ 1 ], data.pole[ 2 ] } : glm::dvec3 { data.attach[ 0 ], data.attach[ 1 ], data.attach[ 2 ] } };
            glm::dvec3 const rope { data.rope[ 0 ], data.rope[ 1 ], data.rope[ 2 ] };
            if( false == data.umbrella.empty() ) { umbrellas[ data.umbrella ].push_back( Resolved ); continue; }   // parasole po pętli, parami
            auto const count { assemble( suspension, *catalog, wire, ( data.has_rope ? &rope : nullptr ), attach ) };
            if( count > 0 ) { ++assembled; parts += count; }
            continue;
        }
        bool good { true };
        suspension.support = simulation::Instances.find( data.support );
        if( suspension.support == nullptr ) {
            ErrorLog( label + "refers to nonexistent support model \"" + data.support + "\"" );
            good = false;
        }
        auto const find_spans = [ & ]( std::vector<std::string> const &Names, std::vector<TTraction *> &Spans ) {
            Spans.clear();
            for( auto const &name : Names ) {
                auto *span { simulation::Traction.find( name ) };
                if( span == nullptr ) {
                    ErrorLog( label + "refers to nonexistent traction piece \"" + name + "\"" );
                    good = false;
                    continue;
                }
                if( catenary_span_symbol( *span ).empty() ) {
                    ErrorLog( label + "refers to traction piece \"" + name + "\" which has no network type" );
                    good = false;
                }
                Spans.emplace_back( span );
            }
        };
        find_spans( data.messenger, suspension.messenger );
        find_spans( data.contact, suspension.contact );
        if( good ) { ++complete; }
    }
    // parasole: oba wysięgniki składane najpierw osobno, żeby poznać wysokości mocowań, potem razem na wspólnym słupie
    auto const *catalog { umbrellas.empty() ? nullptr : cantilevers() };
    for( auto const &umbrella : umbrellas ) {
        auto const points = [ & ]( std::size_t const Index, glm::dvec3 &Wire, glm::dvec3 &Rope, glm::dvec3 &Pole ) {
            auto const &data { Suspensions[ Index ].data };
            Wire = { data.at[ 0 ], data.at[ 1 ], data.at[ 2 ] };
            Rope = { data.rope[ 0 ], data.rope[ 1 ], data.rope[ 2 ] };
            Pole = { data.pole[ 0 ], data.pole[ 1 ], data.pole[ 2 ] };
            return ( data.has_rope ? 1 : 0 );
        };
        auto const alone = [ & ]( std::size_t const Index ) {
            glm::dvec3 wire, rope, pole;
            auto const hasrope { points( Index, wire, rope, pole ) };
            auto const count { assemble( Suspensions[ Index ], *catalog, wire, ( hasrope ? &rope : nullptr ), pole ) };
            if( count > 0 ) { ++assembled; parts += count; }
        };
        if( catalog == nullptr ) { break; }
        auto const &list { umbrella.second };
        auto const &first { Suspensions[ list.front() ].data };
        auto const *pole { catalog->pole( first.pole_type ) };
        auto paired { list.size() == 2 && pole != nullptr };
        if( list.size() != 2 ) {
            ErrorLog( "Bad scenario: umbrella \"" + umbrella.first + "\" has " + std::to_string( list.size() ) + " suspension(s), it needs exactly two; they're assembled separately" );
        }
        else {
            auto const &second { Suspensions[ list.back() ].data };
            auto const dx { second.pole[ 0 ] - first.pole[ 0 ] }, dy { second.pole[ 1 ] - first.pole[ 1 ] }, dz { second.pole[ 2 ] - first.pole[ 2 ] };
            if( second.pole_type != first.pole_type || std::sqrt( dx * dx + dy * dy + dz * dz ) > 0.05 ) {
                ErrorLog( "Bad scenario: umbrella \"" + umbrella.first + "\" has its suspensions on different poles; they're assembled separately" );
                paired = false;
            }
        }
        if( false == paired ) { for( auto const index : list ) { alone( index ); } continue; }
        glm::dvec3 wires[ 2 ], ropes[ 2 ], poles[ 2 ];
        int hasropes[ 2 ];
        catenary::cantilever_result probes[ 2 ];
        auto probed { true };
        for( int side = 0; side < 2; ++side ) {
            hasropes[ side ] = points( list[ side ], wires[ side ], ropes[ side ], poles[ side ] );
            probed = probed && assemble( Suspensions[ list[ side ] ], *catalog, wires[ side ], ( hasropes[ side ] ? &ropes[ side ] : nullptr ), poles[ 0 ], nullptr, &probes[ side ] ) > 0;
        }
        if( false == probed ) { for( auto const index : list ) { alone( index ); } continue; }
        catenary::cantilever_umbrella sides[ 2 ];
        catenary::umbrella_mounts( *catalog, *pole, probes[ 0 ], probes[ 1 ], sides[ 0 ], sides[ 1 ] );
        sides[ 1 ].pole = false;
        for( int side = 0; side < 2; ++side ) {
            auto const count { assemble( Suspensions[ list[ side ] ], *catalog, wires[ side ], ( hasropes[ side ] ? &ropes[ side ] : nullptr ), poles[ 0 ], &sides[ side ] ) };
            if( count > 0 ) { ++assembled; parts += count; }
        }
    }
    WriteLog( ( "Catenary: " + std::to_string( complete ) + " of " + std::to_string( Suspensions.size() - first ) + " suspension(s) linked to supports and traction pieces" ).c_str() );
    if( assembled > 0 ) {
        WriteLog( ( "Catenary: " + std::to_string( assembled ) + " cantilever(s) assembled from " + std::to_string( parts ) + " part(s)" ).c_str() );
    }
}

// zmienia katalog, z którego czytany jest katalog wysięgników; domyślnie "data/". do testów poza symulatorem
void
catenary_scene_data( std::string const &Directory ) {

    Datadirectory = Directory;
    if( false == Datadirectory.empty() && Datadirectory.back() != '/' && Datadirectory.back() != '\\' ) { Datadirectory += '/'; }
}

// przed wczytaniem nowej scenerii: usuwa podwieszenia poprzedniej
void
catenary_scene_clear() {

    Suspensions.clear();
    Notes.clear();
    Resolved = 0;
    Initialized = false;
}
