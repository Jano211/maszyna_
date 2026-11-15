/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/
#include "stdafx.h"
#include "world/catenary_dust.h"
#include "gl/ubo.h"
#include "utilities/Logs.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <locale>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace catenary_dust {

namespace {

// kolor z pliku: #RRGGBB albo trzy liczby 0-1, oba sRGB; zamieniony na liniowy
bool read_color( std::istringstream &Line, glm::vec3 &Color ) {
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
        first.imbue( std::locale::classic() );
        if( false == static_cast<bool>( first >> color.r ) || false == static_cast<bool>( Line >> color.g >> color.b ) ) { return false; }
    }
    Color = glm::pow( glm::clamp( color, glm::vec3 { 0.f }, glm::vec3 { 1.f } ), glm::vec3 { 2.2f } );
    return true;
}

settings load_settings() {
    settings result;
    std::string const filename { "data/traction_dust.txt" };
    std::ifstream file { filename };
    if( false == file.is_open() ) { return result; }
    std::map<std::string, float *> numbers {
        { "strength", &result.strength }, { "side", &result.side }, { "bottom", &result.bottom }, { "contrast", &result.contrast },
        { "gloss", &result.gloss }, { "metal", &result.metal }, { "reflection", &result.reflection }, { "no_rail", &result.no_rail } };
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
            float value;
            good = static_cast<bool>( line >> value );
            if( good ) { *found->second = value; }
        }
        else if( key == "color" ) {
            good = read_color( line, result.color );
        }
        else if( key == "profile" ) {
            // wysokość ilość, wysokość ilość ... - od 2 do kProfilePoints par, wysokości rosnąco
            std::vector<glm::vec2> points;
            glm::vec2 point;
            while( line >> point.x ) {
                if( false == static_cast<bool>( line >> point.y ) ) { good = false; break; }
                points.push_back( point );
            }
            good = good && points.size() >= 2 && points.size() <= static_cast<std::size_t>( kProfilePoints )
                && std::is_sorted( points.begin(), points.end(), []( glm::vec2 const &Left, glm::vec2 const &Right ) { return Left.x < Right.x; } );
            if( good ) {
                // brakujące punkty jak ostatni: profil w shaderze ma zawsze kProfilePoints punktów
                for( int index = 0; index < kProfilePoints; ++index ) {
                    result.profile[ index ] = points[ std::min<std::size_t>( index, points.size() - 1 ) ];
                }
            }
        }
        else { good = false; }
        if( false == good ) {
            ErrorLog( "Bad file \"" + filename + "\" (line " + std::to_string( linenumber ) + "): unknown key or wrong values \"" + text + "\"" );
        }
    }
    return result;
}

} // namespace

settings const &parameters() {
    static settings const loaded { load_settings() };
    return loaded;
}

void setup_scene( gl::scene_ubs &Scene, glm::dvec3 const &Camera ) {
    auto const &set { parameters() };
    auto const wrap = []( double const Value ) {
        auto result { std::fmod( Value, kPeriod ) };
        if( result < 0.0 ) { result += kPeriod; }
        return static_cast<float>( result );
    };
    Scene.dust_origin = glm::vec4 { wrap( Camera.x ), wrap( Camera.y ), wrap( Camera.z ), ( set.strength > 0.f ? 1.f : 0.f ) };
    Scene.dust_color = glm::vec4 { set.color, set.strength };
    Scene.dust_params = glm::vec4 { set.side, set.bottom, set.contrast, set.no_rail };
    Scene.dust_params2 = glm::vec4 { set.gloss, set.metal, set.reflection, 0.f };
    for( int index = 0; index < kProfilePoints / 2; ++index ) {
        Scene.dust_profile[ index ] = glm::vec4 { set.profile[ 2 * index ], set.profile[ 2 * index + 1 ] };
    }
}

} // namespace catenary_dust
