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
#include <vector>

#include <wx/wx.h>

// Sent by the pipeline for every column of the spectrogram.
class SpekHaveSampleEvent : public wxEvent
{
public:
    // The event takes a copy of `values`, the buffer owned by the pipeline
    // is reused as soon as the callback returns.
    SpekHaveSampleEvent(int bands, int sample, std::span<const float> values);
    SpekHaveSampleEvent(const SpekHaveSampleEvent& other);
    ~SpekHaveSampleEvent() override = default;

    int get_bands() const { return this->bands; }
    int get_sample() const { return this->sample; }
    std::span<const float> get_values() const { return this->values; }

    wxEvent *Clone() const override { return new SpekHaveSampleEvent(*this); }

private:
    int bands;
    int sample;
    std::vector<float> values;
};

typedef void (wxEvtHandler::*SpekHaveSampleEventFunction)(SpekHaveSampleEvent&);

wxDECLARE_EVENT(SPEK_HAVE_SAMPLE, SpekHaveSampleEvent);

#define SPEK_EVT_HAVE_SAMPLE(fn) \
    wx__DECLARE_EVT1(SPEK_HAVE_SAMPLE, wxID_ANY, &fn)
