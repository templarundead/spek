// Copyright (C) 2010-2013 Alexander Kojevnikov and contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include <cmath>

#include "spek-ruler-ticks.h"

std::vector<int> spek_ruler_ticks(
    std::span<const int> factors,
    int min_units,
    int max_units,
    double scale,
    double spacing,
    int sample_label_extent
)
{
    if (max_units <= min_units) {
        return {min_units};
    }

    // Select the first factor which leaves enough space between labels.
    int factor = 0;
    for (const int candidate : factors) {
        if (candidate == 0) {
            break;
        }
        if (std::fabs(scale * candidate) >= spacing * sample_label_extent) {
            factor = candidate;
            break;
        }
    }

    std::vector<int> ticks{min_units};
    if (factor > 0) {
        for (int tick = min_units + factor; tick < max_units; tick += factor) {
            if (std::fabs(scale * (max_units - tick)) < sample_label_extent * 1.2) {
                break;
            }
            ticks.push_back(tick);
        }
    }
    ticks.push_back(max_units);
    return ticks;
}
