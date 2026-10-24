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

#include "spek-config.h"

#include "spek-cli.h"
#include "spek-i18n.h"

#ifdef OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#if SPEK_BUILD_GUI
#include "spek-gui.h"
#endif

int main(int argc, char **argv)
{
#if SPEK_HAVE_INTL
    spek_i18n_init_gettext();
#endif

#if SPEK_BUILD_GUI
    if (!spek::wants_cli(argc, argv)) {
#ifdef OS_WIN
        // Keep the console for command-line use, but don't leave a console
        // window beside the GUI when the executable is started normally.
        // If the user has a terminal open already, this only detaches Spek
        // from it and never closes the terminal itself.
        FreeConsole();
#endif
        // Hand the command line over to wxWidgets, the graphical interface
        // registers its own translator so that the core strings are
        // translated through wx as well.
        return spek_run_gui(argc, argv);
    }
#endif

    return spek::run_cli(argc, argv);
}
