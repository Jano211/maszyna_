/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "stdafx.h"
#include "editor/editorMinimap.hpp"
#include "editor/editorForeign.hpp"
#include "editor/editorHttp.hpp"
#include "editor/editorOrthophoto.hpp"

#include "rendering/renderer.h"
#include "utilities/translation.h"
#include "imgui/imgui.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace
{

int constexpr worker_count{2};
std::size_t constexpr kept_tiles{400};  // tiles held in the video memory, 256 KB each
std::size_t constexpr max_queue{256};   // the oldest requests dropped past it
int constexpr uploads_per_frame{8};

std::string slot_name(int const Index)
{
	// generated, no file behind it: the content comes through update_from_memory()
	return "internal_src:editor_minimap_" + std::to_string(Index);
}

double side_of(int const Level)
{
	return 256.0 * std::ldexp(1.0, Level);
}

} // namespace

editor_minimap::~editor_minimap()
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

void editor_minimap::start()
{
	if (false == m_workers.empty())
		return;
	for (int i = 0; i < worker_count; ++i)
		m_workers.emplace_back([this]() { worker(); });
}

void editor_minimap::worker()
{
	editor_http::client client;
	auto const cache{std::filesystem::path{editor_orthophoto::cache_directory()} / "foreign"};
	while (true)
	{
		std::pair<key, std::string> job;
		std::uint32_t generation;
		{
			std::unique_lock<std::mutex> lock{m_mutex};
			m_wake.wait(lock, [this]() { return m_stop || false == m_queue.empty(); });
			if (m_stop)
				return;
			job = m_queue.front();
			m_queue.pop_front();
			generation = m_generation;
		}
		auto const [level, ix, iy]{job.first};
		auto const side{side_of(level)};
		editor_foreign::area const area{ix * side, iy * side, (ix + 1) * side, (iy + 1) * side};
		result done;
		done.where = job.first;
		done.generation = generation;
		std::string error;
		done.ok = editor_foreign::layer(client, job.second, area, pixels, done.rgba, cache, error, [this]() { return m_stop.load(); }, true);
		std::lock_guard<std::mutex> lock{m_mutex};
		if (m_stop)
			return;
		if (false == done.ok)
			m_error = error;
		m_done.push_back(std::move(done));
	}
}

void editor_minimap::clear()
{
	++m_generation;
	{
		std::lock_guard<std::mutex> lock{m_mutex};
		m_queue.clear();
		m_done.clear();
		m_error.clear();
	}
	for (auto &entry : m_tiles)
		if (entry.second.texture != null_handle)
		{
			std::uint8_t const texel[4]{0, 0, 0, 0};
			GfxRenderer->Texture(entry.second.texture).update_from_memory(1, 1, texel);
			m_free.emplace_back(entry.second.slot, entry.second.texture);
		}
	m_tiles.clear();
}

void editor_minimap::release()
{
	clear();
}

void editor_minimap::upload()
{
	std::deque<result> done;
	{
		std::lock_guard<std::mutex> lock{m_mutex};
		for (int i = 0; i < uploads_per_frame && false == m_done.empty(); ++i)
		{
			done.push_back(std::move(m_done.front()));
			m_done.pop_front();
		}
	}
	for (auto &item : done)
	{
		if (item.generation != m_generation)
			continue;
		auto const found{m_tiles.find(item.where)};
		if (found == m_tiles.end())
			continue;
		auto &entry{found->second};
		if (false == item.ok)
		{
			entry.state = 0; // asked for again when it's seen, after a while
			entry.retry = m_frame + 600;
			continue;
		}
		if (item.rgba.size() != static_cast<std::size_t>(pixels) * pixels * 4 ||
		    std::none_of(item.rgba.begin(), item.rgba.end(), [](std::uint8_t const Value) { return Value != 0; }))
		{
			entry.state = 3;
			continue;
		}
		if (entry.texture == null_handle)
		{
			if (false == m_free.empty())
			{
				entry.slot = m_free.back().first;
				entry.texture = m_free.back().second;
				m_free.pop_back();
			}
			else
			{
				entry.slot = m_textures++;
				entry.texture = GfxRenderer->Fetch_Texture(slot_name(entry.slot), true);
			}
		}
		GfxRenderer->Texture(entry.texture).update_from_memory(pixels, pixels, item.rgba.data());
		entry.state = 2;
	}
	// the tiles not seen for longest let go, past the limit
	if (m_tiles.size() <= kept_tiles)
		return;
	std::vector<std::pair<std::uint64_t, key>> ages;
	for (auto const &entry : m_tiles)
		if (entry.second.used + 2 < m_frame)
			ages.emplace_back(entry.second.used, entry.first);
	std::sort(ages.begin(), ages.end());
	for (std::size_t i = 0; i < ages.size() && m_tiles.size() > kept_tiles; ++i)
	{
		auto const found{m_tiles.find(ages[i].second)};
		if (found->second.texture != null_handle)
		{
			std::uint8_t const texel[4]{0, 0, 0, 0};
			GfxRenderer->Texture(found->second.texture).update_from_memory(1, 1, texel);
			m_free.emplace_back(found->second.slot, found->second.texture);
		}
		m_tiles.erase(found);
	}
}

