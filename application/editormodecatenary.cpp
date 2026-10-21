/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

// Catenary in the editor: poles with cantilevers put by the track where the user sees them (on the orthophoto). The card of
// the cantilever is chosen from the catalogue by the distance from the wires to the face of the pole (band A, last digit of
// the card), the cantilever is assembled by the same solver the simulation uses and shown at once. Nothing is laid out
// automatically: the spans between the poles the user put are only checked against the tables of the catalogue.

#include "stdafx.h"
#include "application/editormode.h"
#include "application/editoruilayer.h"
#include "application/editorprojection.h"
#include "editor/editorFormat.hpp"
#include "editor/editorGeometry.hpp"

#include "simulation/simulation.h"
#include "utilities/Globals.h"
#include "world/Track.h"
#include "world/catenary_scene.h"
#include "world/catenary_layout.h"

#include "imgui/imgui.h"
#include "utilities/translation.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>

namespace
{

using geometry::bezier;
using geometry::plan_of;

double const kRailTop{0.18}; // m from the points of a track to the heads of its rails
double const kCatenaryRange{1500.0}; // m from the camera, farther poles aren't drawn nor picked
double const kSpanSearch{150.0}; // m, the farthest a neighbouring pole of the same line is looked for
double const kSameLine{1.5}; // m across the span, farther the two poles belong to different tracks
double const kCurveRadius{4000.0}; // m, a track bent less is a straight for the cards (table 0007)
float const kPolePick{12.0f};

double nearest_parameter(TTrack const &Track, int const Path, glm::dvec3 const &Point)
{
	bezier const curve{Track.m_paths[Path]};
	auto parameter{0.0};
	auto best{std::numeric_limits<double>::max()};
	for (int k = 0; k <= 256; ++k)
	{
		auto const distance{geometry::plan_distance(curve.point(k / 256.0), Point)};
		if (distance < best)
		{
			best = distance;
			parameter = k / 256.0;
		}
	}
	return parameter;
}

glm::dvec2 forward_at(TTrack const &Track, int const Path, double const Parameter)
{
	auto const tangent{bezier{Track.m_paths[Path]}.first(Parameter)};
	glm::dvec2 const planar{tangent.x, tangent.z};
	return glm::length(planar) > 1e-9 ? glm::normalize(planar) : glm::dvec2{0.0, 1.0};
}

glm::dvec2 right_of(glm::dvec2 const &Direction)
{
	return {-Direction.y, Direction.x};
}

// the pole the suspension stands on, or the point it's attached at
glm::dvec3 pole_of(catenary::suspension const &Data)
{
	return Data.has_pole ? glm::dvec3{Data.pole[0], Data.pole[1], Data.pole[2]} : glm::dvec3{Data.attach[0], Data.attach[1], Data.attach[2]};
}

glm::dvec3 wire_of(catenary::suspension const &Data)
{
	return {Data.at[0], Data.at[1], Data.at[2]};
}

// from the pole towards the wires, in the plan; across the track
glm::dvec2 across_of(catenary::suspension const &Data)
{
	auto const delta{plan_of(wire_of(Data)) - plan_of(pole_of(Data))};
	return glm::length(delta) > 1e-6 ? glm::normalize(delta) : glm::dvec2{1.0, 0.0};
}

double pole_top(catenary::suspension const &Data)
{
	auto const *catalog{catenary_scene_catalog()};
	auto const *pole{catalog != nullptr && Data.has_pole ? catalog->pole(Data.pole_type) : nullptr};
	return pole != nullptr ? pole->top : 7.0;
}

// spans between the poles: pairs of the suspensions of the same line next to each other. Two suspensions are of the same
// line when the chord between their wires runs along the track: across the bisector of their directions it's off no more
// than the staggers of the wires make (wires of the neighbouring track are 4 m and more away)
struct span_check
{
	std::size_t from{0}, to{0};
	double length{0.0};
	double radius{0.0};
	double limit{0.0};
};

std::vector<span_check> spans_around(glm::dvec3 const &Camera, std::string const &Network, int const Zone)
{
	std::vector<std::size_t> near;
	for (std::size_t index = 0; index < catenary_scene_size(); ++index)
	{
		auto const *data{catenary_scene_get(index)};
		if (data != nullptr && data->has_at && glm::distance(wire_of(*data), Camera) < kCatenaryRange)
			near.push_back(index);
	}
	auto const &tables{catenary_scene_layout()};
	std::vector<span_check> result;
	for (auto const i : near)
	{
		auto const &a{*catenary_scene_get(i)};
		auto const across{across_of(a)};
		glm::dvec2 const along{-across.y, across.x};
		for (int direction : {1, -1})
		{
			auto best{kSpanSearch};
			std::size_t found{i};
			for (auto const j : near)
			{
				if (j == i)
					continue;
				auto const &b{*catenary_scene_get(j)};
				auto const chord{plan_of(wire_of(b)) - plan_of(wire_of(a))};
				auto const length{glm::length(chord)};
				if (length < 1.0 || length >= best || glm::dot(chord, along) * direction <= 0.0)
					continue;
				// the directions of both suspensions turned to one sense; their bisector is the direction of the chord on a curve
				auto otheracross{across_of(b)};
				if (glm::dot(otheracross, across) < 0.0)
					otheracross = -otheracross;
				auto const bisector{glm::normalize(across + otheracross)};
				if (std::abs(glm::dot(chord, bisector)) > kSameLine)
					continue;
				best = length;
				found = j;
			}
			if (found == i || found < i)
				continue; // each span once, from the lower number
			auto const &b{*catenary_scene_get(found)};
			span_check span;
			span.from = i;
			span.to = found;
			span.length = best;
			// radius from the change of the direction over the span: R = length / angle
			auto otheracross{across_of(b)};
			if (glm::dot(otheracross, across) < 0.0)
				otheracross = -otheracross;
			auto const angle{std::acos(std::clamp(glm::dot(across, otheracross), -1.0, 1.0))};
			span.radius = angle > 1e-4 ? best / angle : 0.0;
			span.limit = catenary::span_limit(tables, Network, Zone, span.radius > kCurveRadius ? 0.0 : span.radius, tables.curve_stagger());
			result.push_back(span);
		}
	}
	// a span found from both ends counts once
	std::sort(result.begin(), result.end(), [](span_check const &A, span_check const &B) { return A.from != B.from ? A.from < B.from : A.to < B.to; });
	result.erase(std::unique(result.begin(), result.end(), [](span_check const &A, span_check const &B) { return A.from == B.from && A.to == B.to; }), result.end());
	return result;
}

char const *const kExecutions[] = {"1 - two contact wires, light arms, hk 1.70", "2 - two contact wires, heavy arms, hk 1.70", "3 - one contact wire, heavy arms, hk 1.70",
                                   "4 - one contact wire, heavy arms, hk 1.30", "5 - one contact wire, light arms, hk 1.70"};

} // namespace

