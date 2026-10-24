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

#include "spek-ruler-ticks.h"

#include "spek-ruler.h"

SpekRuler::SpekRuler(
    int x, int y, Position pos, wxString sample_label,
    std::span<const int> factors, int min_units, int max_units, double spacing,
    double scale, double offset, FormatterCallback formatter,
    int gap, int tick_length
) :
    x(x), y(y), pos(pos), sample_label(sample_label),
    factors(factors), min_units(min_units), max_units(max_units), spacing(spacing),
    scale(scale), offset(offset), formatter(formatter),
    gap(gap), tick_length(tick_length)
{
}

void SpekRuler::draw(wxDC& dc)
{
    // Mesure the sample label.
    wxSize size = dc.GetTextExtent(this->sample_label);
    int len = this->pos == TOP || this->pos == BOTTOM ? size.GetWidth() : size.GetHeight();

    const auto ticks = spek_ruler_ticks(
        this->factors,
        this->min_units,
        this->max_units,
        this->scale,
        this->spacing,
        len
    );
    for (const int tick : ticks) {
        this->draw_tick(dc, tick);
    }
}

void SpekRuler::draw_tick(wxDC& dc, int tick)
{
    wxString label = this->formatter(tick);
    int value = this->pos == TOP || this->pos == BOTTOM ?
        tick : this->max_units + this->min_units - tick;
    double p = this->offset + this->scale * (value - this->min_units);
    wxSize size = dc.GetTextExtent(label);
    int w = size.GetWidth();
    int h = size.GetHeight();

    if (this->pos == TOP) {
        dc.DrawText(label, this->x + p - w / 2, this->y - this->gap - h);
    } else if (this->pos == RIGHT){
        dc.DrawText(label, this->x + this->gap, this->y + p - h / 2);
    } else if (this->pos == BOTTOM) {
        dc.DrawText(label, this->x + p - w / 2, this->y + this->gap);
    } else if (this->pos == LEFT){
        dc.DrawText(label, this->x - w - this->gap, this->y + p - h / 2);
    }

    if (this->pos == TOP) {
        dc.DrawLine(this->x + p, this->y, this->x + p, this->y - this->tick_length);
    } else if (this->pos == RIGHT) {
        dc.DrawLine(this->x, this->y + p, this->x + this->tick_length, this->y + p);
    } else if (this->pos == BOTTOM) {
        dc.DrawLine(this->x + p, this->y, this->x + p, this->y + this->tick_length);
    } else if (this->pos == LEFT) {
        dc.DrawLine(this->x, this->y + p, this->x - this->tick_length, this->y + p);
    }
}
