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

#include <span>

#include <wx/dc.h>
#include <wx/string.h>

// Draws one of the rulers around the spectrogram.
//
// All the sizes are in pixels, `gap` and `tick_length` are passed in so that
// they can be scaled for High-DPI displays.
class SpekRuler
{
public:
    enum Position
    {
        TOP,
        RIGHT,
        BOTTOM,
        LEFT
    };

    using FormatterCallback = wxString (*)(int unit);

    SpekRuler(
        int x, int y, Position pos, wxString sample_label,
        std::span<const int> factors, int min_units, int max_units, double spacing,
        double scale, double offset, FormatterCallback formatter,
        int gap = 10, int tick_length = 4
    );

    void draw(wxDC& dc);

protected:
    void draw_tick(wxDC& dc, int tick);

    int x;
    int y;
    Position pos;
    wxString sample_label;
    std::span<const int> factors;
    int min_units;
    int max_units;
    double spacing;
    double scale;
    double offset;
    FormatterCallback formatter;
    int gap;
    int tick_length;
};
