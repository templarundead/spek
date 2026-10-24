// Copyright (C) 2010-2013 Alexander Kojevnikov and contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once

#include <string>

#include "spek-image.h"
#include "spek-palette.h"

struct SpekFullImageInfo
{
    std::string file_name;
    std::string description;
    double duration = 0.0;
    int sample_rate = 0;
    double min_db = -120.0;
    double max_db = 0.0;
    enum palette palette = PALETTE_DEFAULT;
};

// Adds the same file information, palette and time/frequency/density rulers
// as the GUI around a rendered spectrogram. This renderer deliberately uses
// only SpekImage, not wxMemoryDC: wxGTK would otherwise require a display
// server even for an off-screen command-line render.
SpekImage spek_render_full_image(const SpekImage& spectrogram, const SpekFullImageInfo& info);
