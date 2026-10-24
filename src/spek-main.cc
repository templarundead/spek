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
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "spek-cli.h"
#include "spek-i18n.h"

#ifdef OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

#if SPEK_BUILD_GUI
#include <wx/app.h>
#include <wx/init.h>

#include "spek-gui.h"
#endif

#if SPEK_BUILD_GUI
namespace
{

bool is_console_info_request(int argc, char **argv)
{
    if (argc == 1) {
        return true;
    }
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument(argv[i] == nullptr ? "" : argv[i]);
        if (argument == "-h" || argument == "--help" || argument == "-V"
            || argument == "--version") {
            return true;
        }
    }
    return false;
}

} // namespace
#endif

#ifdef OS_WIN
namespace
{
bool attach_parent_console()
{
    // A GUI-subsystem executable has no console of its own. Attach only for
    // the command-line modes; normal GUI startup must not create or detach a
    // console belonging to the terminal that launched us.
    if (GetConsoleWindow() != nullptr) {
        return true;
    }
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        return false;
    }

    FILE *stream = nullptr;
    (void) freopen_s(&stream, "CONIN$", "r", stdin);
    (void) freopen_s(&stream, "CONOUT$", "w", stdout);
    (void) freopen_s(&stream, "CONOUT$", "w", stderr);
    return true;
}
} // namespace
#endif

#if defined(OS_WIN) && SPEK_BUILD_GUI
namespace
{

// WinMain does not receive argc/argv. Reconstruct them from the native wide
// command line instead of using the ANSI CRT globals, so file names remain
// valid Unicode when the common dispatcher is used.
class WindowsCommandLine
{
public:
    WindowsCommandLine()
    {
        int wide_count = 0;
        wchar_t **wide_argv = CommandLineToArgvW(GetCommandLineW(), &wide_count);
        if (wide_argv == nullptr) {
            return;
        }

        bool converted = true;
        for (int i = 0; i < wide_count; ++i) {
            const int size = WideCharToMultiByte(
                CP_UTF8, 0, wide_argv[i], -1, nullptr, 0, nullptr, nullptr
            );
            if (size <= 0) {
                converted = false;
                break;
            }
            std::string argument(static_cast<std::size_t>(size), '\0');
            if (WideCharToMultiByte(
                    CP_UTF8, 0, wide_argv[i], -1, argument.data(), size, nullptr, nullptr
                ) == 0) {
                converted = false;
                break;
            }
            this->strings.push_back(std::move(argument));
        }
        LocalFree(wide_argv);

        if (!converted) {
            return;
        }
        this->arguments.reserve(this->strings.size() + 1);
        for (auto& argument : this->strings) {
            this->arguments.push_back(argument.data());
        }
        this->arguments.push_back(nullptr);
        this->ready = true;
    }

    bool is_ready() const { return this->ready; }
    int argc() const { return static_cast<int>(this->strings.size()); }
    char **argv() { return this->arguments.data(); }

private:
    std::vector<std::string> strings;
    std::vector<char *> arguments;
    bool ready = false;
};

} // namespace
#endif

int spek_main(int argc, char **argv)
{
    // Select the execution mode before doing anything which could initialize
    // the GUI. Console help and no-argument terminal invocations are handled
    // without wx at all. Analysis uses a console app only, never a GUI app or
    // wxFrame. A Windows Explorer launch without a console opens the GUI.
#if SPEK_BUILD_GUI
    bool use_cli = spek::wants_cli(argc, argv);
#ifdef OS_WIN
    if (argc == 1) {
        // A console invocation with no arguments is the documented help
        // request. Explorer has no parent console, so in that case launch the
        // GUI instead of silently printing help into a non-existent console.
        use_cli = attach_parent_console();
    } else if (use_cli) {
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

#if SPEK_BUILD_GUI
    // Informational console requests must remain useful even on a machine
    // without a display or a working wx runtime.
    if (is_console_info_request(argc, argv)) {
        return spek::run_cli(argc, argv);
    }

    // Use wx's console lifecycle for analysis, but never enter its event
    // loop. In particular, wxAppConsole does not call gtk_init_check(), so
    // this remains suitable for SSH/headless use with a wxGTK build.
    wxApp::SetInstance(new wxAppConsole());
    if (!wxInitialize(argc, argv)) {
        return 1;
    }
    const int exit_code = spek::run_cli(argc, argv);
    wxUninitialize();
    return exit_code;
#else
    return spek::run_cli(argc, argv);
#endif
}

int main(int argc, char **argv)
{
    return spek_main(argc, argv);
}

#if defined(OS_WIN) && SPEK_BUILD_GUI
// Let the Windows linker select WinMainCRTStartup. Using the normal GUI CRT
// entry is important for both static initialization and orderly process exit;
// the old /ENTRY:mainCRTStartup override bypassed this platform convention.
extern "C" int WINAPI WinMain(
    HINSTANCE, HINSTANCE, LPSTR, int
)
{
    WindowsCommandLine command_line;
    if (!command_line.is_ready()) {
        return 1;
    }
    return spek_main(command_line.argc(), command_line.argv());
}
#endif
