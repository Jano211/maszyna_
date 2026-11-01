/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "stdafx.h"
#include "editor/editorSettings.hpp"
#include "utilities/Logs.h"
#include "utilities/utilities.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>

editorSettings EditorSettings;

namespace
{
namespace fs = std::filesystem;

fs::path settings_path()
{
	fs::path p = user_config_path("eu07_editor.ini");
	return p.empty() ? fs::path("eu07_editor.ini") : p;
}

const char *scheme_to_string(editorSettings::movement_scheme scheme)
{
	return scheme == editorSettings::movement_scheme::legacy ? "legacy" : "wsad";
}

// numbers are stored locale-independent
template <typename Type_> void parse(std::string const &Text, Type_ &Out)
{
	std::istringstream stream(Text);
	stream.imbue(std::locale::classic());
	Type_ value;
	if (stream >> value)
		Out = value;
}

const std::string origin_prefix{"ortho_origin@"};
const std::string sheets_prefix{"nmt_sheets@"};
const std::string catenary_prefix{"catenary_file@"};

// settings are whitespace separated key/value pairs, so the scenery name used in the key can't contain spaces
std::string origin_key(std::string const &Scenery)
{
	std::string key = origin_prefix + Scenery;
	for (char &c : key)
		if (std::isspace(static_cast<unsigned char>(c)))
			c = '_';
	return key;
}
}

bool editorSettings::load()
{
	const fs::path path = settings_path();
	std::error_code ec;
	if (!fs::exists(path, ec))
		return false;

	std::ifstream stream(path);
	if (!stream.is_open())
		return false;

	std::string key, value;
	while (stream >> key >> value)
	{
		if (key == "movement_scheme")
			m_movement = (value == "legacy") ? movement_scheme::legacy : movement_scheme::wsad;
		else if (key == "ortho_radius")
			parse(value, m_orthophoto.radius);
		else if (key == "ortho_height")
			parse(value, m_orthophoto.height);
		else if (key == "ortho_opacity")
			parse(value, m_orthophoto.opacity);
		else if (key == "ortho_year")
			parse(value, m_orthophoto.year);
		else if (key == "ortho_4k")
			m_orthophoto.hires = (value == "1");
		else if (key == "ortho_drape")
			m_orthophoto.drape = (value == "1");
		else if (key == "ortho_lift")
			parse(value, m_orthophoto.lift);
		else if (key == "ortho_in_scene")
			m_orthophoto.in_scene = (value == "1");
		else if (key == "platform_edge")
			parse(value, m_platform_edge);
		else if (key == "signal_offset")
			parse(value, m_signal_offset);
		else if (key == "nodebank_previews")
			m_nodebank_previews = (value == "1");
		else if (key == "nodebank_card")
			parse(value, m_nodebank_card);
		else if (key == "colours")
			parse(value, m_palette);
		else if (key == "first_steps")
			m_first_steps = (value == "1");
		else if (key == "terrain_offer")
			m_terrain_offer = (value == "1");
		else if (key == "track_tools_all")
			m_track_tools_more = (value == "1");
		else if (key == "top_ballast")
			m_top_ballast = (value == "1");
		else if (key == "nmt_shallower")
			parse(value, m_nmt_shallower);
		else if (key.rfind("terrain_brush_", 0) == 0)
		{
			// radius;strength
			int tool{-1};
			parse(key.substr(14), tool);
			auto const separator = value.find(';');
			if (tool < 0 || separator == std::string::npos)
				continue;
			float radius{12.f}, strength{4.f};
			parse(value.substr(0, separator), radius);
			parse(value.substr(separator + 1), strength);
			m_terrain_brushes[tool] = {radius, strength};
		}
		else if (key.rfind(sheets_prefix, 0) == 0)
		{
			// on;correction;folder, the spaces of the folder written as |
			auto const first = value.find(';');
			auto const second = first == std::string::npos ? std::string::npos : value.find(';', first + 1);
			if (second == std::string::npos)
				continue;
			nmt_sheets sheets;
			sheets.on = value.substr(0, first) == "1";
			parse(value.substr(first + 1, second - first - 1), sheets.correction);
			sheets.folder = value.substr(second + 1);
			std::replace(sheets.folder.begin(), sheets.folder.end(), '|', ' ');
			m_sheets[key] = sheets;
		}
		else if (key.rfind(catenary_prefix, 0) == 0)
		{
			// the spaces of the file written as |
			std::replace(value.begin(), value.end(), '|', ' ');
			m_catenary_files[key] = value;
		}
		else if (key.rfind(origin_prefix, 0) == 0)
		{
			// northing;easting
			auto const separator = value.find(';');
			if (separator == std::string::npos)
				continue;
			double north{0.0}, east{0.0};
			parse(value.substr(0, separator), north);
			parse(value.substr(separator + 1), east);
			m_orthophoto_origins[key] = {north, east};
		}
	}
	return true;
}