editor_minimap::tile *editor_minimap::ready(key const &Key)
{
	auto const found{m_tiles.find(Key)};
	if (found == m_tiles.end() || found->second.state != 2 || found->second.texture == null_handle)
		return nullptr;
	return &found->second;
}

bool editor_minimap::ui(camera const &Camera, glm::dvec2 &Target)
{
	++m_frame;
	start();
	upload();
	auto const layers{editor_foreign::layers()};
	if (layers.empty())
	{
		ImGui::TextWrapped("%s", STR_C("No map layers: the minimap shows the map lines of editor/foreign_sources.txt (OpenStreetMap, topographic maps). Put the file in the folder of the simulator and reload the sources in the orthophoto window."));
		return false;
	}
	if (Camera.north == 0.0 && Camera.east == 0.0)
	{
		ImGui::TextWrapped("%s", STR_C("The scenery has no place on the map: set its origin in PUWG 1992 in the orthophoto window."));
		return false;
	}
	// the layer
	auto const *current{&layers.front()};
	for (auto const &item : layers)
		if (item.name == m_layer)
			current = &item;
	if (current->name != m_layer)
	{
		m_layer = current->name;
		clear();
	}
	ImGui::SetNextItemWidth(220.0f);
	if (ImGui::BeginCombo("##minimap_layer", STR(current->title).c_str()))
	{
		for (auto const &item : layers)
			if (ImGui::Selectable((STR(item.title) + "###minimap_" + item.name).c_str(), item.name == m_layer) && item.name != m_layer)
			{
				m_layer = item.name;
				clear();
			}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	ImGui::Checkbox(STR_C("Follow the camera"), &m_follow);
	// where the camera is, in PUWG 1992
	auto const camera_east{Camera.east - Camera.x}, camera_north{Camera.north + Camera.z};
	if (m_follow)
	{
		m_east = camera_east;
		m_north = camera_north;
	}
	// the canvas
	auto const low{ImGui::GetCursorScreenPos()};
	auto size{ImGui::GetContentRegionAvail()};
	size.y -= ImGui::GetTextLineHeightWithSpacing();
	if (size.x < 32.0f || size.y < 32.0f)
		return false;
	ImGui::InvisibleButton("##minimap_canvas", size);
	auto const hovered{ImGui::IsItemHovered()};
	auto const high{ImVec2(low.x + size.x, low.y + size.y)};
	auto const middle{ImVec2(low.x + size.x * 0.5f, low.y + size.y * 0.5f)};
	auto const &io{ImGui::GetIO()};
	auto const to_screen = [&](double const East, double const North) {
		return ImVec2(middle.x + static_cast<float>((East - m_east) / m_scale), middle.y - static_cast<float>((North - m_north) / m_scale));
	};
	auto const to_map = [&](ImVec2 const Point, double &East, double &North) {
		East = m_east + (Point.x - middle.x) * m_scale;
		North = m_north - (Point.y - middle.y) * m_scale;
	};
	bool go{false};
	if (hovered)
	{
		// dragged: the map goes with the mouse
		if (ImGui::IsMouseDragging(0) && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f))
		{
			m_follow = false;
			m_east -= io.MouseDelta.x * m_scale;
			m_north += io.MouseDelta.y * m_scale;
		}
		// the wheel: zoomed about the point under the mouse
		if (io.MouseWheel != 0.0f)
		{
			double east, north;
			to_map(io.MousePos, east, north);
			auto const scale{std::clamp(m_scale * std::pow(0.8, static_cast<double>(io.MouseWheel)), 0.25, side_of(max_level) / pixels * 2.0)};
			if (false == m_follow)
			{
				m_east = east - (io.MousePos.x - middle.x) * scale;
				m_north = north + (io.MousePos.y - middle.y) * scale;
			}
			m_scale = scale;
		}
		if (ImGui::IsMouseDoubleClicked(0))
		{
			double east, north;
			to_map(io.MousePos, east, north);
			Target = {Camera.east - east, north - Camera.north};
			go = true;
		}
	}
	// the level whose pixel is closest to that of the screen
	auto const level{std::clamp(static_cast<int>(std::lround(std::log2(std::max(1.0, m_scale)))), 0, max_level)};
	auto const side{side_of(level)};
	double west, north, east, south;
	to_map(low, west, north);
	to_map(high, east, south);
	auto *draw{ImGui::GetWindowDrawList()};
	draw->PushClipRect(low, high, true);
	draw->AddRectFilled(low, high, IM_COL32(40, 40, 40, 255));
	auto const first_x{static_cast<int>(std::floor(west / side))}, last_x{static_cast<int>(std::floor(east / side))};
	auto const first_y{static_cast<int>(std::floor(south / side))}, last_y{static_cast<int>(std::floor(north / side))};
	std::vector<key> wanted;
	for (int iy = first_y; iy <= last_y; ++iy)
		for (int ix = first_x; ix <= last_x; ++ix)
		{
			key const where{level, ix, iy};
			auto &entry{m_tiles[where]};
			entry.used = m_frame;
			auto const tile_low{to_screen(ix * side, (iy + 1) * side)}, tile_high{to_screen((ix + 1) * side, iy * side)};
			if (entry.state == 0 && m_frame >= entry.retry)
			{
				entry.state = 1;
				wanted.push_back(where);
			}
			if (auto const *shown{ready(where)})
			{
				auto const id{GfxRenderer->Texture(shown->texture).get_id()};
				draw->AddImage((ImTextureID)(intptr_t)(id), tile_low, tile_high, ImVec2(0, 0), ImVec2(1, 1));
				continue;
			}
			// not in yet: a part of a coarser one, if there is
			for (int up = 1; up <= 3 && level + up <= max_level; ++up)
			{
				auto const coarse{side_of(level + up)};
				auto const cx{static_cast<int>(std::floor(ix * side / coarse))}, cy{static_cast<int>(std::floor(iy * side / coarse))};
				auto const *shown{ready({level + up, cx, cy})};
				if (shown == nullptr)
					continue;
				auto const u0{static_cast<float>((ix * side - cx * coarse) / coarse)}, u1{static_cast<float>(((ix + 1) * side - cx * coarse) / coarse)};
				auto const v0{static_cast<float>(((cy + 1) * coarse - (iy + 1) * side) / coarse)}, v1{static_cast<float>(((cy + 1) * coarse - iy * side) / coarse)};
				auto const id{GfxRenderer->Texture(shown->texture).get_id()};
				draw->AddImage((ImTextureID)(intptr_t)(id), tile_low, tile_high, ImVec2(u0, v0), ImVec2(u1, v1));
				break;
			}
		}
	if (false == wanted.empty())
	{
		// the middle of the view first
		auto const centre_x{std::floor(m_east / side)}, centre_y{std::floor(m_north / side)};
		std::sort(wanted.begin(), wanted.end(), [&](key const &Left, key const &Right) {
			auto const distance = [&](key const &Key) { return std::abs(std::get<1>(Key) - centre_x) + std::abs(std::get<2>(Key) - centre_y); };
			return distance(Left) > distance(Right);
		});
		{
			std::lock_guard<std::mutex> lock{m_mutex};
			for (auto const &where : wanted)
				m_queue.emplace_front(where, m_layer);
			while (m_queue.size() > max_queue)
			{
				// dropped: asked for again when seen
				auto const found{m_tiles.find(m_queue.back().first)};
				if (found != m_tiles.end() && found->second.state == 1)
					found->second.state = 0;
				m_queue.pop_back();
			}
		}
		m_wake.notify_all();
	}
	// the camera: a triangle pointing where it looks (forward in the scenery is -sin, -cos of the yaw in x, z; x grows west)
	{
		auto const at{to_screen(camera_east, camera_north)};
		auto const dx{std::sin(Camera.yaw)}, dy{std::cos(Camera.yaw)}; // east, south on the screen
		auto const point = [&](float const Forward, float const Side) { return ImVec2(at.x + dx * Forward - dy * Side, at.y + dy * Forward + dx * Side); };
		draw->AddTriangleFilled(point(12.0f, 0.0f), point(-6.0f, 7.0f), point(-6.0f, -7.0f), IM_COL32(230, 40, 40, 255));
		draw->AddTriangle(point(12.0f, 0.0f), point(-6.0f, 7.0f), point(-6.0f, -7.0f), IM_COL32(255, 255, 255, 255), 1.5f);
	}
	// the scale bar
	{
		auto metres{100.0 * m_scale};
		auto const step{std::pow(10.0, std::floor(std::log10(metres)))};
		metres = metres / step >= 5.0 ? 5.0 * step : metres / step >= 2.0 ? 2.0 * step : step;
		auto const length{static_cast<float>(metres / m_scale)};
		ImVec2 const start{low.x + 10.0f, high.y - 12.0f};
		draw->AddRectFilled(ImVec2(start.x - 4.0f, start.y - 18.0f), ImVec2(start.x + length + 4.0f, start.y + 5.0f), IM_COL32(255, 255, 255, 180));
		draw->AddLine(start, ImVec2(start.x + length, start.y), IM_COL32(0, 0, 0, 255), 2.0f);
		auto const label{metres >= 1000.0 ? std::to_string(static_cast<int>(metres / 1000.0)) + " km" : std::to_string(static_cast<int>(metres)) + " m"};
		draw->AddText(ImVec2(start.x, start.y - 16.0f), IM_COL32(0, 0, 0, 255), label.c_str());
	}
	draw->PopClipRect();
	if (hovered)
		ImGui::SetTooltip("%s", STR_C("Drag: move the map. Wheel: zoom. Double click: the camera goes there"));
	// the line under the map
	std::string error;
	std::size_t waiting;
	{
		std::lock_guard<std::mutex> lock{m_mutex};
		error = m_error;
		waiting = m_queue.size();
	}
	if (false == error.empty())
		ImGui::TextDisabled("%s", error.c_str());
	else if (waiting > 0)
		ImGui::TextDisabled(STR_C("Loading: %d"), static_cast<int>(waiting));
	else
		ImGui::TextDisabled("%s", current->credit.c_str());
	return go;
}