void editor_mode::catenary_scan()
{
	auto &tool{m_catenary};
	if (tool.scanned)
		return;
	tool.scanned = true;
	tool.poles.clear();
	if (auto const *catalog{catenary_scene_catalog()}; catalog != nullptr)
		for (auto const &pole : catalog->poles())
			tool.poles.push_back(pole.first);
	tool.networks = catenary_scene_layout().symbols();
	auto const preferred{std::find(tool.networks.begin(), tool.networks.end(), std::string{"YwsC120-2C"})};
	tool.network = preferred != tool.networks.end() ? static_cast<int>(preferred - tool.networks.begin()) : 0;
	auto const pole{std::find(tool.poles.begin(), tool.poles.end(), std::string{"1611-11"})};
	tool.pole = pole != tool.poles.end() ? static_cast<int>(pole - tool.poles.begin()) : 0;
	if (tool.file[0] == '\0')
	{
		auto const files{catenary_scene_files()};
		auto name{files.empty() ? std::filesystem::path{Global.SceneryFile}.stem().string() + "_siec.scm" : files.front()};
		std::snprintf(tool.file, sizeof(tool.file), "%s", name.c_str());
	}
}

bool editor_mode::catenary_spot_at(TTrack &Track, int const Path, glm::dvec3 const &Point, glm::dvec3 const &Cursor, int const Side, catenary_spot &Spot) const
{
	if (Track.m_paths.empty() || Track.m_editorremoved)
		return false;
	auto const path{std::clamp(Path, 0, static_cast<int>(Track.m_paths.size()) - 1)};
	bezier const curve{Track.m_paths[path]};
	auto const parameter{nearest_parameter(Track, path, Point)};
	Spot.track = &Track;
	Spot.path = path;
	Spot.parameter = parameter;
	Spot.axis = curve.point(parameter);
	Spot.forward = forward_at(Track, path, parameter);
	auto const right{right_of(Spot.forward)};
	Spot.side = Side != 0 ? Side : (glm::dot(plan_of(Cursor) - plan_of(Spot.axis), right) >= 0.0 ? 1 : -1);
	// the radius over about 10 m each way; on a curve the wires go to the outside of it (table 0007)
	auto const length{std::max(curve.plan_length(), 1.0)};
	auto const step{std::min(10.0 / length, 0.5)};
	auto const before{curve.point(std::max(0.0, parameter - step))};
	auto const after{curve.point(std::min(1.0, parameter + step))};
	double a[2]{before.x, before.z}, b[2]{Spot.axis.x, Spot.axis.z}, c[2]{after.x, after.z};
	Spot.radius = catenary::plan_radius(a, b, c);
	switch (m_catenary.bend)
	{
	case 1: Spot.curve = false; break;
	case 2: Spot.curve = true; break;
	default: Spot.curve = Spot.radius > 0.0 && Spot.radius < kCurveRadius; break;
	}
	Spot.toward = m_catenary.toward;
	if (Spot.curve && Spot.radius > 0.0)
	{
		// the middle of the arc is outwards of the chord: is the pole on the outside of the curve
		auto const outwards{glm::dot(plan_of(Spot.axis) - 0.5 * (plan_of(before) + plan_of(after)), right) * Spot.side > 0.0};
		Spot.toward = outwards;
	}
	catenary_compose(Spot, m_catenary.clearance, Spot.curve ? std::max(static_cast<double>(m_catenary.stagger), catenary_scene_layout().curve_stagger()) : m_catenary.stagger, Spot.toward);
	return true;
}

