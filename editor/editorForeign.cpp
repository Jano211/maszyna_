/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#ifndef EDITOR_FOREIGN_STANDALONE
#include "stdafx.h"
#include "utilities/Logs.h"
#endif
#include "editor/editorForeign.hpp"
#include "stb/stb_image.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>

#ifdef EDITOR_FOREIGN_STANDALONE
inline void WriteLog(std::string const &) {}
#endif

namespace editor_foreign
{

namespace
{

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------------------------
// the systems of coordinates

double constexpr pi{3.14159265358979323846};
double constexpr degree{pi / 180.0};
double constexpr grs80_a{6378137.0};
double constexpr grs80_f{1.0 / 298.257222101};
double constexpr mercator_radius{6378137.0}; // of the spherical Web Mercator

struct system_of_coordinates
{
	enum class type
	{
		transverse,
		web_mercator,
		geographic // X: longitude, Y: latitude, degrees
	} kind{type::transverse};
	double meridian{0.0}; // radians
	double scale{1.0};
	double false_east{0.0};
	double false_north{0.0};
};

// transverse Mercator on GRS80, Krüger series to the third order of n: millimetres over a zone, centimetres 15 degrees off it
struct kruger
{
	double n, big_a;
	double alpha[3], beta[3], delta[3];
	kruger()
	{
		n = grs80_f / (2.0 - grs80_f);
		auto const n2{n * n}, n3{n2 * n};
		big_a = grs80_a / (1.0 + n) * (1.0 + n2 / 4.0 + n2 * n2 / 64.0);
		alpha[0] = n / 2.0 - 2.0 * n2 / 3.0 + 5.0 * n3 / 16.0;
		alpha[1] = 13.0 * n2 / 48.0 - 3.0 * n3 / 5.0;
		alpha[2] = 61.0 * n3 / 240.0;
		beta[0] = n / 2.0 - 2.0 * n2 / 3.0 + 37.0 * n3 / 96.0;
		beta[1] = n2 / 48.0 + n3 / 15.0;
		beta[2] = 17.0 * n3 / 480.0;
		delta[0] = 2.0 * n - 2.0 * n2 / 3.0 - 2.0 * n3;
		delta[1] = 7.0 * n2 / 3.0 - 8.0 * n3 / 5.0;
		delta[2] = 56.0 * n3 / 15.0;
	}
};

kruger const &series()
{
	static kruger const value;
	return value;
}

system_of_coordinates transverse(double const Meridian, double const Scale, double const FalseEast, double const FalseNorth)
{
	system_of_coordinates result;
	result.meridian = Meridian * degree;
	result.scale = Scale;
	result.false_east = FalseEast;
	result.false_north = FalseNorth;
	return result;
}

bool system_by_code(int const Epsg, system_of_coordinates &Out)
{
	if (Epsg == 2180)
	{
		Out = transverse(19.0, 0.9993, 500000.0, -5300000.0); // PUWG 1992
		return true;
	}
	if (Epsg >= 2176 && Epsg <= 2179)
	{
		auto const zone{Epsg - 2171}; // PUWG 2000, zones 5-8
		Out = transverse(3.0 * zone, 0.999923, zone * 1000000.0 + 500000.0, 0.0);
		return true;
	}
	auto const utm = [&](int const Zone) {
		Out = transverse(6.0 * Zone - 183.0, 0.9996, 500000.0, 0.0);
		return true;
	};
	if (Epsg >= 25828 && Epsg <= 25838)
		return utm(Epsg - 25800); // ETRS89 / UTM
	if (Epsg >= 3040 && Epsg <= 3051)
		return utm(Epsg - 3012); // ETRS-TM26..39 (the axes named north first, the numbers the same)
	if (Epsg >= 32628 && Epsg <= 32638)
		return utm(Epsg - 32600); // WGS84 / UTM
	if (Epsg == 3067)
		return utm(35); // ETRS-TM35FIN
	if (Epsg == 3346)
	{
		Out = transverse(24.0, 0.9998, 500000.0, 0.0); // LKS-94
		return true;
	}
	if (Epsg == 3857 || Epsg == 900913)
	{
		Out = {};
		Out.kind = system_of_coordinates::type::web_mercator;
		return true;
	}
	if (Epsg == 4326 || Epsg == 4258)
	{
		Out = {};
		Out.kind = system_of_coordinates::type::geographic;
		return true;
	}
	return false;
}

// latitude, longitude in radians
void to_grid(system_of_coordinates const &System, double const Latitude, double const Longitude, double &X, double &Y)
{
	switch (System.kind)
	{
	case system_of_coordinates::type::geographic:
		X = Longitude / degree;
		Y = Latitude / degree;
		return;
	case system_of_coordinates::type::web_mercator:
		X = mercator_radius * Longitude;
		Y = mercator_radius * std::log(std::tan(pi / 4.0 + Latitude / 2.0));
		return;
	default:
		break;
	}
	auto const &k{series()};
	auto const root{2.0 * std::sqrt(k.n) / (1.0 + k.n)};
	auto const t{std::sinh(std::atanh(std::sin(Latitude)) - root * std::atanh(root * std::sin(Latitude)))};
	auto const lambda{Longitude - System.meridian};
	auto const xi{std::atan2(t, std::cos(lambda))};
	auto const eta{std::atanh(std::sin(lambda) / std::sqrt(1.0 + t * t))};
	double x{xi}, y{eta};
	for (int j = 1; j <= 3; ++j)
	{
		x += k.alpha[j - 1] * std::sin(2.0 * j * xi) * std::cosh(2.0 * j * eta);
		y += k.alpha[j - 1] * std::cos(2.0 * j * xi) * std::sinh(2.0 * j * eta);
	}
	Y = System.false_north + System.scale * k.big_a * x;
	X = System.false_east + System.scale * k.big_a * y;
}

void to_geographic(system_of_coordinates const &System, double const X, double const Y, double &Latitude, double &Longitude)
{
	switch (System.kind)
	{
	case system_of_coordinates::type::geographic:
		Longitude = X * degree;
		Latitude = Y * degree;
		return;
	case system_of_coordinates::type::web_mercator:
		Longitude = X / mercator_radius;
		Latitude = 2.0 * std::atan(std::exp(Y / mercator_radius)) - pi / 2.0;
		return;
	default:
		break;
	}
	auto const &k{series()};
	auto const xi{(Y - System.false_north) / (System.scale * k.big_a)};
	auto const eta{(X - System.false_east) / (System.scale * k.big_a)};
	double x{xi}, y{eta};
	for (int j = 1; j <= 3; ++j)
	{
		x -= k.beta[j - 1] * std::sin(2.0 * j * xi) * std::cosh(2.0 * j * eta);
		y -= k.beta[j - 1] * std::cos(2.0 * j * xi) * std::sinh(2.0 * j * eta);
	}
	auto const chi{std::asin(std::sin(x) / std::cosh(y))};
	Latitude = chi;
	for (int j = 1; j <= 3; ++j)
		Latitude += k.delta[j - 1] * std::sin(2.0 * j * chi);
	Longitude = System.meridian + std::atan2(std::sinh(y), std::cos(x));
}

system_of_coordinates const &puwg()
{
	static system_of_coordinates const value{transverse(19.0, 0.9993, 500000.0, -5300000.0)};
	return value;
}

void puwg_to(system_of_coordinates const &System, double const East, double const North, double &X, double &Y)
{
	double latitude, longitude;
	to_geographic(puwg(), East, North, latitude, longitude);
	to_grid(System, latitude, longitude, X, Y);
}

// ---------------------------------------------------------------------------------------------
// the sources

enum class service
{
	wms,
	wcs,
	xyz
};

struct source
{
	kind what{kind::ortho};
	std::string name;
	service type{service::wms};
	int epsg{0};
	system_of_coordinates system;
	std::string url;
	double resolution{0.0}; // finest pixel worth asking for, metres of the system (0: as fine as asked)
	int max_pixels{2048};   // width and height a request may ask for
	int priority{50};       // lower first
	bool bounded{false};    // the area below is set
	double west{-180.0}, south{-85.0}, east{180.0}, north{85.0}; // degrees
	int zoom{18};           // xyz: the deepest level (imagery) or the level used (terrain)
	bool terrarium{false};  // xyz terrain: heights in the colours as in the Mapzen tiles
	bool automatic{true};   // terrain: the shift of the heights worked out at the edge of the data before it
	double offset{0.0};     // terrain: the shift of the heights when it isn't, or until it is
	double blend{30.0};     // terrain: metres over which the heights are pulled onto the edge of the data before them
	double feather{10.0};   // imagery: metres over which the edge of the imagery before them fades into theirs
	std::string credit;
	std::string title; // map: the name shown in the list of the layers
	int tile{1};       // map: the tiles of the editor this many times larger
	int magnify{1};    // the map asked for at a pixel this many times larger: its lettering and signs that much larger on the ground
};

char const *const file_name{"editor/foreign_sources.txt"};

// nothing is built in: without the file there are no other sources and no map layers. a sample of the file, with the sources
// tried out, is docs/foreign_sources.txt

struct registry
{
	std::mutex mutex;
	bool loaded{false};
	std::atomic<int> generation{0};
	std::vector<source> sources; // by priority
	std::string status;
	std::map<std::string, std::pair<double, double>> shifts; // terrain: sum and weight of the differences seen at the edges, by name
};

registry &sources_registry()
{
	static registry value;
	return value;
}

bool read_number(std::string const &Text, double &Out)
{
	auto const parsed{std::from_chars(Text.data(), Text.data() + Text.size(), Out)};
	return parsed.ec == std::errc{} && parsed.ptr == Text.data() + Text.size();
}

bool parse_line(std::string const &Line, source &Out, std::string &Problem)
{
	std::istringstream words{Line};
	std::string what, name, type;
	if (!(words >> what >> name >> type))
	{
		Problem = "too few words";
		return false;
	}
	if (what == "ortho")
		Out.what = kind::ortho;
	else if (what == "terrain")
		Out.what = kind::terrain;
	else if (what == "map")
		Out.what = kind::map;
	else
	{
		Problem = "not ortho, terrain or map: " + what;
		return false;
	}
	Out.name = name;
	if (type == "wms")
		Out.type = service::wms;
	else if (type == "wcs")
		Out.type = service::wcs;
	else if (type == "xyz")
		Out.type = service::xyz;
	else
	{
		Problem = "not wms, wcs or xyz: " + type;
		return false;
	}
	if (Out.what == kind::terrain)
		Out.blend = Out.type == service::xyz ? 150.0 : 30.0;
	if (Out.type == service::xyz)
		Out.epsg = 3857;
	std::string word;
	while (words >> word)
	{
		auto const equals{word.find('=')};
		if (equals == std::string::npos)
		{
			Problem = "no = in " + word;
			return false;
		}
		auto const key{word.substr(0, equals)};
		auto const value{word.substr(equals + 1)};
		double number{0.0};
		auto const numeric{read_number(value, number)};
		if (key == "url")
			Out.url = value;
		else if (key == "credit")
		{
			Out.credit = value;
			std::replace(Out.credit.begin(), Out.credit.end(), '_', ' ');
		}
		else if (key == "title")
		{
			Out.title = value;
			std::replace(Out.title.begin(), Out.title.end(), '_', ' ');
		}
		else if (key == "encoding")
			Out.terrarium = value == "terrarium";
		else if (key == "area")
		{
			double values[4];
			std::istringstream parts{value};
			std::string part;
			int count{0};
			while (count < 4 && std::getline(parts, part, ',') && read_number(part, values[count]))
				++count;
			if (count != 4)
			{
				Problem = "area isn't west,south,east,north";
				return false;
			}
			Out.bounded = true;
			Out.west = values[0];
			Out.south = values[1];
			Out.east = values[2];
			Out.north = values[3];
		}
		else if (key == "offset" && value == "auto")
			Out.automatic = true;
		else if (false == numeric)
		{
			Problem = "not a number: " + word;
			return false;
		}
		else if (key == "crs")
			Out.epsg = static_cast<int>(number);
		else if (key == "resolution")
			Out.resolution = number;
		else if (key == "max")
			Out.max_pixels = std::clamp(static_cast<int>(number), 16, 8192);
		else if (key == "priority")
			Out.priority = static_cast<int>(number);
		else if (key == "zoom")
			Out.zoom = std::clamp(static_cast<int>(number), 0, 22);
		else if (key == "offset")
		{
			Out.automatic = false;
			Out.offset = number;
		}
		else if (key == "blend")
			Out.blend = std::max(0.0, number);
		else if (key == "feather")
			Out.feather = std::max(0.0, number);
		else if (key == "tile")
			Out.tile = std::clamp(static_cast<int>(number), 1, 16);
		else if (key == "magnify")
		{
			// a power of two, the levels of the tiles go by twos
			Out.magnify = 1;
			while (Out.magnify * 2 <= std::clamp(static_cast<int>(number), 1, 64))
				Out.magnify *= 2;
		}
		else
		{
			Problem = "unknown key " + key;
			return false;
		}
	}
	if (Out.url.empty())
	{
		Problem = "no url=";
		return false;
	}
	if (false == system_by_code(Out.epsg, Out.system))
	{
		Problem = "crs=" + std::to_string(Out.epsg) + " isn't known";
		return false;
	}
	return true;
}

void load(registry &Registry)
{
	Registry.loaded = true;
	++Registry.generation;
	Registry.sources.clear();
	std::string text;
	if (std::ifstream in{file_name, std::ios::binary}; in.is_open())
		text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	else
	{
		Registry.status = std::string{"no "} + file_name + ": only geoportal.gov.pl";
		return;
	}
	std::ostringstream status;
	std::istringstream lines{text};
	std::string line;
	int number{0};
	while (std::getline(lines, line))
	{
		++number;
		if (false == line.empty() && line.back() == '\r')
			line.pop_back();
		auto const first{line.find_first_not_of(" \t")};
		if (first == std::string::npos || line[first] == '#')
			continue;
		source item;
		std::string problem;
		if (parse_line(line.substr(first), item, problem))
			Registry.sources.push_back(std::move(item));
		else
		{
			status << "line " << number << ": " << problem << "\n";
			WriteLog("Editor: " + std::string{file_name} + ", line " + std::to_string(number) + " skipped: " + problem);
		}
	}
	std::stable_sort(Registry.sources.begin(), Registry.sources.end(), [](source const &Left, source const &Right) { return Left.priority < Right.priority; });
	int counts[3]{0, 0, 0};
	for (auto const &item : Registry.sources)
		++counts[static_cast<int>(item.what)];
	Registry.status = std::to_string(counts[0]) + " imagery, " + std::to_string(counts[1]) + " terrain, " + std::to_string(counts[2]) + " map layers\n" + status.str();
}

// the sources of the kind reaching into the rectangle, by priority
std::vector<source> sources_for(kind const Kind, area const &Area)
{
	// the rectangle in degrees, from its edges
	double west{1e9}, south{1e9}, east{-1e9}, north{-1e9};
	for (int i = 0; i <= 4; ++i)
		for (int j = 0; j <= 4; ++j)
		{
			if (i != 0 && i != 4 && j != 0 && j != 4)
				continue;
			double latitude, longitude;
			to_geographic(puwg(), Area.west + (Area.east - Area.west) * i / 4.0, Area.south + (Area.north - Area.south) * j / 4.0, latitude, longitude);
			west = std::min(west, longitude / degree);
			east = std::max(east, longitude / degree);
			south = std::min(south, latitude / degree);
			north = std::max(north, latitude / degree);
		}
	auto &registry{sources_registry()};
	std::lock_guard<std::mutex> lock{registry.mutex};
	if (false == registry.loaded)
		load(registry);
	std::vector<source> result;
	for (auto const &item : registry.sources)
		if (item.what == Kind && (false == item.bounded || (item.west < east && item.east > west && item.south < north && item.north > south)))
			result.push_back(item);
	return result;
}

// ---------------------------------------------------------------------------------------------
// downloads

std::uint64_t fnv(std::string const &Text)
{
	std::uint64_t hash{1469598103934665603ull};
	for (unsigned char const letter : Text)
	{
		hash ^= letter;
		hash *= 1099511628211ull;
	}
	return hash;
}

std::string hex(std::uint64_t const Value)
{
	char buffer[24];
	std::snprintf(buffer, sizeof(buffer), "%016llx", static_cast<unsigned long long>(Value));
	return buffer;
}

bool read_file(fs::path const &Path, std::vector<std::uint8_t> &Out)
{
	std::ifstream file{Path, std::ios::binary};
	if (false == file.is_open())
		return false;
	Out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	return true;
}

void write_file(fs::path const &Path, std::vector<std::uint8_t> const &Data)
{
	std::error_code fault;
	fs::create_directories(Path.parent_path(), fault);
	std::ostringstream suffix;
	suffix << ".part" << std::this_thread::get_id();
	auto temporary{Path};
	temporary += suffix.str();
	{
		std::ofstream file{temporary, std::ios::binary | std::ios::trunc};
		if (false == file.is_open())
			return;
		file.write(reinterpret_cast<char const *>(Data.data()), static_cast<std::streamsize>(Data.size()));
		if (false == file.good())
		{
			file.close();
			fs::remove(temporary, fault);
			return;
		}
	}
	fs::rename(temporary, Path, fault);
	if (fault)
		fs::remove(temporary, fault);
}

bool is_image(std::vector<std::uint8_t> const &Data)
{
	if (Data.size() >= 3 && Data[0] == 0xFF && Data[1] == 0xD8 && Data[2] == 0xFF)
		return true;
	return Data.size() >= 4 && Data[0] == 0x89 && Data[1] == 'P' && Data[2] == 'N' && Data[3] == 'G';
}

bool is_tiff(std::vector<std::uint8_t> const &Data)
{
	return Data.size() >= 4 && ((Data[0] == 'I' && Data[1] == 'I' && Data[2] == 42) || (Data[0] == 'M' && Data[1] == 'M' && Data[3] == 42));
}

enum class fetched
{
	data,
	nothing, // the place has nothing (a tile not there): kept, so it isn't asked again
	failed
};

// the address for the log, with the keys of the services hidden
std::string loggable(std::string Url)
{
	for (char const *const name : {"api-key=", "api_key=", "apikey=", "key=", "token="})
		for (std::size_t at{0}; (at = Url.find(name, at)) != std::string::npos;)
		{
			at += std::strlen(name);
			auto const end{Url.find('&', at)};
			Url.replace(at, (end == std::string::npos ? Url.size() : end) - at, "***");
		}
	return Url;
}

// a few times per host at most, so a source answering 404 everywhere shows in the log without flooding it
void log_nothing(std::string const &Url)
{
	static std::mutex mutex;
	static std::map<std::string, int> counts;
	auto const start{Url.find("://")};
	auto const host{Url.substr(0, Url.find('/', start == std::string::npos ? 0 : start + 3))};
	{
		std::lock_guard<std::mutex> lock{mutex};
		if (++counts[host] > 5)
			return;
	}
	WriteLog("Editor: other sources, nothing there (HTTP 404): " + loggable(Url));
}

// from the cache or the service. an empty file in the cache: nothing there
fetched download(editor_http::client &Client, std::string const &Url, fs::path const &Path, bool const Tiff, std::vector<std::uint8_t> &Out, std::string &Error,
                 editor_http::stop_check const &Stop)
{
	auto const valid = [&](std::vector<std::uint8_t> const &Data) { return Tiff ? is_tiff(Data) : is_image(Data); };
	if (read_file(Path, Out))
	{
		if (Out.empty())
			return fetched::nothing;
		if (valid(Out))
			return fetched::data;
	}
	for (int attempt = 0; attempt < 3; ++attempt)
	{
		if (Stop())
		{
			Error = "cancelled";
			return fetched::failed;
		}
		if (attempt > 0)
			for (int wait = 0; wait < attempt * 5 && false == Stop(); ++wait)
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
		if (false == Client.get(Url, Out, Error, Stop))
		{
			if (Error == "HTTP 404")
			{
				log_nothing(Url);
				write_file(Path, {});
				Out.clear();
				return fetched::nothing;
			}
			continue;
		}
		if (false == valid(Out))
		{
			Error = "unexpected response: " + std::string(Out.begin(), Out.begin() + std::min<std::size_t>(Out.size(), 300)) + " from " + loggable(Url);
			continue;
		}
		write_file(Path, Out);
		return fetched::data;
	}
	if (Error.find(" from ") == std::string::npos)
		Error += " from " + loggable(Url);
	return fetched::failed;
}

std::string replace_all(std::string Text, std::string const &What, std::string const &With)
{
	for (std::size_t at{0}; (at = Text.find(What, at)) != std::string::npos; at += With.size())
		Text.replace(at, What.size(), With);
	return Text;
}

// ---------------------------------------------------------------------------------------------
// rasters

// a grid in the system of a source: the outer corner of its north-west pixel at X, Y, the pixels Step wide, rows from the north
struct raster
{
	double x{0.0}, y{0.0};
	double step_x{1.0}, step_y{1.0};
	int width{0}, height{0};
	std::vector<std::uint8_t> rgba; // imagery
	std::vector<float> heights;     // terrain, NaN: no data
};

bool decode(std::vector<std::uint8_t> const &Data, int &Width, int &Height, std::vector<std::uint8_t> &Out)
{
	int components{0};
	stbi_set_flip_vertically_on_load_thread(0);
	auto *pixels{stbi_load_from_memory(Data.data(), static_cast<int>(Data.size()), &Width, &Height, &components, 4)};
	if (pixels == nullptr)
		return false;
	Out.assign(pixels, pixels + static_cast<std::size_t>(Width) * Height * 4);
	stbi_image_free(pixels);
	return true;
}

// a GeoTIFF of one band: 32/64-bit floats or 16/32-bit integers, in strips or in tiles, not compressed or deflated with no
// predictor; placed by the tie point and the scale or by the transformation. its nodata and anything past 1e30 is NaN
bool read_geotiff(std::vector<std::uint8_t> const &Data, raster &Out)
{
	if (false == is_tiff(Data))
		return false;
	bool const little{Data[0] == 'I'};
	auto const u16 = [&](std::size_t const At) -> std::uint32_t {
		if (At + 2 > Data.size())
			return 0;
		return little ? Data[At] | (Data[At + 1] << 8) : (Data[At] << 8) | Data[At + 1];
	};
	auto const u32 = [&](std::size_t const At) -> std::uint32_t {
		if (At + 4 > Data.size())
			return 0;
		return little ? Data[At] | (Data[At + 1] << 8) | (Data[At + 2] << 16) | (static_cast<std::uint32_t>(Data[At + 3]) << 24) :
		                (static_cast<std::uint32_t>(Data[At]) << 24) | (Data[At + 1] << 16) | (Data[At + 2] << 8) | Data[At + 3];
	};
	auto const bytes_at = [&](std::uint8_t const *Bytes, int const Size, bool const Little) {
		std::uint64_t bits{0};
		for (int i = 0; i < Size; ++i)
			bits |= static_cast<std::uint64_t>(Bytes[Little ? i : Size - 1 - i]) << (8 * i);
		return bits;
	};
	auto const f64 = [&](std::size_t const At) {
		if (At + 8 > Data.size())
			return 0.0;
		auto const bits{bytes_at(Data.data() + At, 8, little)};
		double value;
		std::memcpy(&value, &bits, sizeof(value));
		return value;
	};
	struct entry
	{
		std::uint32_t type{0};
		std::uint32_t count{0};
		std::size_t at{0};
	};
	std::map<std::uint32_t, entry> entries;
	auto const ifd{static_cast<std::size_t>(u32(4))};
	auto const count{u16(ifd)};
	for (std::uint32_t i = 0; i < count; ++i)
	{
		auto const at{ifd + 2 + i * 12};
		entry value{u16(at + 2), u32(at + 4), at + 8};
		auto const unit{value.type == 3 || value.type == 8 ? 2u : value.type == 12 || value.type == 5 ? 8u : value.type == 1 || value.type == 2 || value.type == 7 ? 1u : 4u};
		if (unit * value.count > 4)
			value.at = u32(at + 8);
		entries[u16(at)] = value;
	}
	auto const number = [&](std::uint32_t const Tag, std::uint32_t const Index) -> std::uint32_t {
		auto const found{entries.find(Tag)};
		if (found == entries.end() || Index >= found->second.count)
			return 0;
		return found->second.type == 3 ? u16(found->second.at + Index * 2) : u32(found->second.at + Index * 4);
	};
	auto const real = [&](std::uint32_t const Tag, std::uint32_t const Index) {
		auto const found{entries.find(Tag)};
		return found == entries.end() || Index >= found->second.count ? 0.0 : f64(found->second.at + Index * 8);
	};
	auto const width{static_cast<int>(number(256, 0))};
	auto const height{static_cast<int>(number(257, 0))};
	auto const bits{static_cast<int>(number(258, 0))};
	auto const format{std::max(1u, number(339, 0))}; // 1 unsigned, 2 signed, 3 float
	auto const compression{std::max(1u, number(259, 0))};
	if (width < 1 || height < 1 || std::max(1u, number(277, 0)) != 1 || (compression != 1 && compression != 8 && compression != 32946) || std::max(1u, number(317, 0)) != 1)
		return false;
	if (false == ((format == 3 && (bits == 32 || bits == 64)) || (format != 3 && (bits == 16 || bits == 32))))
		return false;
	// placement
	if (entries.contains(33550) && entries.contains(33922))
	{
		Out.step_x = real(33550, 0);
		Out.step_y = real(33550, 1);
		Out.x = real(33922, 3) - real(33922, 0) * Out.step_x;
		Out.y = real(33922, 4) + real(33922, 1) * Out.step_y;
	}
	else if (entries.contains(34264))
	{
		// no rotation is taken
		Out.step_x = real(34264, 0);
		Out.step_y = -real(34264, 5);
		Out.x = real(34264, 3);
		Out.y = real(34264, 7);
	}
	else
		return false;
	if (Out.step_x <= 0.0 || Out.step_y <= 0.0)
		return false;
	// GDAL's nodata: the number as text
	double nodata{std::numeric_limits<double>::quiet_NaN()};
	if (auto const found{entries.find(42113)}; found != entries.end() && found->second.at + found->second.count <= Data.size())
	{
		std::string text(reinterpret_cast<char const *>(Data.data() + found->second.at), found->second.count);
		text.erase(std::find(text.begin(), text.end(), '\0'), text.end());
		nodata = std::strtod(text.c_str(), nullptr);
	}
	Out.width = width;
	Out.height = height;
	auto const nan{std::numeric_limits<float>::quiet_NaN()};
	Out.heights.assign(static_cast<std::size_t>(width) * height, nan);
	auto const size{bits / 8};
	auto const value_of = [&](std::uint8_t const *Bytes) {
		auto const raw{bytes_at(Bytes, size, little)};
		double value;
		if (format == 3 && size == 4)
		{
			auto const narrow{static_cast<std::uint32_t>(raw)};
			float single;
			std::memcpy(&single, &narrow, sizeof(single));
			value = single;
		}
		else if (format == 3)
			std::memcpy(&value, &raw, sizeof(value));
		else if (format == 2)
			value = size == 2 ? static_cast<double>(static_cast<std::int16_t>(raw)) : static_cast<double>(static_cast<std::int32_t>(raw));
		else
			value = static_cast<double>(raw);
		if (false == std::isfinite(value) || std::abs(value) > 1e30 || (std::isfinite(nodata) && std::abs(value - nodata) < 1e-6 * std::max(1.0, std::abs(nodata))))
			return nan;
		return static_cast<float>(value);
	};
	// a block (strip or tile) as its bytes, inflated when it has to be
	auto const block = [&](std::size_t const Offset, std::size_t const Length, std::size_t const Expected, std::vector<std::uint8_t> &Buffer) -> std::uint8_t const * {
		if (Offset + Length > Data.size())
			return nullptr;
		if (compression == 1)
			return Length >= Expected ? Data.data() + Offset : nullptr;
		int length{0};
		auto *inflated{stbi_zlib_decode_malloc_guesssize_headerflag(reinterpret_cast<char const *>(Data.data() + Offset), static_cast<int>(Length), static_cast<int>(Expected), &length, 1)};
		if (inflated == nullptr)
			return nullptr;
		Buffer.assign(inflated, inflated + length);
		stbi_image_free(inflated);
		return Buffer.size() >= Expected ? Buffer.data() : nullptr;
	};
	std::vector<std::uint8_t> buffer;
	if (entries.contains(322))
	{
		auto const tile_width{static_cast<int>(number(322, 0))}, tile_height{static_cast<int>(number(323, 0))};
		if (tile_width < 1 || tile_height < 1)
			return false;
		auto const across{(width + tile_width - 1) / tile_width};
		auto const tiles{entries[324].count};
		for (std::uint32_t tile = 0; tile < tiles; ++tile)
		{
			auto const expected{static_cast<std::size_t>(tile_width) * tile_height * size};
			auto const *bytes{block(number(324, tile), number(325, tile), expected, buffer)};
			if (bytes == nullptr)
				return false;
			auto const left{static_cast<int>(tile % across) * tile_width}, top{static_cast<int>(tile / across) * tile_height};
			for (int row = 0; row < tile_height && top + row < height; ++row)
				for (int column = 0; column < tile_width && left + column < width; ++column)
					Out.heights[static_cast<std::size_t>(top + row) * width + left + column] = value_of(bytes + (static_cast<std::size_t>(row) * tile_width + column) * size);
		}
	}
	else if (entries.contains(273))
	{
		auto const rows{std::max(1u, number(278, 0))};
		auto const strips{entries[273].count};
		for (std::uint32_t strip = 0; strip < strips; ++strip)
		{
			auto const first{strip * rows};
			if (first >= static_cast<std::uint32_t>(height))
				break;
			auto const in_strip{std::min<std::uint32_t>(rows, height - first)};
			auto const expected{static_cast<std::size_t>(in_strip) * width * size};
			auto const length{entries.contains(279) ? number(279, strip) : static_cast<std::uint32_t>(expected)};
			auto const *bytes{block(number(273, strip), length, expected, buffer)};
			if (bytes == nullptr)
				return false;
			for (std::uint32_t row = 0; row < in_strip; ++row)
				for (int column = 0; column < width; ++column)
					Out.heights[static_cast<std::size_t>(first + row) * width + column] = value_of(bytes + (static_cast<std::size_t>(row) * width + column) * size);
		}
	}
	else
		return false;
	return true;
}

// the place of a pixel of the target grid in the system of a source: worked out exactly on a net of knots and between them
// along straight lines (over 32 px the curvature of the systems bends nothing worth a pixel)
struct target_grid
{
	double west, north, pixel;
	int width, height;
};

class placer
{
  public:
	placer(target_grid const &Grid, system_of_coordinates const &System) : m_grid{Grid}
	{
		m_across = Grid.width / knots + 2;
		m_down = Grid.height / knots + 2;
		m_places.resize(static_cast<std::size_t>(m_across) * m_down);
		for (int j = 0; j < m_down; ++j)
			for (int i = 0; i < m_across; ++i)
			{
				auto const east{Grid.west + std::min(i * knots, Grid.width) * Grid.pixel};
				auto const north{Grid.north - std::min(j * knots, Grid.height) * Grid.pixel};
				auto &place{m_places[static_cast<std::size_t>(j) * m_across + i]};
				puwg_to(System, east, north, place[0], place[1]);
			}
	}
	// of the middle of the pixel
	void place(int const Column, int const Row, double &X, double &Y) const
	{
		auto const u{(Column + 0.5) / knots}, v{(Row + 0.5) / knots};
		auto const i{std::min(static_cast<int>(u), m_across - 2)}, j{std::min(static_cast<int>(v), m_down - 2)};
		auto const spanu{std::min((i + 1) * knots, m_grid.width) - i * knots}, spanv{std::min((j + 1) * knots, m_grid.height) - j * knots};
		auto const fu{(Column + 0.5 - i * knots) / std::max(1, spanu)}, fv{(Row + 0.5 - j * knots) / std::max(1, spanv)};
		auto const &a{m_places[static_cast<std::size_t>(j) * m_across + i]}, &b{m_places[static_cast<std::size_t>(j) * m_across + i + 1]};
		auto const &c{m_places[static_cast<std::size_t>(j + 1) * m_across + i]}, &d{m_places[static_cast<std::size_t>(j + 1) * m_across + i + 1]};
		X = (a[0] * (1.0 - fu) + b[0] * fu) * (1.0 - fv) + (c[0] * (1.0 - fu) + d[0] * fu) * fv;
		Y = (a[1] * (1.0 - fu) + b[1] * fu) * (1.0 - fv) + (c[1] * (1.0 - fu) + d[1] * fu) * fv;
	}

