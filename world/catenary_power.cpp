/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

// Sieć trakcyjna: obwód prądu stałego nowej sieci. Opis w world/catenary_power.h

#include "world/catenary_power.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <locale>
#include <map>
#include <mutex>
#include <numeric>
#include <sstream>

namespace catenary_power {

namespace {

settings Settings;
bool SettingsLoaded { false };
std::mutex SettingsMutex;

// najmniejszy opór gałęzi: łączniki „bez oporu” nie mogą dawać nieskończonej przewodności (macierz traci uwarunkowanie)
double const kMinResistance { 1.0e-4 };
// odłącznik zamknięty (styki, linki połączeń) i zwarcie uszyniającego
double const kSwitchResistance { 5.0e-4 };

} // namespace

void load_settings( std::string const &Path ) {
    std::lock_guard<std::mutex> lock { SettingsMutex };
    SettingsLoaded = true;
    Settings = settings();
    std::ifstream file { Path.empty() ? std::string( "data/traction_power.txt" ) : Path };
    if( false == file.is_open() ) { return; }
    std::map<std::string, double *> numbers {
        { "rail", &Settings.rail }, { "feeder", &Settings.feeder }, { "cabin", &Settings.cabin }, { "leakage", &Settings.leakage },
        { "regen_limit", &Settings.regen_limit },
        { "copper_contact", &Settings.copper_contact }, { "copper_rope", &Settings.copper_rope },
        { "aluminium", &Settings.aluminium }, { "bronze", &Settings.bronze } };
    std::string text;
    while( std::getline( file, text ) ) {
        if( auto const comment { text.find( "//" ) }; comment != std::string::npos ) { text.erase( comment ); }
        std::istringstream line { text };
        line.imbue( std::locale::classic() );
        std::string key;
        if( false == static_cast<bool>( line >> key ) ) { continue; }
        if( auto const found { numbers.find( key ) }; found != numbers.end() ) {
            double value;
            if( line >> value ) { *found->second = value; }
        }
    }
}

settings const &current_settings() {
    if( false == SettingsLoaded ) { load_settings(); }
    return Settings;
}

int circuit::add_node() {
    m_voltage.push_back( 0.0 );
    m_islandsvalid = false;
    return static_cast<int>( m_voltage.size() ) - 1;
}

int circuit::add_branch( int const A, int const B, double const Resistance ) {
    branch item;
    item.a = A;
    item.b = B;
    item.resistance = std::max( kMinResistance, Resistance );
    m_branches.push_back( item );
    m_islandsvalid = false;
    return static_cast<int>( m_branches.size() ) - 1;
}

int circuit::add_source( source const &Source ) {
    m_sources.push_back( Source );
    m_sources.back().internal = std::max( kMinResistance, Source.internal );
    m_islandsvalid = false;
    return static_cast<int>( m_sources.size() ) - 1;
}

void circuit::clear() {
    m_branches.clear();
    m_sources.clear();
    m_voltage.clear();
    m_islandsvalid = false;
}

void circuit::add_load( int const Branch, double const Current, double const Position ) {
    if( Branch < 0 || Branch >= static_cast<int>( m_branches.size() ) ) { return; }
    m_branches[ Branch ].load += Current;
    m_branches[ Branch ].load_b += Current * std::clamp( Position, 0.0, 1.0 );
}

void circuit::clear_loads() {
    for( auto &item : m_branches ) { item.load = 0.0; item.load_b = 0.0; }
}

double circuit::voltage( int const Node ) const {
    return ( Node >= 0 && Node < static_cast<int>( m_voltage.size() ) ) ? m_voltage[ Node ] : 0.0;
}

double circuit::branch_voltage( int const Branch ) const {
    if( Branch < 0 || Branch >= static_cast<int>( m_branches.size() ) ) { return 0.0; }
    auto const &item { m_branches[ Branch ] };
    // pobór w środku gałęzi: dopływa połówkami z obu końców, każda przez pół oporu
    // stały pobór przy przeciążeniu daje w modelu liniowym napięcie ujemne - pojazd widzi wtedy zero
    return std::max( 0.0, 0.5 * ( voltage( item.a ) + voltage( item.b ) ) - item.load * item.resistance * 0.25 );
}

double circuit::branch_voltage_at( int const Branch, double Position, double const Current ) const {
    if( Branch < 0 || Branch >= static_cast<int>( m_branches.size() ) ) { return 0.0; }
    auto const &item { m_branches[ Branch ] };
    Position = std::clamp( Position, 0.0, 1.0 );
    // pobór w miejscu f: dopływa z końca a przez f oporu i z b przez 1 - f - spadek I·R·f·(1 - f) poniżej prostej między końcami
    return std::max( 0.0, voltage( item.a ) * ( 1.0 - Position ) + voltage( item.b ) * Position - Current * item.resistance * Position * ( 1.0 - Position ) );
}

double circuit::branch_current( int const Branch ) const {
    if( Branch < 0 || Branch >= static_cast<int>( m_branches.size() ) ) { return 0.0; }
    auto const &item { m_branches[ Branch ] };
    return ( voltage( item.a ) - voltage( item.b ) ) / item.resistance;
}

int circuit::island( int const Node ) const {
    return ( m_islandsvalid && Node >= 0 && Node < static_cast<int>( m_island.size() ) ) ? m_island[ Node ] : -1;
}

double circuit::regen_scale( int const Node ) const {
    auto const which { island( Node ) };
    return ( which >= 0 && which < static_cast<int>( m_regenscale.size() ) ) ? m_regenscale[ which ] : 1.0;
}

void circuit::prepare() {
    if( false == m_islandsvalid ) { build_islands(); }
}

bool circuit::island_powered( int const Node ) const {
    auto const which { island( Node ) };
    return ( which >= 0 && which < static_cast<int>( m_islandsources.size() ) ) && false == m_islandsources[ which ].empty();
}

void circuit::build_islands() {
    auto const count { m_voltage.size() };
    std::vector<int> parent( count );
    std::iota( parent.begin(), parent.end(), 0 );
    auto const root = [ & ]( int Node ) {
        while( parent[ Node ] != Node ) {
            parent[ Node ] = parent[ parent[ Node ] ];
            Node = parent[ Node ];
        }
        return Node;
    };
    m_nodebranches.assign( count, {} );
    for( std::size_t index = 0; index < m_branches.size(); ++index ) {
        auto const &item { m_branches[ index ] };
        if( item.a < 0 || item.b < 0 || item.a >= static_cast<int>( count ) || item.b >= static_cast<int>( count ) ) { continue; }
        m_nodebranches[ item.a ].push_back( static_cast<int>( index ) );
        if( item.b != item.a ) { m_nodebranches[ item.b ].push_back( static_cast<int>( index ) ); }
        parent[ root( item.a ) ] = root( item.b );
    }
    m_island.assign( count, -1 );
    m_islandnodes.clear();
    m_islandsources.clear();
    std::map<int, int> numbers;
    for( std::size_t node = 0; node < count; ++node ) {
        auto const top { root( static_cast<int>( node ) ) };
        auto found { numbers.find( top ) };
        if( found == numbers.end() ) {
            found = numbers.emplace( top, static_cast<int>( m_islandnodes.size() ) ).first;
            m_islandnodes.emplace_back();
            m_islandsources.emplace_back();
        }
        m_island[ node ] = found->second;
        m_islandnodes[ found->second ].push_back( static_cast<int>( node ) );
    }
    m_local.assign( count, 0 );
    for( auto const &members : m_islandnodes ) {
        for( std::size_t index = 0; index < members.size(); ++index ) { m_local[ members[ index ] ] = index; }
    }
    m_cache.assign( m_islandnodes.size(), island_cache() );
    m_regenscale.assign( m_islandnodes.size(), 1.0 );
    for( std::size_t index = 0; index < m_sources.size(); ++index ) {
        auto const node { m_sources[ index ].node };
        if( node < 0 || node >= static_cast<int>( count ) ) { continue; }
        m_islandsources[ m_island[ node ] ].push_back( static_cast<int>( index ) );
    }
    m_islandsvalid = true;
}

// rozkład LDLᵀ macierzy przewodności wyspy w obwiedni (profilu) po numeracji odwrotną metodą Cuthilla–McKee: sieć trakcyjna to
// w większości łańcuchy z nielicznymi połączeniami poprzecznymi, więc obwiednia jest wąska i rozkład kosztuje prawie tyle,
// co przejście po węzłach. Rozkład liczony od nowa tylko przy zmianie układu (wyspy) albo stanu zasilaczy (dioda, wyłącznik);
// co krok tylko podstawienie z nowymi prądami odbiorników
void circuit::factor_island( std::size_t const Island ) {
    auto const &config { current_settings() };
    auto &cache { m_cache[ Island ] };
    auto const &nodes { m_islandnodes[ Island ] };
    auto const count { nodes.size() };
    if( cache.position.empty() ) {
        // numeracja: przeszukiwanie wszerz od węzła o najmniejszym stopniu, sąsiedzi rosnąco wg stopnia, na koniec odwrócona
        std::map<int, std::size_t> local;
        for( std::size_t index = 0; index < count; ++index ) { local.emplace( nodes[ index ], index ); }
        std::vector<std::vector<std::pair<std::size_t, double>>> adjacency( count );
        std::vector<char> seen( m_branches.size(), 0 );
        for( auto const node : nodes ) {
            for( auto const index : m_nodebranches[ node ] ) {
                if( seen[ index ] ) { continue; }
                seen[ index ] = 1;
                auto const &item { m_branches[ index ] };
                auto const a { local[ item.a ] }, b { local[ item.b ] };
                if( a == b ) { continue; }
                adjacency[ a ].push_back( { b, 1.0 / item.resistance } );
                adjacency[ b ].push_back( { a, 1.0 / item.resistance } );
            }
        }
        std::vector<std::size_t> order;
        order.reserve( count );
        std::vector<char> placed( count, 0 );
        auto const degree = [ & ]( std::size_t const Node ) { return adjacency[ Node ].size(); };
        while( order.size() < count ) {
            std::size_t start { count };
            for( std::size_t index = 0; index < count; ++index ) {
                if( placed[ index ] ) { continue; }
                if( start == count || degree( index ) < degree( start ) ) { start = index; }
            }
            placed[ start ] = 1;
            auto head { order.size() };
            order.push_back( start );
            while( head < order.size() ) {
                auto const node { order[ head++ ] };
                std::vector<std::size_t> next;
                for( auto const &link : adjacency[ node ] ) {
                    if( false == placed[ link.first ] ) {
                        placed[ link.first ] = 1;
                        next.push_back( link.first );
                    }
                }
                std::sort( next.begin(), next.end(), [ & ]( std::size_t const A, std::size_t const B ) { return degree( A ) < degree( B ); } );
                order.insert( order.end(), next.begin(), next.end() );
            }
        }
        std::reverse( order.begin(), order.end() );
        cache.node.resize( count );
        cache.position.resize( count );
        for( std::size_t index = 0; index < count; ++index ) {
            cache.node[ index ] = nodes[ order[ index ] ];
            cache.position[ order[ index ] ] = index;
        }
        // obwiednia: pierwsza niezerowa kolumna każdego wiersza
        cache.first.resize( count );
        for( std::size_t row = 0; row < count; ++row ) { cache.first[ row ] = row; }
        cache.links.clear();
        for( std::size_t a = 0; a < count; ++a ) {
            for( auto const &link : adjacency[ a ] ) {
                auto const pa { cache.position[ a ] }, pb { cache.position[ link.first ] };
                if( pa > pb ) {
                    cache.first[ pa ] = std::min( cache.first[ pa ], pb );
                    cache.links.push_back( { pa, pb, link.second } );
                }
            }
        }
        cache.start.resize( count + 1 );
        cache.start[ 0 ] = 0;
        for( std::size_t row = 0; row < count; ++row ) { cache.start[ row + 1 ] = cache.start[ row ] + ( row - cache.first[ row ] + 1 ); }
        cache.factored = false;
    }
    // macierz: przewodności gałęzi, upływność, przewodności wewnętrzne przewodzących zasilaczy
    auto &value { cache.value };
    value.assign( cache.start[ count ], 0.0 );
    auto const at = [ & ]( std::size_t const Row, std::size_t const Column ) -> double & { return value[ cache.start[ Row ] + ( Column - cache.first[ Row ] ) ]; };
    for( std::size_t row = 0; row < count; ++row ) { at( row, row ) = config.leakage; }
    for( auto const &link : cache.links ) {
        at( link.row, link.row ) += link.conductance;
        at( link.column, link.column ) += link.conductance;
        at( link.row, link.column ) -= link.conductance;
    }
    for( auto const index : m_islandsources[ Island ] ) {
        auto const &item { m_sources[ index ] };
        if( item.tripped || false == item.conducting ) { continue; }
        auto const row { cache.position[ m_local[ item.node ] ] };
        at( row, row ) += 1.0 / item.internal;
    }
    // LDLᵀ w miejscu: pod przekątną L, na przekątnej D
    for( std::size_t i = 0; i < count; ++i ) {
        auto const fi { cache.first[ i ] };
        for( std::size_t j = fi; j < i; ++j ) {
            auto const fj { cache.first[ j ] };
            auto sum { at( i, j ) };
            for( std::size_t k = std::max( fi, fj ); k < j; ++k ) { sum -= at( i, k ) * at( j, k ) * at( k, k ); }
            at( i, j ) = sum / at( j, j );
        }
        auto diagonal { at( i, i ) };
        for( std::size_t k = fi; k < i; ++k ) { diagonal -= at( i, k ) * at( i, k ) * at( k, k ); }
        at( i, i ) = diagonal;
    }
    cache.factored = true;
}

// podstawienie: x = (LDLᵀ)⁻¹ · b, wszystko w numeracji rozkładu
void circuit::substitute( std::size_t const Island, std::vector<double> &Values ) const {
    auto const &cache { m_cache[ Island ] };
    auto const count { cache.first.size() };
    auto const at = [ & ]( std::size_t const Row, std::size_t const Column ) { return cache.value[ cache.start[ Row ] + ( Column - cache.first[ Row ] ) ]; };
    for( std::size_t i = 0; i < count; ++i ) {
        for( std::size_t k = cache.first[ i ]; k < i; ++k ) { Values[ i ] -= at( i, k ) * Values[ k ]; }
    }
    for( std::size_t i = 0; i < count; ++i ) { Values[ i ] /= at( i, i ); }
    for( std::size_t i = count; i-- > 0; ) {
        for( std::size_t k = cache.first[ i ]; k < i; ++k ) { Values[ k ] -= at( i, k ) * Values[ i ]; }
    }
}

// jedna wyspa: G·v = I (prądy źródeł minus odbiorniki); diody prostowników i ograniczenie rekuperacji rozwiązywane powtórzeniami
void circuit::solve_island( std::size_t const Island ) {
    auto const &config { current_settings() };
    auto const &nodes { m_islandnodes[ Island ] };
    auto const &sources { m_islandsources[ Island ] };
    auto const count { nodes.size() };
    if( count == 0 ) { return; }
    // bez żadnego załączonego zasilacza wyspa jest bez napięcia (pobór nie ma skąd płynąć, a rekuperacji nie ma kto przyjąć)
    if( std::none_of( sources.begin(), sources.end(), [ & ]( int const Index ) { return false == m_sources[ Index ].tripped; } ) ) {
        for( auto const node : nodes ) { m_voltage[ node ] = 0.0; }
        return;
    }
    auto &cache { m_cache[ Island ] };
    // stan zasilaczy, z którym liczony był rozkład
    auto const pattern = [ & ]() {
        std::vector<char> result;
        for( auto const index : sources ) { result.push_back( static_cast<char>( ( m_sources[ index ].tripped ? 2 : 0 ) | ( m_sources[ index ].conducting ? 1 : 0 ) ) ); }
        return result;
    };
    // pobór i rekuperacja w węzłach (numeracja rozkładu); rekuperację ogranicza napięcie sieci
    if( cache.position.empty() ) { factor_island( Island ); cache.pattern = pattern(); }
    std::vector<double> load( count, 0.0 ), regen( count, 0.0 );
    for( auto const node : nodes ) {
        for( auto const index : m_nodebranches[ node ] ) {
            auto const &item { m_branches[ index ] };
            if( item.load == 0.0 ) { continue; }
            auto &target { item.load > 0.0 ? load : regen };
            // gałąź liczona przy każdym ze swoich końców osobno: do końca a część 1 - f, do b część f poboru
            auto const share { item.a == item.b ? item.load : node == item.b ? item.load_b : item.load - item.load_b };
            target[ cache.position[ m_local[ node ] ] ] += share;
        }
    }
    std::vector<double> x( count );
    auto const linear = [ & ]( double const Regen ) {
        if( false == cache.factored || cache.pattern != pattern() ) {
            factor_island( Island );
            cache.pattern = pattern();
        }
        std::fill( x.begin(), x.end(), 0.0 );
        for( auto const index : sources ) {
            auto const &item { m_sources[ index ] };
            if( item.tripped || false == item.conducting ) { continue; }
            x[ cache.position[ m_local[ item.node ] ] ] += item.voltage / item.internal;
        }
        for( std::size_t index = 0; index < count; ++index ) { x[ index ] -= load[ index ] + Regen * regen[ index ]; }
        substitute( Island, x );
    };
    auto const node_voltage = [ & ]( int const Node ) { return x[ cache.position[ m_local[ Node ] ] ]; };
    // diody: prostownik bez rekuperacji nie przyjmuje prądu wstecznego; włącza się znowu, gdy napięcie sieci spadnie poniżej źródłowego
    auto const diodes = [ & ]() {
        auto changed { false };
        for( auto const index : sources ) {
            auto &item { m_sources[ index ] };
            if( item.tripped ) { continue; }
            auto const v { node_voltage( item.node ) };
            if( item.conducting && false == item.recuperation && v > item.voltage + 1.0e-6 ) {
                item.conducting = false;
                changed = true;
            }
            else if( false == item.conducting && ( item.recuperation || v < item.voltage - 1.0e-6 ) ) {
                item.conducting = true;
                changed = true;
            }
        }
        return changed;
    };
    auto const highest = [ & ]() {
        double result { 0.0 };
        for( auto const value : x ) { result = std::max( result, value ); }
        return result;
    };
    auto const hasregen { std::any_of( regen.begin(), regen.end(), []( double const Value ) { return Value != 0.0; } ) };
    double scale { 1.0 };
    for( int attempt = 0; attempt < 8; ++attempt ) {
        linear( scale );
        if( diodes() ) { continue; }
        // sieć nie przyjmuje całej rekuperacji (brak odbiorników i zasilacza z rekuperacją): pojazdy oddają tylko tyle, ile utrzyma
        // napięcie poniżej progu. Napięcie zależy od części oddawanej liniowo - dwa rozwiązania (bez rekuperacji i z obecną) wyznaczają ją
        if( hasregen && highest() > config.regen_limit + 0.5 ) {
            auto const top { highest() };
            linear( 0.0 );
            auto const bottom { highest() };
            scale = ( top > bottom ) ? std::max( 0.0, scale * ( config.regen_limit - bottom ) / ( top - bottom ) ) : 0.0;
            linear( scale );
            diodes();
        }
        break;
    }
    for( std::size_t index = 0; index < count; ++index ) { m_voltage[ cache.node[ index ] ] = x[ index ]; }
    // rekuperacja przyjęta przez sieć: zapisana w gałęziach (pojazd odczyta, ile mógł oddać)
    m_regenscale[ Island ] = scale;
}

void circuit::solve( double const Dt ) {
    if( false == m_islandsvalid ) { build_islands(); }
    for( std::size_t island = 0; island < m_islandnodes.size(); ++island ) { solve_island( island ); }
    // prądy zasilaczy i wyłączniki szybkie
    for( auto &item : m_sources ) {
        auto const v { voltage( item.node ) };
        item.current = ( item.tripped || false == item.conducting ) ? 0.0 : ( item.voltage - v ) / item.internal;
        item.output = item.tripped ? 0.0 : v;
        if( item.tripped ) {
            item.timer += Dt;
            if( item.locked ? item.timer >= item.slow_timeout : item.timer >= item.fast_timeout ) {
                item.tripped = false;
                item.conducting = true;
                if( item.locked ) {
                    item.locked = false;
                    item.trips = 0;
                }
                item.timer = 0.0;
            }
            continue;
        }
        item.timer += Dt;
        // spokój dłużej niż długa przerwa: licznik ponownych załączeń od zera
        if( item.trips > 0 && item.timer > item.slow_timeout ) { item.trips = 0; }
        if( item.trip_current > 0.0 && std::abs( item.current ) > item.trip_current ) {
            item.tripped = true;
            item.timer = 0.0;
            ++item.trips;
            item.locked = ( item.trips > item.repetition );
        }
    }
}

namespace {

std::size_t find_root( std::vector<std::size_t> &Parent, std::size_t Item ) {
    while( Parent[ Item ] != Item ) {
        Parent[ Item ] = Parent[ Parent[ Item ] ];
        Item = Parent[ Item ];
    }
    return Item;
}

double distance( double const *A, double const *B ) {
    return std::sqrt( ( A[ 0 ] - B[ 0 ] ) * ( A[ 0 ] - B[ 0 ] ) + ( A[ 1 ] - B[ 1 ] ) * ( A[ 1 ] - B[ 1 ] ) + ( A[ 2 ] - B[ 2 ] ) * ( A[ 2 ] - B[ 2 ] ) );
}

} // namespace

void build( std::vector<span_input> const &Spans, std::vector<source_input> const &Sources, mapping &Result, std::vector<source> const &Previous, std::vector<link_input> const &Links ) {
    auto const &config { current_settings() };
    auto &network { Result.network };
    network.clear();
    Result.branch.assign( Spans.size(), -1 );
    Result.branch_after.assign( Spans.size(), -1 );
    Result.sources.clear();
    Result.spans = Spans;
    if( Spans.empty() ) { return; }
    std::map<std::string, std::size_t> named;
    for( std::size_t index = 0; index < Sources.size(); ++index ) { named.emplace( Sources[ index ].parameters.name, index ); }
    auto const section = [ & ]( std::string const &Name ) {
        auto const found { named.find( Name ) };
        return found != named.end() && Sources[ found->second ].section;
    };
    auto const gapped = []( span_input const &Span ) { return Span.gap[ 0 ] >= 0.0 && Span.gap[ 1 ] > Span.gap[ 0 ]; };
    auto const joined = [ & ]( span_input const &A, span_input const &B ) {
        // przęsło z izolatorem: granica elektryczna w jego przerwie, z sąsiadami łączy się zawsze
        return A.power == B.power || section( A.power ) || section( B.power ) || gapped( A ) || gapped( B );
    };
    // końce przęseł (przęsło * 2 + koniec) łączone w węzły
    auto const count { Spans.size() };
    std::vector<std::size_t> parent( count * 2 );
    std::iota( parent.begin(), parent.end(), 0 );
    auto const join = [ & ]( std::size_t const A, std::size_t const B ) { parent[ find_root( parent, A ) ] = find_root( parent, B ); };
    for( std::size_t item = 0; item < count; ++item ) {
        auto const &span { Spans[ item ] };
        for( int end = 0; end < 2; ++end ) {
            auto const next { span.next[ end ] };
            if( next < 0 || next >= static_cast<int>( count ) || false == joined( span, Spans[ next ] ) ) { continue; }
            join( item * 2 + end, static_cast<std::size_t>( next ) * 2 + span.nextend[ end ] );
        }
        for( auto const other : span.parallel ) {
            if( other < 0 || other >= static_cast<int>( count ) || other == static_cast<int>( item ) || Spans[ other ].power != span.power ) { continue; }
            for( int end = 0; end < 2; ++end ) {
                for( int otherend = 0; otherend < 2; ++otherend ) {
                    if( distance( span.ends[ end ], Spans[ other ].ends[ otherend ] ) < 3.0 ) { join( item * 2 + end, static_cast<std::size_t>( other ) * 2 + otherend ); }
                }
            }
        }
    }
    std::map<std::size_t, int> nodes;
    auto const node_of = [ & ]( std::size_t const Item ) {
        auto const top { find_root( parent, Item ) };
        auto found { nodes.find( top ) };
        if( found == nodes.end() ) { found = nodes.emplace( top, network.add_node() ).first; }
        return found->second;
    };
    std::vector<int> branches( count ), after( count, -1 );
    for( std::size_t item = 0; item < count; ++item ) {
        auto const &span { Spans[ item ] };
        auto const length { distance( span.ends[ 0 ], span.ends[ 1 ] ) };
        auto const perm { span.resistivity + config.rail * 1.0e-3 };
        if( span.gap[ 0 ] >= 0.0 && span.gap[ 1 ] > span.gap[ 0 ] ) {
            // izolator sekcyjny: dwie gałęzie, od końców do brzegów przerwy (brzegi to osobne węzły - przerwa w obwodzie)
            auto const g0 { std::clamp( span.gap[ 0 ], 0.0, 1.0 ) }, g1 { std::clamp( span.gap[ 1 ], 0.0, 1.0 ) };
            branches[ item ] = network.add_branch( node_of( item * 2 ), network.add_node(), length * g0 * perm );
            after[ item ] = network.add_branch( network.add_node(), node_of( item * 2 + 1 ), length * ( 1.0 - g1 ) * perm );
            continue;
        }
        branches[ item ] = network.add_branch( node_of( item * 2 ), node_of( item * 2 + 1 ), length * perm );
    }
    // części sieci (końce połączone przęsłami): zasilacz przyłączony w każdej, w której są przęsła z jego nazwą
    auto whole { parent };
    for( std::size_t item = 0; item < count; ++item ) {
        if( false == gapped( Spans[ item ] ) ) { whole[ find_root( whole, item * 2 ) ] = find_root( whole, item * 2 + 1 ); }
    }
    auto const linked = [ & ]( link_input const &Link ) {
        return Link.a >= 0 && Link.b >= 0 && Link.a < static_cast<int>( count ) && Link.b < static_cast<int>( count );
    };
    // odłączniki: łączą części w jedną całość dla zasilaczy (w każdym stanie)
    for( auto const &link : Links ) {
        if( false == linked( link ) ) { continue; }
        whole[ find_root( whole, static_cast<std::size_t>( link.a ) * 2 + link.a_end ) ] = find_root( whole, static_cast<std::size_t>( link.b ) * 2 + link.b_end );
    }
    std::map<std::pair<std::size_t, std::size_t>, std::pair<double, std::size_t>> nearest; // (zasilacz, część) -> (odległość, koniec)
    for( std::size_t item = 0; item < count; ++item ) {
        auto const found { named.find( Spans[ item ].power ) };
        if( found == named.end() ) { continue; }
        // przęsło z izolatorem: jego zasilacz to część przed przerwą (koniec 0); koniec 1 należy już do sekcji za izolatorem
        for( int end = 0; end < ( gapped( Spans[ item ] ) ? 1 : 2 ); ++end ) {
            auto const key { std::make_pair( found->second, find_root( whole, item * 2 + end ) ) };
            auto const gap { distance( Spans[ item ].ends[ end ], Sources[ found->second ].location ) };
            auto place { nearest.find( key ) };
            if( place == nearest.end() || gap < place->second.first ) { nearest[ key ] = { gap, item * 2 + end }; }
        }
    }
    std::map<std::size_t, int> buses;    // zasilacz -> węzeł jego szyny
    for( auto const &[ key, place ] : nearest ) {
        auto const &input { Sources[ key.first ] };
        auto bus { buses.find( key.first ) };
        if( bus == buses.end() ) {
            bus = buses.emplace( key.first, network.add_node() ).first;
            if( false == input.section ) {
                auto item { input.parameters };
                item.node = bus->second;
                for( auto const &kept : Previous ) {
                    if( kept.name != item.name ) { continue; }
                    item.tripped = kept.tripped;
                    item.locked = kept.locked;
                    item.timer = kept.timer;
                    item.trips = kept.trips;
                    item.conducting = kept.conducting;
                }
                network.add_source( item );
                Result.sources.push_back( item.name );
            }
        }
        network.add_branch( bus->second, node_of( place.second ), input.section ? config.cabin : config.feeder );
    }
    // odłączniki: zamknięty - gałąź, uszyniony - strona B zwarta z szynami (źródło 0 V przyjmujące prąd w obie strony, bez wyłącznika)
    for( auto const &link : Links ) {
        if( false == linked( link ) ) { continue; }
        auto const a { node_of( static_cast<std::size_t>( link.a ) * 2 + link.a_end ) }, b { node_of( static_cast<std::size_t>( link.b ) * 2 + link.b_end ) };
        if( link.state == 0 ) { network.add_branch( a, b, kSwitchResistance ); }
        else if( link.state == 2 ) {
            source earth;
            earth.name = "earth:" + link.name;
            earth.node = b;
            earth.voltage = 0.0;
            earth.internal = kSwitchResistance;
            earth.recuperation = true;
            earth.earth = true;
            network.add_source( earth );
            Result.sources.push_back( earth.name );
        }
    }
    // w obwodzie tylko przęsła części z zasilaczem; reszta (np. nazwa zasilacza bez wpisu w scenerii) po staremu
    network.prepare();
    for( std::size_t item = 0; item < count; ++item ) {
        auto const &first { network.branches()[ branches[ item ] ] };
        if( network.island_powered( first.a ) ) { Result.branch[ item ] = branches[ item ]; }
        if( after[ item ] >= 0 && network.island_powered( network.branches()[ after[ item ] ].b ) ) { Result.branch_after[ item ] = after[ item ]; }
    }
}

bool mapping::locate( std::size_t const Span, double Along, int &Branch, double &Position ) const {
    if( Span >= branch.size() ) { return false; }
    Along = std::clamp( Along, 0.0, 1.0 );
    auto const gapped { Span < spans.size() && spans[ Span ].gap[ 0 ] >= 0.0 && spans[ Span ].gap[ 1 ] > spans[ Span ].gap[ 0 ] };
    if( false == gapped ) {
        Branch = branch[ Span ];
        Position = Along;
        return Branch >= 0;
    }
    auto const g0 { std::clamp( spans[ Span ].gap[ 0 ], 0.0, 1.0 ) }, g1 { std::clamp( spans[ Span ].gap[ 1 ], 0.0, 1.0 ) };
    if( Along < 0.5 * ( g0 + g1 ) ) {
        Branch = branch[ Span ];
        Position = g0 > 1.0e-6 ? std::min( 1.0, Along / g0 ) : 1.0;
    }
    else {
        Branch = branch_after[ Span ];
        Position = g1 < 1.0 - 1.0e-6 ? std::max( 0.0, ( Along - g1 ) / ( 1.0 - g1 ) ) : 0.0;
    }
    return Branch >= 0;
}

} // namespace catenary_power
