// Copyright (C) 2010-2013 Alexander Kojevnikov and contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once

#include <span>
#include <vector>

// Selects ruler ticks independently of the drawing toolkit. Both the wx GUI
// and the headless full-image renderer use this function so their labels have
// the same spacing and positions.
std::vector<int> spek_ruler_ticks(
    std::span<const int> factors,
    int min_units,
    int max_units,
    double scale,
    double spacing,
    int sample_label_extent
);