void editor_mode::catenary_compose(catenary_spot &Spot, double const Clearance, double const Stagger, bool const Toward) const
{
	auto const &tool{m_catenary};
	Spot.card = 0;
	Spot.problem.clear();
	Spot.toward = Toward;
	auto const *catalog{catenary_scene_catalog()};
	auto const *pole{catalog != nullptr && tool.pole >= 0 && tool.pole < static_cast<int>(tool.poles.size()) ? catalog->pole(tool.poles[tool.pole]) : nullptr};
	if (pole == nullptr)
	{
		Spot.problem = STR("No pole: the catalogue (data/traction_cantilevers.txt, data/traction_supports.txt) isn't loaded or has no poles");
		return;
	}
	auto const height{static_cast<double>(tool.height)};
	auto const railhead{Spot.axis.y + kRailTop};
	auto const right{right_of(Spot.forward) * static_cast<double>(Spot.side)}; // from the track to the pole
	glm::dvec3 const out{right.x, 0.0, right.y};
	auto const axisdistance{Clearance + 0.5 * pole->depth(0.0)};
	auto const offset{Toward ? Stagger : -Stagger};
	auto const polepoint{glm::dvec3{Spot.axis.x, railhead, Spot.axis.z} + out * axisdistance};
	auto const wire{glm::dvec3{Spot.axis.x, railhead + height, Spot.axis.z} + out * offset};
	Spot.reach = axisdistance - offset - 0.5 * pole->depth(height);
	catenary::suspension_family family;
	family.swinging = tool.swinging;
	family.curve = Spot.curve;
	family.toward_pole = Toward;
	auto const decade{catenary::family_decade(family)};
	auto const *scheme{catalog->scheme_for_reach(decade, tool.execution, Spot.reach)};
	auto &data{Spot.data};
	data = catenary::suspension();
	data.range_max = 1000.0;
	data.execution = tool.execution;
	data.has_at = true;
	data.at[0] = wire.x;
	data.at[1] = wire.y;
	data.at[2] = wire.z;
	data.has_pole = true;
	data.pole_type = pole->name;
	data.pole[0] = polepoint.x;
	data.pole[1] = polepoint.y;
	data.pole[2] = polepoint.z;
	if (tool.insulators25)
		data.insulators = "25";
	if (scheme == nullptr)
	{
		Spot.problem = format(STR_C("No card %d0-%d9 in execution %d for A = %.2f m (cards cover A 1.45-4.45 m, not all of them are in every family)"), decade / 10, decade / 10, tool.execution, Spot.reach);
		return;
	}
	Spot.card = scheme->card;
	data.card = scheme->card;
	data.has_rope = true;
	data.rope[0] = wire.x;
	data.rope[1] = wire.y + scheme->height;
	data.rope[2] = wire.z;
}

