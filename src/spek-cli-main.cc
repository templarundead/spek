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

// Windows has two different shell contracts: a GUI-subprocess is started
// asynchronously by cmd/PowerShell, while a console process is awaited. Keep
// spek.exe as the GUI application and use this small, wx-free console frontend
// for interactive command-line work. In particular, do not implement this as
// a wrapper around the GUI executable: direct console handles, redirection and
// the exit status then remain completely ordinary.

#include "spek-config.h"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include <windows.h>

#include "spek-cli.h"
#include "spek-i18n.h"

namespace
{

int run(int argc, char **argv)
{
#if SPEK_HAVE_INTL
    spek_i18n_init_gettext();
#endif
    return spek::run_cli(argc, argv);
}

} // namespace

// wmain gives us the command line without lossy conversion through the
// process code page. The rest of Spek uses UTF-8 on every platform.
int wmain(int argc, wchar_t **wide_argv)
{
    std::vector<std::string> strings;
    strings.reserve(static_cast<std::size_t>(argc));

    for (int i = 0; i < argc; ++i) {
        const int size = WideCharToMultiByte(
            CP_UTF8, 0, wide_argv[i], -1, nullptr, 0, nullptr, nullptr
        );
        if (size <= 0) {
            std::fwprintf(stderr, L"spek: cannot convert the command line to UTF-8\n");
            return 1;
        }

        std::string argument(static_cast<std::size_t>(size), '\0');
        if (WideCharToMultiByte(
                CP_UTF8, 0, wide_argv[i], -1, argument.data(), size, nullptr, nullptr
            ) == 0) {
            std::fwprintf(stderr, L"spek: cannot convert the command line to UTF-8\n");
            return 1;
        }
        strings.push_back(std::move(argument));
    }

    std::vector<char *> arguments;
    arguments.reserve(strings.size() + 1);
    for (auto& argument : strings) {
        arguments.push_back(argument.data());
    }
    arguments.push_back(nullptr);

    return run(static_cast<int>(strings.size()), arguments.data());
}
