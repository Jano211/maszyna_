/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "stdafx.h"
#include "world/catenary_span.h"

#include <memory>

#include "world/Traction.h"
#include "world/catenary_network.h"
#include "utilities/Globals.h"
#include "utilities/Logs.h"
#include "rendering/renderer.h"
#include "utilities/utilities.h"

#include <fstream>
#include <locale>
#include <sstream>

catenary_settings CatenarySettings;
catenary_crossing_candidates CatenaryCrossingCandidates { nullptr };

// stan przęsła rysowanego wg typu sieci
struct catenary_span {
    std::string symbol;                                         // typ sieci z dopisku "type"
    catenary::network_definition const *definition { nullptr }; // opis typu z katalogu; nullptr gdy brak albo przęsła nie da się policzyć
    std::vector<double> hangergaps;                             // odstępy wieszaków dobrane do rozpiętości; puste = odcinek bez wieszaków
    gfx::geometrybank_handle solidbank;                         // osobny bank na bryły tego przęsła
    catenary_solid solid;                                       // bryły do oglądania z bliska
    bool solidloaded { false };
    std::vector<catenary::solid_vertex> attached;               // bryła dołączona (lina kotwienia), układ scenerii
    std::shared_ptr<catenary::network_definition> own;          // kopia typu z naciągiem z dopisku "tension"
    bool rope2set[ 2 ] { false, false };                        // końce drugiej liny nośnej z dopisku "rope2"
    catenary::vec3 rope2[ 2 ];
    double spread[ 2 ] { 0.0, 0.0 };                            // rozstaw przewodów w uchwytach na końcach z dopisku "spread"
    bool pulled { false };                                      // część rozpiętości przedzielonej odciągami sieci (dopisek "pull")
    double pull[ 2 ] { 0.0, 0.0 };                              // początek przęsła od pierwszego podwieszenia rozpiętości i jej długość
    bool gapped[ 2 ] { false, false };                          // przerwa na izolator sekcyjny: [0] przewody jezdne, [1] lina nośna
    double gap[ 2 ][ 2 ] { { 0.0, 0.0 }, { 0.0, 0.0 } };          // od - do, w planie od pPoint1
};

