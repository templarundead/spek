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

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <limits>
#include <utility>

#include "spek-utils.h"

namespace
{

// TODO: s/audio/media/
constexpr std::array<std::string_view, 29> AUDIO_EXTENSIONS{
    "3gp", "aac", "aif", "aifc", "aiff", "amr", "awb", "ape", "au",  "dts",
    "flac", "flv", "gsm", "m4a", "m4p", "mp3", "mp4", "mp+", "mpc", "mpp",
    "oga", "ogg", "opus", "ra",  "ram", "snd", "wav", "wma", "wv",
};

// Parses the leading number of a version component, returning the
// parsed value and the rest of the string (starting with the separator).
std::pair<long long, std::string_view> parse_component(std::string_view s)
{
    long long value = 0;
    const char *first = s.data();
    const char *last = first + s.size();
    auto result = std::from_chars(first, last, value);

    if (result.ec == std::errc::result_out_of_range) {
        value = std::numeric_limits<long long>::max();
    } else if (result.ec == std::errc::invalid_argument) {
        // No digits at all, e.g. the component is empty or is ".foo".
        value = 0;
    }

    return {value, s.substr(static_cast<size_t>(result.ptr - first))};
}

} // namespace

int spek_vercmp(std::string_view a, std::string_view b)
{
    while (true) {
        if (a.empty() && b.empty()) {
            return 0;
        }
        if (a.empty()) {
            return -1;
        }
        if (b.empty()) {
            return 1;
        }

        auto [x, rest_a] = parse_component(a);
        auto [y, rest_b] = parse_component(b);

        if (x < y) {
            return -1;
        }
        if (x > y) {
            return 1;
        }

        if (rest_a.empty() && rest_b.empty()) {
            return 0;
        }
        if (rest_a.empty()) {
            return -1;
        }
        if (rest_b.empty()) {
            return 1;
        }

        // Skip the separator, e.g. the dot in "1.2.3".
        a = rest_a.substr(1);
        b = rest_b.substr(1);
    }
}

std::span<const std::string_view> spek_audio_extensions()
{
    return AUDIO_EXTENSIONS;
}

bool spek_has_audio_extension(std::string_view path)
{
    const size_t dot = path.rfind('.');
    if (dot == std::string_view::npos) {
        return false;
    }
    const size_t separator = path.find_last_of("/\\");
    if (separator != std::string_view::npos && separator > dot) {
        // The dot belongs to a directory name, e.g. "some.dir/file".
        return false;
    }

    std::string extension(path.substr(dot + 1));
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    return std::ranges::find(AUDIO_EXTENSIONS, std::string_view(extension)) != AUDIO_EXTENSIONS.end();
}
