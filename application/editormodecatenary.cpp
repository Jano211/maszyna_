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

// the top of the pole over its point; a drop hangs down from its point, so its "top" is negative
double pole_top(catenary::suspension const &Data)
{
	auto const *catalog{catenary_scene_catalog()};
	auto const *pole{catalog != nullptr && Data.has_pole ? catalog->pole(Data.pole_type) : nullptr};
	if (pole != nullptr && false == pole->drop.empty())
		return -pole->length;
	return pole != nullptr ? pole->top : 7.0;
}

// spans to check against the catalogue: the spans of the chains, and between the poles which aren't in any chain the pairs of
// the same line next to each other. Two suspensions are of the same line when the chord between their wires runs along the
// track: across the bisector of their directions it's off no more than the staggers of the wires make (the wires of the
// neighbouring track are 4 m and more away)
struct span_check
{
	std::size_t from{0}, to{0};
	double length{0.0};
	double radius{0.0};
	double limit{0.0};
	int chain{-1};
};

// radius of the track over the span from the change of the direction of the cantilevers: R = length / angle
double span_radius(catenary::suspension const &A, catenary::suspension const &B, double const Length)
{
	auto const across{across_of(A)};
	auto otheracross{across_of(B)};
	if (glm::dot(otheracross, across) < 0.0)
		otheracross = -otheracross;
	auto const angle{std::acos(std::clamp(glm::dot(across, otheracross), -1.0, 1.0))};
	return angle > 1e-4 ? Length / angle : 0.0;
}

std::vector<span_check> spans_around(glm::dvec3 const &Camera, std::string const &Network, int const Zone)
{
	auto const &tables{catenary_scene_layout()};
	std::vector<span_check> result;
	auto const add = [&](std::size_t const From, std::size_t const To, std::string const &Symbol, int const Chain) {
		auto const &a{*catenary_scene_get(From)};
		auto const &b{*catenary_scene_get(To)};
		span_check span;
		span.from = From;
		span.to = To;
		span.chain = Chain;
		span.length = glm::length(plan_of(wire_of(b)) - plan_of(wire_of(a)));
		span.radius = span_radius(a, b, span.length);
		span.limit = catenary::span_limit(tables, Symbol.empty() ? Network : Symbol, Zone, span.radius > kCurveRadius ? 0.0 : span.radius, tables.curve_stagger());
		result.push_back(span);
	};
	std::vector<bool> chained(catenary_scene_size(), false);
	for (std::size_t chain = 0; chain < catenary_chain_size(); ++chain)
	{
		auto const *data{catenary_chain_get(chain)};
		if (data == nullptr)
			continue;
		auto const members{catenary_chain_members(chain)};
		for (std::size_t k = 0; k < members.size(); ++k)
		{
			if (members[k] < chained.size())
				chained[members[k]] = true;
			if (k + 1 >= members.size() || catenary_scene_get(members[k]) == nullptr || catenary_scene_get(members[k + 1]) == nullptr)
				continue;
			if (glm::distance(wire_of(*catenary_scene_get(members[k])), Camera) > kCatenaryRange)
				continue;
			add(members[k], members[k + 1], data->type, static_cast<int>(chain));
		}
	}
	std::vector<std::size_t> nearby;
	for (std::size_t index = 0; index < catenary_scene_size(); ++index)
	{
		auto const *data{catenary_scene_get(index)};
		if (data != nullptr && data->has_at && false == chained[index] && glm::distance(wire_of(*data), Camera) < kCatenaryRange)
			nearby.push_back(index);
	}
	std::vector<std::pair<std::size_t, std::size_t>> pairs;
	for (auto const i : nearby)
	{
		auto const &a{*catenary_scene_get(i)};
		auto const across{across_of(a)};
		glm::dvec2 const along{-across.y, across.x};
		for (int direction : {1, -1})
		{
			auto best{kSpanSearch};
			std::size_t found{i};
			for (auto const j : nearby)
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
			if (found != i)
				pairs.emplace_back(std::min(i, found), std::max(i, found));
		}
	}
	// a span found from both ends counts once
	std::sort(pairs.begin(), pairs.end());
	pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());
	for (auto const &pair : pairs)
		add(pair.first, pair.second, {}, -1);
	return result;
}

// distance on the screen from the mouse to the segment between two points, the largest float if it's behind the camera
float mouse_to_segment(screen_projection const &Projection, glm::dvec3 const &A, glm::dvec3 const &B)
{
	ImVec2 a, b;
	if (false == Projection.project(A, a) || false == Projection.project(B, b))
		return std::numeric_limits<float>::max();
	auto const &mouse{ImGui::GetIO().MousePos};
	ImVec2 const span{b.x - a.x, b.y - a.y};
	auto const length2{span.x * span.x + span.y * span.y};
	auto const t{length2 > 1e-6f ? std::clamp(((mouse.x - a.x) * span.x + (mouse.y - a.y) * span.y) / length2, 0.0f, 1.0f) : 0.0f};
	auto const dx{a.x + span.x * t - mouse.x};
	auto const dy{a.y + span.y * t - mouse.y};
	return dx * dx + dy * dy;
}

char const *const kExecutions[] = {"1 - two contact wires, light arms, hk 1.70", "2 - two contact wires, heavy arms, hk 1.70", "3 - one contact wire, heavy arms, hk 1.70",
                                   "4 - one contact wire, heavy arms, hk 1.30", "5 - one contact wire, light arms, hk 1.70"};
char const *const kMaterials[] = {"cu", "al"};
// the states of the wires in the panel, and as they're written in the entries
char const *const kStates[] = {"new", "worn", "tarnished"};
int const kStateValues[] = {0, 2, 1};

} // namespace