std::string editor_mode::catenary_intent_label(catenary_spot const &Spot) const
{
	auto const where{Spot.curve ? format(STR_C("curve R %.0f m"), Spot.radius) : std::string{STR("straight")}};
	auto const offset{Spot.toward ? STR("towards the pole") : STR("away from the pole")};
	if (Spot.card == 0)
		return format(STR_C("Pole here, %s, wires %s: %s"), where.c_str(), offset.c_str(), Spot.problem.c_str());
	return format(STR_C("Click: pole %s, card %d execution %d, A = %.2f m (%s, wires %s)"), Spot.data.pole_type.c_str(), Spot.card, Spot.data.execution, Spot.reach, where.c_str(), offset.c_str());
}

int editor_mode::catenary_hit() const
{
	screen_projection const projection;
	auto const &mouse{ImGui::GetIO().MousePos};
	auto best{kPolePick * kPolePick};
	int result{-1};
	glm::dvec3 const camera{Global.pCamera.Pos};
	for (std::size_t index = 0; index < catenary_scene_size(); ++index)
	{
		auto const *data{catenary_scene_get(index)};
		if (data == nullptr || false == data->has_at)
			continue;
		auto const foot{pole_of(*data)};
		if (glm::distance(foot, camera) > kCatenaryRange)
			continue;
		ImVec2 a, b;
		if (false == projection.project(foot, a) || false == projection.project(foot + glm::dvec3{0.0, pole_top(*data), 0.0}, b))
			continue;
		ImVec2 const span{b.x - a.x, b.y - a.y};
		auto const length2{span.x * span.x + span.y * span.y};
		auto const t{length2 > 1e-6f ? std::clamp(((mouse.x - a.x) * span.x + (mouse.y - a.y) * span.y) / length2, 0.0f, 1.0f) : 0.0f};
		auto const dx{a.x + span.x * t - mouse.x};
		auto const dy{a.y + span.y * t - mouse.y};
		if (dx * dx + dy * dy < best)
		{
			best = dx * dx + dy * dy;
			result = static_cast<int>(index);
		}
	}
	return result;
}

