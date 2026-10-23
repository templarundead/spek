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
#include <vector>

#include "spek-png.h"

// A simple 24-bit RGB image, used by the command line interface which
// cannot rely on wxImage.
class SpekImage
{
public:
    SpekImage() = default;
    SpekImage(int width, int height) { this->create(width, height); }

    void create(int width, int height);
    void clear();

    bool empty() const { return this->width_ <= 0 || this->height_ <= 0; }
    int width() const { return this->width_; }
    int height() const { return this->height_; }

    // Sets the pixel at (x, y) to `rgb`, which is 0xRRGGBB.
    void set_pixel(int x, int y, uint32_t rgb);

    // Writes one column of pixels, `column` has height() entries.
    void set_column(int x, std::span<const uint32_t> column);

    std::span<const uint8_t> pixels() const { return this->pixels_; }

    [[nodiscard]] std::expected<void, PngError> save_png(std::string_view path) const
    {
        return spek_png_write(path, this->pixels_, this->width_, this->height_);
    }

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<uint8_t> pixels_;
};