void editor_mode::catenary_scan()
{
	auto &tool{m_catenary};
	if (tool.scanned)
		return;
	tool.scanned = true;
	tool.poles.clear();
	if (auto const *catalog{catenary_scene_catalog()}; catalog != nullptr)
	{
		for (auto const &pole : catalog->poles())
			if (pole.second.drop.empty())
				tool.poles.push_back(pole.first);
		tool.masts.clear();
		for (auto const &mast : catalog->masts())
			tool.masts.push_back(mast.first);
	}
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
		Spot.toward = glm::dot(plan_of(Spot.axis) - 0.5 * (plan_of(before) + plan_of(after)), right) * Spot.side > 0.0;
	}
	auto const stagger{Spot.curve ? std::max(static_cast<double>(m_catenary.stagger), catenary_scene_layout().curve_stagger()) : static_cast<double>(m_catenary.stagger)};
	catenary_compose(Spot, m_catenary.clearance, stagger, Spot.toward);
	// a gantry is selected: the cantilever hangs on a drop on its beam
	glm::dvec3 from, to;
	double bottom{0.0};
	auto const *catalog{catenary_scene_catalog()};
	if (m_catenary.gantry_placing || m_catenary.gantry_selected < 0 || catalog == nullptr || false == catenary_gantry_beam(static_cast<std::size_t>(m_catenary.gantry_selected), from, to, bottom))
		return true;
	auto const *gantry{catenary_gantry_get(static_cast<std::size_t>(m_catenary.gantry_selected))};
	auto const *beam{gantry != nullptr ? catalog->beam(gantry->beam) : nullptr};
	std::vector<std::pair<double, std::string>> drops;
	for (auto const &pole : catalog->poles())
		if (beam != nullptr && pole.second.drop == beam->drop)
			drops.emplace_back(pole.second.length, pole.first);
	std::sort(drops.begin(), drops.end());
	if (drops.empty())
	{
		Spot.card = 0;
		Spot.problem = STR("No drops in the catalogue for this beam");
		return true;
	}
	// along the track onto the line of the beam
	catenary_compose(Spot, m_catenary.clearance, stagger, Spot.toward, drops.front().second);
	auto const p{plan_of(glm::dvec3{Spot.data.pole[0], 0.0, Spot.data.pole[2]})};
	auto const f{Spot.forward};
	auto const origin{plan_of(from)};
	auto const u{plan_of(to) - origin};
	auto const denominator{f.x * u.y - f.y * u.x};
	if (std::abs(denominator) < 1e-6)
	{
		Spot.card = 0;
		Spot.problem = STR("The track runs along the beam of the gantry");
		return true;
	}
	auto const s{((origin.x - p.x) * u.y - (origin.y - p.y) * u.x) / denominator};
	auto const t{((origin.x - p.x) * f.y - (origin.y - p.y) * f.x) / denominator};
	if (t < 0.0 || t > 1.0)
	{
		Spot.card = 0;
		Spot.problem = STR("The drop would be off the beam of the gantry");
		return true;
	}
	Spot.axis.x += f.x * s;
	Spot.axis.z += f.y * s;
	// the shortest drop which reaches down to the lowest mount of the cantilever
	auto const take = [&](std::string const &Name) {
		catenary_compose(Spot, m_catenary.clearance, stagger, Spot.toward, Name);
		Spot.data.pole[1] = bottom;
	};
	for (auto const &drop : drops)
	{
		take(drop.second);
		if (Spot.card == 0)
			return true;
		catenary::cantilever_result probe;
		catenary_scene_probe(Spot.data, probe);
		if (std::none_of(probe.warnings.begin(), probe.warnings.end(), [](std::string const &Warning) { return Warning.find("below the lowest mount") != std::string::npos; }))
			return true;
	}
	take(drops.back().second);
	return true;
}

// the line of the beam of the gantry at its bottom (where the drops hang from)
bool editor_mode::catenary_gantry_beam(std::size_t const Index, glm::dvec3 &From, glm::dvec3 &To, double &Bottom) const
{
	auto const *gantry{catenary_gantry_get(Index)};
	auto const *catalog{catenary_scene_catalog()};
	auto const *mast{gantry != nullptr && catalog != nullptr ? catalog->mast(gantry->masts[0]) : nullptr};
	if (mast == nullptr)
		return false;
	glm::dvec3 const a{gantry->points[0][0], gantry->points[0][1], gantry->points[0][2]};
	glm::dvec3 const b{gantry->points[1][0], gantry->points[1][1], gantry->points[1][2]};
	auto const heading{std::atan2(b.x - a.x, b.z - a.z)};
	// along the track as the scene sees the X of the gantry (see cantilever_to_scene)
	glm::dvec3 const along{-std::cos(heading), 0.0, std::sin(heading)};
	auto const shift{along * (gantry->side * mast->offset)};
	Bottom = a.y + (gantry->high ? mast->mount_high : mast->mount_low);
	From = a + shift;
	To = b + shift;
	From.y = To.y = Bottom;
	return true;
}

glm::dvec3 editor_mode::catenary_mast_point() const
{
	// on the ground, at the height of the rail heads of the track under the cursor if there's one
	auto point{cursor_ground()};
	if (m_hover.track != nullptr)
		point.y = m_hover.point.y + kRailTop;
	return point;
}

// the second mast: towards the cursor, as far as the nearest beam of the catalogue spans
bool editor_mode::catenary_gantry_second(glm::dvec3 const &Point, glm::dvec3 &Second, std::string &Beam) const
{
	auto const &tool{m_catenary};
	auto const *catalog{catenary_scene_catalog()};
	if (catalog == nullptr || false == tool.gantry_first.has_value())
		return false;
	auto const first{*tool.gantry_first};
	auto const delta{plan_of(Point) - plan_of(first)};
	auto const distance{glm::length(delta)};
	if (distance < 1.0)
		return false;
	auto best{std::numeric_limits<double>::max()};
	double span{0.0};
	for (auto const &beam : catalog->beams())
		if (std::abs(beam.second.span - distance) < best)
		{
			best = std::abs(beam.second.span - distance);
			span = beam.second.span;
			Beam = beam.first;
		}
	if (span <= 0.0)
		return false;
	auto const direction{delta / distance};
	Second = {first.x + direction.x * span, Point.y, first.z + direction.y * span};
	return true;
}

