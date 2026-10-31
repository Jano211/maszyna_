/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "world/catenary_overlap.h"
#include <algorithm>
#include <fstream>
#include <locale>
#include <sstream>

namespace catenary {

namespace {

bool
to_number( std::string const &Text, double &Value ) {
    std::istringstream stream { Text };
    stream.imbue( std::locale::classic() );
    stream >> Value;
    return false == stream.fail() && stream.eof();
}

bool
to_role( std::string const &Text, overlap_role &Role ) {
    if( Text == "-" ) { Role = overlap_role::none; return true; }
    if( Text == "anchor" ) { Role = overlap_role::anchor; return true; }
    if( Text == "work" ) { Role = overlap_role::work; return true; }
    if( Text == "lift" ) { Role = overlap_role::lift; return true; }
    return false;
}

} // namespace

std::size_t
overlap_card::first_crossing() const {
    for( std::size_t index = 0; index < poles.size(); ++index ) {
        for( auto const role : poles[ index ].net ) {
            if( role == overlap_role::work || role == overlap_role::lift ) { return index; }
        }
    }
    return poles.size();
}

double
overlap_card::offset( std::size_t const Index ) const {
    double at { 0.0 };
    std::vector<double> along( poles.size(), 0.0 );
    for( std::size_t index = 0; index < poles.size(); ++index ) {
        if( index > 0 ) { at += poles[ index ].distance; }
        along[ index ] = at;
    }
    auto const first { first_crossing() };
    if( Index >= poles.size() || first >= poles.size() ) { return 0.0; }
    return along[ Index ] - along[ first ];
}

double
overlap_card::length() const {
    double total { 0.0 };
    for( std::size_t index = 1; index < poles.size(); ++index ) { total += poles[ index ].distance; }
    return total;
}

bool
overlap_card::fits( int const Network ) const {
    return std::find( networks.begin(), networks.end(), Network ) != networks.end();
}

void
parse_overlaps( std::vector<std::string> const &Lines, std::string const &Source, std::vector<overlap_card> &Output, std::vector<std::string> &Errors ) {

    overlap_card card;
    bool open { false }, good { true };
    std::size_t number { 0 };
    auto const fail = [ & ]( std::string const &Reason ) {
        Errors.push_back( "Bad overlap file \"" + Source + "\" (line " + std::to_string( number ) + "): " + Reason );
        good = false;
    };
    for( auto line : Lines ) {
        ++number;
        if( auto const comment { line.find( "//" ) }; comment != std::string::npos ) { line.erase( comment ); }
        std::string title;
        if( auto const colon { line.find( ':' ) }; colon != std::string::npos ) {
            title = line.substr( colon + 1 );
            line.erase( colon );
            auto const first { title.find_first_not_of( " \t\r" ) }, last { title.find_last_not_of( " \t\r" ) };
            title = first == std::string::npos ? std::string() : title.substr( first, last - first + 1 );
        }
        std::istringstream stream { line };
        std::vector<std::string> words;
        for( std::string word; stream >> word; ) { words.push_back( word ); }
        if( words.empty() ) { continue; }
        auto const &key { words[ 0 ] };
        if( key == "overlap" ) {
            if( open ) { fail( "\"overlap\" without \"endoverlap\" before it" ); }
            card = overlap_card();
            open = true;
            good = true;
            if( words.size() < 2 ) { fail( "\"overlap\" needs a card number" ); continue; }
            card.name = words[ 1 ];
            card.title = title;
            for( std::size_t index = 2; index < words.size(); ++index ) {
                if( words[ index ] == "networks" ) { continue; }
                if( words[ index ] == "curve" ) { card.curve = true; continue; }
                double code;
                if( to_number( words[ index ], code ) ) { card.networks.push_back( static_cast<int>( code ) ); }
                else { fail( "unexpected \"" + words[ index ] + "\" in the header of " + card.name ); }
            }
            continue;
        }
        if( false == open ) { fail( "\"" + key + "\" outside of an overlap" ); continue; }
        if( key == "endoverlap" ) {
            open = false;
            if( card.poles.size() < 2 ) { fail( card.name + " has fewer than two poles" ); }
            if( card.first_crossing() >= card.poles.size() ) { fail( card.name + " has no crossing pole" ); }
            if( good ) { Output.push_back( card ); }
            continue;
        }
        if( key == "pole" ) {
            overlap_pole pole;
            if( words.size() < 5 || false == to_number( words[ 2 ], pole.distance ) || false == to_role( words[ 3 ], pole.net[ 0 ] ) || false == to_role( words[ 4 ], pole.net[ 1 ] ) ) {
                fail( "\"pole\" needs a name, a distance and the roles of both networks (anchor, work, lift, -)" );
                continue;
            }
            pole.name = words[ 1 ];
            pole.same = words.size() > 5 && words[ 5 ] == "same";
            card.poles.push_back( pole );
            continue;
        }
        // pary klucz - liczba (kilka w jednej linii); anchor ma dwie liczby
        for( std::size_t index = 0; index < words.size(); ) {
            auto const &name { words[ index ] };
            double value { 0.0 };
            auto const number_at = [ & ]( std::size_t const At, double &Value ) { return At < words.size() && to_number( words[ At ], Value ); };
            if( name == "anchor" ) {
                if( false == number_at( index + 1, card.anchor_rope ) || false == number_at( index + 2, card.anchor_wire ) ) { fail( "\"anchor\" needs two heights" ); break; }
                index += 3;
                continue;
            }
            if( false == number_at( index + 1, value ) ) { fail( "\"" + name + "\" needs a number" ); break; }
            if( name == "z" ) { card.stagger = value; }
            else if( name == "lift" ) { card.lift = value; }
            else if( name == "common" ) { card.common = value; }
            else if( name == "hk" ) { card.height = value; }
            else { fail( "unexpected \"" + name + "\"" ); break; }
            index += 2;
        }
    }
    if( open ) { fail( "the file ends inside overlap " + card.name ); }
}

bool
load_overlaps( std::string const &Filename, std::vector<overlap_card> &Output, std::vector<std::string> &Errors ) {
    std::ifstream file { Filename };
    if( false == file.is_open() ) { return false; }
    std::vector<std::string> lines;
    for( std::string line; std::getline( file, line ); ) {
        if( false == line.empty() && line.back() == '\r' ) { line.pop_back(); }
        lines.push_back( line );
    }
    parse_overlaps( lines, Filename, Output, Errors );
    return true;
}

} // namespace catenary
