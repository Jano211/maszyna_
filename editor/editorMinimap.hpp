/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include <glm/glm.hpp>

#include "interfaces/ITexture.h" // texture_handle

struct ImDrawList;
struct ImVec2;

// the map layers of editor_foreign (OpenStreetMap, topographic maps and the like, from editor/foreign_sources.txt) drawn under the
// tracks of the minimap of the editor, north up. The tiles are squares of the PUWG 1992 grid, 256 px each, 256 m * 2^level on
// a side, fetched in the background
class editor_minimap
{
  public:
	// what's in view: the origin of the scenery in PUWG 1992, the corners (x, z) of the part of the scenery in view, the metres a
	// pixel, and the point of the window of a point (x, z) of the scenery
	struct view
	{
		double north{0.0};
		double east{0.0};
		glm::dvec2 low{0.0};
		glm::dvec2 high{0.0};
		double scale{4.0};
		std::function<ImVec2(glm::dvec2 const &)> screen;
	};

	editor_minimap() = default;
	~editor_minimap();
	editor_minimap(editor_minimap const &) = delete;
	editor_minimap &operator=(editor_minimap const &) = delete;

	// the switch and the layer, above the map; true when it's to be drawn. Placed: the scenery has its place in PUWG 1992
	bool controls(bool const Placed);
	// under the tracks
	void draw(ImDrawList &Drawlist, view const &View);
	// whose the map is, or how many of its tiles are still coming
	std::string status();
	// the tiles out of the video memory and the downloads waiting dropped (when the window is closed)
	void release();

  private:
	using key = std::tuple<int, int, int>; // level, easting index, northing index
	struct tile
	{
		texture_handle texture{null_handle};
		int slot{-1};
		int state{0}; // 0 nothing, 1 asked for, 2 shown, 3 nothing there
		std::uint64_t used{0};
		std::uint64_t retry{0}; // the frame it may be asked for again after it failed
	};
	struct result
	{
		key where;
		std::uint32_t generation{0};
		bool ok{false};
		std::vector<std::uint8_t> rgba;
	};

	static int constexpr pixels{256};
	static int constexpr max_level{10};

	void worker();
	void start();
	void clear();
	tile *ready(key const &Key);
	void upload();
	void draw_tile(ImDrawList &Drawlist, key const &Where, std::function<ImVec2(double, double)> const &Screen, std::vector<key> &Wanted);
	void ask(std::vector<key> &Wanted, double const Centre_x, double const Centre_y);

	bool m_on{false};
	std::string m_layer;
	std::string m_credit;
	std::uint64_t m_frame{0};
	std::map<key, tile> m_tiles;
	std::vector<std::pair<int, texture_handle>> m_free; // textures of the tiles dropped, given to the next ones
	int m_textures{0};
	std::string m_error;

	std::mutex m_mutex;
	std::condition_variable m_wake;
	std::deque<std::pair<key, std::string>> m_queue; // the newest first
	std::deque<result> m_done;
	std::atomic<std::uint32_t> m_generation{1};
	std::atomic<bool> m_stop{false};
	std::vector<std::thread> m_workers;
};
