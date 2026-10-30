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

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <unordered_map>

namespace catenary_arcing {

namespace {

// najwięcej iskier naraz (stare wypadają)
std::size_t constexpr kMaxSparks { 1500 };
// opór powietrza kropli (1/s) i grawitacja
float constexpr kDrag { 3.5f };
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

// zasilacz przęsła (granica sekcji: inny zasilacz)
std::string power_of( TTraction const *Span ) {
    return ( Span != nullptr ? Span->asPowerSupplyName : std::string() );
}

// oblodzenie przęsła: wspólne z pogody, każde przęsło trochę inaczej (osłonięte, nad wodą, w przekopie) - stałe dla przęsła
float span_icing( TTraction const *Span ) {
    auto const base { icing() };
    if( base <= 0.f || Span == nullptr ) { return base; }
    auto const key { static_cast<std::uint32_t>( reinterpret_cast<std::uintptr_t>( Span ) >> 4 ) * 2654435761u };
    auto const spread { 0.4f + 1.2f * static_cast<float>( key >> 8 ) / static_cast<float>( 0xffffffu ) };
    return std::clamp( base * spread, 0.f, 1.f );
}

// łuk na pantografie: siła 0-1, czas trwania
void ignite( pantograph_state &State, float const Strength, float const Duration ) {
    State.arcstrength = std::max( State.arcstrength, std::clamp( Strength, 0.f, 1.f ) );
    State.arctime = std::max( State.arctime, Duration );
    State.sparktime = std::max( State.sparktime, Duration );
}

// iskry z punktu styku: lecą z prędkością pojazdu i rozpryskują się (więcej w tył i w bok, trochę w górę)
void emit( pantograph_state const &State, float const Strength, double const Dt ) {
    auto const scale { std::max( 0.f, Global.CatenaryArcingScale ) };
    auto const rate { ( 40.f + 400.f * Strength ) * scale };   // iskier na sekundę
    auto count { static_cast<int>( rate * static_cast<float>( Dt ) ) };
    if( uniform( 0.f, 1.f ) < rate * static_cast<float>( Dt ) - static_cast<float>( count ) ) { ++count; }
    for( int index = 0; index < count; ++index ) {
        if( Sparks.size() >= kMaxSparks ) { Sparks.erase( Sparks.begin(), Sparks.begin() + static_cast<std::ptrdiff_t>( kMaxSparks / 10 ) ); }
        spark item;
        item.position = item.previous = State.point + glm::dvec3 { uniform( -0.05f, 0.05f ), uniform( -0.02f, 0.02f ), uniform( -0.05f, 0.05f ) };
        auto const spray { 2.f + 8.f * Strength };
        item.velocity = glm::vec3 { State.velocity } * uniform( 0.6f, 1.0f )
            + glm::vec3 { uniform( -spray, spray ), uniform( -0.3f * spray, 0.8f * spray ), uniform( -spray, spray ) };
        item.life = uniform( 0.2f, 0.5f + 0.7f * Strength );
        item.heat = uniform( 0.7f, 1.f );
        Sparks.push_back( item );
    }
}

} // namespace

float icing() {
    if( false == Global.CatenaryArcing ) { return 0.f; }
    auto const temperature { Global.AirTemperature };
    // szadź i gołoledź: od +1 °C w dół, najwięcej ok. -3 °C, przy silnym mrozie powietrze suche i mniej osadu
    float frost { 0.f };
    if( temperature < 1.f ) {
        frost = ( temperature > -3.f ?
            ( 1.f - temperature ) / 4.f :
            std::max( 0.25f, 1.f - ( -3.f - temperature ) / 20.f ) );
    }
    if( frost <= 0.f ) { return 0.f; }
    // wilgoć: mgła (widzialność poniżej ok. 2 km), opady (overcast ponad 1); sucho - sam mróz daje niewiele
    auto const fog { std::clamp( ( 2000.f - Global.fFogEnd ) / 1700.f, 0.f, 1.f ) };
    auto const precipitation { std::clamp( Global.Overcast - 1.f, 0.f, 1.f ) };
    auto const moisture { std::clamp( 0.25f + 0.75f * std::max( fog, precipitation ), 0.f, 1.f ) };
    return std::clamp( frost * moisture, 0.f, 1.f );
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
    float started { 0.f };
    // przejazd na przęsło innej sekcji pod prądem: ślizg zwiera albo przerywa sekcje (izolator sekcyjny, izolowane przęsło naprężenia)
    auto const power { power_of( Span ) };
    if( Span != state.span ) {
        if( state.span != nullptr && Span != nullptr && false == state.power.empty() && false == power.empty() && power != state.power && current > 20.0 ) {
            auto const strength { std::clamp( static_cast<float>( current / 1500.0 ), 0.2f, 1.f ) };
            ignite( state, strength, uniform( 0.05f, 0.15f ) );
            started = std::max( started, strength );
        }
        state.span = Span;
        state.power = power;
    }
    // oblodzony przewód: ślizg co chwila odskakuje od lodu. Częstość rośnie z prędkością i prądem (przy samym prądzie
    // pomocniczym błyski są słabe i rzadkie); na postoju ślizg nie trze, więc nie iskrzy
    auto const ice { span_icing( Span ) };
    if( ice > 0.f && speed > 1.f ) {
        auto const load { std::clamp( static_cast<float>( current / 1000.0 ), 0.05f, 1.5f ) };
        auto const pace { std::clamp( speed / 25.f, 0.f, 1.6f ) };
        auto const rate { 10.f * ice * pace * std::sqrt( load ) * std::max( 0.f, Global.CatenaryArcingScale ) };   // łuków na sekundę
        if( uniform( 0.f, 1.f ) < 1.f - std::exp( -rate * static_cast<float>( Dt ) ) ) {
            auto const strength { std::clamp( uniform( 0.2f, 1.f ) * std::sqrt( load ) * ( 0.5f + 0.5f * ice ), 0.05f, 1.f ) };
            ignite( state, strength, uniform( 0.02f, 0.12f ) * ( 0.5f + strength ) );
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
    ignite( state, strength, uniform( 0.03f, 0.06f ) + 0.08f * strength );
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
    if( current < 15.0 ) { return 0.f; }
    // oderwanie pod obciążeniem: łuk ciągnie się za ślizgiem, aż się przerwie
    auto const strength { std::clamp( static_cast<float>( current / 800.0 ), 0.25f, 1.f ) };
    ignite( state, strength, 0.12f + 0.35f * strength );
    return strength;
}

void update( double const Dt ) {
    Arcs.clear();
    if( Dt > 0.0 ) {
        auto const dt { static_cast<float>( Dt ) };
        for( auto entry { Pantographs.begin() }; entry != Pantographs.end(); ) {
            auto &state { entry->second };
            if( state.arctime > 0.f ) {
                // łuk migocze: jasność skacze z kroku na krok
                auto const flicker { uniform( 0.35f, 1.f ) };
                arc item;
                item.position = state.point;
                item.intensity = state.arcstrength * flicker;
                // łuk niebieski do niebiesko-białego (Jan 2026-10-30: bardziej niebieski)
                item.color = glm::mix( glm::vec3 { 0.25f, 0.4f, 1.f }, glm::vec3 { 0.5f, 0.62f, 1.f }, uniform( 0.f, 1.f ) );
                Arcs.push_back( item );
                emit( state, state.arcstrength, Dt );
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
            item.velocity += ( -kDrag * item.velocity + glm::vec3 { 0.f, -kGravity, 0.f } ) * dt;
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
