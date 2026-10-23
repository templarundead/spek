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

#include <cstdint>
#include <expected>
#include <span>
#include <string_view>

enum class PngError
{
    OK,
    BAD_SIZE,
    CANNOT_OPEN,
    ENCODE_FAILED,
    WRITE_FAILED,
};

// A minimal PNG writer, used to save spectrograms without pulling in
// wxWidgets (or any other GUI toolkit) into the command line interface.
//
// `rgb` holds `width * height` top-down pixels, three bytes per pixel.
[[nodiscard]] std::expected<void, PngError> spek_png_write(
    std::string_view path, std::span<const uint8_t> rgb, int width, int height
);
