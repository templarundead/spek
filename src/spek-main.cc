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

#include <cstdio>
#include <cstdlib>

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

#ifdef OS_WIN
namespace
{
void attach_parent_console()
{
    // A GUI-subsystem executable has no console of its own. Attach only for
    // the command-line modes; normal GUI startup must not create or detach a
    // console belonging to the terminal that launched us.
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        return;
    }

    FILE *stream = nullptr;
    (void) freopen_s(&stream, "CONIN$", "r", stdin);
    (void) freopen_s(&stream, "CONOUT$", "w", stdout);
    (void) freopen_s(&stream, "CONOUT$", "w", stderr);
}
} // namespace
#endif

int main(int argc, char **argv)
{
    // Select the execution mode before doing anything which could initialize
    // wxWidgets. In particular, the CLI must not call wxInitialize(): this is
    // a wxGTK build and even base-module initialization is unnecessary for the
    // toolkit-independent analysis pipeline.
#if SPEK_BUILD_GUI
    const bool use_cli = spek::wants_cli(argc, argv);
#ifdef OS_WIN
    if (use_cli) {
        attach_parent_console();
    }
#endif
#endif

#if defined(OS_UNIX) && SPEK_STATIC_LINK
    if (std::getenv("SPEK_TRACE_STARTUP") != nullptr) {
        std::fputs("Spek: main() reached\n", stderr);
    }
#endif

#if SPEK_HAVE_INTL
    spek_i18n_init_gettext();
#endif

#if SPEK_BUILD_GUI
    if (!use_cli) {
        // Hand the command line over to wxWidgets only for an actual GUI
        // invocation. The graphical interface registers its own translator so
        // that the core strings are translated through wx as well.
        return spek_run_gui(argc, argv);
    }
#endif

    // Run the analysis directly, without constructing wxApp, wxFrame or any
    // other wx object. Correspondingly, no wxUninitialize() is needed here.
    return spek::run_cli(argc, argv);
}