bool editorSettings::save()
{
	const fs::path path = settings_path();
	std::error_code ec;
	fs::create_directories(path.parent_path(), ec);

	std::ofstream stream(path, std::ios::trunc);
	if (!stream.is_open())
	{
		ErrorLog("failed to save editor settings");
		return false;
	}

	stream.imbue(std::locale::classic());
	stream << "movement_scheme " << scheme_to_string(m_movement) << "\n";
	stream << "ortho_radius " << m_orthophoto.radius << "\n";
	stream << "ortho_height " << m_orthophoto.height << "\n";
	stream << "ortho_opacity " << m_orthophoto.opacity << "\n";
	stream << "ortho_year " << m_orthophoto.year << "\n";
	stream << "ortho_4k " << (m_orthophoto.hires ? 1 : 0) << "\n";
	stream << "ortho_drape " << (m_orthophoto.drape ? 1 : 0) << "\n";
	stream << "ortho_lift " << m_orthophoto.lift << "\n";
	stream << "ortho_in_scene " << (m_orthophoto.in_scene ? 1 : 0) << "\n";
	stream << "platform_edge " << m_platform_edge << "\n";
	stream << "signal_offset " << m_signal_offset << "\n";
	stream << "nodebank_previews " << (m_nodebank_previews ? 1 : 0) << "\n";
	stream << "nodebank_card " << m_nodebank_card << "\n";
	stream << "colours " << m_palette << "\n";
	stream << "first_steps " << (m_first_steps ? 1 : 0) << "\n";
	stream << "terrain_offer " << (m_terrain_offer ? 1 : 0) << "\n";
	stream << "track_tools_all " << (m_track_tools_more ? 1 : 0) << "\n";
	stream << "top_ballast " << (m_top_ballast ? 1 : 0) << "\n";
	stream << "nmt_shallower " << m_nmt_shallower << "\n";
	for (auto const &brush : m_terrain_brushes)
		stream << "terrain_brush_" << brush.first << " " << brush.second.first << ";" << brush.second.second << "\n";
	stream << std::fixed << std::setprecision(3);
	for (auto const &origin : m_orthophoto_origins)
		stream << origin.first << " " << origin.second.first << ";" << origin.second.second << "\n";
	for (auto const &entry : m_sheets)
	{
		auto folder = entry.second.folder;
		std::replace(folder.begin(), folder.end(), ' ', '|');
		stream << entry.first << " " << (entry.second.on ? 1 : 0) << ";" << entry.second.correction << ";" << folder << "\n";
	}
	for (auto const &entry : m_catenary_files)
	{
		auto file = entry.second;
		std::replace(file.begin(), file.end(), ' ', '|');
		stream << entry.first << " " << file << "\n";
	}
	return true;
}

bool editorSettings::orthophoto_origin(std::string const &Scenery, double &North, double &East) const
{
	auto const lookup = m_orthophoto_origins.find(origin_key(Scenery));
	if (lookup == m_orthophoto_origins.end())
		return false;
	North = lookup->second.first;
	East = lookup->second.second;
	return true;
}

void editorSettings::orthophoto_origin(std::string const &Scenery, double North, double East)
{
	m_orthophoto_origins[origin_key(Scenery)] = {North, East};
}

editorSettings::nmt_sheets editorSettings::sheets(std::string const &Scenery) const
{
	auto key = sheets_prefix + Scenery;
	for (char &c : key)
		if (std::isspace(static_cast<unsigned char>(c)))
			c = '_';
	auto const lookup = m_sheets.find(key);
	return lookup == m_sheets.end() ? nmt_sheets{} : lookup->second;
}

void editorSettings::sheets(std::string const &Scenery, nmt_sheets const &Sheets)
{
	auto key = sheets_prefix + Scenery;
	for (char &c : key)
		if (std::isspace(static_cast<unsigned char>(c)))
			c = '_';
	m_sheets[key] = Sheets;
}

std::string editorSettings::catenary_file(std::string const &Scenery) const
{
	auto key = catenary_prefix + Scenery;
	for (char &c : key)
		if (std::isspace(static_cast<unsigned char>(c)))
			c = '_';
	auto const lookup = m_catenary_files.find(key);
	return lookup == m_catenary_files.end() ? std::string{} : lookup->second;
}

void editorSettings::catenary_file(std::string const &Scenery, std::string const &File)
{
	auto key = catenary_prefix + Scenery;
	for (char &c : key)
		if (std::isspace(static_cast<unsigned char>(c)))
			c = '_';
	if (File.empty())
		m_catenary_files.erase(key);
	else
		m_catenary_files[key] = File;
}

bool editorSettings::terrain_brush_stored(int const Tool, float &Radius, float &Strength) const
{
	auto const lookup = m_terrain_brushes.find(Tool);
	if (lookup == m_terrain_brushes.end())
		return false;
	Radius = lookup->second.first;
	Strength = lookup->second.second;
	return true;
}

void editorSettings::terrain_brush(int const Tool, float const Radius, float const Strength)
{
	m_terrain_brushes[Tool] = {Radius, Strength};
}
