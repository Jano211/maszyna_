/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "stdafx.h"
#include "world/catenary_arcing.h"
#include "world/Traction.h"
#include "utilities/Globals.h"
#include "utilities/Logs.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
#include <unordered_map>

namespace catenary_arcing {

namespace {

float constexpr kGravity { 9.81f };

struct pantograph_state {
    TTraction const *span { nullptr };
    std::string power;              // zasilacz przęsła pod ślizgiem przy poprzednim kroku
    double current { 0.0 };         // prąd przy poprzednim kroku
    glm::dvec3 point { 0.0 };
    glm::dvec3 velocity { 0.0 };
    // łuk trwający
    float arctime { 0.f };          // ile jeszcze, s
    float arcstrength { 0.f };      // 0-1
    float sparktime { 0.f };        // iskry sypią się, dopóki trwa łuk
    float sustain { 0.f };          // ciągły słaby łuk (mocne oblodzenie), jasność 0-1; zeruje się, gdy ślizg nie jest w styku
    bool touched { false };         // ten pantograf był w styku w tym kroku (czyszczenie nieużywanych)
};

std::unordered_map<void const *, pantograph_state> Pantographs;
std::vector<arc> Arcs;
std::vector<spark> Sparks;
std::size_t Revision { 0 };
std::mt19937 Random { 0x1907u };

float uniform( float const Low, float const High ) {
    return std::uniform_real_distribution<float>( Low, High )( Random );
}

// kolor z pliku: #RRGGBB (sRGB) albo trzy liczby 0-1 (sRGB); Linear - zamienione na liniowe
bool read_color( std::istringstream &Line, glm::vec3 &Color, bool const Linear ) {
    std::string word;
    if( false == static_cast<bool>( Line >> word ) ) { return false; }
    glm::vec3 color;
    if( word[ 0 ] == '#' && word.size() == 7 ) {
        unsigned int value { 0 };
        std::istringstream hex { word.substr( 1 ) };
        if( false == static_cast<bool>( hex >> std::hex >> value ) ) { return false; }
        color = glm::vec3 { ( value >> 16 ) & 0xff, ( value >> 8 ) & 0xff, value & 0xff } / 255.f;
    }
    else {
        std::istringstream first { word };
        if( false == static_cast<bool>( first >> color.r ) || false == static_cast<bool>( Line >> color.g >> color.b ) ) { return false; }
    }
    Color = ( Linear ? glm::pow( glm::max( color, glm::vec3 { 0.f } ), glm::vec3 { 2.2f } ) : color );
    return true;
}

settings load_settings() {
    settings result;
    std::ifstream file { "data/traction_arcing.txt" };
    if( false == file.is_open() ) { return result; }
    // klucz -> liczby
    std::map<std::string, std::pair<float *, int>> numbers {
        { "icing_start", { &result.icing_start, 1 } }, { "icing_peak", { &result.icing_peak, 1 } },
        { "icing_cold_span", { &result.icing_cold_span, 1 } }, { "icing_cold_floor", { &result.icing_cold_floor, 1 } },
        { "icing_fog", { result.icing_fog, 2 } }, { "icing_dry", { &result.icing_dry, 1 } }, { "icing_spread", { result.icing_spread, 2 } },
        { "ice_rate", { &result.ice_rate, 1 } }, { "ice_speed", { &result.ice_speed, 1 } }, { "ice_current", { &result.ice_current, 1 } },
        { "ice_min_speed", { &result.ice_min_speed, 1 } }, { "ice_duration", { result.ice_duration, 2 } },
        { "sustain_from", { &result.sustain_from, 1 } }, { "sustain_level", { &result.sustain_level, 1 } }, { "sustain_sparks", { &result.sustain_sparks, 1 } },
        { "section_current", { &result.section_current, 1 } }, { "section_reference", { &result.section_reference, 1 } },
        { "section_duration", { result.section_duration, 2 } },
        { "release_current", { &result.release_current, 1 } }, { "release_reference", { &result.release_reference, 1 } },
        { "release_duration", { result.release_duration, 2 } },
        { "touch_base", { &result.touch_base, 1 } }, { "touch_filter", { &result.touch_filter, 1 } }, { "touch_ice", { &result.touch_ice, 1 } },
        { "touch_current", { &result.touch_current, 1 } }, { "touch_duration", { result.touch_duration, 2 } },
        { "arc_flicker", { &result.arc_flicker, 1 } }, { "arc_light", { &result.arc_light, 1 } }, { "arc_light_range", { &result.arc_light_range, 1 } },
        { "arc_core_size", { result.arc_core_size, 2 } },
        { "sparks_rate", { result.sparks_rate, 2 } }, { "sparks_spray", { result.sparks_spray, 2 } }, { "sparks_carry", { result.sparks_carry, 2 } },
        { "sparks_life", { result.sparks_life, 3 } }, { "sparks_drag", { &result.sparks_drag, 1 } }, { "sparks_max", { &result.sparks_max, 1 } } };
    std::string text;
    int linenumber { 0 };
    while( std::getline( file, text ) ) {
        ++linenumber;
        if( auto const comment { text.find( "//" ) }; comment != std::string::npos ) { text.erase( comment ); }
        std::istringstream line { text };
        line.imbue( std::locale::classic() );
        std::string key;
        if( false == static_cast<bool>( line >> key ) ) { continue; }
        auto good { true };
        if( auto const found { numbers.find( key ) }; found != numbers.end() ) {
            float values[ 3 ];
            for( int index = 0; index < found->second.second; ++index ) { good = good && static_cast<bool>( line >> values[ index ] ); }
            if( good ) { std::copy( values, values + found->second.second, found->second.first ); }
        }
        else if( key == "arc_color" ) {
            good = read_color( line, result.arc_color[ 0 ], true ) && read_color( line, result.arc_color[ 1 ], true );
        }
        else if( key.compare( 0, 11, "spark_color" ) == 0 || key == "core_color" ) {
            // kolor sRGB, jasność, alfa
            glm::vec3 color;
            float brightness { 1.f }, alpha { 1.f };
            good = read_color( line, color, false ) && static_cast<bool>( line >> brightness >> alpha );
            auto *target { key == "core_color" ? &result.core_color : nullptr };
            if( key.size() == 13 && key[ 11 ] == '_' && key[ 12 ] >= '1' && key[ 12 ] <= '4' ) { target = &result.spark_color[ key[ 12 ] - '1' ]; }
            if( target == nullptr ) { good = false; }
            if( good ) { *target = glm::vec4 { color * brightness, alpha }; }
        }
        else { good = false; }
        if( false == good ) {
            ErrorLog( "Bad file \"data/traction_arcing.txt\" (line " + std::to_string( linenumber ) + "): unknown key or wrong values \"" + text + "\"" );
        }
    }
    return result;
}

// zasilacz przęsła (granica sekcji: inny zasilacz)
std::string power_of( TTraction const *Span ) {
    return ( Span != nullptr ? Span->asPowerSupplyName : std::string() );
}

// oblodzenie przęsła: wspólne z pogody, każde przęsło trochę inaczej (osłonięte, nad wodą, w przekopie) - stałe dla przęsła
float span_icing( TTraction const *Span ) {
    auto const base { icing() };
    if( base <= 0.f || Span == nullptr ) { return base; }
    auto const key { static_cast<std::uint32_t>( reinterpret_cast<std::uintptr_t>( Span ) >> 4 ) * 2654435761u };
    auto const &set { parameters() };
    auto const spread { set.icing_spread[ 0 ] + ( set.icing_spread[ 1 ] - set.icing_spread[ 0 ] ) * static_cast<float>( key >> 8 ) / static_cast<float>( 0xffffffu ) };
    return std::clamp( base * spread, 0.f, 1.f );
}

// łuk na pantografie: siła 0-1, czas trwania
void ignite( pantograph_state &State, float const Strength, float const Duration ) {
    State.arcstrength = std::max( State.arcstrength, std::clamp( Strength, 0.f, 1.f ) );
    State.arctime = std::max( State.arctime, Duration );
    State.sparktime = std::max( State.sparktime, Duration );
}

// iskry z punktu styku: lecą z prędkością pojazdu i rozpryskują się (więcej w tył i w bok, trochę w górę)
void emit( pantograph_state const &State, float const Strength, double const Dt, float const Rate = -1.f ) {
    auto const &set { parameters() };
    auto const scale { std::max( 0.f, Global.CatenaryArcingScale ) };
    auto const rate { ( Rate >= 0.f ? Rate : set.sparks_rate[ 0 ] + set.sparks_rate[ 1 ] * Strength ) * scale };   // iskier na sekundę
    auto const limit { static_cast<std::size_t>( std::max( 10.f, set.sparks_max ) ) };
    auto count { static_cast<int>( rate * static_cast<float>( Dt ) ) };
    if( uniform( 0.f, 1.f ) < rate * static_cast<float>( Dt ) - static_cast<float>( count ) ) { ++count; }
    for( int index = 0; index < count; ++index ) {
        if( Sparks.size() >= limit ) { Sparks.erase( Sparks.begin(), Sparks.begin() + static_cast<std::ptrdiff_t>( limit / 10 ) ); }
        spark item;
        item.position = item.previous = State.point + glm::dvec3 { uniform( -0.05f, 0.05f ), uniform( -0.02f, 0.02f ), uniform( -0.05f, 0.05f ) };
        auto const spray { set.sparks_spray[ 0 ] + set.sparks_spray[ 1 ] * Strength };
        item.velocity = glm::vec3 { State.velocity } * uniform( set.sparks_carry[ 0 ], std::max( set.sparks_carry[ 0 ], set.sparks_carry[ 1 ] ) )
            + glm::vec3 { uniform( -spray, spray ), uniform( -0.3f * spray, 0.8f * spray ), uniform( -spray, spray ) };
        item.life = uniform( set.sparks_life[ 0 ], std::max( set.sparks_life[ 0 ], set.sparks_life[ 1 ] + set.sparks_life[ 2 ] * Strength ) );
        item.heat = uniform( 0.7f, 1.f );
        Sparks.push_back( item );
    }
}

} // namespace

settings const &parameters() {
    static settings const loaded { load_settings() };
    return loaded;
}

float icing() {
    if( false == Global.CatenaryArcing ) { return 0.f; }
    auto const &set { parameters() };
    auto const temperature { Global.AirTemperature };
    // szadź i gołoledź: od icing_start w dół, najwięcej przy icing_peak, przy silnym mrozie powietrze suche i mniej osadu
    float frost { 0.f };
    if( temperature < set.icing_start ) {
        frost = ( temperature > set.icing_peak ?
            ( set.icing_start - temperature ) / std::max( 0.1f, set.icing_start - set.icing_peak ) :
            std::max( set.icing_cold_floor, 1.f - ( set.icing_peak - temperature ) / std::max( 0.1f, set.icing_cold_span ) ) );
    }
    if( frost <= 0.f ) { return 0.f; }
    // wilgoć: mgła, opady (overcast ponad 1); sucho - sam mróz daje niewiele
    auto const fog { std::clamp( ( set.icing_fog[ 0 ] - Global.fFogEnd ) / std::max( 1.f, set.icing_fog[ 0 ] - set.icing_fog[ 1 ] ), 0.f, 1.f ) };
    auto const precipitation { std::clamp( Global.Overcast - 1.f, 0.f, 1.f ) };
    auto const moisture { std::clamp( set.icing_dry + ( 1.f - set.icing_dry ) * std::max( fog, precipitation ), 0.f, 1.f ) };
    return std::clamp( frost * moisture, 0.f, 1.f );
}

double touch_strength( bool const Filter, double const Current ) {
    auto const &set { parameters() };
    return set.touch_base + ( Filter ? set.touch_filter : 0.f ) + set.touch_ice * icing() + std::min( 0.4, std::abs( Current ) / std::max( 1.f, set.touch_current ) );
}

float contact( void const *Key, glm::dvec3 const &Point, glm::dvec3 const &Velocity, double const Current, TTraction const *Span, double const Dt ) {
    if( false == Global.CatenaryArcing || Key == nullptr || Dt <= 0.0 ) { return 0.f; }
    auto &state { Pantographs[ Key ] };
    // prędkość z drogi punktu styku (podana służy, gdy pantograf jest nowy albo przeskoczył - teleport, nowy pojazd)
    auto velocity { Velocity };
    if( state.touched || state.span != nullptr ) {
        auto const moved { ( Point - state.point ) / Dt };
        if( glm::length( moved ) < 150.0 ) { velocity = glm::mix( state.velocity, moved, 0.3 ); }
    }
    state.point = Point;
    state.velocity = velocity;
    state.touched = true;
    auto const current { std::abs( Current ) };
    auto const speed { static_cast<float>( glm::length( velocity ) ) };
    auto const &set { parameters() };
    float started { 0.f };
    // przejazd na przęsło innej sekcji pod prądem: ślizg zwiera albo przerywa sekcje (izolator sekcyjny, izolowane przęsło naprężenia)
    auto const power { power_of( Span ) };
    if( Span != state.span ) {
        if( state.span != nullptr && Span != nullptr && false == state.power.empty() && false == power.empty() && power != state.power && current > set.section_current ) {
            auto const strength { std::clamp( static_cast<float>( current / std::max( 1.f, set.section_reference ) ), 0.2f, 1.f ) };
            ignite( state, strength, uniform( set.section_duration[ 0 ], std::max( set.section_duration[ 0 ], set.section_duration[ 1 ] ) ) );
            started = std::max( started, strength );
        }
        state.span = Span;
        state.power = power;
    }
    // oblodzony przewód: ślizg co chwila odskakuje od lodu. Częstość rośnie z prędkością i prądem (przy samym prądzie
    // pomocniczym błyski są słabe i rzadkie); na postoju ślizg nie trze, więc nie iskrzy
    auto const ice { span_icing( Span ) };
    // mocne oblodzenie: ciągły słaby łuk (rośnie od sustain_from do pełnego oblodzenia, z prądem i prędkością)
    state.sustain = 0.f;
    if( ice > set.sustain_from && speed > set.ice_min_speed && current > 5.0 ) {
        auto const level { ( ice - set.sustain_from ) / std::max( 0.01f, 1.f - set.sustain_from ) };
        auto const load { std::clamp( static_cast<float>( current / std::max( 1.f, set.ice_current ) ), 0.1f, 1.f ) };
        auto const pace { std::clamp( speed / std::max( 1.f, set.ice_speed ), 0.2f, 1.f ) };
        state.sustain = std::clamp( level * std::sqrt( load * pace ), 0.f, 1.f );
    }
    if( ice > 0.f && speed > set.ice_min_speed ) {
        auto const load { std::clamp( static_cast<float>( current / std::max( 1.f, set.ice_current ) ), 0.05f, 1.5f ) };
        auto const pace { std::clamp( speed / std::max( 1.f, set.ice_speed ), 0.f, 1.6f ) };
        auto const rate { set.ice_rate * ice * pace * std::sqrt( load ) * std::max( 0.f, Global.CatenaryArcingScale ) };   // łuków na sekundę
        if( uniform( 0.f, 1.f ) < 1.f - std::exp( -rate * static_cast<float>( Dt ) ) ) {
            auto const strength { std::clamp( uniform( 0.2f, 1.f ) * std::sqrt( load ) * ( 0.5f + 0.5f * ice ), 0.05f, 1.f ) };
            ignite( state, strength, uniform( set.ice_duration[ 0 ], std::max( set.ice_duration[ 0 ], set.ice_duration[ 1 ] ) ) * ( 0.5f + strength ) );
            started = std::max( started, strength );
        }
    }
    state.current = current;
    return started;
}

float touch( void const *Key, glm::dvec3 const &Point, double const Strength ) {
    if( false == Global.CatenaryArcing || Key == nullptr || Strength <= 0.0 ) { return 0.f; }
    auto &state { Pantographs[ Key ] };
    state.point = Point;
    state.touched = true;
    auto const strength { std::clamp( static_cast<float>( Strength ), 0.f, 1.f ) };
    auto const &set { parameters() };
    ignite( state, strength, uniform( set.touch_duration[ 0 ], std::max( set.touch_duration[ 0 ], set.touch_duration[ 1 ] ) ) + 0.08f * strength );
    return strength;
}

float release( void const *Key, glm::dvec3 const &Point, glm::dvec3 const &Velocity, double const Current ) {
    if( false == Global.CatenaryArcing || Key == nullptr ) { return 0.f; }
    auto &state { Pantographs[ Key ] };
    state.point = Point;
    if( glm::length( Velocity ) > 0.0 ) { state.velocity = Velocity; }
    state.span = nullptr;
    state.power.clear();
    auto const current { std::max( std::abs( Current ), state.current ) };
    state.current = 0.0;
    auto const &set { parameters() };
    if( current < set.release_current ) { return 0.f; }
    // oderwanie pod obciążeniem: łuk ciągnie się za ślizgiem, aż się przerwie
    auto const strength { std::clamp( static_cast<float>( current / std::max( 1.f, set.release_reference ) ), 0.25f, 1.f ) };
    ignite( state, strength, set.release_duration[ 0 ] + set.release_duration[ 1 ] * strength );
    return strength;
}

void update( double const Dt ) {
    Arcs.clear();
    if( Dt > 0.0 ) {
        auto const dt { static_cast<float>( Dt ) };
        auto const &set { parameters() };
        for( auto entry { Pantographs.begin() }; entry != Pantographs.end(); ) {
            auto &state { entry->second };
            // ciągły łuk przy mocnym oblodzeniu: słaby, migocze mocniej niż błyski, sypie trochę iskier; trwa, dopóki ślizg jest w styku
            auto const sustained { state.touched ? state.sustain * std::clamp( set.sustain_level, 0.f, 1.f ) : 0.f };
            if( false == state.touched ) { state.sustain = 0.f; }
            if( state.arctime > 0.f || sustained > 0.f ) {
                // łuk migocze: jasność skacze z kroku na krok
                auto const flicker { uniform( std::clamp( set.arc_flicker, 0.f, 1.f ), 1.f ) };
                arc item;
                item.position = state.point;
                item.intensity = std::max( state.arctime > 0.f ? state.arcstrength * flicker : 0.f, sustained * uniform( 0.2f, 1.f ) );
                // łuk niebieski do niebiesko-białego (Jan 2026-10-30: bardziej niebieski), co krok inny odcień z zakresu
                item.color = glm::mix( set.arc_color[ 0 ], set.arc_color[ 1 ], uniform( 0.f, 1.f ) );
                Arcs.push_back( item );
                if( state.arctime > 0.f ) { emit( state, state.arcstrength, Dt ); }
                if( sustained > 0.f ) { emit( state, sustained, Dt, set.sustain_sparks * state.sustain ); }
                state.arctime -= dt;
                state.sparktime -= dt;
                if( state.arctime <= 0.f ) { state.arcstrength = 0.f; }
            }
            // pantograf, którego nikt nie dotknął od dawna (pojazd usunięty): zapomniany, gdy nic już na nim nie trwa
            if( false == state.touched && state.arctime <= 0.f ) { entry = Pantographs.erase( entry ); continue; }
            state.touched = false;
            ++entry;
        }
        for( auto &item : Sparks ) {
            item.previous = item.position;
            item.velocity += ( -set.sparks_drag * item.velocity + glm::vec3 { 0.f, -kGravity, 0.f } ) * dt;
            item.position += glm::dvec3 { item.velocity * dt };
            item.age += dt;
            item.heat = std::max( 0.f, item.heat - dt / std::max( item.life, 0.05f ) );
        }
        Sparks.erase( std::remove_if( Sparks.begin(), Sparks.end(), []( spark const &Item ) { return Item.age >= Item.life; } ), Sparks.end() );
    }
    ++Revision;
}

std::vector<arc> const &arcs() { return Arcs; }
std::vector<spark> const &sparks() { return Sparks; }
std::size_t revision() { return Revision; }

void clear() {
    Pantographs.clear();
    Arcs.clear();
    Sparks.clear();
    ++Revision;
}

} // namespace catenary_arcing
