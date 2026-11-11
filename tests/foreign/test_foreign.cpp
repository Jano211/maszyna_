// checks of editor/editorForeign.cpp off the simulator: the systems of coordinates, reading GeoTIFFs and filling the terrain and
// the imagery from sources served by a fake http client. build (from the root of the repository):
//   g++ -std=c++20 -O1 -I. -DEDITOR_FOREIGN_STANDALONE tests/foreign/test_foreign.cpp editor/editorForeign.cpp stb/stb_image.c -o test_foreign
// and run it in an empty folder with the path of the sample file: ./test_foreign <repository>/docs/foreign_sources.txt
// (it copies it to editor/foreign_sources.txt there and keeps the cache next to it)

#include "editor/editorForeign.hpp"
#include "stb/stb_image_write.h"

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

struct editor_http::client::platform
{
};
editor_http::client::client() = default;
editor_http::client::~client() = default;
bool editor_http::available()
{
	return true;
}

namespace
{

int failures{0};

void check(bool const Condition, std::string const &What)
{
	std::printf("%s %s\n", Condition ? "ok  " : "FAIL", What.c_str());
	if (false == Condition)
		++failures;
}

std::string parameter(std::string const &Url, std::string const &Key)
{
	auto at{Url.find(Key + "=")};
	if (at == std::string::npos)
		return {};
	at += Key.size() + 1;
	return Url.substr(at, Url.find('&', at) - at);
}

// the plane of the fake Slovak terrain: in ETRS-TM34, only south of the border at 5460000
double slovak_height(double const X, double const Y)
{
	return 300.0 + 0.01 * (X - 565000.0) + 0.02 * (Y - 5450000.0);
}

// a big-endian GeoTIFF of floats in 16 x 16 tiles, placed by a transformation, nodata as GDAL writes it
std::vector<std::uint8_t> geotiff(double const MinX, double const MinY, double const MaxX, double const MaxY, int const Width, int const Height)
{
	std::vector<std::uint8_t> out;
	auto const put16 = [&](std::uint32_t const V) {
		out.push_back(V >> 8);
		out.push_back(V & 255);
	};
	auto const put32 = [&](std::uint32_t const V) {
		for (int i = 3; i >= 0; --i)
			out.push_back((V >> (8 * i)) & 255);
	};
	auto const put64 = [&](double const V) {
		std::uint64_t bits;
		std::memcpy(&bits, &V, 8);
		for (int i = 7; i >= 0; --i)
			out.push_back((bits >> (8 * i)) & 255);
	};
	int constexpr tile{16};
	auto const across{(Width + tile - 1) / tile}, down{(Height + tile - 1) / tile};
	auto const tiles{across * down};
	std::string const nodata{"3.4E38"};
	// header, then the data, the arrays and the directory
	out = {'M', 'M', 0, 42, 0, 0, 0, 0};
	std::vector<std::uint32_t> offsets;
	auto const step_x{(MaxX - MinX) / Width}, step_y{(MaxY - MinY) / Height};
	for (int t = 0; t < tiles; ++t)
	{
		offsets.push_back(static_cast<std::uint32_t>(out.size()));
		auto const left{(t % across) * tile}, top{(t / across) * tile};
		for (int r = 0; r < tile; ++r)
			for (int c = 0; c < tile; ++c)
			{
				auto const x{MinX + (left + c + 0.5) * step_x}, y{MaxY - (top + r + 0.5) * step_y};
				float const value{y < 5460000.0 ? static_cast<float>(slovak_height(x, y)) : 3.4e38f};
				std::uint32_t bits;
				std::memcpy(&bits, &value, 4);
				put32(bits);
			}
	}
	auto const offsets_at{static_cast<std::uint32_t>(out.size())};
	for (auto const o : offsets)
		put32(o);
	auto const counts_at{static_cast<std::uint32_t>(out.size())};
	for (int t = 0; t < tiles; ++t)
		put32(tile * tile * 4);
	auto const matrix_at{static_cast<std::uint32_t>(out.size())};
	double const matrix[16]{step_x, 0, 0, MinX, 0, -step_y, 0, MaxY, 0, 0, 0, 0, 0, 0, 0, 1};
	for (auto const m : matrix)
		put64(m);
	auto const nodata_at{static_cast<std::uint32_t>(out.size())};
	for (auto const ch : nodata)
		out.push_back(ch);
	out.push_back(0);
	if (out.size() % 2)
		out.push_back(0);
	auto const ifd{static_cast<std::uint32_t>(out.size())};
	out[4] = ifd >> 24;
	out[5] = (ifd >> 16) & 255;
	out[6] = (ifd >> 8) & 255;
	out[7] = ifd & 255;
	struct tag
	{
		std::uint16_t id, type;
		std::uint32_t count, value;
		bool short_value;
	};
	std::vector<tag> tags{{256, 3, 1, static_cast<std::uint32_t>(Width), true},
	                      {257, 3, 1, static_cast<std::uint32_t>(Height), true},
	                      {258, 3, 1, 32, true},
	                      {259, 3, 1, 1, true},
	                      {277, 3, 1, 1, true},
	                      {322, 3, 1, tile, true},
	                      {323, 3, 1, tile, true},
	                      {324, 4, static_cast<std::uint32_t>(tiles), offsets_at, false},
	                      {325, 4, static_cast<std::uint32_t>(tiles), counts_at, false},
	                      {339, 3, 1, 3, true},
	                      {34264, 12, 16, matrix_at, false},
	                      {42113, 2, static_cast<std::uint32_t>(nodata.size() + 1), nodata_at, false}};
	if (tiles == 1)
	{
		tags[7].value = offsets[0];
		tags[8].value = tile * tile * 4;
	}
	put16(static_cast<std::uint32_t>(tags.size()));
	for (auto const &t : tags)
	{
		put16(t.id);
		put16(t.type);
		put32(t.count);
		if (t.short_value)
		{
			put16(t.value);
			put16(0);
		}
		else
			put32(t.value);
	}
	put32(0);
	return out;
}

void write_to(void *Context, void *Data, int Size)
{
	auto &out{*static_cast<std::vector<std::uint8_t> *>(Context)};
	out.insert(out.end(), static_cast<std::uint8_t *>(Data), static_cast<std::uint8_t *>(Data) + Size);
}

std::vector<std::uint8_t> png(int const Width, int const Height, std::vector<std::uint8_t> const &Rgba)
{
	std::vector<std::uint8_t> out;
	stbi_write_png_to_func(write_to, &out, Width, Height, 4, Rgba.data(), Width * 4);
	return out;
}

int requests{0};

} // namespace