int editor_mode::catenary_gantry_hit() const
{
	screen_projection const projection;
	auto best{kPolePick * kPolePick};
	int result{-1};
	for (std::size_t index = 0; index < catenary_gantry_size(); ++index)
	{
		glm::dvec3 from, to;
		double bottom{0.0};
		if (false == catenary_gantry_beam(index, from, to, bottom) || glm::distance(from, glm::dvec3{Global.pCamera.Pos}) > kCatenaryRange)
			continue;
		auto const distance{mouse_to_segment(projection, from, to)};
		if (distance < best)
		{
			best = distance;
			result = static_cast<int>(index);
		}
	}
	return result;
}

void editor_mode::catenary_compose(catenary_spot &Spot, double const Clearance, double const Stagger, bool const Toward, std::string const &Pole) const
{
	auto const &tool{m_catenary};
	Spot.card = 0;
	Spot.problem.clear();
	Spot.toward = Toward;
	auto const *catalog{catenary_scene_catalog()};
	auto const *pole{catalog == nullptr ? nullptr : false == Pole.empty() ? catalog->pole(Pole) : tool.pole >= 0 && tool.pole < static_cast<int>(tool.poles.size()) ? catalog->pole(tool.poles[tool.pole]) : nullptr};
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

// where the suspension stands by the track: the place it was put at in this session, or the nearest path to its wires
editor_mode::catenary_spot const *editor_mode::catenary_spot_of(std::size_t const Index)
{
	auto &tool{m_catenary};
	if (auto const found{tool.spots.find(Index)}; found != tool.spots.end())
		return &found->second;
	auto const *data{catenary_scene_get(Index)};
	if (data == nullptr || false == data->has_at)
		return nullptr;
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
	if (nearest == nullptr)
		return nullptr;
	catenary_spot spot;
	catenary_spot_at(*nearest, nearestpath, wire, pole, 0, spot);
	// what the suspension has, not what the panel would make
	auto const right{right_of(spot.forward) * static_cast<double>(spot.side)};
	spot.toward = glm::dot(plan_of(wire) - plan_of(spot.axis), right) >= 0.0;
	spot.data = *data;
	spot.card = data->card;
	auto const decade{(data->card / 10) % 10};
	if (data->card / 100 == 11 || data->card / 100 == 12)
		spot.curve = decade == 3 || decade == 4;
	return &(tool.spots[Index] = spot);
}

std::string editor_mode::catenary_intent_label(catenary_spot const &Spot) const
{
	auto const where{Spot.curve ? format(STR_C("curve R %.0f m"), Spot.radius) : std::string{STR("straight")}};
	auto const offset{Spot.toward ? STR("towards the pole") : STR("away from the pole")};
	if (Spot.card == 0)
		return format(STR_C("Pole here, %s, wires %s: %s"), where.c_str(), offset.c_str(), Spot.problem.c_str());
	auto label{format(STR_C("Click: pole %s, card %d execution %d, A = %.2f m (%s, wires %s)"), Spot.data.pole_type.c_str(), Spot.card, Spot.data.execution, Spot.reach, where.c_str(), offset.c_str())};
	if (m_catenary.chaining && m_catenary.last >= 0 && catenary_scene_get(static_cast<std::size_t>(m_catenary.last)) != nullptr)
		label += "\n" + format(STR_C("with the span from the last pole, %.1f m"), glm::length(plan_of(wire_of(Spot.data)) - plan_of(wire_of(*catenary_scene_get(static_cast<std::size_t>(m_catenary.last))))));
	return label;
}

int editor_mode::catenary_hit() const
{
	screen_projection const projection;
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
		auto const distance{mouse_to_segment(projection, foot, foot + glm::dvec3{0.0, pole_top(*data), 0.0})};
		if (distance < best)
		{
			best = distance;
			result = static_cast<int>(index);
		}
	}
	return result;
}

