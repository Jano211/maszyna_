/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <array>
#include <cstring>

#include "imgui/imgui.h"

// the debug windows of the renderers (opengl 4.6 and vulkan 1.4): each opened on its own from Windows > Renderer of the menu
// (debug mode) and closed with its own close button. they used to open all at once with "Headlight config", without a way to
// close any of them (Jan 2026-11-14)
namespace gfx
{

struct debug_window_entry
{
	char const *name;
	bool open;
};

inline std::array<debug_window_entry, 8> &debug_windows()
{
	static std::array<debug_window_entry, 8> windows{{{"Headlight config", false},
	                                                    {"Light clusters", false},
	                                                    {"Sun shadows", false},
	                                                    {"Weather", false},
	                                                    {"Particles", false},
	                                                    {"Sky and exposure", false},
	                                                    {"Instance culling", false},
	                                                    {"Pickbuffer", false}}};
	return windows;
}

// the open state of a window by its name (a name not on the list: a flag of its own, closed)
inline bool &debug_window(char const *Name)
{
	for (auto &window : debug_windows())
		if (std::strcmp(window.name, Name) == 0)
			return window.open;
	static bool unknown{false};
	unknown = false;
	return unknown;
}

inline bool debug_windows_any()
{
	for (auto const &window : debug_windows())
		if (window.open)
			return true;
	return false;
}

inline void debug_windows_all(bool const Open)
{
	for (auto &window : debug_windows())
		window.open = Open;
}

// the items of the menu: every window, then all of them on and off
inline void debug_windows_menu()
{
	for (auto &window : debug_windows())
		ImGui::MenuItem(window.name, nullptr, &window.open);
	ImGui::Separator();
	if (ImGui::MenuItem("Open all"))
		debug_windows_all(true);
	if (ImGui::MenuItem("Close all"))
		debug_windows_all(false);
}

} // namespace gfx