bool editor_http::client::get(std::string const &Url, std::vector<std::uint8_t> &Out, std::string &Error, stop_check const &)
{
	++requests;
	if (std::getenv("TRACE"))
		std::printf("  GET %s\n", Url.c_str());
	if (Url.find("GetCoverage") != std::string::npos)
	{
		double box[4];
		std::sscanf(parameter(Url, "BBOX").c_str(), "%lf,%lf,%lf,%lf", &box[0], &box[1], &box[2], &box[3]);
		Out = geotiff(box[0], box[1], box[2], box[3], std::stoi(parameter(Url, "WIDTH")), std::stoi(parameter(Url, "HEIGHT")));
		return true;
	}
	if (Url.find("GetMap") != std::string::npos)
	{
		// red south of the border, transparent north of it
		double box[4];
		std::sscanf(parameter(Url, "BBOX").c_str(), "%lf,%lf,%lf,%lf", &box[0], &box[1], &box[2], &box[3]);
		auto const width{std::stoi(parameter(Url, "WIDTH"))}, height{std::stoi(parameter(Url, "HEIGHT"))};
		std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width) * height * 4, 0);
		for (int r = 0; r < height; ++r)
			for (int c = 0; c < width; ++c)
				if (box[3] - (r + 0.5) * (box[3] - box[1]) / height < 5460000.0)
				{
					auto *p{&rgba[(static_cast<std::size_t>(r) * width + c) * 4]};
					p[0] = 200;
					p[3] = 255;
				}
		Out = png(width, height, rgba);
		return true;
	}
	if (Url.find("terrarium") != std::string::npos)
	{
		// 250 m everywhere
		std::vector<std::uint8_t> rgba(256 * 256 * 4, 255);
		auto const coded{250.0 + 32768.0};
		for (std::size_t i = 0; i < 256 * 256; ++i)
		{
			rgba[i * 4] = static_cast<std::uint8_t>(static_cast<int>(coded) / 256);
			rgba[i * 4 + 1] = static_cast<std::uint8_t>(static_cast<int>(coded) % 256);
			rgba[i * 4 + 2] = 0;
		}
		Out = png(256, 256, rgba);
		return true;
	}
	if (Url.find("arcgisonline") != std::string::npos || Url.find("eox") != std::string::npos || Url.find("openstreetmap") != std::string::npos)
	{
		// blue everywhere
		std::vector<std::uint8_t> rgba(256 * 256 * 4, 0);
		for (std::size_t i = 0; i < 256 * 256; ++i)
		{
			rgba[i * 4 + 2] = 200;
			rgba[i * 4 + 3] = 255;
		}
		Out = png(256, 256, rgba);
		return true;
	}
	Error = "HTTP 404";
	return false;
}

