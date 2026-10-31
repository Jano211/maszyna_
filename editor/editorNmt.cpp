/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "stdafx.h"
#include "editor/editorNmt.hpp"
#include "editor/editorHttp.hpp"
#include "utilities/Logs.h"
#include "utilities/translation.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <charconv>
#include <sstream>

namespace
{

namespace fs = std::filesystem;

// the model at 1 m of the WCS service of geoportal.gov.pl; the name of the model is read from its capabilities
char const *const service{"https://mapy.geoportal.gov.pl/wss/service/PZGIK/NMT/GRID1/WCS/DigitalTerrainModelFormatTIFF"};
char const *const fallback_coverage{"DTM_PL-KRON86-NH_TIFF"};
char const *const cache_folder{"editor/cache/nmt/grid1/"};
int constexpr worker_count{2}; // the service is quick, but isn't to be hammered
int constexpr border{1}; // pixels of the neighbouring squares fetched around a square, so the heights interpolate across its edges
float constexpr no_data{-1000.f}; // the model gives far lower values outside of the country

// the name of the model among the names the capabilities of the service list: the first which says it's a terrain model
std::string coverage_of(std::string const &Capabilities)
{
	std::vector<std::string> names;
	for (std::size_t at{0}; (at = Capabilities.find("name>", at)) != std::string::npos;)
	{
		at += 5;
		auto const end{Capabilities.find('<', at)};
		if (end == std::string::npos)
			break;
		names.emplace_back(Capabilities.substr(at, end - at));
	}
	for (auto const &name : names)
	{
		std::string lower{name};
		std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char const Letter) { return static_cast<char>(std::tolower(Letter)); });
		if (lower.find("dtm") != std::string::npos || lower.find("nmt") != std::string::npos || lower.find("terrain") != std::string::npos)
			return name;
	}
	return fallback_coverage;
}

