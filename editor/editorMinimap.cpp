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
#include "editor/editorFormat.hpp"
#include "editor/editorHttp.hpp"
#include "editor/editorOrthophoto.hpp"

#include "rendering/renderer.h"
#include "utilities/translation.h"
#include "imgui/imgui.h"

#include <algorithm>
#include <cfloat>
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

bool editor_minimap::controls(bool const Placed)
{
	++m_frame;
	start();
	upload();
	auto const layers{editor_foreign::layers()};
	ImGui::Checkbox(STR_C("Map underneath"), &m_on);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", STR_C("A map under the tracks: the map layers of editor/foreign_sources.txt (OpenStreetMap, topographic maps), north up.\n"
		                              "Off, the minimap is the plan of the tracks alone"));
	if (false == m_on)
		return false;
	if (layers.empty())
	{
		ImGui::TextWrapped("%s", STR_C("No map layers: the map comes from the map lines of editor/foreign_sources.txt (OpenStreetMap, topographic maps). Put the file in the folder of the simulator and reload the sources in the orthophoto settings."));
		return false;
	}
	if (false == Placed)
	{
		ImGui::TextWrapped("%s", STR_C("The scenery has no place on the map: set its origin in PUWG 1992 in the orthophoto settings."));
		return false;
	}
	// the layer: the one picked, at first the Polish topographic maps of geoportal.gov.pl (topo_pl), else OpenStreetMap,
	// else the first in the file (the order of the file is no good guide: a foreign map may stand before the world ones)
	editor_foreign::layer_info const *current{nullptr};
	for (auto const *wanted : {m_layer.c_str(), "topo_pl", "osm"})
	{
		for (auto const &item : layers)
			if (item.name == wanted)
				current = &item;
		if (current != nullptr)
			break;
	}
	if (current == nullptr)
		current = &layers.front();
	if (current->name != m_layer)
	{
		m_layer = current->name;
		clear();
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(-FLT_MIN);
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
	m_credit = current->credit.empty() ? STR(current->title) : current->credit;
	return true;
}

void editor_minimap::draw(ImDrawList &Drawlist, view const &View)
{
	// the corners of the part of the scenery in view, in PUWG 1992 (east = origin - x, north = origin + z)
	auto const west{View.east - View.high.x};
	auto const east{View.east - View.low.x};
	auto const south{View.north + View.low.y};
	auto const north{View.north + View.high.y};
	auto const screen = [&](double const East, double const North) { return View.screen(glm::dvec2{View.east - East, North - View.north}); };
	// the level whose pixel is closest to that of the screen
	auto const level{std::clamp(static_cast<int>(std::lround(std::log2(std::max(1.0, View.scale)))), 0, max_level)};
	auto const side{side_of(level)};
	auto const first_x{static_cast<int>(std::floor(west / side))}, last_x{static_cast<int>(std::floor(east / side))};
	auto const first_y{static_cast<int>(std::floor(south / side))}, last_y{static_cast<int>(std::floor(north / side))};
	// too many tiles for the view: the map is too small at this scale, nothing asked for
	if (static_cast<long long>(last_x - first_x + 1) * (last_y - first_y + 1) > static_cast<long long>(kept_tiles / 2))
		return;
	std::vector<key> wanted;
	for (int iy = first_y; iy <= last_y; ++iy)
		for (int ix = first_x; ix <= last_x; ++ix)
			draw_tile(Drawlist, {level, ix, iy}, screen, wanted);
	if (false == wanted.empty())
		ask(wanted, (View.east - (View.low.x + View.high.x) * 0.5) / side, (View.north + (View.low.y + View.high.y) * 0.5) / side);
}

void editor_minimap::draw_tile(ImDrawList &Drawlist, key const &Where, std::function<ImVec2(double, double)> const &Screen, std::vector<key> &Wanted)
{
	auto const [level, ix, iy]{Where};
	auto const side{side_of(level)};
	auto &entry{m_tiles[Where]};
	entry.used = m_frame;
	if (entry.state == 0 && m_frame >= entry.retry)
	{
		entry.state = 1;
		Wanted.push_back(Where);
	}
	// the corners of the tile, north-west first and clockwise: the first row of its image is its north edge
	auto const quad = [&](texture_handle const Texture, ImVec2 const &Low, ImVec2 const &High) {
		auto const id{GfxRenderer->Texture(Texture).get_id()};
		Drawlist.AddImageQuad((ImTextureID)(intptr_t)(id), Screen(ix * side, (iy + 1) * side), Screen((ix + 1) * side, (iy + 1) * side), Screen((ix + 1) * side, iy * side),
		                      Screen(ix * side, iy * side), Low, ImVec2(High.x, Low.y), High, ImVec2(Low.x, High.y));
	};
	if (auto const *shown{ready(Where)})
	{
		quad(shown->texture, ImVec2(0, 0), ImVec2(1, 1));
		return;
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
		quad(shown->texture, ImVec2(u0, v0), ImVec2(u1, v1));
		return;
	}
}

void editor_minimap::ask(std::vector<key> &Wanted, double const Centre_x, double const Centre_y)
{
	// the middle of the view first
	std::sort(Wanted.begin(), Wanted.end(), [&](key const &Left, key const &Right) {
		auto const distance = [&](key const &Key) { return std::abs(std::get<1>(Key) - Centre_x) + std::abs(std::get<2>(Key) - Centre_y); };
		return distance(Left) > distance(Right);
	});
	{
		std::scoped_lock lock{m_mutex};
		for (auto const &where : Wanted)
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

std::string editor_minimap::status()
{
	std::scoped_lock lock{m_mutex};
	if (false == m_error.empty())
		return m_error;
	if (false == m_queue.empty())
		return format(STR_C("Loading: %d"), static_cast<int>(m_queue.size()));
	return m_credit;
}