namespace {


catenary::network_definition const *
definition( TTraction const &Traction ) {
    return ( Traction.m_catenaryspan ? Traction.m_catenaryspan->definition : nullptr );
}

catenary::span_points
points( TTraction const &Traction ) {
    catenary::span_points result {
        { Traction.pPoint1.x, Traction.pPoint1.y, Traction.pPoint1.z },
        { Traction.pPoint2.x, Traction.pPoint2.y, Traction.pPoint2.z },
        { Traction.pPoint3.x, Traction.pPoint3.y, Traction.pPoint3.z },
        { Traction.pPoint4.x, Traction.pPoint4.y, Traction.pPoint4.z } };
    if( Traction.m_catenaryspan ) {
        auto const &span { *Traction.m_catenaryspan };
        result.messenger2_start_set = span.rope2set[ 0 ];
        result.messenger2_end_set = span.rope2set[ 1 ];
        result.messenger2_start = span.rope2[ 0 ];
        result.messenger2_end = span.rope2[ 1 ];
        result.contact_spread[ 0 ] = span.spread[ 0 ];
        result.contact_spread[ 1 ] = span.spread[ 1 ];
    }
    return result;
}

// długość podwieszenia Y, jaką typ sieci przewiduje dla tego przęsła; 0 gdy brak
double
y_length( TTraction const &Traction ) {

    auto const *type { definition( Traction ) };
    if( type == nullptr ) { return 0.0; }
    if( Traction.m_catenaryspan->hangergaps.empty() ) { return 0.0; } // odcinek bez wieszaków
    if( type->type.y_tension <= 0.0 ) { return 0.0; }
    auto const length { std::hypot( Traction.pPoint2.x - Traction.pPoint1.x, Traction.pPoint2.z - Traction.pPoint1.z ) };
    // przęsło przedzielone odciągiem: linka Y wg całej rozpiętości między podwieszeniami
    auto const span { Traction.m_catenaryspan->pulled ? Traction.m_catenaryspan->pull[ 1 ] : length };
    auto const ylength { type->type.y_length( span ) };
    return ( ylength < length ? ylength : 0.0 );
}

// koniec przęsła (0: Point1, 1: Point2) jest na odciągu sieci, nie na podwieszeniu
bool
pull_end( TTraction const &Traction, int const End ) {
    if( !Traction.m_catenaryspan || false == Traction.m_catenaryspan->pulled ) { return false; }
    auto const &pull { Traction.m_catenaryspan->pull };
    auto const length { std::hypot( Traction.pPoint2.x - Traction.pPoint1.x, Traction.pPoint2.z - Traction.pPoint1.z ) };
    return ( End == 0 ? pull[ 0 ] > 0.01 : pull[ 0 ] + length < pull[ 1 ] - 0.01 );
}

// czy przy podwieszeniu od strony wskazanego końca (0: Point1, 1: Point2) jest linka Y wspólna z sąsiednim przęsłem.
// linka Y to jedno cięgno na dwa przęsła, więc jest tylko wtedy, gdy oba przęsła ją przewidują, w tej samej długości
// i z tych samych przewodów, a ich liny nośne schodzą się w jednym punkcie
bool
y_at( TTraction const &Traction, int const End ) {

    auto const ylength { y_length( Traction ) };
    if( ylength <= 0.0 ) { return false; }
    // przy odciągu sieci linki Y nie ma (odciąg nie podwiesza sieci)
    if( pull_end( Traction, End ) ) { return false; }
    auto const *neighbour { Traction.hvNext[ End ] };
    if( neighbour == nullptr ) { return false; }
    auto const neighbourend { Traction.iNext[ End ] };
    if( neighbour->hvNext[ neighbourend ] != &Traction ) { return false; }
    if( std::abs( y_length( *neighbour ) - ylength ) > 0.01 ) { return false; }
    auto const &type { definition( Traction )->type };
    auto const &neighbourtype { definition( *neighbour )->type };
    if( type.y_tension != neighbourtype.y_tension
     || type.y_mass != neighbourtype.y_mass
     || type.messenger_count != neighbourtype.messenger_count ) {
        return false;
    }
    auto const &rope { End == 0 ? Traction.pPoint3 : Traction.pPoint4 };
    auto const &neighbourrope { neighbourend == 0 ? neighbour->pPoint3 : neighbour->pPoint4 };
    return ( glm::length( rope - neighbourrope ) < 0.05 );
}

// liczy przęsło osobno, z linkami Y tam, gdzie są wspólne z sąsiadami; false gdy się nie da (zgłasza błąd raz)
bool
solve( TTraction &Traction, catenary::span_solution &Solution ) {

    auto const *type { definition( Traction ) };
    if( type == nullptr ) { return false; }

    std::string error;
    if( false == catenary::solve_span_points( *type, Traction.m_catenaryspan->hangergaps, points( Traction ), y_at( Traction, 0 ), y_at( Traction, 1 ), Solution, error ) ) {
        ErrorLog(
            "Bad scenario: traction piece \"" + Traction.name() + "\" of type \"" + Traction.m_catenaryspan->symbol + "\" at location " + to_string( Traction.pPoint1 )
            + " can't be calculated, " + error + "; legacy geometry used instead" );
        Traction.m_catenaryspan->definition = nullptr;
        return false;
    }
    return true;
}

// połowa linki Y od strony wskazanego końca, do uzgodnienia z sąsiednim przęsłem; false gdy przęsła nie da się policzyć
bool
y_half( TTraction &Traction, int const End, catenary::y_half &Half ) {

    catenary::span_solution solution;
    if( false == solve( Traction, solution ) ) { return false; }
    Half = ( End == 0 ? solution.result.y_half_start : solution.result.y_half_end );
    return true;
}

// liczy przęsło i uzgadnia linki Y z sąsiednimi przęsłami, żeby połówki spotkały się w osi podwieszenia
bool
solve_joined( TTraction &Traction, catenary::span_solution &Solution ) {

    if( false == solve( Traction, Solution ) ) { return false; }
    for( int end = 0; end < 2; ++end ) {
        if( false == y_at( Traction, end ) ) { continue; }
        catenary::y_half neighbourhalf;
        if( y_half( *Traction.hvNext[ end ], Traction.iNext[ end ], neighbourhalf ) ) {
            catenary::join_y( *definition( Traction ), Solution, end == 1, neighbourhalf );
        }
        else {
            // sąsiad jednak nie da się policzyć i narysuje się po staremu, więc linki Y tu nie będzie
            if( false == solve( Traction, Solution ) ) { return false; }
            end = -1; // od nowa, już bez tego sąsiada
        }
    }
    return true;
}

// odległość punktu w planie od pierwszego końca przęsła, wzdłuż przęsła
double
plan_along( TTraction const &Traction, catenary::vec3 const &Point ) {
    auto const dx { Traction.pPoint2.x - Traction.pPoint1.x }, dz { Traction.pPoint2.z - Traction.pPoint1.z };
    auto const length { std::hypot( dx, dz ) };
    if( length < 1e-6 ) { return 0.0; }
    return ( ( Point.x - Traction.pPoint1.x ) * dx + ( Point.z - Traction.pPoint1.z ) * dz ) / length;
}

// przerwy na izolator sekcyjny: odcinki przewodów jezdnych i liny nośnej w przerwie wycięte, wieszaki i ich uchwyty w przerwach usunięte
void
cut_gaps( TTraction const &Traction, catenary::span_geometry &Geometry ) {
    if( false == static_cast<bool>( Traction.m_catenaryspan ) ) { return; }
    auto const &span { *Traction.m_catenaryspan };
    if( false == span.gapped[ 0 ] && false == span.gapped[ 1 ] ) { return; }
    auto const inside = [ & ]( int const Which, double const At ) {
        return span.gapped[ Which ] && At > span.gap[ Which ][ 0 ] && At < span.gap[ Which ][ 1 ];
    };
    auto const lerp = []( catenary::vec3 const &A, catenary::vec3 const &B, double const T ) {
        return catenary::vec3 { A.x + ( B.x - A.x ) * T, A.y + ( B.y - A.y ) * T, A.z + ( B.z - A.z ) * T };
    };
    std::vector<catenary::span_element> kept;
    kept.reserve( Geometry.elements.size() );
    for( auto const &element : Geometry.elements ) {
        auto const which { element.kind == catenary::element_kind::contact ? 0 : element.kind == catenary::element_kind::messenger ? 1 : -1 };
        auto const a { plan_along( Traction, element.from ) }, b { plan_along( Traction, element.to ) };
        if( which < 0 ) {
            // wieszak z liny na przewód (albo inny łącznik) w przerwie którejkolwiek - bez niego
            if( element.kind == catenary::element_kind::hanger && ( inside( 0, a ) || inside( 1, a ) ) ) { continue; }
            kept.push_back( element );
            continue;
        }
        if( false == span.gapped[ which ] ) { kept.push_back( element ); continue; }
        auto const g0 { span.gap[ which ][ 0 ] }, g1 { span.gap[ which ][ 1 ] };
        auto const lo { std::min( a, b ) }, hi { std::max( a, b ) };
        if( hi <= g0 || lo >= g1 || hi - lo < 1e-9 ) { kept.push_back( element ); continue; }
        // części odcinka przed przerwą i za nią
        auto const at = [ & ]( double const D ) { return lerp( element.from, element.to, ( D - a ) / ( b - a ) ); };
        if( lo < g0 ) {
            auto piece { element };
            if( a < b ) { piece.to = at( g0 ); } else { piece.from = at( g0 ); }
            kept.push_back( piece );
        }
        if( hi > g1 ) {
            auto piece { element };
            if( a < b ) { piece.from = at( g1 ); } else { piece.to = at( g1 ); }
            kept.push_back( piece );
        }
    }
    Geometry.elements.swap( kept );
    Geometry.fittings.erase(
        std::remove_if( Geometry.fittings.begin(), Geometry.fittings.end(), [ & ]( catenary::span_fitting const &F ) {
            auto const d { plan_along( Traction, F.position ) };
            return ( F.kind == catenary::fitting_kind::rope_clamp || F.kind == catenary::fitting_kind::wire_clamp ) && ( inside( 0, d ) || inside( 1, d ) ); } ),
        Geometry.fittings.end() );
}

// zbiera elementy przęsła: policzone, z linkami Y uzgodnionymi z sąsiadami także w planie; Gaps - z przerwami na izolator sekcyjny
bool
build( TTraction &Traction, catenary::span_geometry &Geometry, bool const Gaps = true ) {

    catenary::span_solution solution;
    if( false == solve_joined( Traction, solution ) ) { return false; }
    auto const spanpoints { points( Traction ) };
    // przebieg linek Y w planie, wspólny z sąsiednim przęsłem: na łuku linka przechodzi obok załomu liny przy podwieszeniu
    catenary::y_plan plans[ 2 ];
    for( int end = 0; end < 2; ++end ) {
        if( false == y_at( Traction, end ) ) { continue; }
        auto &neighbour { *Traction.hvNext[ end ] };
        catenary::span_solution neighboursolution;
        if( false == solve_joined( neighbour, neighboursolution ) ) { continue; }
        plans[ end ] =
            catenary::solve_y_plan(
                *definition( Traction ),
                catenary::make_y_side( solution, spanpoints, end == 1 ),
                catenary::make_y_side( neighboursolution, points( neighbour ), Traction.iNext[ end ] == 1 ) );
    }
    catenary::build_geometry( *definition( Traction ), solution, spanpoints, Traction.WireOffset, Geometry, plans[ 0 ], plans[ 1 ] );
    if( Gaps ) { cut_gaps( Traction, Geometry ); }
    return true;
}

gfx::basic_vertex
vertex( TTraction const &Traction, catenary::vec3 const &Point ) {
    gfx::basic_vertex result;
    result.position =
        glm::vec3(
            Point.x - Traction.m_origin.x,
            Point.y - Traction.m_origin.y,
            Point.z - Traction.m_origin.z );
    return result;
}

// zamienia bryły na wierzchołki do banku geometrii
gfx::vertex_array
vertices( TTraction const &Traction, std::vector<catenary::solid_vertex> const &Solid ) {
    gfx::vertex_array result;
    result.reserve( Solid.size() );
    for( auto const &point : Solid ) {
        auto item { vertex( Traction, point.position ) };
        item.normal = glm::vec3( point.normal.x, point.normal.y, point.normal.z );
        result.emplace_back( item );
    }
    return result;
}

// opróżnia fragment geometrii, zwalniając jego pamięć
void
release( gfx::geometry_handle const &Geometry ) {
    if( Geometry == null_handle ) { return; }
    gfx::vertex_array empty;
    gfx::userdata_array userdata;
    GfxRenderer->Replace( empty, userdata, Geometry, GL_TRIANGLES );
}

// banki brył przęseł usuniętych albo wczytanych od nowa, do ponownego użycia. Menedżer banków nie usuwa banków, a edytor
// przy przeciąganiu suwaka robił przęsło od nowa co klatkę - każde z własnym, nowym bankiem (2026-11-06)
std::vector<gfx::geometrybank_handle> SolidBanks;

// bryły przęsła opróżnione; bank zostaje przy opisie (Keep) albo wraca do puli
void
give_back_solid( catenary_span &Span, bool const Keep ) {
    if( Span.solidloaded ) {
        release( Span.solid.wires );
        release( Span.solid.fittings );
        release( Span.solid.rope );
    }
    Span.solid = {};
    Span.solidloaded = false;
    if( Keep || Span.solidbank == null_handle ) { return; }
    SolidBanks.push_back( Span.solidbank );
    Span.solidbank = {};
}

// wygląd metalu: [ metal: miedź, aluminium ][ przewód, osprzęt ][ stan: nowa, przejściowa, czarna, zaśniedziała ]
// osprzęt sieci miedzianej jest z brązu albo mosiądzu. Czarna (Jan 2026-11-01): matowa, prawie bez połysku - wartości na oko
catenary_metal Metals[ 2 ][ 2 ][ 4 ] {
    { { { { 0.80f, 0.50f, 0.27f }, 0.6f, 0.45f, 0.40f }, { { 0.50f, 0.30f, 0.16f }, 0.8f, 0.18f, 0.15f }, { { 0.11f, 0.09f, 0.08f }, 0.5f, 0.08f, 0.05f }, { { 0.30f, 0.52f, 0.46f }, 0.3f, 0.10f, 0.10f } },
      { { { 0.82f, 0.60f, 0.31f }, 0.6f, 0.40f, 0.35f }, { { 0.52f, 0.40f, 0.20f }, 0.8f, 0.15f, 0.15f }, { { 0.14f, 0.12f, 0.10f }, 0.5f, 0.08f, 0.05f }, { { 0.36f, 0.46f, 0.38f }, 0.4f, 0.10f, 0.10f } } },
    { { { { 0.80f, 0.81f, 0.83f }, 0.9f, 0.45f, 0.30f }, { { 0.55f, 0.55f, 0.56f }, 0.8f, 0.25f, 0.15f }, { { 0.16f, 0.16f, 0.16f }, 0.6f, 0.08f, 0.05f }, { { 0.30f, 0.30f, 0.31f }, 0.6f, 0.15f, 0.10f } },
      { { { 0.70f, 0.71f, 0.73f }, 0.9f, 0.35f, 0.30f }, { { 0.48f, 0.48f, 0.49f }, 0.8f, 0.20f, 0.15f }, { { 0.18f, 0.18f, 0.18f }, 0.6f, 0.08f, 0.05f }, { { 0.32f, 0.32f, 0.33f }, 0.6f, 0.15f, 0.10f } } } };
bool MetalsLoaded { false };

// czyta data/traction_materials.txt, o ile jest. wiersz: <copper|aluminium> <wire|fitting> <new|worn|black|tarnished> r g b metaliczność połysk odbicie
void
load_metals() {

    MetalsLoaded = true;
    std::ifstream file { "data/traction_materials.txt" };
    if( !file ) { return; }
    std::string line;
    int linenumber { 0 };
    while( std::getline( file, line ) ) {
        ++linenumber;
        auto const comment { line.find( "//" ) };
        if( comment != std::string::npos ) { line.erase( comment ); }
        std::istringstream stream { line };
        stream.imbue( std::locale::classic() );
        std::string metal, part, state;
        if( !( stream >> metal ) ) { continue; } // pusty wiersz
        catenary_metal value {};
        stream >> part >> state >> value.color[ 0 ] >> value.color[ 1 ] >> value.color[ 2 ] >> value.metalness >> value.gloss >> value.reflection;
        auto const metalindex { metal == "copper" ? 0 : metal == "aluminium" ? 1 : -1 };
        auto const partindex { part == "wire" ? 0 : part == "fitting" ? 1 : -1 };
        auto const stateindex { state == "new" ? 0 : state == "worn" ? 1 : state == "black" ? 2 : state == "tarnished" ? 3 : -1 };
        if( stream.fail() || metalindex < 0 || partindex < 0 || stateindex < 0 ) {
            ErrorLog( "Bad file \"data/traction_materials.txt\" (line " + std::to_string( linenumber ) + "): expected <copper|aluminium> <wire|fitting> <new|worn|black|tarnished> and six numbers" );
            continue;
        }
        for( auto *number : { &value.color[ 0 ], &value.color[ 1 ], &value.color[ 2 ], &value.metalness, &value.gloss, &value.reflection } ) {
            *number = std::clamp( *number, 0.f, 1.f );
        }
        Metals[ metalindex ][ partindex ][ stateindex ] = value;
    }
}

} // namespace

