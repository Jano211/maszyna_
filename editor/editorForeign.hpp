/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "editor/editorHttp.hpp"

// orthophoto and terrain beyond geoportal.gov.pl: the services of other countries (WMS, WCS) and the world-wide tiles (XYZ),
// listed in a text file (sources_file) the editor writes with the defaults when there's none. each source is in its own
// coordinate system; what it gives is laid onto the PUWG 1992 grid of the editor here. all of the systems known share the datum
// of PUWG 1992 (ETRS89, GRS80), the world-wide ones are in WGS84, which is within a metre of it.
//
// the sources only fill what geoportal.gov.pl leaves empty (outside Poland, or where it has nothing), one after the other by
// their priority. at the edge of what was there the imagery is faded into theirs and the heights are pulled onto the ones
// there, so there's no step at the border
namespace editor_foreign
{

// a rectangle of PUWG 1992, metres
struct area
{
	double west{0.0}, south{0.0}, east{0.0}, north{0.0};
};

enum class kind
{
	ortho,
	terrain
};

// whether any source of the kind reaches into the rectangle
bool reaches(kind const Kind, area const &Area);

// the imagery of the rectangle, Pixels x Pixels of RGBA with the rows from the north. Rgba holds what there is already, the
// pixels with alpha under 255 are filled from the sources, an empty Rgba is all filled from them. Cache: the folder the
// downloads are kept in. false if a source couldn't be reached (to be tried again later); what was there stays then
bool ortho(editor_http::client &Client, area const &Area, int const Pixels, std::vector<std::uint8_t> &Rgba, std::filesystem::path const &Cache, std::string &Error,
           editor_http::stop_check const &Stop);

// the solid black patches of imagery made transparent: some services fill a place they have no imagery for with black (as
// geoportal.gov.pl does along parts of the Slovak border), which would cover the imagery of the next source. a patch is black
// where at least Block x Block pixels are all nearly black; its edge, softened by the jpeg, goes with it. a single dark pixel
// or a shadow stays. returns the pixels made transparent
std::size_t clear_black(std::vector<std::uint8_t> &Rgba, int const Width, int const Height, int const Block = 7);
// the share of the pixels nearly black, a quick test before clear_black
double black_share(std::vector<std::uint8_t> const &Rgba);

// the heights of a grid of Width x Height pixels of Pixel metres, the outer corner of its north-west pixel at West, North,
// rows from the north. the values at or below NoData are filled from the sources, the others stay. false if a source
// couldn't be reached
bool terrain(editor_http::client &Client, double const West, double const North, double const Pixel, int const Width, int const Height, std::vector<float> &Heights,
             float const NoData, std::string &Error, editor_http::stop_check const &Stop);

// the file with the sources, made with the defaults if it isn't there
std::string sources_file();
// reads the file again
void reload();
// changes each time the file is read, so what was worked out from the sources can be worked out anew
int generation();
// what reading the file said: the sources found, or what's wrong with its lines
std::string status();
// whom the sources of the kind come from, for the credits under the windows; empty when there's none
std::string credits(kind const Kind);
// the downloads of the terrain sources (the imagery ones are kept with the orthophoto cache)
std::filesystem::path terrain_cache();

// the systems of coordinates known, by their EPSG codes; for the tests
bool known_system(int const Epsg);
// a point of a system to PUWG 1992 and back; false for a system unknown
bool to_puwg(int const Epsg, double const X, double const Y, double &East, double &North);
bool from_puwg(int const Epsg, double const East, double const North, double &X, double &Y);

} // namespace editor_foreign
