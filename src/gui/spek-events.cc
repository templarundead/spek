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

#include "spek-events.h"

wxDEFINE_EVENT(SPEK_HAVE_SAMPLE, SpekHaveSampleEvent);

SpekHaveSampleEvent::SpekHaveSampleEvent(int bands, int sample, std::span<const float> values) :
    wxEvent(), bands(bands), sample(sample)
{
    SetEventType(SPEK_HAVE_SAMPLE);
    this->values.assign(values.begin(), values.end());
}

SpekHaveSampleEvent::SpekHaveSampleEvent(const SpekHaveSampleEvent& other) :
    wxEvent(other), bands(other.bands), sample(other.sample), values(other.values)
{
    SetEventType(SPEK_HAVE_SAMPLE);
}