void editor_mode::catenary_select(int const Index)
{
	auto &tool{m_catenary};
	tool.selected = Index;
	tool.probed = false;
	auto const *data{Index >= 0 ? catenary_scene_get(static_cast<std::size_t>(Index)) : nullptr};
	if (data == nullptr)
	{
		tool.selected = -1;
		return;
	}
	// the settings of the panel take what the suspension has, so that changing them changes it
	if (auto const found{std::find(tool.poles.begin(), tool.poles.end(), data->pole_type)}; found != tool.poles.end())
		tool.pole = static_cast<int>(found - tool.poles.begin());
	tool.execution = std::clamp(data->execution, 1, 5);
	tool.swinging = data->card / 100 != 12;
	tool.insulators25 = data->insulators == "25";
	auto const decade{(data->card / 10) % 10};
	if (data->card / 100 == 11 || data->card / 100 == 12)
	{
		tool.bend = (decade == 3 || decade == 4) ? 2 : 1;
		tool.toward = decade == 1 || decade == 3;
	}
	// where it stands by the track: the one it was put by in this session, or the nearest one
	if (tool.spots.find(static_cast<std::size_t>(Index)) == tool.spots.end())
	{
		auto const pole{pole_of(*data)};
		auto const wire{wire_of(*data)};
		TTrack *nearest{nullptr};
		int nearestpath{0};
		auto best{12.0};
		for (auto *track : simulation::Paths.sequence())
		{
			if (track == nullptr || track->m_editorremoved || track->m_paths.empty())
				continue;
			for (int path = 0; path < static_cast<int>(track->m_paths.size()); ++path)
			{
				bezier const curve{track->m_paths[path]};
				if (geometry::plan_distance(curve.point(0.5), wire) > curve.plan_length() * 0.5 + best)
					continue;
				auto const distance{geometry::plan_distance(curve.point(nearest_parameter(*track, path, wire)), wire)};
				if (distance < best)
				{
					best = distance;
					nearest = track;
					nearestpath = path;
				}
			}
		}
		if (nearest != nullptr)
		{
			catenary_spot spot;
			catenary_spot_at(*nearest, nearestpath, wire, pole, 0, spot);
			tool.spots[static_cast<std::size_t>(Index)] = spot;
		}
	}
	auto const found{tool.spots.find(static_cast<std::size_t>(Index))};
	if (found == tool.spots.end())
	{
		tool.status = STR("The suspension isn't by any track: it can't be changed here");
		return;
	}
	auto const &spot{found->second};
	auto const right{right_of(spot.forward) * static_cast<double>(spot.side)};
	auto const *catalog{catenary_scene_catalog()};
	auto const *pole{catalog != nullptr ? catalog->pole(data->pole_type) : nullptr};
	auto const railhead{spot.axis.y + kRailTop};
	auto const offset{glm::dot(plan_of(wire_of(*data)) - plan_of(spot.axis), right)};
	tool.stagger = static_cast<float>(std::abs(offset));
	tool.toward = offset >= 0.0;
	tool.height = static_cast<float>(data->at[1] - railhead);
	if (pole != nullptr)
		tool.clearance = static_cast<float>(glm::dot(plan_of(pole_of(*data)) - plan_of(spot.axis), right) - 0.5 * pole->depth(0.0));
}

void editor_mode::catenary_apply(catenary_spot const &Spot)
{
	auto &tool{m_catenary};
	if (tool.selected < 0)
		return;
	auto const index{static_cast<std::size_t>(tool.selected)};
	auto const *current{catenary_scene_get(index)};
	if (current == nullptr || Spot.card == 0)
		return;
	auto data{Spot.data};
	// what the panel doesn't set stays as it was
	data.name = current->name;
	data.range_max = current->range_max;
	data.range_min = current->range_min;
	data.skin = current->skin;
	data.overrides = current->overrides;
	data.umbrella = current->umbrella;
	// dragging a slider changes it many times: all of that is taken back at once
	if (tool.undo.empty() || tool.undo.back().index != index || tool.undo.back().removed || false == tool.undo.back().before.has_value())
		tool.undo.push_back({index, *current, false});
	catenary_scene_update(index, data);
	tool.spots[index] = Spot;
	tool.probed = false;
}

void editor_mode::catenary_press(track_intent const &Intent)
{
	auto &tool{m_catenary};
	tool.status.clear();
	if (Intent.what == track_intent::kind::catenary_select)
	{
		catenary_select(Intent.index);
		return;
	}
	if (Intent.track == nullptr)
		return;
	catenary_spot spot;
	if (false == catenary_spot_at(*Intent.track, Intent.path, m_hover.point, cursor_level(m_hover.point.y), 0, spot))
		return;
	if (spot.card == 0)
	{
		tool.status = spot.problem;
		return;
	}
	auto const index{catenary_scene_add(tool.file, spot.data)};
	tool.spots[index] = spot;
	tool.undo.push_back({index, std::nullopt, false});
	tool.selected = -1;
	tool.status = format(STR_C("Pole %s with card %d execution %d placed, A = %.2f m"), spot.data.pole_type.c_str(), spot.card, spot.data.execution, spot.reach);
	if (catenary_scene_parts(index) == 0)
		tool.status += STR(" - the cantilever didn't assemble, see the log");
	// on a straight the wires zigzag: the next pole the other way
	if (tool.alternate && false == spot.curve)
		tool.toward = false == tool.toward;
}