// a GeoTIFF as the service gives it: one band of 32-bit floats in strips, not compressed, placed on the grid by its tie point
// and pixel scale. false for anything else
template <typename Square>
bool read_geotiff(std::vector<std::uint8_t> const &Data, Square &Out)
{
	if (Data.size() < 8)
		return false;
	bool const little{Data[0] == 'I' && Data[1] == 'I'};
	if (false == little && false == (Data[0] == 'M' && Data[1] == 'M'))
		return false;
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
	auto const f64 = [&](std::size_t const At) {
		std::uint64_t bits{0};
		for (int i = 0; i < 8; ++i)
			bits |= static_cast<std::uint64_t>(At + i < Data.size() ? Data[At + (little ? i : 7 - i)] : 0) << (8 * i);
		double value;
		std::memcpy(&value, &bits, sizeof(value));
		return value;
	};
	if (u16(2) != 42)
		return false;
	struct entry
	{
		std::uint32_t type{0};
		std::uint32_t count{0};
		std::size_t at{0}; // of the values: in the entry when they fit in four bytes
	};
	std::map<std::uint32_t, entry> entries;
	auto const ifd{static_cast<std::size_t>(u32(4))};
	auto const count{u16(ifd)};
	for (std::uint32_t i = 0; i < count; ++i)
	{
		auto const at{ifd + 2 + i * 12};
		entry value{u16(at + 2), u32(at + 4), at + 8};
		auto const size{(value.type == 3 ? 2u : value.type == 12 ? 8u : 4u) * value.count};
		if (size > 4)
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
	if (width < 2 || height < 2 || number(258, 0) != 32 || number(339, 0) != 3 || std::max(1u, number(259, 0)) != 1 || false == entries.contains(273) || false == entries.contains(33550) ||
	    false == entries.contains(33922))
		return false;
	auto const rows{std::max(1u, number(278, 0))};
	Out.width = width;
	Out.height = height;
	Out.heights.assign(static_cast<std::size_t>(width) * height, no_data - 1.f);
	auto const strips{entries[273].count};
	for (std::uint32_t strip = 0; strip < strips; ++strip)
	{
		auto const offset{static_cast<std::size_t>(number(273, strip))};
		auto const first{strip * rows};
		for (std::uint32_t row = first; row < std::min<std::uint32_t>(first + rows, height); ++row)
			for (int column = 0; column < width; ++column)
			{
				auto const bits{u32(offset + ((row - first) * width + column) * 4)};
				float value;
				std::memcpy(&value, &bits, sizeof(value));
				Out.heights[static_cast<std::size_t>(row) * width + column] = value;
			}
	}
	Out.pixel = real(33550, 0);
	Out.east = real(33922, 3) - real(33922, 0) * Out.pixel;
	Out.north = real(33922, 4) + real(33922, 1) * Out.pixel;
	return Out.pixel > 0.0;
}

// transverse Mercator on GRS80 (Krüger series to the third order of n, millimetres over the width of a zone): PUWG 1992 and
// the zones of PUWG 2000 share the ellipsoid and the datum, so a point goes from one to the other through its latitude and
// longitude, with no shift
struct mercator
{
	double meridian; // radians
	double scale;
	double false_east;
	double false_north;
};

double constexpr grs80_a{6378137.0};
double constexpr grs80_f{1.0 / 298.257222101};
double constexpr degree{3.14159265358979323846 / 180.0};

mercator system_of(int const Zone)
{
	if (Zone == 0)
		return {19.0 * degree, 0.9993, 500000.0, -5300000.0};
	return {3.0 * Zone * degree, 0.999923, Zone * 1000000.0 + 500000.0, 0.0};
}

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

// latitude and longitude (radians) to easting and northing
void to_grid(mercator const &System, double const Latitude, double const Longitude, double &East, double &North)
{
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
	North = System.false_north + System.scale * k.big_a * x;
	East = System.false_east + System.scale * k.big_a * y;
}

void to_geographic(mercator const &System, double const East, double const North, double &Latitude, double &Longitude)
{
	auto const &k{series()};
	auto const xi{(North - System.false_north) / (System.scale * k.big_a)};
	auto const eta{(East - System.false_east) / (System.scale * k.big_a)};
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

// a point from one system to the other (0: PUWG 1992, 5-8: zones of PUWG 2000)
void regrid(int const From, int const To, double const East, double const North, double &OutEast, double &OutNorth)
{
	if (From == To)
	{
		OutEast = East;
		OutNorth = North;
		return;
	}
	double latitude, longitude;
	to_geographic(system_of(From), East, North, latitude, longitude);
	to_grid(system_of(To), latitude, longitude, OutEast, OutNorth);
}

// paths are kept as UTF-8, as ImGui gives them
fs::path from_utf8(std::string const &Text)
{
	return fs::path(std::u8string(Text.begin(), Text.end()));
}

std::string to_utf8(fs::path const &Path)
{
	auto const text{Path.u8string()};
	return std::string(text.begin(), text.end());
}

std::string lowercase(std::string Text)
{
	std::transform(Text.begin(), Text.end(), Text.begin(), [](unsigned char const Letter) { return static_cast<char>(std::tolower(Letter)); });
	return Text;
}

// the header of an ESRI ASCII grid: the keys and the values, up to the first number of the grid. false if it isn't one.
// Body: where the numbers begin
bool read_asc_header(std::string const &Text, std::map<std::string, double> &Header, std::size_t &Body)
{
	std::size_t at{0};
	while (at < Text.size())
	{
		while (at < Text.size() && std::isspace(static_cast<unsigned char>(Text[at])))
			++at;
		if (at >= Text.size() || false == std::isalpha(static_cast<unsigned char>(Text[at])))
			break;
		auto const keyend{Text.find_first_of(" \t\r\n", at)};
		if (keyend == std::string::npos)
			return false;
		auto const key{lowercase(Text.substr(at, keyend - at))};
		at = keyend;
		while (at < Text.size() && (Text[at] == ' ' || Text[at] == '\t'))
			++at;
		auto const valueend{Text.find_first_of(" \t\r\n", at)};
		auto const value{Text.substr(at, valueend == std::string::npos ? std::string::npos : valueend - at)};
		double number{0.0};
		auto const parsed{std::from_chars(value.data(), value.data() + value.size(), number)};
		if (parsed.ec != std::errc{})
			return false;
		Header[key] = number;
		at = valueend == std::string::npos ? Text.size() : valueend;
	}
	Body = at;
	return Header.contains("ncols") && Header.contains("nrows") && Header.contains("cellsize") && (Header.contains("xllcorner") || Header.contains("xllcenter")) &&
	       (Header.contains("yllcorner") || Header.contains("yllcenter"));
}

std::size_t constexpr kept_sheets{8}; // sheets whose heights stay read, ~25 MB each at 1 m

} // namespace

editor_nmt::editor_nmt() = default;

editor_nmt::~editor_nmt()
{
	{
		std::lock_guard<std::mutex> lock{m_mutex};
		m_stop = true;
		m_queue.clear();
	}
	m_wake.notify_all();
	for (auto &worker : m_workers)
		if (worker.joinable())
			worker.join();
}

bool editor_nmt::can_download()
{
	return editor_http::available();
}

void editor_nmt::place(double const North, double const East)
{
	std::lock_guard<std::mutex> lock{m_mutex};
	m_north = North;
	m_east = East;
	// which system a sheet is in is told by where it lands against the scenery
	if (false == m_folder.empty() && (m_indexednorth != North || m_indexedeast != East))
	{
		index_sheets();
		m_queue.clear();
		m_queued.clear();
		m_squares.clear();
	}
}

void editor_nmt::local(std::string const &Folder, double const Correction)
{
	{
		std::lock_guard<std::mutex> lock{m_mutex};
		if (Folder == m_folder && Correction == m_correction)
			return;
		m_folder = Folder;
		m_correction = Correction;
		m_sheets.clear();
		m_localstatus.clear();
		if (false == m_folder.empty())
			index_sheets();
		m_queue.clear();
		m_queued.clear();
		m_squares.clear();
	}
	std::lock_guard<std::mutex> lock{m_sheetmutex};
	m_sheetheights.clear();
}

std::string editor_nmt::local_status() const
{
	std::lock_guard<std::mutex> lock{m_mutex};
	return m_localstatus;
}

// NOTE: called with m_mutex held
void editor_nmt::index_sheets()
{
	m_sheets.clear();
	m_indexednorth = m_north;
	m_indexedeast = m_east;
	std::error_code fault;
	if (false == fs::is_directory(from_utf8(m_folder), fault))
	{
		m_localstatus = STR("The folder isn't there");
		return;
	}
	std::vector<std::string> files;
	for (auto it{fs::recursive_directory_iterator(from_utf8(m_folder), fs::directory_options::skip_permission_denied, fault)}; it != fs::recursive_directory_iterator(); it.increment(fault))
	{
		if (fault)
			break;
		if (it->is_regular_file(fault) && lowercase(to_utf8(it->path().extension())) == ".asc")
			files.push_back(to_utf8(it->path()));
	}
	// where sheets overlap the one first by name is taken
	std::sort(files.begin(), files.end());
	bool const placed{m_north != 0.0 || m_east != 0.0};
	int counts[9]{};
	std::size_t skipped{0};
	for (auto const &file : files)
	{
		std::ifstream input{from_utf8(file), std::ios::binary};
		std::string text(4096, '\0');
		input.read(text.data(), static_cast<std::streamsize>(text.size()));
		text.resize(static_cast<std::size_t>(input.gcount()));
		std::map<std::string, double> header;
		std::size_t body;
		if (false == read_asc_header(text, header, body))
		{
			++skipped;
			WriteLog("Editor: NMT sheet " + file + " skipped: not an ESRI ASCII grid", logtype::generic);
			continue;
		}
		sheet item;
		item.file = file;
		item.cell = header["cellsize"];
		item.columns = static_cast<int>(header["ncols"]);
		item.rows = static_cast<int>(header["nrows"]);
		if (header.contains("nodata_value"))
			item.nodata = header["nodata_value"];
		auto const x{header.contains("xllcorner") ? header["xllcorner"] : header["xllcenter"] - 0.5 * item.cell};
		auto const y{header.contains("yllcorner") ? header["yllcorner"] : header["yllcenter"] - 0.5 * item.cell};
		if (item.cell <= 0.0 || item.columns < 2 || item.rows < 2)
		{
			++skipped;
			continue;
		}
		// the system and the order of the coordinates told apart: PUWG 1992 has both under a million, PUWG 2000 has the zone in
		// the millions of the easting; some tools write the northing first, as the surveyors do. of what fits, what lands
		// nearest the scenery
		struct guess
		{
			int zone;
			bool swapped;
			double east, north; // the middle in PUWG 1992
		};
		std::vector<guess> guesses;
		for (int swapped = 0; swapped < 2; ++swapped)
		{
			auto const east{swapped ? y : x};
			auto const north{swapped ? x : y};
			int zone{-1};
			if (east > 0.0 && east < 1000000.0 && north > 0.0 && north < 1000000.0)
				zone = 0;
			else if (auto const millions{static_cast<int>(std::floor(east / 1000000.0))}; millions >= 5 && millions <= 8 && north > 5000000.0 && north < 6300000.0)
				zone = millions;
			if (zone < 0)
				continue;
			double middleeast, middlenorth;
			regrid(zone, 0, east + 0.5 * item.columns * item.cell, north + 0.5 * item.rows * item.cell, middleeast, middlenorth);
			guesses.push_back({zone, swapped != 0, middleeast, middlenorth});
		}
		if (guesses.empty())
		{
			++skipped;
			WriteLog("Editor: NMT sheet " + file + " skipped: its coordinates are neither PUWG 1992 nor PUWG 2000", logtype::generic);
			continue;
		}
		auto best{guesses.front()};
		if (placed)
			for (auto const &other : guesses)
				if (std::hypot(other.east - m_east, other.north - m_north) < std::hypot(best.east - m_east, best.north - m_north))
					best = other;
		item.zone = best.zone;
		item.east = best.swapped ? y : x;
		item.south = best.swapped ? x : y;
		// the extent in PUWG 1992 from its corners and the middles of its edges; a zone of 2000 turns the grid a little
		item.low_east = item.low_north = std::numeric_limits<double>::max();
		item.high_east = item.high_north = std::numeric_limits<double>::lowest();
		for (int i = 0; i <= 2; ++i)
			for (int j = 0; j <= 2; ++j)
			{
				double e, n;
				regrid(item.zone, 0, item.east + 0.5 * i * item.columns * item.cell, item.south + 0.5 * j * item.rows * item.cell, e, n);
				item.low_east = std::min(item.low_east, e);
				item.high_east = std::max(item.high_east, e);
				item.low_north = std::min(item.low_north, n);
				item.high_north = std::max(item.high_north, n);
			}
		++counts[item.zone];
		m_sheets.push_back(std::move(item));
	}
	std::ostringstream status;
	status << STR("Sheets") << ": " << m_sheets.size() << " (PUWG 1992: " << counts[0];
	for (int zone = 5; zone <= 8; ++zone)
		if (counts[zone] > 0)
			status << ", PUWG 2000/" << zone << ": " << counts[zone];
	status << ")";
	if (skipped > 0)
		status << ", " << STR("skipped") << ": " << skipped;
	m_localstatus = status.str();
	WriteLog("Editor: NMT sheets in " + m_folder + " - " + m_localstatus, logtype::generic);
}

std::shared_ptr<std::vector<float> const> editor_nmt::sheet_heights(sheet const &Sheet)
{
	std::lock_guard<std::mutex> lock{m_sheetmutex};
	for (auto it{m_sheetheights.begin()}; it != m_sheetheights.end(); ++it)
		if (it->first == Sheet.file)
		{
			auto found{*it};
			m_sheetheights.erase(it);
			m_sheetheights.push_front(found);
			return found.second;
		}
	std::ifstream input{from_utf8(Sheet.file), std::ios::binary};
	if (false == input.is_open())
		return nullptr;
	std::string text{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
	std::map<std::string, double> header;
	std::size_t at;
	if (false == read_asc_header(text, header, at))
		return nullptr;
	auto heights{std::make_shared<std::vector<float>>(static_cast<std::size_t>(Sheet.columns) * Sheet.rows, no_data - 1.f)};
	auto const *cursor{text.data() + at};
	auto const *const end{text.data() + text.size()};
	for (auto &value : *heights)
	{
		while (cursor < end && (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n'))
			++cursor;
		if (cursor >= end)
			break;
		double number;
		auto const parsed{std::from_chars(cursor, end, number)};
		if (parsed.ec != std::errc{})
			break;
		cursor = parsed.ptr;
		if (std::abs(number - Sheet.nodata) > 1e-3)
			value = static_cast<float>(number);
	}
	m_sheetheights.emplace_front(Sheet.file, heights);
	while (m_sheetheights.size() > kept_sheets)
		m_sheetheights.pop_back();
	return heights;
}

std::shared_ptr<editor_nmt::square> editor_nmt::fetch_local(key const &Key, bool &Missing)
{
	std::vector<sheet> sheets;
	double correction;
	{
		std::lock_guard<std::mutex> lock{m_mutex};
		correction = m_correction;
		auto const low_east{Key.first * square_size - border}, low_north{Key.second * square_size - border};
		auto const high_east{low_east + square_size + 2 * border}, high_north{low_north + square_size + 2 * border};
		for (auto const &item : m_sheets)
			if (item.high_east > low_east && item.low_east < high_east && item.high_north > low_north && item.low_north < high_north)
				sheets.push_back(item);
	}
	Missing = true;
	if (sheets.empty())
		return nullptr;
	auto result{std::make_shared<square>()};
	auto const pixels{static_cast<int>(square_size) + 2 * border};
	result->width = result->height = pixels;
	result->pixel = 1.0;
	result->east = Key.first * square_size - border;
	result->north = Key.second * square_size - border + pixels;
	result->heights.assign(static_cast<std::size_t>(pixels) * pixels, no_data - 1.f);
	std::vector<char> filled(result->heights.size(), 0);
	bool any{false};
	for (auto const &item : sheets)
	{
		auto const heights{sheet_heights(item)};
		if (heights == nullptr)
			continue;
		// the place in the sheet worked out exactly on a coarse net and between its knots along straight lines: over 50 m the
		// turn of a zone of 2000 against 1992 bends nothing
		int constexpr knots{50};
		auto const net{pixels / knots + 2};
		std::vector<glm::dvec2> places(static_cast<std::size_t>(net) * net);
		for (int j = 0; j < net; ++j)
			for (int i = 0; i < net; ++i)
				regrid(0, item.zone, result->east + std::min(i * knots, pixels), result->north - std::min(j * knots, pixels), places[j * net + i].x, places[j * net + i].y);
		auto const top{item.south + item.rows * item.cell};
		auto const at = [&](int const C, int const R) { return (*heights)[static_cast<std::size_t>(R) * item.columns + C]; };
		for (int row = 0; row < pixels; ++row)
			for (int column = 0; column < pixels; ++column)
			{
				auto const index{static_cast<std::size_t>(row) * pixels + column};
				if (filled[index])
					continue;
				// the middle of the pixel on the net
				auto const u{(column + 0.5) / knots}, v{(row + 0.5) / knots};
				auto const i{std::min(static_cast<int>(u), net - 2)}, j{std::min(static_cast<int>(v), net - 2)};
				// the knots lie every 50 px but the last one at the edge
				auto const spanu{std::min((i + 1) * knots, pixels) - i * knots}, spanv{std::min((j + 1) * knots, pixels) - j * knots};
				auto const fu{(column + 0.5 - i * knots) / spanu}, fv{(row + 0.5 - j * knots) / spanv};
				auto const p{(places[j * net + i] * (1.0 - fu) + places[j * net + i + 1] * fu) * (1.0 - fv) + (places[(j + 1) * net + i] * (1.0 - fu) + places[(j + 1) * net + i + 1] * fu) * fv};
				auto const c{(p.x - item.east) / item.cell - 0.5};
				auto const r{(top - p.y) / item.cell - 0.5};
				if (c < -0.5 || r < -0.5 || c > item.columns - 0.5 || r > item.rows - 0.5)
					continue;
				auto const c0{std::clamp(static_cast<int>(std::floor(c)), 0, item.columns - 2)};
				auto const r0{std::clamp(static_cast<int>(std::floor(r)), 0, item.rows - 2)};
				auto const fc{std::clamp(c - c0, 0.0, 1.0)}, fr{std::clamp(r - r0, 0.0, 1.0)};
				float const corners[4]{at(c0, r0), at(c0 + 1, r0), at(c0, r0 + 1), at(c0 + 1, r0 + 1)};
				float value;
				if (std::all_of(std::begin(corners), std::end(corners), [](float const Value) { return Value > no_data; }))
					value = static_cast<float>((corners[0] * (1.0 - fc) + corners[1] * fc) * (1.0 - fr) + (corners[2] * (1.0 - fc) + corners[3] * fc) * fr);
				else
				{
					value = corners[(fr > 0.5 ? 2 : 0) + (fc > 0.5 ? 1 : 0)];
					if (value <= no_data)
						continue;
				}
				result->heights[index] = static_cast<float>(value + correction);
				filled[index] = 1;
				any = true;
			}
	}
	if (false == any)
		return nullptr;
	Missing = std::find(filled.begin(), filled.end(), 0) != filled.end();
	return result;
}

bool editor_nmt::placed() const
{
	std::lock_guard<std::mutex> lock{m_mutex};
	return m_north != 0.0 || m_east != 0.0;
}

editor_nmt::key editor_nmt::key_of(double const East, double const North) const
{
	return {static_cast<int>(std::floor(East / square_size)), static_cast<int>(std::floor(North / square_size))};
}

bool editor_nmt::height(double const X, double const Z, double &Height) const
{
	std::shared_ptr<square const> found;
	double east, north;
	{
		std::lock_guard<std::mutex> lock{m_mutex};
		east = m_east - X;
		north = m_north + Z;
		auto const it{m_squares.find(key_of(east, north))};
		if (it == m_squares.end())
			return false;
		found = it->second;
	}
	auto const &grid{*found};
	if (grid.failed)
		return false;
	// the values lie in the middles of the pixels
	auto const column{(east - grid.east) / grid.pixel - 0.5};
	auto const row{(grid.north - north) / grid.pixel - 0.5};
	auto const c0{std::clamp(static_cast<int>(std::floor(column)), 0, grid.width - 2)};
	auto const r0{std::clamp(static_cast<int>(std::floor(row)), 0, grid.height - 2)};
	auto const fc{std::clamp(column - c0, 0.0, 1.0)};
	auto const fr{std::clamp(row - r0, 0.0, 1.0)};
	auto const at = [&](int const C, int const R) { return grid.heights[static_cast<std::size_t>(R) * grid.width + C]; };
	float const corners[4]{at(c0, r0), at(c0 + 1, r0), at(c0, r0 + 1), at(c0 + 1, r0 + 1)};
	if (std::all_of(std::begin(corners), std::end(corners), [](float const Value) { return std::isfinite(Value) && Value > no_data; }))
	{
		Height = (corners[0] * (1.0 - fc) + corners[1] * fc) * (1.0 - fr) + (corners[2] * (1.0 - fc) + corners[3] * fc) * fr;
		return true;
	}
	// at the edge of the data: the nearest value there is
	auto const nearest{corners[(fr > 0.5 ? 2 : 0) + (fc > 0.5 ? 1 : 0)]};
	if (false == std::isfinite(nearest) || nearest <= no_data)
		return false;
	Height = nearest;
	return true;
}

bool editor_nmt::request(glm::dvec2 const &Min, glm::dvec2 const &Max)
{
	std::vector<key> wanted;
	bool all{true};
	{
		std::lock_guard<std::mutex> lock{m_mutex};
		auto const low{key_of(m_east - Max.x, m_north + Min.y)};
		auto const high{key_of(m_east - Min.x, m_north + Max.y)};
		auto const middle{key_of(m_east - 0.5 * (Min.x + Max.x), m_north + 0.5 * (Min.y + Max.y))};
		for (auto e = low.first; e <= high.first; ++e)
			for (auto n = low.second; n <= high.second; ++n)
			{
				key const square{e, n};
				if (m_squares.contains(square))
					continue;
				all = false;
				if (false == m_queued.contains(square))
					wanted.push_back(square);
			}
		std::sort(wanted.begin(), wanted.end(), [&](key const &Left, key const &Right) {
			auto const distance = [&](key const &Square) { return std::abs(Square.first - middle.first) + std::abs(Square.second - middle.second); };
			return distance(Left) < distance(Right);
		});
		for (auto const &square : wanted)
		{
			m_queued.insert(square);
			m_queue.push_back(square);
		}
		if (false == wanted.empty() && m_workers.empty())
			for (int i = 0; i < worker_count; ++i)
				m_workers.emplace_back([this]() { work(); });
	}
	if (false == wanted.empty())
		m_wake.notify_all();
	return all;
}

std::size_t editor_nmt::waiting() const
{
	std::lock_guard<std::mutex> lock{m_mutex};
	return m_queued.size();
}

std::string editor_nmt::error() const
{
	std::lock_guard<std::mutex> lock{m_mutex};
	return m_error;
}

void editor_nmt::clear()
{
	std::lock_guard<std::mutex> lock{m_mutex};
	m_queue.clear();
	m_queued.clear();
	m_squares.clear();
}

void editor_nmt::work()
{
	while (true)
	{
		key square;
		{
			std::unique_lock<std::mutex> lock{m_mutex};
			m_wake.wait(lock, [this]() { return m_stop || false == m_queue.empty(); });
			if (m_stop)
				return;
			square = m_queue.front();
			m_queue.pop_front();
		}
		std::string error;
		auto fetched{fetch(square, error)};
		{
			std::lock_guard<std::mutex> lock{m_mutex};
			if (false == m_queued.contains(square))
				continue; // cleared meanwhile
			m_queued.erase(square);
			m_squares[square] = std::move(fetched);
			m_error = error;
		}
	}
}

std::shared_ptr<editor_nmt::square const> editor_nmt::fetch(key const &Key, std::string &Error)
{
	bool folder;
	{
		std::lock_guard<std::mutex> lock{m_mutex};
		folder = false == m_sheets.empty();
	}
	if (false == folder)
		return fetch_service(Key, Error);
	bool missing;
	auto local{fetch_local(Key, missing)};
	if (local == nullptr)
		return fetch_service(Key, Error);
	if (missing)
	{
		// the edge of the sheets: the rest of the square from the service
		auto const service{fetch_service(Key, Error)};
		if (false == service->failed && service->width == local->width && service->height == local->height && std::abs(service->east - local->east) < 0.01 && std::abs(service->north - local->north) < 0.01)
			for (std::size_t i = 0; i < local->heights.size(); ++i)
				if (local->heights[i] <= no_data)
					local->heights[i] = service->heights[i];
		Error.clear();
	}
	return local;
}

std::shared_ptr<editor_nmt::square const> editor_nmt::fetch_service(key const &Key, std::string &Error)
{
	auto result{std::make_shared<square>()};
	std::error_code fault;
	fs::create_directories(cache_folder, fault);
	auto const file{std::string{cache_folder} + std::to_string(Key.first) + "_" + std::to_string(Key.second) + ".tif"};
	std::vector<std::uint8_t> data;
	if (std::ifstream cached{file, std::ios::binary}; cached.is_open())
	{
		data.assign(std::istreambuf_iterator<char>(cached), std::istreambuf_iterator<char>());
		if (read_geotiff(data, *result))
			return result;
		// damaged by a fetch broken off: fetched again
		fs::remove(file, fault);
	}
	if (false == can_download())
	{
		Error = STRN("This build can't fetch from the internet");
		result->failed = true;
		return result;
	}
	editor_http::client client;
	auto const stop = [this]() {
		std::lock_guard<std::mutex> lock{m_mutex};
		return m_stop;
	};
	std::string coverage;
	{
		std::lock_guard<std::mutex> lock{m_mutex};
		coverage = m_coverage;
	}
	if (coverage.empty())
	{
		std::vector<std::uint8_t> capabilities;
		std::string unused;
		coverage = client.get(std::string{service} + "?SERVICE=WCS&VERSION=1.0.0&REQUEST=GetCapabilities", capabilities, unused, stop) ?
		               coverage_of(std::string(capabilities.begin(), capabilities.end())) :
		               fallback_coverage;
		std::lock_guard<std::mutex> lock{m_mutex};
		m_coverage = coverage;
	}
	// the square with a pixel of each neighbour around it
	auto const pixels{static_cast<int>(square_size) + 2 * border};
	auto const east{Key.first * square_size - border};
	auto const north{Key.second * square_size - border};
	char bbox[128];
	std::snprintf(bbox, sizeof(bbox), "%.0f,%.0f,%.0f,%.0f", east, north, east + pixels, north + pixels);
	auto const url{std::string{service} + "?SERVICE=WCS&VERSION=1.0.0&REQUEST=GetCoverage&COVERAGE=" + coverage + "&CRS=EPSG:2180&RESPONSE_CRS=EPSG:2180&BBOX=" + bbox +
	               "&WIDTH=" + std::to_string(pixels) + "&HEIGHT=" + std::to_string(pixels) + "&FORMAT=GEOTIFF"};
	for (int attempt = 0; attempt < 3; ++attempt)
	{
		if (client.get(url, data, Error, stop) && read_geotiff(data, *result))
		{
			if (std::ofstream out{file, std::ios::binary}; out.is_open())
				out.write(reinterpret_cast<char const *>(data.data()), static_cast<std::streamsize>(data.size()));
			Error.clear();
			return result;
		}
		if (stop())
			break;
		if (Error.empty())
			Error = STRN("The service gave no terrain model");
	}
	WriteLog("Editor: NMT square " + std::to_string(Key.first) + "," + std::to_string(Key.second) + " not fetched: " + Error, logtype::generic);
	result->failed = true;
	return result;
}
