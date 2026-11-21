/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

// Sieć trakcyjna: przęsła nowej sieci i zasilacze scenerii jako obwód prądu stałego. Opis w world/catenary_power_scene.h

#include "stdafx.h"

#include "world/catenary_power_scene.h"

#include "simulation/simulation.h"
#include "utilities/Logs.h"
#include "world/Traction.h"
#include "world/TractionPower.h"
#include "world/catenary_network.h"
#include "world/catenary_power.h"
#include "world/catenary_span.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <unordered_map>

namespace {

struct power_state {
    catenary_power::mapping map;
    std::unordered_map<TTraction const *, std::size_t> spans;  // przęsło w obwodzie -> numer przęsła w map
    std::unordered_map<std::string, int> sources;              // nazwa zasilacza -> źródło obwodu
    std::vector<double> pending;                               // pobór zgłoszony od ostatniego kroku, wg gałęzi
    std::vector<double> pending_b;                             // jego część przypadająca na koniec b gałęzi
    bool dirty { true };
    std::size_t spancount { 0 };
    std::size_t sourcecount { 0 };
};

power_state State;

// opór przęsła na metr: przewody jezdne i liny nośne równolegle (z typu sieci), bez typu - z wpisu przęsła
double span_resistivity( TTraction const &Span ) {
    auto const &config { catenary_power::current_settings() };
    auto const symbol { catenary_span_symbol( Span ) };
    if( false == symbol.empty() ) {
        if( auto const *definition { catenary::types().find( symbol ) }; definition != nullptr ) {
            auto const aluminium { Span.Material == 2 };
            // przekroje w mm², opór właściwy w Ω·mm²/m
            auto const contact { definition->type.contact_count * definition->contact_wire.area * 1.0e6 / ( aluminium ? config.aluminium : config.copper_contact ) };
            auto const messenger { definition->type.messenger_count * definition->messenger_wire.area * 1.0e6 / ( aluminium ? config.aluminium : config.copper_rope ) };
            if( contact + messenger > 0.0 ) { return 1.0 / ( contact + messenger ); }
        }
    }
    return Span.fResistivity > 0.f ? Span.fResistivity : 0.075e-3;
}

void copy_point( glm::dvec3 const &Point, double *Target ) {
    Target[ 0 ] = Point.x;
    Target[ 1 ] = Point.y;
    Target[ 2 ] = Point.z;
}

void rebuild() {
    State.spans.clear();
    State.sources.clear();
    State.dirty = false;
    std::vector<TTraction *> spans;
    for( auto *span : simulation::Traction.sequence() ) {
        if( span != nullptr && span->m_catenary ) { spans.push_back( span ); }
    }
    State.spancount = spans.size();
    State.sourcecount = simulation::Powergrid.sequence().size();
    std::unordered_map<TTraction const *, int> index;
    for( std::size_t item = 0; item < spans.size(); ++item ) { index.emplace( spans[ item ], static_cast<int>( item ) ); }
    std::vector<catenary_power::span_input> inputs( spans.size() );
    for( std::size_t item = 0; item < spans.size(); ++item ) {
        auto const &span { *spans[ item ] };
        auto &input { inputs[ item ] };
        input.power = span.asPowerSupplyName;
        copy_point( span.pPoint1, input.ends[ 0 ] );
        copy_point( span.pPoint2, input.ends[ 1 ] );
        for( int end = 0; end < 2; ++end ) {
            if( auto const found { index.find( span.hvNext[ end ] ) }; span.hvNext[ end ] != nullptr && found != index.end() ) {
                input.next[ end ] = found->second;
                input.nextend[ end ] = span.iNext[ end ];
            }
        }
        // bieżnia wspólna: lista zapętlona; licznik na wypadek listy, która nie wraca do tego przęsła
        auto const *other { span.hvParallel };
        for( int step = 0; other != nullptr && other != &span && step < 16; ++step, other = other->hvParallel ) {
            if( auto const found { index.find( other ) }; found != index.end() ) { input.parallel.push_back( found->second ); }
        }
        input.resistivity = span_resistivity( span );
        // izolator sekcyjny w środku przęsła: przerwa w przewodach jezdnych jako część długości (w planie)
        double from, to;
        auto const plan { std::hypot( span.pPoint2.x - span.pPoint1.x, span.pPoint2.z - span.pPoint1.z ) };
        if( plan > 1e-3 && catenary_span_contact_gap( span, from, to ) ) {
            input.gap[ 0 ] = std::clamp( from / plan, 0.0, 1.0 );
            input.gap[ 1 ] = std::clamp( to / plan, 0.0, 1.0 );
        }
    }
    // zasilacze, do których odwołują się przęsła nowej sieci
    std::vector<catenary_power::source_input> sources;
    {
        std::unordered_map<std::string, bool> used;
        for( auto const &input : inputs ) { used.emplace( input.power, true ); }
        for( auto const &[ name, flag ] : used ) {
            auto const *source { simulation::Powergrid.find( name ) };
            if( source == nullptr ) { continue; }
            catenary_power::source_input item;
            item.parameters.name = name;
            item.parameters.voltage = source->nominal_voltage();
            item.parameters.internal = source->internal_resistance();
            item.parameters.trip_current = source->max_output_current();
            item.parameters.fast_timeout = source->fast_fuse_timeout();
            item.parameters.repetition = source->fast_fuse_repetition();
            item.parameters.slow_timeout = source->slow_fuse_timeout();
            item.parameters.recuperation = source->recuperation();
            item.section = source->bSection;
            copy_point( source->location(), item.location );
            sources.push_back( item );
        }
    }
    auto const previous { State.map.network.sources() };
    catenary_power::build( inputs, sources, State.map, previous );
    std::size_t kept { 0 };
    for( std::size_t item = 0; item < spans.size(); ++item ) {
        if( State.map.branch[ item ] < 0 && State.map.branch_after[ item ] < 0 ) { continue; }
        State.spans.emplace( spans[ item ], item );
        ++kept;
    }
    for( std::size_t item = 0; item < State.map.sources.size(); ++item ) { State.sources.emplace( State.map.sources[ item ], static_cast<int>( item ) ); }
    State.pending.assign( State.map.network.branches().size(), 0.0 );
    State.pending_b.assign( State.map.network.branches().size(), 0.0 );
    if( false == spans.empty() ) {
        WriteLog( "Catenary power: " + std::to_string( kept ) + " of " + std::to_string( spans.size() ) + " spans in the circuit, "
            + std::to_string( State.map.network.nodes() ) + " nodes, " + std::to_string( State.map.network.sources().size() ) + " feeders" );
    }
}

// zmiana parametrów zasilacza (edytor): przebudowa, bo zmienia się rozkład obwodu
bool sources_changed() {
    for( auto const &item : State.map.network.sources() ) {
        auto const *source { simulation::Powergrid.find( item.name ) };
        if( source == nullptr
         || source->nominal_voltage() != item.voltage
         || std::max( 1.0e-4, source->internal_resistance() ) != item.internal
         || source->max_output_current() != item.trip_current
         || source->recuperation() != item.recuperation ) {
            return true;
        }
    }
    return false;
}

} // namespace

