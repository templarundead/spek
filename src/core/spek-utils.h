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

#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

// Compare version numbers, e.g. "1.9.2" < "1.10.0".
// Returns -1, 0 or 1 when `a` is smaller than, equal to or greater than `b`.
int spek_vercmp(std::string_view a, std::string_view b);

// File extensions (without the dot) of the audio formats Spek can open.
std::span<const std::string_view> spek_audio_extensions();

// True when the extension of `path` is one of spek_audio_extensions().
bool spek_has_audio_extension(std::string_view path);

// Small printf-style helper.
//
// <format> is still missing from some of the C++23 toolchains we support,
// and all the formats used by Spek are plain printf ones, so we build the
// string by hand. Only the types printf() can handle are accepted.
template <typename... Args>
inline std::string spek_format(const char *fmt, const Args&... args)
{
    static_assert(
        ((std::is_arithmetic_v<std::decay_t<Args>>
          || std::is_pointer_v<std::decay_t<Args>>) && ...),
        "spek_format() only supports the types understood by printf()"
    );

    int len = snprintf(nullptr, 0, fmt, args...);
    if (len < 0) {
        return {};
    }

    std::string result(static_cast<size_t>(len) + 1, '\0');
    snprintf(result.data(), result.size(), fmt, args...);
    result.resize(static_cast<size_t>(len));
    return result;
}