// wygląd przewodów albo osprzętu przęsła o podanym materiale i stanie z wpisu scenerii
catenary_metal const &
catenary_span_metal( TTraction const &Traction, bool const Fitting ) {

    if( false == MetalsLoaded ) { load_metals(); }
    // stan z wpisu przęsła: bit 0 zaśniedziała, bit 1 przejściowa, bit 2 czarna; zaśniedziała ma pierwszeństwo, potem czarna
    auto const state { ( Traction.DamageFlag & 1 ) != 0 ? 3 : ( Traction.DamageFlag & 4 ) != 0 ? 2 : ( Traction.DamageFlag & 2 ) != 0 ? 1 : 0 };
    return Metals[ Traction.Material == 2 ? 1 : 0 ][ Fitting ? 1 : 0 ][ state ];
}

// przypisuje przęsłu typ sieci z dopisku "type" i dobiera rozstaw wieszaków
void
catenary_span_assign( TTraction &Traction, std::string const &Symbol, double const Messenger, double const Contact, glm::dvec3 const *Rope2, bool const *Rope2set, double const *Spread, double const *Pull ) {

    // przęsło wczytane od nowa (edytor, catenary_span_reload): bank brył przechodzi na nowy opis, bryły zrobią się od nowa
    gfx::geometrybank_handle bank;
    if( Traction.m_catenaryspan ) {
        give_back_solid( *Traction.m_catenaryspan, true );
        bank = Traction.m_catenaryspan->solidbank;
    }
    Traction.m_catenaryspan.reset();
    if( Symbol.empty() ) {
        if( bank != null_handle ) { SolidBanks.push_back( bank ); }
        return;
    }

    auto span { std::make_shared<catenary_span>() };
    span->symbol = Symbol;
    span->solidbank = bank;
    for( int end = 0; end < 2 && Rope2 != nullptr && Rope2set != nullptr; ++end ) {
        span->rope2set[ end ] = Rope2set[ end ];
        span->rope2[ end ] = catenary::vec3 { Rope2[ end ].x, Rope2[ end ].y, Rope2[ end ].z };
    }
    if( Spread != nullptr ) { span->spread[ 0 ] = Spread[ 0 ]; span->spread[ 1 ] = Spread[ 1 ]; }
    Traction.m_catenaryspan = span;
    if( false == Global.CatenaryProcedural ) { return; }

    auto &registry { catenary::types() };
    if( !registry.on_error ) {
        registry.on_error = []( std::string const &Message ) { ErrorLog( Message ); };
    }
    span->definition = registry.find( Symbol );
    if( span->definition == nullptr ) { return; } // brak typu został już zgłoszony, przęsło rysuje się po staremu
    // naciąg z dopisku "tension" (daN): przęsło liczone z kopią typu
    if( Messenger > 0.0 || Contact > 0.0 ) {
        span->own = std::make_shared<catenary::network_definition>( *span->definition );
        if( Messenger > 0.0 ) { span->own->type.messenger_tension = 10.0 * Messenger; }
        if( Contact > 0.0 && span->own->type.contact_tension > 0.0 ) { span->own->type.contact_tension = 10.0 * Contact; }
        span->definition = span->own.get();
    }
    // odstęp wieszaków 0 w scenerii oznacza odcinek bez wieszaków (do kotwienia albo urządzenia naprężającego):
    // rysowane są same liny i przewody w liczbie wg typu, a rozstaw wieszaków zostaje pusty
    if( Traction.iNumSections == 0 ) { return; }
    auto const length { std::hypot( Traction.pPoint2.x - Traction.pPoint1.x, Traction.pPoint2.z - Traction.pPoint1.z ) };
    if( Pull != nullptr && Pull[ 1 ] > length + 0.01 ) {
        // część rozpiętości przedzielonej odciągami: wieszaki tablicy całej rozpiętości, które wypadają na tym przęśle
        span->pulled = true;
        span->pull[ 0 ] = Pull[ 0 ];
        span->pull[ 1 ] = Pull[ 1 ];
        auto const gaps { registry.hanger_gaps( span->definition->hanger_table, Pull[ 1 ] ) };
        std::vector<double> inside;
        double at { 0.0 };
        for( std::size_t k = 0; k + 1 < gaps.size(); ++k ) {
            at += gaps[ k ];
            auto const local { at - Pull[ 0 ] };
            if( local > 0.05 && local < length - 0.05 ) { inside.push_back( local ); }
        }
        double previous { 0.0 };
        for( auto const position : inside ) { span->hangergaps.push_back( position - previous ); previous = position; }
        span->hangergaps.push_back( length - previous );
        return;
    }
    span->hangergaps = registry.hanger_gaps( span->definition->hanger_table, length );
}

