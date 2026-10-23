// Copyright (C) 2010-2013 Alexander Kojevnikov and contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <wx/string.h>
#include <wx/window.h>

// Platform-specific initialisation code.
void spek_platform_init();

// Not quite XDG-compatible, but close enough.
wxString spek_platform_config_path(const wxString& app_name);

// Setting non-default locale under GTK+ is tricky (see e.g. how FileZilla does it). We will
// just disable the language setting for GTK+ users and will always use the system locale.
bool spek_platform_can_change_language();

// Fonts are smaller on OSX.
double spek_platform_font_scale();

// True when the operating system is using a dark theme.
bool spek_platform_is_dark();

// Asks the window manager to draw the window decorations (title bar,
// scroll bars) in dark mode. Only needed on Windows, every other platform
// follows the system theme on its own.
void spek_platform_enable_dark_mode(wxWindow *window);
