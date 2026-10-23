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

#include "spek-image.h"

void SpekImage::create(int width, int height)
{
    this->width_ = std::max(width, 0);
    this->height_ = std::max(height, 0);
    this->pixels_.assign(
        static_cast<size_t>(this->width_) * static_cast<size_t>(this->height_) * 3, 0
    );
}

void SpekImage::clear()
{
    this->width_ = 0;
    this->height_ = 0;
    this->pixels_.clear();
}

void SpekImage::set_pixel(int x, int y, uint32_t rgb)
{
    if (x < 0 || y < 0 || x >= this->width_ || y >= this->height_) {
        return;
    }

    const size_t offset =
        (static_cast<size_t>(y) * static_cast<size_t>(this->width_) + static_cast<size_t>(x)) * 3;
    this->pixels_[offset] = static_cast<uint8_t>((rgb >> 16) & 0xFF);
    this->pixels_[offset + 1] = static_cast<uint8_t>((rgb >> 8) & 0xFF);
    this->pixels_[offset + 2] = static_cast<uint8_t>(rgb & 0xFF);
}

void SpekImage::set_column(int x, std::span<const uint32_t> column)
{
    if (x < 0 || x >= this->width_) {
        return;
    }

    const int height = std::min(this->height_, static_cast<int>(column.size()));
    for (int y = 0; y < height; ++y) {
        this->set_pixel(x, y, column[static_cast<size_t>(y)]);
    }
}