// wygląd liny stalowej (ocynk, lekko zmatowiały)
catenary_metal const &
catenary_rope_metal() {
    static catenary_metal const steel { { 0.56f, 0.57f, 0.58f }, 0.8f, 0.30f, 0.20f };
    return steel;
}

// symbol typu sieci przypisany przęsłu; pusty gdy brak
std::string
catenary_span_symbol( TTraction const &Traction ) {
    return ( Traction.m_catenaryspan ? Traction.m_catenaryspan->symbol : std::string() );
}

// odcinki przęsła policzone wg typu sieci; false gdy przęsło ma być narysowane po staremu
bool
catenary_span_lines( TTraction &Traction, gfx::vertex_array &Vertices ) {

    if( definition( Traction ) == nullptr ) { return false; }
    catenary::span_geometry geometry;
    if( false == build( Traction, geometry ) ) { return false; }
    Vertices.reserve( Vertices.size() + 2 * geometry.elements.size() );
    for( auto const &element : geometry.elements ) {
        Vertices.emplace_back( vertex( Traction, element.from ) );
        Vertices.emplace_back( vertex( Traction, element.to ) );
    }
    return true;
}

// bryły przęsła do oglądania z bliska; tworzone, gdy kamera jest blisko przęsła, i zwalniane, gdy się oddali
catenary_solid
catenary_span_solid( TTraction &Traction, glm::dvec3 const &Camera ) {

    if( definition( Traction ) == nullptr ) { return {}; }
    auto &span { *Traction.m_catenaryspan };
    // odległość kamery od odcinka łączącego końce przewodu jezdnego
    auto const axis { Traction.pPoint2 - Traction.pPoint1 };
    auto const lengthsquared { glm::dot( axis, axis ) };
    auto const t { lengthsquared > 0.0 ? std::clamp( glm::dot( Camera - Traction.pPoint1, axis ) / lengthsquared, 0.0, 1.0 ) : 0.0 };
    // przybliżenie kamery i rozdzielczość skracają odległość tak samo, jak przy doborze szczegółowości reszty scenerii
    auto const distance {
        glm::length( Camera - ( Traction.pPoint1 + axis * t ) )
        / ( std::max( 0.01, static_cast<double>( Global.ZoomFactor ) ) * std::sqrt( std::max( 0.01, static_cast<double>( Global.fDistanceFactor ) ) ) ) };
    // bryły powstają w zasięgu z ustawień i są zwalniane z zapasem, żeby nie migały na granicy
    auto const loaddistance { static_cast<double>( CatenarySettings.solid_range ) };
    auto const releasedistance { loaddistance * 1.25 + 10.0 };

    if( span.solidloaded ) {
        if( distance > releasedistance || loaddistance <= 0.0 ) {
            // kamera daleko: zwolnienie pamięci, linie wystarczą
            release( span.solid.wires );
            release( span.solid.fittings );
            release( span.solid.rope );
            span.solid = {};
            span.solidloaded = false;
            return {};
        }
        return span.solid;
    }
    if( loaddistance <= 0.0 || distance > loaddistance ) { return {}; }

    catenary::span_geometry geometry;
    if( false == build( Traction, geometry ) ) { return {}; }
    std::vector<catenary::solid_vertex> wires, fittings;
    catenary::build_solid( *definition( Traction ), geometry, wires, fittings );
    if( wires.empty() ) { return {}; }
    // ogranicznik na skrzyżowaniu nad rozjazdem (33-0561): przewody dwóch łańcuchów krzyżują się w planie bliżej niż 3 cm w pionie.
    // Które przęsło jest dolne (szlakowe - ogranicznik na nim), decyduje raz cały odcinek obu łańcuchów wokół skrzyżowania (to przęsło
    // z sąsiednimi i przęsła drugiego łańcucha z sąsiednimi): suma różnic wysokości we wszystkich przejściach - nie każde przejście
    // osobno, bo przewód rozjazdowy w "V" (przechodzi nad szlakowymi i po chwili wraca, użytkownik 2026-11-16) w jednym przejściu
    // wypadał niżej i ogranicznik przeskakiwał na niego. Przejścia na tym samym dolnym przewodzie - jeden ogranicznik od pierwszego do
    // ostatniego; rysuje go przęsło, w którym leży jego środek (ogranicznik może sięgać przez koniec przęsła)
    if( CatenaryCrossingCandidates != nullptr ) {
        auto const radius { 0.5 * catenary::wire_diameter( definition( Traction )->contact_wire ) };
        std::vector<std::pair<TTraction *, catenary::span_geometry>> mine;
        auto const add_mine = [ & ]( TTraction *Span ) {
            if( Span == nullptr || definition( *Span ) == nullptr ) { return; }
            if( std::any_of( mine.begin(), mine.end(), [ Span ]( auto const &M ) { return M.first == Span; } ) ) { return; }
            catenary::span_geometry built;
            if( Span == &Traction ) { built = geometry; }
            else if( false == build( *Span, built ) ) { return; }
            mine.emplace_back( Span, std::move( built ) );
        };
        add_mine( &Traction );
        add_mine( Traction.hvNext[ 0 ] );
        add_mine( Traction.hvNext[ 1 ] );
        auto const ismine = [ & ]( TTraction const *Span ) { return std::any_of( mine.begin(), mine.end(), [ Span ]( auto const &M ) { return M.first == Span; } ); };
        // przęsła innych łańcuchów przy tym odcinku, z sąsiednimi
        std::vector<TTraction *> others;
        auto const add_other = [ & ]( TTraction *Span ) {
            if( Span == nullptr || ismine( Span ) || definition( *Span ) == nullptr ) { return; }
            if( std::find( others.begin(), others.end(), Span ) == others.end() ) { others.push_back( Span ); }
        };
        for( std::size_t k = 0; k < mine.size(); ++k ) {
            for( auto *other : CatenaryCrossingCandidates( *mine[ k ].first ) ) {
                add_other( other );
                if( other != nullptr ) { add_other( other->hvNext[ 0 ] ); add_other( other->hvNext[ 1 ] ); }
            }
        }
        // podział na ciągłe odcinki (przęsła połączone końcami) - każdy odcinek drugiego łańcucha osobno
        std::vector<bool> used( others.size(), false );
        for( std::size_t start = 0; start < others.size(); ++start ) {
            if( used[ start ] ) { continue; }
            std::vector<std::size_t> part { start };
            used[ start ] = true;
            for( std::size_t k = 0; k < part.size(); ++k ) {
                for( std::size_t j = 0; j < others.size(); ++j ) {
                    if( used[ j ] ) { continue; }
                    auto const *a { others[ part[ k ] ] }, *b { others[ j ] };
                    if( a->hvNext[ 0 ] == b || a->hvNext[ 1 ] == b || b->hvNext[ 0 ] == a || b->hvNext[ 1 ] == a ) { used[ j ] = true; part.push_back( j ); }
                }
            }
            std::vector<catenary::crossing_raw> points;
            std::vector<TTraction const *> owners;
            for( auto const index : part ) {
                catenary::span_geometry othergeometry;
                if( false == build( *others[ index ], othergeometry ) ) { continue; }
                for( auto const &item : mine ) {
                    catenary::crossing_points( item.second, othergeometry, catenary::kCrossingLimiterGap, points );
                    owners.resize( points.size(), item.first );
                }
            }
            if( points.empty() ) { continue; }
            // dolne jest to przęsło, gdy drugi łańcuch idzie średnio wyżej (remis - rozstrzyga nazwa, żeby oba łańcuchy zgodziły się)
            auto sum { 0.0 };
            for( auto const &point : points ) { sum += point.gap; }
            auto const lower { sum > 1e-6 || ( std::abs( sum ) <= 1e-6 && Traction.m_name < others[ part.front() ]->m_name ) };
            if( false == lower ) { continue; }
            std::vector<catenary::crossing_point> crossings;
            catenary::group_crossings( points, crossings );
            for( auto const &crossing : crossings ) {
                // środek ogranicznika - w którym przęśle tego łańcucha (najbliższy punkt przejścia)
                auto const &a { crossing.position };
                auto const &b { crossing.spread ? crossing.last : crossing.position };
                catenary::vec3 const middle { 0.5 * ( a.x + b.x ), 0.5 * ( a.y + b.y ), 0.5 * ( a.z + b.z ) };
                std::size_t nearest { 0 };
                auto best { 1e30 };
                for( std::size_t k = 0; k < points.size(); ++k ) {
                    auto const &p { points[ k ].position };
                    auto const distance { std::hypot( p.x - middle.x, p.z - middle.z ) };
                    if( distance < best ) { best = distance; nearest = k; }
                }
                if( owners[ nearest ] != &Traction ) { continue; }
                catenary::build_crossing_limiter( crossing, radius, wires, fittings );
            }
        }
    }
    if( span.solidbank == null_handle ) {
        if( false == SolidBanks.empty() ) {
            span.solidbank = SolidBanks.back();
            SolidBanks.pop_back();
        }
        else {
            span.solidbank = GfxRenderer->Create_Bank();
        }
    }
    // każde ponowne wczytanie tworzy nowe fragmenty w banku tego przęsła; poprzednie zostały opróżnione przy zwalnianiu
    gfx::userdata_array userdata;
    {
        auto data { vertices( Traction, wires ) };
        span.solid.wires = GfxRenderer->Insert( data, userdata, span.solidbank, GL_TRIANGLES );
    }
    if( false == fittings.empty() ) {
        auto data { vertices( Traction, fittings ) };
        span.solid.fittings = GfxRenderer->Insert( data, userdata, span.solidbank, GL_TRIANGLES );
    }
    if( false == span.attached.empty() ) {
        auto data { vertices( Traction, span.attached ) };
        span.solid.rope = GfxRenderer->Insert( data, userdata, span.solidbank, GL_TRIANGLES );
    }
    span.solidloaded = ( span.solid.wires != null_handle );
    return ( span.solidloaded ? span.solid : catenary_solid() );
}

