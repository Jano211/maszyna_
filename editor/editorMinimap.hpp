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
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include <glm/glm.hpp>

#include "interfaces/ITexture.h" // texture_handle

// a 2D map in a window of the editor: the map layers of editor_foreign (OpenStreetMap, topographic maps and the like, from
// editor/foreign_sources.txt) around the camera, dragged and zoomed with the mouse; a double click takes the camera there.
// the tiles are squares of the PUWG 1992 grid, 256 px each, 256 m * 2^level on a side, fetched in the background
class editor_minimap
{
  public:
	struct camera
	{
		double north{0.0}, east{0.0}; // PUWG 1992 of the point (0,0,0) of the scenery
		double x{0.0}, z{0.0};         // where the camera is in the scenery
		float yaw{0.0f};               // Camera.Angle.y
	};

	editor_minimap() = default;
	~editor_minimap();
	editor_minimap(editor_minimap const &) = delete;
	editor_minimap &operator=(editor_minimap const &) = delete;

	// the contents of the window, inside ImGui::Begin; true with Target (x, z of the scenery) when the camera is to go there
	bool ui(camera const &Camera, glm::dvec2 &Target);
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

	std::string m_layer;
	double m_scale{4.0}; // metres per pixel of the screen
	bool m_follow{true};
	double m_north{0.0}, m_east{0.0}; // the middle of the map when it doesn't follow the camera
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
