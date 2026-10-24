// Copyright (C) 2010-2013 Alexander Kojevnikov and contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "spek-full-image.h"
#include "spek-ruler-ticks.h"

#include "test.h"

namespace
{

bool contains_color(const SpekImage& image, uint32_t color)
{
    const auto pixels = image.pixels();
    for (std::size_t offset = 0; offset + 2 < pixels.size(); offset += 3) {
        const uint32_t candidate = (static_cast<uint32_t>(pixels[offset]) << 16)
            | (static_cast<uint32_t>(pixels[offset + 1]) << 8)
            | static_cast<uint32_t>(pixels[offset + 2]);
        if (candidate == color) {
            return true;
        }
    }
    return false;
}

void test_ticks()
{
    constexpr std::array factors{1, 2, 5, 10, 0};
    const std::vector<int> expected{0, 5, 10};
    const auto ticks = spek_ruler_ticks(factors, 0, 10, 2.0, 2.0, 3);
    test("shared ruler tick selection", true, ticks == expected);

    const auto empty_range = spek_ruler_ticks(factors, 4, 4, 1.0, 1.0, 3);
    test("empty ruler range has one tick", true, empty_range == std::vector<int>{4});
}

void test_composition()
{
    constexpr uint32_t color = 0x123456;
    SpekImage raw(80, 32);
    for (int y = 0; y < raw.height(); ++y) {
        for (int x = 0; x < raw.width(); ++x) {
            raw.set_pixel(x, y, color);
        }
    }

    SpekFullImageInfo info;
    info.file_name = "sample.wav";
    info.description = "FLAC, 44100 Hz, channel 1 / 2";
    info.duration = 65.0;
    info.sample_rate = 44100;
    info.palette = PALETTE_MONO;

    const SpekImage full = spek_render_full_image(raw, info);
    test("full image has horizontal rulers", true, full.width() > raw.width());
    test("full image has vertical metadata", true, full.height() > raw.height());
    test("full image keeps the spectrogram", true, contains_color(full, color));

    info.file_name = std::string(160, 'x') + ".wav";
    const SpekImage wide = spek_render_full_image(raw, info);
    test("full image grows for long names", true, wide.width() > full.width());

    const SpekImage empty = spek_render_full_image(SpekImage(), info);
    test("empty source produces an empty image", true, empty.empty());
}

} // namespace

void test_full_image()
{
    run("ruler ticks", test_ticks);
    run("full image", test_composition);
}