namespace {

// bryły przęsła zwolnione; zrobią się od nowa, gdy kamera podejdzie
void
drop_solid( TTraction &Traction ) {
    if( false == static_cast<bool>( Traction.m_catenaryspan ) ) { return; }
    auto &span { *Traction.m_catenaryspan };
    if( false == span.solidloaded ) { return; }
    release( span.solid.wires );
    release( span.solid.fittings );
    release( span.solid.rope );
    span.solid = {};
    span.solidloaded = false;
}

} // namespace

// przęsło po zmianie sąsiadów (przęsło obok zrobione albo usunięte w edytorze): linie liczone od nowa, bryły do zrobienia
// od nowa. kształt przęsła zależy od sąsiadów (wspólne linki Y), a linie powstają raz, przy tworzeniu geometrii sekcji,
// i bryły też są trzymane, dopóki kamera jest blisko – bez tego zostawały policzone dla poprzednich sąsiadów
void
catenary_span_refresh( TTraction &Traction ) {
    drop_solid( Traction );
    if( Traction.m_geometry == null_handle ) { return; }
    gfx::vertex_array vertices;
    if( false == catenary_span_lines( Traction, vertices ) ) { return; }
    gfx::userdata_array userdata;
    GfxRenderer->Replace( vertices, userdata, Traction.m_geometry, GL_LINES );
}