// the chain whose span is under the cursor; -1 if none
int editor_mode::catenary_span_hit() const
{
	screen_projection const projection;
	auto best{kPolePick * kPolePick * 0.5f};
	int result{-1};
	glm::dvec3 const camera{Global.pCamera.Pos};
	for (std::size_t chain = 0; chain < catenary_chain_size(); ++chain)
	{
		if (catenary_chain_get(chain) == nullptr)
			continue;
		auto const members{catenary_chain_members(chain)};
		for (std::size_t k = 0; k + 1 < members.size(); ++k)
		{
			auto const *a{catenary_scene_get(members[k])};
			auto const *b{catenary_scene_get(members[k + 1])};
			if (a == nullptr || b == nullptr || glm::distance(wire_of(*a), camera) > kCatenaryRange)
				continue;
			auto const distance{mouse_to_segment(projection, wire_of(*a), wire_of(*b))};
			if (distance < best)
			{
				best = distance;
				result = static_cast<int>(chain);
			}
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
	// the chain it's in is shown with it
	std::size_t position{0};
	tool.chain_selected = catenary_chain_of(static_cast<std::size_t>(Index), position);
	// the settings of the panel take what the suspension has, so that changing them changes it
	if (auto const found{std::find(tool.poles.begin(), tool.poles.end(), data->pole_type)}; found != tool.poles.end())
		tool.pole = static_cast<int>(found - tool.poles.begin());
	tool.execution = std::clamp(data->execution, 1, 5);
	tool.swinging = data->card / 100 != 12;
	tool.insulators25 = data->insulators == "25";
	auto const decade{(data->card / 10) % 10};
	if (data->card / 100 == 11 || data->card / 100 == 12)
		tool.bend = (decade == 3 || decade == 4) ? 2 : 1;
	auto const *spot{catenary_spot_of(static_cast<std::size_t>(Index))};
	if (spot == nullptr)
	{
		tool.status = STR("The suspension isn't by any track: it can't be changed here");
		return;
	}
	auto const right{right_of(spot->forward) * static_cast<double>(spot->side)};
	auto const *catalog{catenary_scene_catalog()};
	auto const *pole{catalog != nullptr ? catalog->pole(data->pole_type) : nullptr};
	auto const railhead{spot->axis.y + kRailTop};
	auto const offset{glm::dot(plan_of(wire_of(*data)) - plan_of(spot->axis), right)};
	tool.stagger = static_cast<float>(std::abs(offset));
	tool.toward = offset >= 0.0;
	tool.height = static_cast<float>(data->at[1] - railhead);
	if (pole != nullptr)
		tool.clearance = static_cast<float>(glm::dot(plan_of(pole_of(*data)) - plan_of(spot->axis), right) - 0.5 * pole->depth(0.0));
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
	auto const &last{tool.undo.empty() ? catenary_change{} : tool.undo.back()};
	if (tool.undo.empty() || last.chain || last.index != index || last.removed || false == last.before.has_value())
		tool.undo.push_back({++tool.group, false, index, *current, std::nullopt, false});
	catenary_scene_update(index, data);
	tool.spots[index] = Spot;
	tool.spots[index].data = data;
	tool.probed = false;
}

// the poles of the chain get the settings of the selected one: pole, its face from the track, execution, height of the wires,
// insulators. Each keeps its own stagger, so the zigzag stays as it is
void editor_mode::catenary_copy(std::vector<std::size_t> const &Targets)
{
	auto &tool{m_catenary};
	++tool.group;
	std::size_t copied{0}, failed{0};
	for (auto const index : Targets)
	{
		if (static_cast<int>(index) == tool.selected)
			continue;
		auto const *current{catenary_scene_get(index)};
		auto const *found{catenary_spot_of(index)};
		if (current == nullptr || found == nullptr)
		{
			++failed;
			continue;
		}
		auto spot{*found};
		auto const right{right_of(spot.forward) * static_cast<double>(spot.side)};
		auto const offset{glm::dot(plan_of(wire_of(*current)) - plan_of(spot.axis), right)};
		auto const *catalog{catenary_scene_catalog()};
		auto const *own{catalog != nullptr ? catalog->pole(current->pole_type) : nullptr};
		auto const drop{own != nullptr && false == own->drop.empty()};
		catenary_compose(spot, tool.clearance, std::abs(offset), offset >= 0.0, drop ? current->pole_type : std::string{});
		if (drop)
			spot.data.pole[1] = current->pole[1];
		if (spot.card == 0)
		{
			++failed;
			continue;
		}
		auto data{spot.data};
		data.name = current->name;
		data.range_max = current->range_max;
		data.range_min = current->range_min;
		data.skin = current->skin;
		data.overrides = current->overrides;
		data.umbrella = current->umbrella;
		tool.undo.push_back({tool.group, false, index, *current, std::nullopt, false});
		catenary_scene_update(index, data);
		tool.spots[index] = spot;
		tool.spots[index].data = data;
		++copied;
	}
	tool.status = failed == 0 ? format(STR_C("Settings copied to %zu poles"), copied) : format(STR_C("Settings copied to %zu poles, %zu couldn't take them (no card for their A or not by a track)"), copied, failed);
}

// the stagger the next pole of the chain gets: the other way than the last one on a straight; after the user put three the same
// way in a row they're kept that way, until one goes the other way again
bool editor_mode::catenary_propose_toward(bool &Toward)
{
	auto &tool{m_catenary};
	if (false == tool.alternate || tool.last < 0)
		return false;
	std::vector<std::size_t> order;
	if (tool.chain >= 0 && catenary_chain_get(static_cast<std::size_t>(tool.chain)) != nullptr)
	{
		order = catenary_chain_members(static_cast<std::size_t>(tool.chain));
		if (false == tool.append)
			std::reverse(order.begin(), order.end());
	}
	else
		order = {static_cast<std::size_t>(tool.last)};
	int streak{0};
	bool last{true};
	for (auto item{order.rbegin()}; item != order.rend(); ++item)
	{
		auto const *spot{catenary_spot_of(*item)};
		if (spot == nullptr)
			break;
		if (streak == 0)
			last = spot->toward;
		else if (spot->toward != last || spot->curve)
			break;
		if (spot->curve)
		{
			// a curve decides the stagger itself: the next one simply goes the other way
			streak = 1;
			break;
		}
		++streak;
	}
	if (streak == 0)
		return false;
	Toward = streak >= 3 ? last : false == last;
	return true;
}

void editor_mode::catenary_place(catenary_spot const &Spot)
{
	auto &tool{m_catenary};
	++tool.group;
	auto data{Spot.data};
	std::string const file{tool.file};
	data.name = catenary_scene_new_name(file);
	auto const index{catenary_scene_add(file, data)};
	tool.spots[index] = Spot;
	tool.spots[index].data = data;
	tool.undo.push_back({tool.group, false, index, std::nullopt, std::nullopt, false});
	tool.selected = -1;
	tool.status = format(STR_C("Pole %s with card %d execution %d placed, A = %.2f m"), data.pole_type.c_str(), Spot.card, data.execution, Spot.reach);
	if (catenary_scene_parts(index) == 0)
		tool.status += STR(" - the cantilever didn't assemble, see the log");
	// the span from the last pole: a new chain of the two, or one more pole of the chain
	auto const *previous{tool.chaining && tool.last >= 0 ? catenary_scene_get(static_cast<std::size_t>(tool.last)) : nullptr};
	if (previous != nullptr && catenary_scene_file_of(static_cast<std::size_t>(tool.last)) == file)
	{
		auto previousdata{*previous};
		if (previousdata.name.empty())
		{
			// a pole from before the editor had no name, a chain needs it
			tool.undo.push_back({tool.group, false, static_cast<std::size_t>(tool.last), previousdata, std::nullopt, false});
			previousdata.name = catenary_scene_new_name(file);
			catenary_scene_update(static_cast<std::size_t>(tool.last), previousdata);
		}
		auto const *chain{tool.chain >= 0 ? catenary_chain_get(static_cast<std::size_t>(tool.chain)) : nullptr};
		if (chain != nullptr && catenary_chain_file(static_cast<std::size_t>(tool.chain)) == file)
		{
			auto changed{*chain};
			tool.undo.push_back({tool.group, true, static_cast<std::size_t>(tool.chain), std::nullopt, *chain, false});
			if (tool.append)
				changed.suspensions.push_back(data.name);
			else
				changed.suspensions.insert(changed.suspensions.begin(), data.name);
			catenary_chain_update(static_cast<std::size_t>(tool.chain), changed);
		}
		else
		{
			catenary::chain created;
			created.name = catenary_scene_new_chain_name(file);
			created.power = tool.power;
			created.voltage = tool.voltage;
			created.material = kMaterials[std::clamp(tool.material, 0, 1)];
			created.state = kStateValues[std::clamp(tool.state, 0, 2)];
			created.type = tool.networks.empty() ? std::string{} : tool.networks[std::clamp(tool.network, 0, static_cast<int>(tool.networks.size()) - 1)];
			created.suspensions = {previousdata.name, data.name};
			tool.chain = static_cast<int>(catenary_chain_add(file, created));
			tool.append = true;
			tool.undo.push_back({tool.group, true, static_cast<std::size_t>(tool.chain), std::nullopt, std::nullopt, false});
		}
		tool.chain_selected = tool.chain;
	}
	else
		tool.chain = -1; // nothing to go on from: the next span starts a new chain
	tool.last = static_cast<int>(index);
	// the next one: the other way, unless the user keeps putting them the same way
	if (bool toward{tool.toward}; catenary_propose_toward(toward))
		tool.toward = toward;
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
	if (Intent.what == track_intent::kind::catenary_gantry)
	{
		tool.gantry_selected = Intent.index;
		tool.selected = -1;
		tool.status = STR("The cantilevers now go on drops on this gantry: LMB over a track");
		return;
	}
	if (Intent.what == track_intent::kind::catenary_mast)
	{
		auto const point{catenary_mast_point()};
		if (false == tool.gantry_first.has_value())
		{
			tool.gantry_first = point;
			return;
		}
		glm::dvec3 second;
		std::string beam;
		if (false == catenary_gantry_second(point, second, beam) || tool.masts.empty())
			return;
		catenary::gantry gantry;
		std::string const file{tool.file};
		gantry.name = catenary_scene_new_gantry_name(file);
		gantry.beam = beam;
		gantry.high = tool.gantry_high;
		gantry.side = tool.gantry_side;
		gantry.masts[0] = tool.masts[std::clamp(tool.mast_first, 0, static_cast<int>(tool.masts.size()) - 1)];
		gantry.masts[1] = tool.masts[std::clamp(tool.mast_second, 0, static_cast<int>(tool.masts.size()) - 1)];
		auto const first{*tool.gantry_first};
		for (int i = 0; i < 3; ++i)
		{
			gantry.points[0][i] = first[i];
			gantry.points[1][i] = second[i];
		}
		auto const index{catenary_gantry_add(file, gantry)};
		catenary_change change;
		change.group = ++tool.group;
		change.gantry = true;
		change.index = index;
		tool.undo.push_back(change);
		tool.gantry_selected = static_cast<int>(index);
		tool.gantry_placing = false;
		tool.gantry_first.reset();
		tool.status = format(STR_C("Gantry %s with beam %s placed; LMB over a track puts a drop with a cantilever on it"), gantry.name.c_str(), beam.c_str());
		return;
	}
	if (Intent.what == track_intent::kind::catenary_chain)
	{
		tool.selected = -1;
		tool.chain_selected = Intent.index;
		if (auto const *chain{catenary_chain_get(static_cast<std::size_t>(Intent.index))}; chain != nullptr)
		{
			// the panel shows what the chain has
			std::snprintf(tool.power, sizeof(tool.power), "%s", chain->power.c_str());
			tool.voltage = static_cast<float>(chain->voltage);
			tool.material = chain->material == "al" ? 1 : 0;
			tool.state = chain->state == 2 ? 1 : chain->state == 1 ? 2 : 0;
			if (auto const found{std::find(tool.networks.begin(), tool.networks.end(), chain->type)}; found != tool.networks.end())
				tool.network = static_cast<int>(found - tool.networks.begin());
		}
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
	catenary_place(spot);
}

// the panel settings of the chain go to the selected chain
void editor_mode::catenary_chain_apply()
{
	auto &tool{m_catenary};
	if (tool.chain_selected < 0)
		return;
	auto const index{static_cast<std::size_t>(tool.chain_selected)};
	auto const *chain{catenary_chain_get(index)};
	if (chain == nullptr)
		return;
	auto changed{*chain};
	changed.power = tool.power;
	changed.voltage = tool.voltage;
	changed.material = kMaterials[std::clamp(tool.material, 0, 1)];
	changed.state = kStateValues[std::clamp(tool.state, 0, 2)];
	if (false == tool.networks.empty())
		changed.type = tool.networks[std::clamp(tool.network, 0, static_cast<int>(tool.networks.size()) - 1)];
	auto const &last{tool.undo.empty() ? catenary_change{} : tool.undo.back()};
	if (tool.undo.empty() || false == last.chain || last.index != index || false == last.chain_before.has_value())
		tool.undo.push_back({++tool.group, true, index, std::nullopt, *chain, false});
	catenary_chain_update(index, changed);
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
	++tool.group;
	// the chains it's in lose it: their lists are kept to be taken back
	for (std::size_t chain = 0; chain < catenary_chain_size(); ++chain)
	{
		auto const *data{catenary_chain_get(chain)};
		if (data != nullptr && false == current->name.empty() && catenary_chain_file(chain) == catenary_scene_file_of(index)
		    && std::find(data->suspensions.begin(), data->suspensions.end(), current->name) != data->suspensions.end())
			tool.undo.push_back({tool.group, true, chain, std::nullopt, *data, false});
	}
	tool.undo.push_back({tool.group, false, index, *current, std::nullopt, true});
	catenary_scene_remove(index);
	tool.selected = -1;
	if (tool.last == static_cast<int>(index))
		tool.last = -1;
	tool.status = STR("Pole removed");
	return true;
}

void editor_mode::catenary_undo()
{
	auto &tool{m_catenary};
	if (tool.undo.empty())
		return;
	auto const group{tool.undo.back().group};
	while (false == tool.undo.empty() && tool.undo.back().group == group)
	{
		auto const change{tool.undo.back()};
		tool.undo.pop_back();
		if (change.gantry)
		{
			if (change.gantry_before.has_value())
				catenary_gantry_update(change.index, *change.gantry_before);
			else
			{
				catenary_gantry_remove(change.index);
				if (tool.gantry_selected == static_cast<int>(change.index))
					tool.gantry_selected = -1;
			}
			continue;
		}
		if (change.chain)
		{
			if (change.chain_before.has_value())
				catenary_chain_update(change.index, *change.chain_before);
			else
			{
				catenary_chain_remove(change.index);
				if (tool.chain == static_cast<int>(change.index))
					tool.chain = -1;
			}
			continue;
		}
		if (false == change.before.has_value())
		{
			catenary_scene_remove(change.index);
			if (tool.last == static_cast<int>(change.index))
				tool.last = -1;
		}
		else if (change.removed)
			catenary_scene_restore(change.index, *change.before);
		else
			catenary_scene_update(change.index, *change.before);
		tool.spots.erase(change.index);
	}
	// the next span goes on from the end of the chain as it is now
	if (tool.chain >= 0 && catenary_chain_get(static_cast<std::size_t>(tool.chain)) != nullptr)
	{
		auto const members{catenary_chain_members(static_cast<std::size_t>(tool.chain))};
		tool.last = members.empty() ? -1 : static_cast<int>(tool.append ? members.back() : members.front());
	}
	else if (tool.last >= 0 && catenary_scene_get(static_cast<std::size_t>(tool.last)) == nullptr)
		tool.last = -1;
	tool.selected = -1;
	tool.probed = false;
	tool.status = STR("Taken back");
}

void editor_mode::render_catenary_chain_ui()
{
	auto &tool{m_catenary};
	auto const *chain{tool.chain_selected >= 0 ? catenary_chain_get(static_cast<std::size_t>(tool.chain_selected)) : nullptr};
	ImGui::Separator();
	if (chain == nullptr)
	{
		ImGui::TextDisabled("%s", STR_C("New chains take these settings; LMB on a span selects its chain"));
	}
	else
	{
		ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "%s", format(STR_C("Chain %s: %zu poles, %zu spans; changes apply to all of them"), chain->name.c_str(), chain->suspensions.size(), chain->suspensions.size() - 1).c_str());
	}
	auto changed{false};
	ImGui::PushItemWidth(180.0f);
	if (false == tool.networks.empty() && ImGui::BeginCombo(STR_C("Network type"), tool.networks[std::clamp(tool.network, 0, static_cast<int>(tool.networks.size()) - 1)].c_str()))
	{
		for (int i = 0; i < static_cast<int>(tool.networks.size()); ++i)
			if (ImGui::Selectable(tool.networks[i].c_str(), i == tool.network))
			{
				tool.network = i;
				changed = true;
			}
		ImGui::EndCombo();
	}
	if (ImGui::Combo(STR_C("Material"), &tool.material, kMaterials, 2))
		changed = true;
	std::string const statenames[] = {STR(kStates[0]), STR(kStates[1]), STR(kStates[2])};
	char const *const states[] = {statenames[0].c_str(), statenames[1].c_str(), statenames[2].c_str()};
	if (ImGui::Combo(STR_C("Wires"), &tool.state, states, 3))
		changed = true;
	ImGui::InputText(STR_C("Power source"), tool.power, sizeof(tool.power));
	if (ImGui::IsItemDeactivatedAfterEdit())
		changed = true;
	if (ImGui::InputFloat(STR_C("Voltage (V)"), &tool.voltage, 0.0f, 0.0f, "%.0f"))
		changed = true;
	ImGui::PopItemWidth();
	if (changed && chain != nullptr)
		catenary_chain_apply();
	if (chain != nullptr)
	{
		if (ImGui::Button(STR_C("Continue the chain from its end")))
		{
			tool.chain = tool.chain_selected;
			tool.append = true;
			auto const members{catenary_chain_members(static_cast<std::size_t>(tool.chain))};
			tool.last = members.empty() ? -1 : static_cast<int>(members.back());
			tool.selected = -1;
			if (bool toward{tool.toward}; catenary_propose_toward(toward))
				tool.toward = toward;
		}
		ImGui::SameLine();
		if (ImGui::Button(STR_C("from its start")))
		{
			tool.chain = tool.chain_selected;
			tool.append = false;
			auto const members{catenary_chain_members(static_cast<std::size_t>(tool.chain))};
			tool.last = members.empty() ? -1 : static_cast<int>(members.front());
			tool.selected = -1;
			if (bool toward{tool.toward}; catenary_propose_toward(toward))
				tool.toward = toward;
		}
	}
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
		ImGui::Checkbox(STR_C("Chain: a span from the last pole"), &tool.chaining);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", STR_C("Each pole placed is joined with a span to the one placed before it, one chain from anchor to anchor"));
		if (tool.last >= 0 && catenary_scene_get(static_cast<std::size_t>(tool.last)) != nullptr)
		{
			ImGui::SameLine();
			if (ImGui::Button(STR_C("End the chain")))
			{
				tool.chain = -1;
				tool.last = -1;
			}
		}
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
		ImGui::SetTooltip("%s", STR_C("The next pole of the chain gets the wires the other way than the last one; after three the same way in a row it keeps that way, until one goes the other way"));
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
	ImGui::RadioButton(STR_C("Wind zone I"), &tool.zone, 1);
	ImGui::SameLine();
	ImGui::RadioButton(STR_C("Wind zone II"), &tool.zone, 2);
	if (tool.networks.empty())
		ImGui::TextDisabled("%s", STR_C("No data/traction_layout.txt: the spans aren't checked"));
	ImGui::PopItemWidth();

	// the selected pole follows the settings at once
	if (tool.selected >= 0 && changed)
	{
		if (auto const *found{catenary_spot_of(static_cast<std::size_t>(tool.selected))}; found != nullptr)
		{
			auto spot{*found};
			spot.curve = tool.bend == 2 || (tool.bend == 0 && spot.radius > 0.0 && spot.radius < kCurveRadius);
			// a drop on a gantry stays the drop it is, hanging from the beam
			auto const *current{catenary_scene_get(static_cast<std::size_t>(tool.selected))};
			auto const *own{current != nullptr ? catalog->pole(current->pole_type) : nullptr};
			auto const drop{own != nullptr && false == own->drop.empty()};
			catenary_compose(spot, tool.clearance, tool.stagger, tool.toward, drop ? current->pole_type : std::string{});
			if (drop)
				spot.data.pole[1] = current->pole[1];
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
			auto const *spot{catenary_spot_of(static_cast<std::size_t>(tool.selected))};
			ImGui::Text("%s", format(STR_C("Card %d execution %d, A = %.2f m, %zu parts"), data->card, data->execution, spot != nullptr ? spot->reach : 0.0, catenary_scene_parts(static_cast<std::size_t>(tool.selected))).c_str());
			if (false == tool.probe_good)
				ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "%s", STR_C("The cantilever doesn't assemble:"));
			for (auto const &warning : tool.probe.warnings)
				ImGui::TextWrapped("- %s", warning.c_str());
			// copying to the neighbours in the chain
			std::size_t position{0};
			auto const chain{catenary_chain_of(static_cast<std::size_t>(tool.selected), position)};
			if (chain >= 0)
			{
				auto const members{catenary_chain_members(static_cast<std::size_t>(chain))};
				ImGui::TextUnformatted(STR_C("Copy to:"));
				ImGui::SameLine();
				if (position > 0 && ImGui::SmallButton(STR_C("previous")))
					catenary_copy({members[position - 1]});
				ImGui::SameLine();
				if (position + 1 < members.size() && ImGui::SmallButton(STR_C("next")))
					catenary_copy({members[position + 1]});
				ImGui::SameLine();
				if (ImGui::SmallButton(STR_C("both")))
				{
					std::vector<std::size_t> targets;
					if (position > 0)
						targets.push_back(members[position - 1]);
					if (position + 1 < members.size())
						targets.push_back(members[position + 1]);
					catenary_copy(targets);
				}
				ImGui::SameLine();
				if (ImGui::SmallButton(STR_C("the whole chain")))
					catenary_copy(members);
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s", STR_C("The pole, its face from the track, execution, height of the wires and insulators; each pole keeps its stagger"));
			}
			if (ImGui::Button(STR_C("Remove the pole")))
				catenary_delete_selected();
			ImGui::SameLine();
			if (ImGui::Button(STR_C("Done")))
				tool.selected = -1;
		}
	}
	render_catenary_chain_ui();
	// gantries
	ImGui::Separator();
	if (false == tool.masts.empty())
	{
		ImGui::PushItemWidth(120.0f);
		auto const combo = [&](char const *Label, int &Chosen) {
			if (ImGui::BeginCombo(Label, tool.masts[std::clamp(Chosen, 0, static_cast<int>(tool.masts.size()) - 1)].c_str()))
			{
				for (int i = 0; i < static_cast<int>(tool.masts.size()); ++i)
					if (ImGui::Selectable(tool.masts[i].c_str(), i == Chosen))
						Chosen = i;
				ImGui::EndCombo();
			}
		};
		combo(STR_C("First mast"), tool.mast_first);
		ImGui::SameLine();
		combo(STR_C("Second mast"), tool.mast_second);
		ImGui::PopItemWidth();
		ImGui::TextUnformatted(STR_C("Beam:"));
		ImGui::SameLine();
		if (ImGui::RadioButton(STR_C("lower mount"), false == tool.gantry_high))
			tool.gantry_high = false;
		ImGui::SameLine();
		if (ImGui::RadioButton(STR_C("upper mount"), tool.gantry_high))
			tool.gantry_high = true;
		ImGui::SameLine();
		if (ImGui::Button(tool.gantry_side > 0 ? STR_C("Beam on side +") : STR_C("Beam on side -")))
			tool.gantry_side = -tool.gantry_side;
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", STR_C("On which side of the masts, along the track, the beam is fixed"));
		if (ImGui::Button(tool.gantry_placing ? STR_C("Cancel the gantry") : STR_C("Place a gantry")))
		{
			tool.gantry_placing = false == tool.gantry_placing;
			tool.gantry_first.reset();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", STR_C("LMB the first mast, LMB the second: the beam is the one of the catalogue nearest to that distance"));
		if (auto const *gantry{tool.gantry_selected >= 0 ? catenary_gantry_get(static_cast<std::size_t>(tool.gantry_selected)) : nullptr}; gantry != nullptr)
		{
			ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "%s", format(STR_C("Gantry %s, beam %s: the cantilevers go on drops on it"), gantry->name.c_str(), gantry->beam.c_str()).c_str());
			if (ImGui::Button(STR_C("Remove the gantry")))
			{
				catenary_change change;
				change.group = ++tool.group;
				change.gantry = true;
				change.index = static_cast<std::size_t>(tool.gantry_selected);
				change.gantry_before = *gantry;
				tool.undo.push_back(change);
				catenary_gantry_remove(static_cast<std::size_t>(tool.gantry_selected));
				tool.gantry_selected = -1;
			}
			ImGui::SameLine();
			if (ImGui::Button(STR_C("Back to the poles")))
				tool.gantry_selected = -1;
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
	// gantries: the masts and the line of the bottom of the beam, where the drops hang from
	auto const *catalog{catenary_scene_catalog()};
	for (std::size_t index = 0; index < catenary_gantry_size(); ++index)
	{
		glm::dvec3 from, to;
		double bottom{0.0};
		auto const *gantry{catenary_gantry_get(index)};
		if (gantry == nullptr || false == catenary_gantry_beam(index, from, to, bottom) || glm::distance(from, camera) > kCatenaryRange)
			continue;
		auto const selected{static_cast<int>(index) == tool.gantry_selected};
		auto const hovered{m_intent.what == track_intent::kind::catenary_gantry && m_intent.index == static_cast<int>(index)};
		auto const broken{catenary_gantry_parts(index) == 0};
		auto const colour{selected ? overlay_color::selected : hovered ? overlay_color::highlight : broken ? overlay_color::invalid : IM_COL32(255, 160, 60, 170)};
		projection.line(drawlist, from, to, colour, selected || hovered ? 3.0f : 1.5f);
		for (int mast = 0; mast < 2; ++mast)
		{
			auto const *type{catalog != nullptr ? catalog->mast(gantry->masts[mast]) : nullptr};
			glm::dvec3 const foot{gantry->points[mast][0], gantry->points[mast][1], gantry->points[mast][2]};
			projection.line(drawlist, foot, foot + glm::dvec3{0.0, type != nullptr ? type->height : 8.0, 0.0}, colour, selected || hovered ? 3.0f : 1.5f);
		}
		ImVec2 middle;
		if ((selected || hovered || broken) && projection.project(0.5 * (from + to) + glm::dvec3{0.0, 1.2, 0.0}, middle))
			drawlist->AddText(middle, colour, (gantry->name + "  " + gantry->beam).c_str());
	}
	// the gantry being placed: the first mast, and the second one where the beam would reach
	if (tool.gantry_placing && tool.gantry_first.has_value())
	{
		auto const first{*tool.gantry_first};
		projection.line(drawlist, first, first + glm::dvec3{0.0, 8.0, 0.0}, overlay_color::grip, 3.0f);
		glm::dvec3 second;
		std::string beam;
		if (catenary_gantry_second(catenary_mast_point(), second, beam))
		{
			projection.line(drawlist, second, second + glm::dvec3{0.0, 8.0, 0.0}, overlay_color::marked, 3.0f);
			projection.line(drawlist, first + glm::dvec3{0.0, 7.5, 0.0}, second + glm::dvec3{0.0, 7.5, 0.0}, overlay_color::marked, 2.0f);
			ImVec2 middle;
			if (projection.project(0.5 * (first + second) + glm::dvec3{0.0, 8.2, 0.0}, middle))
				drawlist->AddText(middle, overlay_color::marked, format("%s  %.1f m", beam.c_str(), glm::length(plan_of(second) - plan_of(first))).c_str());
		}
	}
	for (std::size_t index = 0; index < catenary_scene_size(); ++index)
	{
		auto const *data{catenary_scene_get(index)};
		if (data == nullptr || false == data->has_at || glm::distance(pole_of(*data), camera) > kCatenaryRange)
			continue;
		auto const selected{static_cast<int>(index) == tool.selected};
		auto const hovered{m_intent.what == track_intent::kind::catenary_select && m_intent.index == static_cast<int>(index)};
		auto const broken{catenary_scene_parts(index) == 0};
		auto const last{static_cast<int>(index) == tool.last && tool.chaining};
		auto const colour{selected ? overlay_color::selected : hovered ? overlay_color::highlight : broken ? overlay_color::invalid : last ? overlay_color::grip : IM_COL32(255, 210, 60, 150)};
		draw_pole(*data, colour, selected || hovered || last ? 3.0f : 1.5f);
		ImVec2 head;
		if ((selected || hovered || broken) && projection.project(pole_of(*data) + glm::dvec3{0.0, pole_top(*data) + 0.3, 0.0}, head))
		{
			auto const label{format("%d-%d", data->card, data->execution) + (broken ? std::string{"  "} + STR("doesn't assemble") : std::string{})};
			drawlist->AddText(ImVec2(head.x + 6.0f, head.y - 6.0f), colour, label.c_str());
		}
	}
	// spans against the catalogue: green within the limit, red over it; the selected chain brighter
	auto const &network{tool.networks.empty() ? std::string{} : tool.networks[std::clamp(tool.network, 0, static_cast<int>(tool.networks.size()) - 1)]};
	for (auto const &span : spans_around(camera, network, tool.zone))
	{
		auto const a{wire_of(*catenary_scene_get(span.from))};
		auto const b{wire_of(*catenary_scene_get(span.to))};
		auto const over{span.limit > 0.0 && span.length > span.limit + 0.05};
		auto const chosen{span.chain >= 0 && (span.chain == tool.chain_selected || (m_intent.what == track_intent::kind::catenary_chain && m_intent.index == span.chain))};
		auto const colour{over ? overlay_color::invalid : chosen ? overlay_color::selected : span.chain >= 0 ? IM_COL32(90, 230, 120, 200) : IM_COL32(90, 230, 120, 110)};
		projection.line(drawlist, a, b, colour, over || chosen ? 2.5f : 1.2f);
		ImVec2 middle;
		if (projection.project(0.5 * (a + b), middle))
		{
			auto const label{span.limit > 0.0 ? format("%.1f / %.0f m", span.length, span.limit) : format("%.1f m", span.length)};
			auto const text{span.radius > 0.0 && span.radius < kCurveRadius ? label + format("  R %.0f", span.radius) : label};
			drawlist->AddText(ImVec2(middle.x + 4.0f, middle.y + 2.0f), colour, text.c_str());
		}
	}
	// what a click puts there, with the span from the last pole
	if (m_intent.what == track_intent::kind::catenary_place && m_hover.track != nullptr)
	{
		catenary_spot spot;
		if (catenary_spot_at(*m_hover.track, m_hover.path, m_hover.point, cursor_level(m_hover.point.y), 0, spot))
		{
			auto const colour{spot.card != 0 ? overlay_color::marked : overlay_color::invalid};
			draw_pole(spot.data, colour, 3.0f);
			projection.line(drawlist, spot.axis + glm::dvec3{0.0, kRailTop, 0.0}, pole_of(spot.data), colour, 1.5f);
			if (tool.chaining && tool.last >= 0)
				if (auto const *previous{catenary_scene_get(static_cast<std::size_t>(tool.last))}; previous != nullptr)
					projection.line(drawlist, wire_of(*previous), wire_of(spot.data), colour, 1.5f);
		}
	}
}