bool editor_mode::catenary_delete_selected()
{
	auto &tool{m_catenary};
	if (tool.selected < 0)
		return false;
	auto const index{static_cast<std::size_t>(tool.selected)};
	auto const *current{catenary_scene_get(index)};
	if (current == nullptr)
		return false;
	tool.undo.push_back({index, *current, true});
	catenary_scene_remove(index);
	tool.selected = -1;
	tool.status = STR("Pole removed");
	return true;
}

void editor_mode::catenary_undo()
{
	auto &tool{m_catenary};
	if (tool.undo.empty())
		return;
	auto const change{tool.undo.back()};
	tool.undo.pop_back();
	if (false == change.before.has_value())
		catenary_scene_remove(change.index);
	else if (change.removed)
		catenary_scene_restore(change.index, *change.before);
	else
		catenary_scene_update(change.index, *change.before);
	tool.selected = -1;
	tool.probed = false;
	tool.status = STR("Taken back");
}

void editor_mode::render_catenary_ui()
{
	auto &tool{m_catenary};
	catenary_scan();
	auto const *catalog{catenary_scene_catalog()};
	if (catalog == nullptr || tool.poles.empty())
	{
		ImGui::TextWrapped("%s", STR_C("The cantilever catalogue isn't loaded: data/traction_cantilevers.txt and data/traction_supports.txt (with the poles) are needed"));
		return;
	}
	auto changed{false};
	ImGui::PushItemWidth(180.0f);
	if (tool.selected < 0)
	{
		ImGui::InputText(STR_C("Catenary file"), tool.file, sizeof(tool.file));
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", STR_C("File in the scenery directory the new poles go to; a new one is added to the scenery with the directive catenary <file> endcatenary when it's saved"));
	}
	else
		ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "%s", format(STR_C("Selected pole (%s): changes apply to it at once"), catenary_scene_file_of(static_cast<std::size_t>(tool.selected)).c_str()).c_str());

	if (ImGui::BeginCombo(STR_C("Pole"), tool.poles[std::clamp(tool.pole, 0, static_cast<int>(tool.poles.size()) - 1)].c_str()))
	{
		for (int i = 0; i < static_cast<int>(tool.poles.size()); ++i)
			if (ImGui::Selectable(tool.poles[i].c_str(), i == tool.pole))
			{
				tool.pole = i;
				changed = true;
			}
		ImGui::EndCombo();
	}
	changed |= ImGui::SliderFloat(STR_C("Face of the pole from the track axis (m)"), &tool.clearance, 1.60f, 4.50f, "%.2f");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", STR_C("Structure gauge 2.20 or 2.50 m by the catalogue (card 0007); drag it and the cantilever follows, the card changes with band A. Ctrl+click types a value"));
	changed |= ImGui::SliderFloat(STR_C("Stagger of the wires (m)"), &tool.stagger, 0.0f, 0.45f, "%.2f");
	if (ImGui::Checkbox(STR_C("Wires towards the pole"), &tool.toward))
		changed = true;
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", STR_C("On a straight the wires zigzag: towards the pole and away from it in turn. On a curve they go to its outside, which decides it"));
	ImGui::SameLine();
	ImGui::Checkbox(STR_C("In turn"), &tool.alternate);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", STR_C("After a pole on a straight is placed, the next one gets the wires the other way"));
	std::string const bendnames[] = {STR("by the track"), STR("straight"), STR("curve")};
	char const *const bends[] = {bendnames[0].c_str(), bendnames[1].c_str(), bendnames[2].c_str()};
	if (ImGui::Combo(STR_C("Cards for"), &tool.bend, bends, 3))
		changed = true;
	if (ImGui::Checkbox(STR_C("Swinging cantilever (11xx)"), &tool.swinging))
		changed = true;
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", STR_C("Off: the fixed ones (12xx)"));
	if (ImGui::BeginCombo(STR_C("Execution"), STR(kExecutions[std::clamp(tool.execution, 1, 5) - 1]).c_str()))
	{
		for (int i = 1; i <= 5; ++i)
			if (ImGui::Selectable(STR(kExecutions[i - 1]).c_str(), i == tool.execution))
			{
				tool.execution = i;
				changed = true;
			}
		ImGui::EndCombo();
	}
	changed |= ImGui::SliderFloat(STR_C("Contact wires over the rails (m)"), &tool.height, 4.90f, 6.50f, "%.2f");
	changed |= ImGui::Checkbox(STR_C("25 kV insulators"), &tool.insulators25);
	ImGui::Separator();
	if (false == tool.networks.empty())
	{
		if (ImGui::BeginCombo(STR_C("Network (for the spans)"), tool.networks[std::clamp(tool.network, 0, static_cast<int>(tool.networks.size()) - 1)].c_str()))
		{
			for (int i = 0; i < static_cast<int>(tool.networks.size()); ++i)
				if (ImGui::Selectable(tool.networks[i].c_str(), i == tool.network))
					tool.network = i;
			ImGui::EndCombo();
		}
		ImGui::RadioButton(STR_C("Wind zone I"), &tool.zone, 1);
		ImGui::SameLine();
		ImGui::RadioButton(STR_C("Wind zone II"), &tool.zone, 2);
	}
	else
		ImGui::TextDisabled("%s", STR_C("No data/traction_layout.txt: the spans aren't checked"));
	ImGui::PopItemWidth();

	// the selected pole follows the settings at once
	if (tool.selected >= 0 && changed)
	{
		auto const found{tool.spots.find(static_cast<std::size_t>(tool.selected))};
		if (found != tool.spots.end())
		{
			auto spot{found->second};
			spot.curve = tool.bend == 2 || (tool.bend == 0 && spot.radius > 0.0 && spot.radius < kCurveRadius);
			catenary_compose(spot, tool.clearance, tool.stagger, tool.toward);
			if (spot.card != 0)
				catenary_apply(spot);
			else
				tool.status = spot.problem;
		}
	}
	if (tool.selected >= 0)
	{
		auto const *data{catenary_scene_get(static_cast<std::size_t>(tool.selected))};
		if (data == nullptr)
			tool.selected = -1;
		else
		{
			if (false == tool.probed)
			{
				tool.probe_good = catenary_scene_probe(*data, tool.probe);
				tool.probed = true;
			}
			auto const spot{tool.spots.find(static_cast<std::size_t>(tool.selected))};
			ImGui::Text("%s", format(STR_C("Card %d execution %d, A = %.2f m, %zu parts"), data->card, data->execution, spot != tool.spots.end() ? spot->second.reach : 0.0, catenary_scene_parts(static_cast<std::size_t>(tool.selected))).c_str());
			if (false == tool.probe_good)
				ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "%s", STR_C("The cantilever doesn't assemble:"));
			for (auto const &warning : tool.probe.warnings)
				ImGui::TextWrapped("- %s", warning.c_str());
			if (ImGui::Button(STR_C("Remove the pole")))
				catenary_delete_selected();
			ImGui::SameLine();
			if (ImGui::Button(STR_C("Done")))
				tool.selected = -1;
		}
	}
	if (false == tool.undo.empty())
	{
		if (ImGui::Button(STR_C("Take back")))
			catenary_undo();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", STR_C("Takes back the last change of the catenary (Ctrl+Z of the editor doesn't)"));
	}
	if (catenary_scene_changed())
		ImGui::TextDisabled("%s", STR_C("The catenary files are written when the scenery is saved"));
	if (false == tool.status.empty())
		ImGui::TextWrapped("%s", tool.status.c_str());
}