// zwalnia linie i bryły usuwanego przęsła
void
catenary_span_release( TTraction &Traction ) {
    if( Traction.m_catenaryspan ) { give_back_solid( *Traction.m_catenaryspan, false ); }
    release( Traction.m_geometry );
}

// bryła spoza przęsła rysowana razem z nim (lina kotwienia); zrobi się przy następnym podejściu kamery
bool
catenary_span_attach( TTraction &Traction, std::vector<catenary::solid_vertex> const &Triangles ) {
    if( false == static_cast<bool>( Traction.m_catenaryspan ) ) { return false; }
    drop_solid( Traction );
    Traction.m_catenaryspan->attached = Triangles;
    return ( Traction.m_catenaryspan->definition != nullptr );
}

// przerwy na izolator sekcyjny (dopisek "gap"); linie i bryły zrobią się od nowa przy tworzeniu geometrii
void
catenary_span_gaps( TTraction &Traction, double const *Contact, double const *Messenger ) {
    if( false == static_cast<bool>( Traction.m_catenaryspan ) ) { return; }
    auto &span { *Traction.m_catenaryspan };
    double const *given[ 2 ] { Contact, Messenger };
    for( int which = 0; which < 2; ++which ) {
        span.gapped[ which ] = ( given[ which ] != nullptr && given[ which ][ 1 ] > given[ which ][ 0 ] );
        if( span.gapped[ which ] ) { span.gap[ which ][ 0 ] = given[ which ][ 0 ]; span.gap[ which ][ 1 ] = given[ which ][ 1 ]; }
    }
}