int main(int Count, char **Arguments)
{
	using namespace editor_foreign;
	// the systems
	double x, y, e, n;
	from_puwg(4326, 0, 0, x, y);
	check(to_puwg(4326, 19.0, 52.0, e, n) && std::abs(e - 500000.0) < 0.001 && std::abs(n - 459309.209) < 0.01, "PUWG 1992 of 52N 19E: " + std::to_string(e) + " " + std::to_string(n));
	check(to_puwg(4326, 21.0, 49.0, e, n) && from_puwg(3046, e, n, x, y) && std::abs(x - 500000.0) < 0.001 && std::abs(y - 5427455.781) < 0.01,
	      "ETRS-TM34 of 49N 21E: " + std::to_string(x) + " " + std::to_string(y));
	double worst{0.0};
	for (int code : {2176, 2177, 2178, 2179, 3046, 25834, 3067, 3346, 3857, 4326})
		for (double lon = 14.0; lon <= 32.0; lon += 3.0)
			for (double lat = 47.0; lat <= 69.0; lat += 4.0)
			{
				to_puwg(4326, lon, lat, e, n);
				double a, b, e2, n2;
				from_puwg(code, e, n, a, b);
				to_puwg(code, a, b, e2, n2);
				worst = std::max(worst, std::hypot(e2 - e, n2 - n));
			}
	check(worst < 0.01, "round trips through the systems, worst " + std::to_string(worst) + " m");
	check(false == known_system(5514), "Krovak isn't taken");
	// without the file nothing but geoportal.gov.pl
	std::filesystem::remove_all("editor");
	check(false == reaches(kind::ortho, {500000, 500000, 500256, 500256}) && layers().empty() && false == std::filesystem::exists("editor"), "no file, no other sources: " + status());
	// the sample file
	if (Count < 2)
	{
		std::printf("FAIL the path of docs/foreign_sources.txt not given\n");
		return 1;
	}
	std::filesystem::create_directories("editor");
	std::filesystem::copy_file(Arguments[1], "editor/foreign_sources.txt", std::filesystem::copy_options::overwrite_existing);
	reload();
	check(sources_file() == "editor/foreign_sources.txt", "the file of the sources");
	check(status().rfind("7 imagery, 4 terrain, 5 map layers", 0) == 0, "the sample read: " + status());
	check(credits(kind::terrain) == "ÚGKK SR, ČÚZK, GeoBasis-DE/LGB, Mapzen terrain tiles", "credits: " + credits(kind::terrain));
	// a square across the Slovak border near Medzilaborce: in ETRS-TM34 the border of the fake data runs at 5460000
	to_puwg(3046, 566000.0, 5460000.0, e, n);
	auto const west{std::floor(e / 500.0) * 500.0}, north{std::floor(n / 500.0) * 500.0 + 500.0};
	check(reaches(kind::terrain, {west, north - 500.0, west + 500.0, north}), "the Slovak terrain reaches the square");
	int const size{502};
	float const nodata{-1000.f};
	// the Polish data: north of the border, 0.7 m above the Slovak plane (as if in another datum)
	std::vector<float> heights(static_cast<std::size_t>(size) * size, nodata - 1.f);
	for (int r = 0; r < size; ++r)
		for (int c = 0; c < size; ++c)
		{
			double px, py;
			from_puwg(3046, west + c + 0.5, north - r - 0.5, px, py);
			if (py >= 5460000.0)
				heights[static_cast<std::size_t>(r) * size + c] = static_cast<float>(slovak_height(px, py) + 0.7);
		}
	auto const polish{heights};
	editor_http::client client;
	std::string error;
	check(terrain(client, west, north, 1.0, size, size, heights, nodata, error, []() { return false; }), "the terrain filled " + error);
	double worst_step{0.0}, worst_far{0.0}, worst_kept{0.0};
	int holes{0};
	for (int r = 1; r < size - 1; ++r)
		for (int c = 0; c < size; ++c)
		{
			auto const i{static_cast<std::size_t>(r) * size + c};
			if (heights[i] <= nodata)
			{
				++holes;
				continue;
			}
			if (polish[i] > nodata)
				worst_kept = std::max(worst_kept, static_cast<double>(std::abs(heights[i] - polish[i])));
			// the step between the rows, less the slope of the plane
			auto const step{std::abs((heights[i] - heights[i - size]) - (polish[i] > nodata && polish[i - size] > nodata ? polish[i] - polish[i - size] : heights[i] - heights[i - size]))};
			double px, py;
			from_puwg(3046, west + c + 0.5, north - r - 0.5, px, py);
			if (py < 5460000.0 - 60.0)
				worst_far = std::max(worst_far, std::abs(heights[i] - (slovak_height(px, py) + 0.7)));
			(void)step;
			if (polish[i] <= nodata && polish[i - size] > nodata)
			{
				double qx, qy;
				from_puwg(3046, west + c + 0.5, north - r + 0.5, qx, qy);
				auto const expected{slovak_height(px, py) - slovak_height(qx, qy)};
				auto const error_here{std::abs((heights[i] - heights[i - size]) - expected)};
				if (error_here > 0.05 && std::getenv("TRACE"))
					std::printf("  step at %d,%d: %f above %f, plane %f above %f\n", c, r, heights[i], heights[i - size], slovak_height(px, py), slovak_height(qx, qy));
				worst_step = std::max(worst_step, error_here);
			}
		}
	check(holes == 0, "no holes left: " + std::to_string(holes));
	check(worst_kept < 1e-4, "the Polish heights kept: " + std::to_string(worst_kept));
	check(worst_step < 0.05, "no step at the border: " + std::to_string(worst_step) + " m");
	check(worst_far < 0.05, "away from it the shift of the edge: " + std::to_string(worst_far) + " m");
	// all of it cached: asked again, nothing goes to the service
	auto const before{requests};
	auto again{polish};
	terrain(client, west, north, 1.0, size, size, again, nodata, error, []() { return false; });
	check(requests == before, "the second time from the cache");
	// far in Ukraine only the world tiles: 250 m + the shift seen in Slovakia (0.7)... no: the terrarium has its own
	std::vector<float> ukraine;
	to_puwg(4326, 23.2, 49.5, e, n);
	check(terrain(client, e, n, 2.0, 100, 100, ukraine, nodata, error, []() { return false; }) && std::abs(ukraine[5050] - 250.f) < 0.01f,
	      "Ukraine from the terrain tiles: " + std::to_string(ukraine.empty() ? 0.f : ukraine[5050]));
	// the imagery: the top half Polish green, the bottom the Slovak red, the world blue nowhere (all covered)
	int const pixels{256};
	std::vector<std::uint8_t> rgba(static_cast<std::size_t>(pixels) * pixels * 4, 0);
	to_puwg(3046, 566000.0, 5460000.0, e, n);
	area const tile{e - 128.0, n - 128.0, e + 128.0, n + 128.0};
	for (int r = 0; r < pixels; ++r)
		for (int c = 0; c < pixels; ++c)
		{
			double px, py;
			from_puwg(3046, tile.west + c + 0.5, tile.north - r - 0.5, px, py);
			if (py >= 5460000.0)
			{
				auto *p{&rgba[(static_cast<std::size_t>(r) * pixels + c) * 4]};
				p[1] = 200;
				p[3] = 255;
			}
		}
	check(ortho(client, tile, pixels, rgba, "editor/cache/ortho", error, []() { return false; }), "the imagery filled " + error);
	auto const pixel = [&](int const C, int const R) { return &rgba[(static_cast<std::size_t>(R) * pixels + C) * 4]; };
	check(pixel(128, 5)[1] == 200 && pixel(128, 5)[0] == 0, "Polish at the top");
	check(pixel(128, 250)[0] == 200 && pixel(128, 250)[1] == 0 && pixel(128, 250)[3] == 255, "Slovak at the bottom");
	// next to the border on the Polish side: the Slovak has nothing there, so it isn't faded into the world imagery
	int last_polish{0};
	for (int r = 0; r < pixels; ++r)
		if (pixel(128, r)[1] == 200)
			last_polish = r;
	check(last_polish > 0 && pixel(128, last_polish)[2] == 0 && pixel(128, last_polish - 3)[2] == 0, "the Polish edge not faded into the world imagery");
	bool opaque{true};
	for (std::size_t i = 3; i < rgba.size(); i += 4)
		opaque = opaque && rgba[i] == 255;
	check(opaque, "nothing transparent left");
	// the map layers: the defaults are there, one of them gives the whole tile
	{
		auto const list{layers()};
		check(list.size() == 5 && list[3].name == "topo_pl" && list[4].name == "isok" && list[1].name == "osm" && list[4].title == "Cieniowanie terenu (ISOK)", "five map layers, osm second, ISOK last");
		check(list[1].tile == 4 && list[0].tile == 1, "OSM with tiles 4 times larger");
		std::vector<std::uint8_t> map;
		check(layer(client, "osm", tile, 512, map, "editor/cache/ortho", error, []() { return false; }) && map.size() == 512u * 512u * 4u && map[2] == 200 && map[3] == 255,
		      "the OSM layer filled the tile");
		check(false == layer(client, "nothing", tile, 64, map, "editor/cache/ortho", error, []() { return false; }), "an unknown layer refused");
	}
	// black where there's no imagery: a solid black half with a jpeg-like fringe goes, a dark shadow of 3 x 3 px stays
	{
		int const w{64};
		std::vector<std::uint8_t> picture(static_cast<std::size_t>(w) * w * 4, 255);
		for (int r = 0; r < w; ++r)
			for (int c = 0; c < w; ++c)
			{
				auto *p{&picture[(static_cast<std::size_t>(r) * w + c) * 4]};
				std::uint8_t value{120};
				if (c >= 32)
					value = 3; // the black half
				else if (c >= 30)
					value = 35; // its fringe
				else if (r >= 10 && r < 13 && c >= 10 && c < 13)
					value = 2; // a shadow
				p[0] = p[1] = p[2] = value;
				p[3] = 255;
			}
		auto const cleared{clear_black(picture, w, w)};
		auto const alpha = [&](int const C, int const R) { return picture[(static_cast<std::size_t>(R) * w + C) * 4 + 3]; };
		check(alpha(50, 30) == 0 && alpha(32, 0) == 0 && alpha(63, 63) == 0, "the black half cleared");
		check(alpha(31, 30) == 0 && alpha(30, 30) == 0, "its fringe with it");
		check(alpha(11, 11) == 255 && alpha(20, 30) == 255 && alpha(29, 30) == 255, "the shadow and the imagery stay");
		check(cleared == 34u * w, "cleared " + std::to_string(cleared));
	}
	std::printf("%s\n", failures ? "FAILED" : "all passed");
	return failures ? 1 : 0;
}