  private:
	static int constexpr knots{32};
	target_grid m_grid;
	int m_across{0}, m_down{0};
	std::vector<std::array<double, 2>> m_places;
};

// the rectangle of the system of a source covering the target grid, with a margin of Margin of its units
void native_bounds(target_grid const &Grid, system_of_coordinates const &System, double const Margin, double &MinX, double &MinY, double &MaxX, double &MaxY)
{
	MinX = MinY = 1e300;
	MaxX = MaxY = -1e300;
	int constexpr steps{8};
	for (int i = 0; i <= steps; ++i)
		for (int j = 0; j <= steps; ++j)
		{
			if (i != 0 && i != steps && j != 0 && j != steps)
				continue;
			double x, y;
			puwg_to(System, Grid.west + Grid.width * Grid.pixel * i / steps, Grid.north - Grid.height * Grid.pixel * j / steps, x, y);
			MinX = std::min(MinX, x);
			MaxX = std::max(MaxX, x);
			MinY = std::min(MinY, y);
			MaxY = std::max(MaxY, y);
		}
	MinX -= Margin;
	MinY -= Margin;
	MaxX += Margin;
	MaxY += Margin;
}

std::string format_bbox(double const MinX, double const MinY, double const MaxX, double const MaxY, bool const Degrees)
{
	char buffer[160];
	if (Degrees)
		std::snprintf(buffer, sizeof(buffer), "%.8f,%.8f,%.8f,%.8f", MinX, MinY, MaxX, MaxY);
	else
		std::snprintf(buffer, sizeof(buffer), "%.3f,%.3f,%.3f,%.3f", MinX, MinY, MaxX, MaxY);
	return buffer;
}

// a WMS or WCS asked for the target grid, at its pixel or the finest of the source if that's coarser
fetched fetch_service(editor_http::client &Client, source const &Source, target_grid const &Grid, fs::path const &Cache, raster &Out, std::string &Error,
                      editor_http::stop_check const &Stop)
{
	auto const degrees{Source.system.kind == system_of_coordinates::type::geographic};
	// the pixel of the target in units of the system of the source
	auto pixel{Grid.pixel};
	if (degrees)
		pixel /= 111320.0;
	pixel *= Source.magnify;
	if (Source.resolution > 0.0)
		pixel = std::max(pixel, Source.resolution);
	double minx, miny, maxx, maxy;
	native_bounds(Grid, Source.system, 2.0 * pixel, minx, miny, maxx, maxy);
	// on the pixels of the source, so the same place asks the same each time
	minx = std::floor(minx / pixel) * pixel;
	miny = std::floor(miny / pixel) * pixel;
	maxx = std::ceil(maxx / pixel) * pixel;
	maxy = std::ceil(maxy / pixel) * pixel;
	auto width{static_cast<int>(std::lround((maxx - minx) / pixel))};
	auto height{static_cast<int>(std::lround((maxy - miny) / pixel))};
	if (width > Source.max_pixels || height > Source.max_pixels)
	{
		auto const shrink{std::max(width, height) / static_cast<double>(Source.max_pixels)};
		width = std::max(1, static_cast<int>(std::ceil(width / shrink)));
		height = std::max(1, static_cast<int>(std::ceil(height / shrink)));
	}
	if (width < 1 || height < 1)
		return fetched::nothing;
	auto url{replace_all(Source.url, "{bbox}", format_bbox(minx, miny, maxx, maxy, degrees))};
	// the corners one by one, for WCS 2.0 (SUBSET=E({minx},{maxx})&SUBSET=N({miny},{maxy}))
	auto const number = [&](double const Value) {
		char buffer[48];
		std::snprintf(buffer, sizeof(buffer), degrees ? "%.8f" : "%.3f", Value);
		return std::string{buffer};
	};
	url = replace_all(url, "{minx}", number(minx));
	url = replace_all(url, "{miny}", number(miny));
	url = replace_all(url, "{maxx}", number(maxx));
	url = replace_all(url, "{maxy}", number(maxy));
	url = replace_all(url, "{width}", std::to_string(width));
	url = replace_all(url, "{height}", std::to_string(height));
	auto const terrain{Source.what == kind::terrain};
	auto const path{Cache / Source.name / (hex(fnv(url)) + (terrain ? ".tif" : ".img"))};
	std::vector<std::uint8_t> data;
	auto const result{download(Client, url, path, terrain, data, Error, Stop)};
	if (result != fetched::data)
		return result;
	if (terrain)
	{
		if (false == read_geotiff(data, Out))
		{
			std::error_code fault;
			fs::remove(path, fault);
			Error = "the terrain of " + Source.name + " isn't a GeoTIFF this can read";
			return fetched::failed;
		}
		return fetched::data;
	}
	if (false == decode(data, Out.width, Out.height, Out.rgba))
	{
		std::error_code fault;
		fs::remove(path, fault);
		Error = std::string{"image decoding failed: "} + stbi_failure_reason();
		return fetched::failed;
	}
	Out.x = minx;
	Out.y = maxy;
	Out.step_x = (maxx - minx) / Out.width;
	Out.step_y = (maxy - miny) / Out.height;
	return fetched::data;
}

// the tiles of an XYZ service covering the target grid, put together into one raster of Web Mercator
fetched fetch_tiles(editor_http::client &Client, source const &Source, target_grid const &Grid, fs::path const &Cache, raster &Out, std::string &Error,
                    editor_http::stop_check const &Stop)
{
	double minx, miny, maxx, maxy;
	native_bounds(Grid, Source.system, 0.0, minx, miny, maxx, maxy);
	auto const world{2.0 * pi * mercator_radius};
	// the level: imagery as fine as the target pixel (on the ground: the Mercator scale is 1/cos of the latitude), terrain as set
	int zoom{Source.zoom};
	if (Source.what != kind::terrain)
	{
		double latitude, longitude;
		to_geographic(Source.system, 0.5 * (minx + maxx), 0.5 * (miny + maxy), latitude, longitude);
		auto const ground{Grid.pixel * Source.magnify / std::max(0.05, std::cos(latitude))}; // the target pixel in Mercator metres
		zoom = std::clamp(static_cast<int>(std::ceil(std::log2(world / 256.0 / ground))), 0, Source.zoom);
	}
	// no more than 8 x 8 tiles in one go
	int first_x, last_x, first_y, last_y;
	while (true)
	{
		auto const tiles{static_cast<double>(1 << zoom)};
		auto const size{world / tiles};
		first_x = static_cast<int>(std::floor((minx + world / 2.0) / size));
		last_x = static_cast<int>(std::floor((maxx + world / 2.0) / size));
		first_y = static_cast<int>(std::floor((world / 2.0 - maxy) / size));
		last_y = static_cast<int>(std::floor((world / 2.0 - miny) / size));
		if ((last_x - first_x + 1) * (last_y - first_y + 1) <= 64 || zoom == 0)
			break;
		--zoom;
	}
	auto const count{1 << zoom};
	first_x = std::clamp(first_x, 0, count - 1);
	last_x = std::clamp(last_x, 0, count - 1);
	first_y = std::clamp(first_y, 0, count - 1);
	last_y = std::clamp(last_y, 0, count - 1);
	auto const terrain{Source.what == kind::terrain};
	int tile_pixels{256};
	auto const across{last_x - first_x + 1}, down{last_y - first_y + 1};
	auto const size{world / count};
	bool any{false};
	for (int ty = first_y; ty <= last_y; ++ty)
		for (int tx = first_x; tx <= last_x; ++tx)
		{
			auto url{replace_all(Source.url, "{z}", std::to_string(zoom))};
			url = replace_all(url, "{x}", std::to_string(tx));
			url = replace_all(url, "{y}", std::to_string(ty));
			auto const path{Cache / Source.name / std::to_string(zoom) / std::to_string(tx) / (std::to_string(ty) + ".img")};
			std::vector<std::uint8_t> data;
			auto const result{download(Client, url, path, false, data, Error, Stop)};
			if (result == fetched::failed)
				return result;
			if (result == fetched::nothing)
				continue;
			int width, height;
			std::vector<std::uint8_t> pixels;
			if (false == decode(data, width, height, pixels) || width != height || width < 1)
			{
				std::error_code fault;
				fs::remove(path, fault);
				Error = "a tile of " + Source.name + " couldn't be decoded";
				return fetched::failed;
			}
			if (false == any)
			{
				// the raster made when the first tile is in, at its size
				any = true;
				tile_pixels = width;
				Out.width = across * tile_pixels;
				Out.height = down * tile_pixels;
				Out.x = first_x * size - world / 2.0;
				Out.y = world / 2.0 - first_y * size;
				Out.step_x = Out.step_y = size / tile_pixels;
				if (terrain)
					Out.heights.assign(static_cast<std::size_t>(Out.width) * Out.height, std::numeric_limits<float>::quiet_NaN());
				else
					Out.rgba.assign(static_cast<std::size_t>(Out.width) * Out.height * 4, 0);
			}
			if (width != tile_pixels)
				continue; // tiles of one service are all of a size
			auto const left{(tx - first_x) * tile_pixels}, top{(ty - first_y) * tile_pixels};
			for (int row = 0; row < tile_pixels; ++row)
				for (int column = 0; column < tile_pixels; ++column)
				{
					auto const *pixel{&pixels[(static_cast<std::size_t>(row) * tile_pixels + column) * 4]};
					auto const index{static_cast<std::size_t>(top + row) * Out.width + left + column};
					if (terrain)
					{
						if (Source.terrarium)
							Out.heights[index] = static_cast<float>(pixel[0] * 256.0 + pixel[1] + pixel[2] / 256.0 - 32768.0);
					}
					else
						std::memcpy(&Out.rgba[index * 4], pixel, 4);
				}
		}
	return any ? fetched::data : fetched::nothing;
}

fetched fetch(editor_http::client &Client, source const &Source, target_grid const &Grid, fs::path const &Cache, raster &Out, std::string &Error,
              editor_http::stop_check const &Stop)
{
	return Source.type == service::xyz ? fetch_tiles(Client, Source, Grid, Cache, Out, Error, Stop) : fetch_service(Client, Source, Grid, Cache, Out, Error, Stop);
}

// the raster of a source sampled at the pixels of the target grid
void sample_rgba(raster const &Raster, source const &Source, target_grid const &Grid, std::vector<std::uint8_t> &Out)
{
	Out.assign(static_cast<std::size_t>(Grid.width) * Grid.height * 4, 0);
	if (Raster.width < 1 || Raster.height < 1)
		return;
	placer const places{Grid, Source.system};
	for (int row = 0; row < Grid.height; ++row)
		for (int column = 0; column < Grid.width; ++column)
		{
			double x, y;
			places.place(column, row, x, y);
			auto const c{(x - Raster.x) / Raster.step_x - 0.5}, r{(Raster.y - y) / Raster.step_y - 0.5};
			if (c < -0.5 || r < -0.5 || c > Raster.width - 0.5 || r > Raster.height - 0.5)
				continue;
			auto const c0{std::clamp(static_cast<int>(std::floor(c)), 0, std::max(0, Raster.width - 2))};
			auto const r0{std::clamp(static_cast<int>(std::floor(r)), 0, std::max(0, Raster.height - 2))};
			auto const c1{std::min(c0 + 1, Raster.width - 1)}, r1{std::min(r0 + 1, Raster.height - 1)};
			auto const fc{std::clamp(c - c0, 0.0, 1.0)}, fr{std::clamp(r - r0, 0.0, 1.0)};
			auto const at = [&](int const C, int const R) { return &Raster.rgba[(static_cast<std::size_t>(R) * Raster.width + C) * 4]; };
			std::uint8_t const *corners[4]{at(c0, r0), at(c1, r0), at(c0, r1), at(c1, r1)};
			double const weights[4]{(1.0 - fc) * (1.0 - fr), fc * (1.0 - fr), (1.0 - fc) * fr, fc * fr};
			// with the colours weighted by their alpha, so the transparent ones don't darken the edge
			double alpha{0.0}, colour[3]{0.0, 0.0, 0.0};
			for (int k = 0; k < 4; ++k)
			{
				auto const a{weights[k] * corners[k][3]};
				alpha += a;
				for (int ch = 0; ch < 3; ++ch)
					colour[ch] += a * corners[k][ch];
			}
			auto *pixel{&Out[(static_cast<std::size_t>(row) * Grid.width + column) * 4]};
			if (alpha <= 0.0)
				continue;
			for (int ch = 0; ch < 3; ++ch)
				pixel[ch] = static_cast<std::uint8_t>(std::clamp(colour[ch] / alpha + 0.5, 0.0, 255.0));
			pixel[3] = static_cast<std::uint8_t>(std::clamp(alpha + 0.5, 0.0, 255.0));
		}
}

void sample_heights(raster const &Raster, source const &Source, target_grid const &Grid, std::vector<float> &Out)
{
	auto const nan{std::numeric_limits<float>::quiet_NaN()};
	Out.assign(static_cast<std::size_t>(Grid.width) * Grid.height, nan);
	if (Raster.width < 1 || Raster.height < 1)
		return;
	placer const places{Grid, Source.system};
	for (int row = 0; row < Grid.height; ++row)
		for (int column = 0; column < Grid.width; ++column)
		{
			double x, y;
			places.place(column, row, x, y);
			auto const c{(x - Raster.x) / Raster.step_x - 0.5}, r{(Raster.y - y) / Raster.step_y - 0.5};
			if (c < -0.5 || r < -0.5 || c > Raster.width - 0.5 || r > Raster.height - 0.5)
				continue;
			auto const c0{std::clamp(static_cast<int>(std::floor(c)), 0, std::max(0, Raster.width - 2))};
			auto const r0{std::clamp(static_cast<int>(std::floor(r)), 0, std::max(0, Raster.height - 2))};
			auto const c1{std::min(c0 + 1, Raster.width - 1)}, r1{std::min(r0 + 1, Raster.height - 1)};
			auto const fc{std::clamp(c - c0, 0.0, 1.0)}, fr{std::clamp(r - r0, 0.0, 1.0)};
			auto const at = [&](int const C, int const R) { return Raster.heights[static_cast<std::size_t>(R) * Raster.width + C]; };
			float const corners[4]{at(c0, r0), at(c1, r0), at(c0, r1), at(c1, r1)};
			auto &value{Out[static_cast<std::size_t>(row) * Grid.width + column]};
			if (std::all_of(std::begin(corners), std::end(corners), [](float const V) { return std::isfinite(V); }))
				value = static_cast<float>((corners[0] * (1.0 - fc) + corners[1] * fc) * (1.0 - fr) + (corners[2] * (1.0 - fc) + corners[3] * fc) * fr);
			else
				value = corners[(fr > 0.5 ? 2 : 0) + (fc > 0.5 ? 1 : 0)];
		}
}

// the distance (pixels, chamfer 3-4 / 3) of each pixel to the nearest seed, and what that seed carries; up to Limit pixels
void spread(int const Width, int const Height, std::vector<char> const &Seeds, std::vector<float> const &Carried, float const Limit, std::vector<float> &Distance,
            std::vector<float> &Value)
{
	auto const farthest{std::numeric_limits<float>::max()};
	Distance.assign(Seeds.size(), farthest);
	Value.assign(Seeds.size(), 0.f);
	for (std::size_t i = 0; i < Seeds.size(); ++i)
		if (Seeds[i])
		{
			Distance[i] = 0.f;
			Value[i] = Carried.empty() ? 0.f : Carried[i];
		}
	auto const relax = [&](int const X, int const Y, int const NX, int const NY, float const Step) {
		if (NX < 0 || NY < 0 || NX >= Width || NY >= Height)
			return;
		auto const from{static_cast<std::size_t>(NY) * Width + NX}, to{static_cast<std::size_t>(Y) * Width + X};
		if (Distance[from] + Step < Distance[to] && Distance[from] + Step <= Limit)
		{
			Distance[to] = Distance[from] + Step;
			Value[to] = Value[from];
		}
	};
	float constexpr straight{1.f}, diagonal{4.f / 3.f};
	for (int y = 0; y < Height; ++y)
		for (int x = 0; x < Width; ++x)
		{
			relax(x, y, x - 1, y, straight);
			relax(x, y, x, y - 1, straight);
			relax(x, y, x - 1, y - 1, diagonal);
			relax(x, y, x + 1, y - 1, diagonal);
		}
	for (int y = Height - 1; y >= 0; --y)
		for (int x = Width - 1; x >= 0; --x)
		{
			relax(x, y, x + 1, y, straight);
			relax(x, y, x, y + 1, straight);
			relax(x, y, x + 1, y + 1, diagonal);
			relax(x, y, x - 1, y + 1, diagonal);
		}
}

} // namespace

// ---------------------------------------------------------------------------------------------

bool known_system(int const Epsg)
{
	system_of_coordinates unused;
	return system_by_code(Epsg, unused);
}

bool to_puwg(int const Epsg, double const X, double const Y, double &East, double &North)
{
	system_of_coordinates system;
	if (false == system_by_code(Epsg, system))
		return false;
	double latitude, longitude;
	to_geographic(system, X, Y, latitude, longitude);
	to_grid(puwg(), latitude, longitude, East, North);
	return true;
}

bool from_puwg(int const Epsg, double const East, double const North, double &X, double &Y)
{
	system_of_coordinates system;
	if (false == system_by_code(Epsg, system))
		return false;
	puwg_to(system, East, North, X, Y);
	return true;
}

std::string sources_file()
{
	auto &registry{sources_registry()};
	std::lock_guard<std::mutex> lock{registry.mutex};
	if (false == registry.loaded)
		load(registry);
	return file_name;
}

void reload()
{
	auto &registry{sources_registry()};
	std::lock_guard<std::mutex> lock{registry.mutex};
	load(registry);
}

int generation()
{
	auto &registry{sources_registry()};
	{
		std::lock_guard<std::mutex> lock{registry.mutex};
		if (false == registry.loaded)
			load(registry);
	}
	return registry.generation;
}

std::string status()
{
	auto &registry{sources_registry()};
	std::lock_guard<std::mutex> lock{registry.mutex};
	if (false == registry.loaded)
		load(registry);
	return registry.status;
}

std::string credits(kind const Kind)
{
	auto &registry{sources_registry()};
	std::lock_guard<std::mutex> lock{registry.mutex};
	if (false == registry.loaded)
		load(registry);
	std::string result;
	for (auto const &item : registry.sources)
		if (item.what == Kind && false == item.credit.empty() && result.find(item.credit) == std::string::npos)
			result += (result.empty() ? "" : ", ") + item.credit;
	return result;
}

fs::path terrain_cache()
{
	return "editor/cache/foreign";
}

namespace
{
int constexpr black_level{12};  // the brightest channel of a pixel taken as black
int constexpr dark_edge{60};    // the brightest channel of a pixel at the edge of a black patch going with it
int constexpr edge_pixels{3};   // how far from the black core its edge goes

bool nearly_black(std::uint8_t const *Pixel, int const Level)
{
	return Pixel[3] != 0 && Pixel[0] <= Level && Pixel[1] <= Level && Pixel[2] <= Level;
}

// sums of a mask over rectangles: Sum[(y)*(W+1)+x] over the pixels left of x and above y
std::vector<std::uint32_t> integral(std::vector<char> const &Mask, int const Width, int const Height)
{
	std::vector<std::uint32_t> sum(static_cast<std::size_t>(Width + 1) * (Height + 1), 0);
	for (int y = 0; y < Height; ++y)
	{
		std::uint32_t row{0};
		for (int x = 0; x < Width; ++x)
		{
			row += Mask[static_cast<std::size_t>(y) * Width + x] ? 1 : 0;
			sum[static_cast<std::size_t>(y + 1) * (Width + 1) + x + 1] = sum[static_cast<std::size_t>(y) * (Width + 1) + x + 1] + row;
		}
	}
	return sum;
}

std::uint32_t box(std::vector<std::uint32_t> const &Sum, int const Width, int const Height, int X0, int Y0, int X1, int Y1)
{
	X0 = std::clamp(X0, 0, Width);
	X1 = std::clamp(X1, 0, Width);
	Y0 = std::clamp(Y0, 0, Height);
	Y1 = std::clamp(Y1, 0, Height);
	if (X1 <= X0 || Y1 <= Y0)
		return 0;
	auto const at = [&](int const X, int const Y) { return Sum[static_cast<std::size_t>(Y) * (Width + 1) + X]; };
	return at(X1, Y1) - at(X0, Y1) - at(X1, Y0) + at(X0, Y0);
}
} // namespace

double black_share(std::vector<std::uint8_t> const &Rgba)
{
	if (Rgba.size() < 4)
		return 0.0;
	std::size_t black{0};
	for (std::size_t i = 0; i + 3 < Rgba.size(); i += 4)
		black += nearly_black(&Rgba[i], black_level);
	return black / (Rgba.size() / 4.0);
}

std::size_t clear_black(std::vector<std::uint8_t> &Rgba, int const Width, int const Height, int const Block)
{
	auto const count{static_cast<std::size_t>(Width) * Height};
	if (Rgba.size() != count * 4 || Width < Block || Height < Block)
		return 0;
	std::vector<char> black(count, 0);
	bool any{false};
	for (std::size_t i = 0; i < count; ++i)
	{
		black[i] = nearly_black(&Rgba[i * 4], black_level);
		any = any || black[i];
	}
	if (false == any)
		return 0;
	// the core: pixels whose whole block around them is black
	auto const sums{integral(black, Width, Height)};
	auto const half{Block / 2};
	auto const full{static_cast<std::uint32_t>(Block * Block)};
	std::vector<char> core(count, 0);
	bool cores{false};
	for (int y = 0; y < Height; ++y)
		for (int x = 0; x < Width; ++x)
			if (black[static_cast<std::size_t>(y) * Width + x] && box(sums, Width, Height, x - half, y - half, x - half + Block, y - half + Block) == full)
			{
				core[static_cast<std::size_t>(y) * Width + x] = 1;
				cores = true;
			}
	if (false == cores)
		return 0;
	// the patch: the dark pixels close enough to the core (the core grown back to the size of the black, and its jpeg fringe)
	auto const cores_sum{integral(core, Width, Height)};
	auto const reach{half + edge_pixels};
	std::size_t cleared{0};
	for (int y = 0; y < Height; ++y)
		for (int x = 0; x < Width; ++x)
		{
			auto *pixel{&Rgba[(static_cast<std::size_t>(y) * Width + x) * 4]};
			if (pixel[3] == 0 || false == nearly_black(pixel, dark_edge))
				continue;
			if (box(cores_sum, Width, Height, x - reach, y - reach, x + reach + 1, y + reach + 1) == 0)
				continue;
			pixel[3] = 0;
			++cleared;
		}
	return cleared;
}

bool reaches(kind const Kind, area const &Area)
{
	return false == sources_for(Kind, Area).empty();
}

namespace
{
bool compose(editor_http::client &Client, std::vector<source> const &Sources, area const &Area, int const Pixels, std::vector<std::uint8_t> &Rgba, fs::path const &Cache,
             std::string &Error, editor_http::stop_check const &Stop);
}

bool ortho(editor_http::client &Client, area const &Area, int const Pixels, std::vector<std::uint8_t> &Rgba, fs::path const &Cache, std::string &Error,
           editor_http::stop_check const &Stop)
{
	return compose(Client, sources_for(kind::ortho, Area), Area, Pixels, Rgba, Cache, Error, Stop);
}

std::vector<layer_info> layers()
{
	auto &registry{sources_registry()};
	std::lock_guard<std::mutex> lock{registry.mutex};
	if (false == registry.loaded)
		load(registry);
	std::vector<layer_info> result;
	for (auto const &item : registry.sources)
		if (item.what == kind::map)
			result.push_back({item.name, item.title.empty() ? item.name : item.title, item.credit, item.tile});
	return result;
}

bool layer(editor_http::client &Client, std::string const &Name, area const &Area, int const Pixels, std::vector<std::uint8_t> &Rgba, fs::path const &Cache,
           std::string &Error, editor_http::stop_check const &Stop, bool const Plain)
{
	auto sources{sources_for(kind::map, Area)};
	sources.erase(std::remove_if(sources.begin(), sources.end(), [&](source const &Item) { return Item.name != Name; }), sources.end());
	if (Plain)
		for (auto &item : sources)
			item.magnify = 1;
	Rgba.clear();
	if (sources.empty())
	{
		Error = "no map layer " + Name;
		return false;
	}
	return compose(Client, sources, Area, Pixels, Rgba, Cache / "maps", Error, Stop);
}

namespace
{
bool compose(editor_http::client &Client, std::vector<source> const &Sources, area const &Area, int const Pixels, std::vector<std::uint8_t> &Rgba, fs::path const &Cache,
             std::string &Error, editor_http::stop_check const &Stop)
{
	auto const count{static_cast<std::size_t>(Pixels) * Pixels};
	if (Rgba.size() != count * 4)
		Rgba.assign(count * 4, 0);
	auto const opaque = [&](std::vector<std::uint8_t> const &Image) {
		for (std::size_t i = 3; i < Image.size(); i += 4)
			if (Image[i] != 255)
				return false;
		return true;
	};
	if (opaque(Rgba))
		return true;
	target_grid const grid{Area.west, Area.north, (Area.east - Area.west) / Pixels, Pixels, Pixels};
	// the sources one under the other: each fills what those before it left
	std::vector<std::uint8_t> below(count * 4, 0);
	// which of them is on top at each pixel, -1: none
	std::vector<float> owner(count, -1.f);
	bool ok{true};
	auto const &sources{Sources};
	for (std::size_t index = 0; index < sources.size(); ++index)
	{
		auto const &item{sources[index]};
		raster native;
		std::string error;
		auto const result{fetch(Client, item, grid, Cache, native, error, Stop)};
		if (result == fetched::failed)
		{
			Error = item.name + ": " + error;
			ok = false;
			continue;
		}
		if (result == fetched::nothing)
			continue;
		std::vector<std::uint8_t> layer;
		sample_rgba(native, item, grid, layer);
		if (item.what != kind::map)
			clear_black(layer, grid.width, grid.height);
		for (std::size_t i = 0; i < count; ++i)
		{
			auto *pixel{&below[i * 4]};
			auto const *add{&layer[i * 4]};
			if (pixel[3] == 255 || add[3] == 0)
				continue;
			if (owner[i] < 0.f && add[3] >= 128)
				owner[i] = static_cast<float>(index);
			// what's there over what's added
			double const top{pixel[3] / 255.0}, under{add[3] / 255.0 * (1.0 - top)};
			auto const alpha{top + under};
			for (int ch = 0; ch < 3; ++ch)
				pixel[ch] = static_cast<std::uint8_t>(std::clamp((pixel[ch] * top + add[ch] * under) / alpha + 0.5, 0.0, 255.0));
			pixel[3] = static_cast<std::uint8_t>(std::clamp(alpha * 255.0 + 0.5, 0.0, 255.0));
		}
		if (opaque(below))
			break;
	}
	// what was there over them, its edge faded into the source which fills the hole next to it, where that source lies under it
	// too (not into another one, which would show as a band along the edge)
	auto feather{0.0};
	for (auto const &item : sources)
		feather = std::max(feather, item.feather);
	auto const limit{static_cast<float>(feather / grid.pixel)};
	std::vector<char> holes(count, 0);
	for (std::size_t i = 0; i < count; ++i)
		holes[i] = Rgba[i * 4 + 3] < 255;
	std::vector<float> distance, nearest;
	if (limit > 0.f)
		spread(Pixels, Pixels, holes, owner, limit, distance, nearest);
	for (std::size_t i = 0; i < count; ++i)
	{
		auto *pixel{&Rgba[i * 4]};
		auto const *under{&below[i * 4]};
		double top{pixel[3] / 255.0};
		if (top > 0.0 && limit > 0.f && under[3] == 255 && distance[i] <= limit && nearest[i] >= 0.f && nearest[i] == owner[i])
			top *= std::clamp(distance[i] / limit, 0.f, 1.f);
		double const rest{under[3] / 255.0 * (1.0 - top)};
		auto const alpha{top + rest};
		if (alpha <= 0.0)
			continue;
		for (int ch = 0; ch < 3; ++ch)
			pixel[ch] = static_cast<std::uint8_t>(std::clamp((pixel[ch] * top + under[ch] * rest) / alpha + 0.5, 0.0, 255.0));
		pixel[3] = static_cast<std::uint8_t>(std::clamp(alpha * 255.0 + 0.5, 0.0, 255.0));
	}
	return ok;
}

} // namespace

bool terrain(editor_http::client &Client, double const West, double const North, double const Pixel, int const Width, int const Height, std::vector<float> &Heights,
             float const NoData, std::string &Error, editor_http::stop_check const &Stop)
{
	auto const count{static_cast<std::size_t>(Width) * Height};
	if (Heights.size() != count)
		Heights.assign(count, NoData - 1.f);
	auto const valid = [&](float const Value) { return std::isfinite(Value) && Value > NoData; };
	std::vector<char> original(count, 0);
	bool full{true};
	for (std::size_t i = 0; i < count; ++i)
	{
		original[i] = valid(Heights[i]);
		full = full && original[i];
	}
	if (full)
		return true;
	area const where{West, North - Height * Pixel, West + Width * Pixel, North};
	target_grid const grid{West, North, Pixel, Width, Height};
	// the edge of the data there is: its pixels next to a hole
	std::vector<char> edge(count, 0);
	bool any_edge{false};
	for (int y = 0; y < Height; ++y)
		for (int x = 0; x < Width; ++x)
		{
			auto const i{static_cast<std::size_t>(y) * Width + x};
			if (false == original[i])
				continue;
			auto const hole = [&](int const X, int const Y) { return X >= 0 && Y >= 0 && X < Width && Y < Height && false == original[static_cast<std::size_t>(Y) * Width + X]; };
			edge[i] = hole(x - 1, y) || hole(x + 1, y) || hole(x, y - 1) || hole(x, y + 1);
			any_edge = any_edge || edge[i];
		}
	auto &registry{sources_registry()};
	bool ok{true};
	std::vector<char> filled(original);
	for (auto const &item : sources_for(kind::terrain, where))
	{
		raster native;
		std::string error;
		auto const result{fetch(Client, item, grid, terrain_cache(), native, error, Stop)};
		if (result == fetched::failed)
		{
			Error = item.name + ": " + error;
			ok = false;
			continue;
		}
		if (result == fetched::nothing)
			continue;
		std::vector<float> layer;
		sample_heights(native, item, grid, layer);
		// the differences at the edge of the data there is, where this source has its heights too
		std::vector<char> seeds(count, 0);
		std::vector<float> differences(count, 0.f);
		double sum{0.0};
		std::size_t seen{0};
		if (any_edge)
			for (int y = 0; y < Height; ++y)
				for (int x = 0; x < Width; ++x)
				{
					auto const i{static_cast<std::size_t>(y) * Width + x};
					if (false == std::isfinite(layer[i]))
						continue;
					if (edge[i])
					{
						// both there: the difference as it is
						seeds[i] = 1;
						differences[i] = Heights[i] - layer[i];
					}
					else if (false == original[i])
					{
						// the data of most countries ends at the border, as does the Polish one: there they're only next to each other,
						// so the heights of the pixel are set against those of the Polish pixels around it (an error of the slope over a
						// metre, which evens out along the edge)
						double around{0.0};
						int next{0};
						for (int dy = -1; dy <= 1; ++dy)
							for (int dx = -1; dx <= 1; ++dx)
							{
								auto const nx{x + dx}, ny{y + dy};
								if (nx < 0 || ny < 0 || nx >= Width || ny >= Height)
									continue;
								auto const j{static_cast<std::size_t>(ny) * Width + nx};
								if (original[j])
								{
									around += Heights[j];
									++next;
								}
							}
						if (next == 0)
							continue;
						seeds[i] = 1;
						differences[i] = static_cast<float>(around / next - layer[i]);
					}
					else
						continue;
					sum += differences[i];
					++seen;
				}
		// the shift of the whole source: as set, or as seen at the edges so far
		double shift{item.offset};
		if (item.automatic)
		{
			std::lock_guard<std::mutex> lock{registry.mutex};
			auto &tally{registry.shifts[item.name]};
			// a few hundred pixels of one edge count as much as one more edge
			if (seen > 0)
			{
				auto const weight{std::min<double>(1.0, seen / 200.0)};
				tally.first += weight * sum / seen;
				tally.second += weight;
			}
			if (tally.second > 0.0)
				shift = tally.first / tally.second;
		}
		// near the edge pulled onto the heights there, further away the shift of the source
		std::vector<float> distance, carried;
		auto const limit{static_cast<float>(item.blend / Pixel)};
		if (seen > 0 && limit > 0.f)
			spread(Width, Height, seeds, differences, limit, distance, carried);
		for (std::size_t i = 0; i < count; ++i)
		{
			if (filled[i] || false == std::isfinite(layer[i]))
				continue;
			auto correction{shift};
			if (false == distance.empty() && distance[i] <= limit)
			{
				auto const t{distance[i] / limit};
				correction = carried[i] * (1.0 - t) + shift * t;
			}
			Heights[i] = static_cast<float>(layer[i] + correction);
			filled[i] = 1;
		}
		if (std::find(filled.begin(), filled.end(), 0) == filled.end())
			break;
	}
	return ok;
}

} // namespace editor_foreign