// punkt liny nośnej albo osi przewodów jezdnych w odległości Along w planie od pierwszego końca (średnia z przewodów, które tam są)
bool
catenary_span_point( TTraction &Traction, double const Along, bool const Messenger, glm::dvec3 &Point ) {
    if( definition( Traction ) == nullptr ) { return false; }
    catenary::span_geometry geometry;
    if( false == build( Traction, geometry, false ) ) { return false; }
    auto const kind { Messenger ? catenary::element_kind::messenger : catenary::element_kind::contact };
    double sum[ 3 ] { 0.0, 0.0, 0.0 };
    int count { 0 };
    for( auto const &element : geometry.elements ) {
        if( element.kind != kind ) { continue; }
        auto const a { plan_along( Traction, element.from ) }, b { plan_along( Traction, element.to ) };
        if( std::abs( b - a ) < 1e-9 || Along < std::min( a, b ) || Along > std::max( a, b ) ) { continue; }
        auto const t { ( Along - a ) / ( b - a ) };
        sum[ 0 ] += element.from.x + ( element.to.x - element.from.x ) * t;
        sum[ 1 ] += element.from.y + ( element.to.y - element.from.y ) * t;
        sum[ 2 ] += element.from.z + ( element.to.z - element.from.z ) * t;
        ++count;
        // lina nośna: pierwsza (górna) wystarczy - druga lina biegnie obok niej
        if( Messenger ) { break; }
    }
    if( count == 0 ) { return false; }
    Point.x = sum[ 0 ] / count;
    Point.y = sum[ 1 ] / count;
    Point.z = sum[ 2 ] / count;
    return true;
}