void editor_mode::draw_catenary_overlay() const
{
	auto const &tool{m_catenary};
	screen_projection const projection;
	auto *drawlist{ImGui::GetBackgroundDrawList(ImGui::GetMainViewport())};
	glm::dvec3 const camera{Global.pCamera.Pos};
	auto const draw_pole = [&](catenary::suspension const &Data, ImU32 const Colour, float const Width) {
		auto const foot{pole_of(Data)};
		auto const wire{wire_of(Data)};
		projection.line(drawlist, foot, foot + glm::dvec3{0.0, pole_top(Data), 0.0}, Colour, Width);
		projection.line(drawlist, glm::dvec3{foot.x, wire.y, foot.z}, wire, Colour, Width * 0.6f);
		ImVec2 point;
		if (projection.project(wire, point))
			drawlist->AddCircleFilled(point, Width + 1.5f, Colour);
	};
	for (std::size_t index = 0; index < catenary_scene_size(); ++index)
	{
		auto const *data{catenary_scene_get(index)};
		if (data == nullptr || false == data->has_at || glm::distance(pole_of(*data), camera) > kCatenaryRange)
			continue;
		auto const selected{static_cast<int>(index) == tool.selected};
		auto const hovered{m_intent.what == track_intent::kind::catenary_select && m_intent.index == static_cast<int>(index)};
		auto const broken{catenary_scene_parts(index) == 0};
		auto const colour{selected ? overlay_color::selected : hovered ? overlay_color::highlight : broken ? overlay_color::invalid : IM_COL32(255, 210, 60, 150)};
		draw_pole(*data, colour, selected || hovered ? 3.0f : 1.5f);
		ImVec2 head;
		if ((selected || hovered || broken) && projection.project(pole_of(*data) + glm::dvec3{0.0, pole_top(*data) + 0.3, 0.0}, head))
		{
			auto const label{format("%d-%d", data->card, data->execution) + (broken ? std::string{"  "} + STR("doesn't assemble") : std::string{})};
			drawlist->AddText(ImVec2(head.x + 6.0f, head.y - 6.0f), colour, label.c_str());
		}
	}
	// spans against the catalogue: green within the limit, red over it
	if (false == tool.networks.empty())
	{
		auto const &network{tool.networks[std::clamp(tool.network, 0, static_cast<int>(tool.networks.size()) - 1)]};
		for (auto const &span : spans_around(camera, network, tool.zone))
		{
			auto const a{wire_of(*catenary_scene_get(span.from))};
			auto const b{wire_of(*catenary_scene_get(span.to))};
			auto const over{span.limit > 0.0 && span.length > span.limit + 0.05};
			auto const colour{over ? overlay_color::invalid : IM_COL32(90, 230, 120, 170)};
			projection.line(drawlist, a, b, colour, over ? 2.5f : 1.2f);
			ImVec2 middle;
			if (projection.project(0.5 * (a + b), middle))
			{
				auto const label{span.limit > 0.0 ? format("%.1f / %.0f m", span.length, span.limit) : format("%.1f m", span.length)};
				auto const text{span.radius > 0.0 && span.radius < kCurveRadius ? label + format("  R %.0f", span.radius) : label};
				drawlist->AddText(ImVec2(middle.x + 4.0f, middle.y + 2.0f), colour, text.c_str());
			}
		}
	}
	// what a click puts there
	if (m_intent.what == track_intent::kind::catenary_place && m_hover.track != nullptr)
	{
		catenary_spot spot;
		if (catenary_spot_at(*m_hover.track, m_hover.path, m_hover.point, cursor_level(m_hover.point.y), 0, spot))
		{
			auto const colour{spot.card != 0 ? overlay_color::marked : overlay_color::invalid};
			draw_pole(spot.data, colour, 3.0f);
			projection.line(drawlist, spot.axis + glm::dvec3{0.0, kRailTop, 0.0}, pole_of(spot.data), colour, 1.5f);
		}
	}
}