void catenary_power_changed() {
    State.dirty = true;
    // przęsła mogły zniknąć: żadnych wskaźników do nich do przebudowy
    State.spans.clear();
}

void catenary_power_update( double const Dt ) {
    if( Dt <= 0.0 ) { return; }
    if( State.dirty
     || State.sourcecount != simulation::Powergrid.sequence().size()
     || sources_changed() ) {
        rebuild();
    }
    else {
        // przęsła dodane albo usunięte bez zgłoszenia: liczba się nie zgadza
        std::size_t count { 0 };
        for( auto const *span : simulation::Traction.sequence() ) {
            if( span != nullptr && span->m_catenary ) { ++count; }
        }
        if( count != State.spancount ) { rebuild(); }
    }
    if( State.spans.empty() ) { return; }
    auto &circuit { State.map.network };
    circuit.clear_loads();
    for( std::size_t item = 0; item < State.pending.size(); ++item ) {
        if( State.pending[ item ] == 0.0 ) { continue; }
        // pobór w miejscu wynikającym z części na koniec b (kilka pojazdów na gałęzi - średnie miejsce)
        circuit.add_load( static_cast<int>( item ), State.pending[ item ], State.pending_b[ item ] / State.pending[ item ] );
    }
    std::fill( State.pending.begin(), State.pending.end(), 0.0 );
    std::fill( State.pending_b.begin(), State.pending_b.end(), 0.0 );
    circuit.solve( Dt );
}

bool catenary_power_voltage( TTraction const &Span, double const Current, double const Along, double &Voltage ) {
    if( State.dirty ) { return false; }
    auto const found { State.spans.find( &Span ) };
    if( found == State.spans.end() ) { return false; }
    int branch { -1 };
    double position { 0.5 };
    if( false == State.map.locate( found->second, Along < 0.0 ? 0.5 : Along, branch, position ) ) {
        // część przęsła za izolatorem bez zasilacza (np. sekcja bez zasilania)
        Voltage = 0.0;
        return true;
    }
    State.pending[ branch ] += Current;
    State.pending_b[ branch ] += Current * position;
    Voltage = State.map.network.branch_voltage_at( branch, position, Current );
    return true;
}

bool catenary_power_feeder_state( std::string const &Name, catenary_power_feeder &Feeder ) {
    auto const found { State.sources.find( Name ) };
    if( found == State.sources.end() ) { return false; }
    auto const &item { State.map.network.sources()[ found->second ] };
    Feeder.current = item.current;
    Feeder.voltage = item.output;
    Feeder.tripped = item.tripped;
    Feeder.locked = item.locked;
    Feeder.conducting = item.conducting;
    Feeder.timer = item.tripped ? item.timer : 0.0;
    Feeder.trips = item.trips;
    return true;
}
