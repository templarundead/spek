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

#include <memory>

#include <wx/wx.h>

#include "spek-palette.h"
#include "spek-pipeline.h"

class SpekHaveSampleEvent;

// The spectrogram itself: everything inside the main window except the
// menu, the toolbar and the info bar.
class SpekSpectrogram : public wxWindow
{
public:
    explicit SpekSpectrogram(wxFrame *parent);
    ~SpekSpectrogram() override;

    void open(const wxString& path);
    void save(const wxString& path);

private:
    // The layout of the spectrogram inside the window, in pixels.
    //
    // The paddings are calculated from the size of the labels and the fonts
    // in use, so that they are big enough on High-DPI displays and with
    // large fonts, and the size of the plot is never negative
    // (see https://github.com/alexkay/spek/issues/349).
    struct Layout
    {
        int lpad = 0; // Space to the left of the plot.
        int tpad = 0; // Space above the plot.
        int rpad = 0; // Space to the right of the plot.
        int bpad = 0; // Space below the plot.
        int gap = 0; // Space between the elements.
        int ruler = 0; // Width of the palette next to the plot.
        int tick = 0; // Length of the ruler ticks.
        int width = 0; // Width of the plot, never negative.
        int height = 0; // Height of the plot, never negative.
    };

    // Fonts used to draw the labels, with their heights in pixels.
    struct Fonts
    {
        wxFont normal;
        wxFont large;
        wxFont small_font;
        int normal_height = 0;
        int large_height = 0;
        int small_height = 0;
    };

    void on_char(wxKeyEvent& evt);
    void on_paint(wxPaintEvent& evt);
    void on_size(wxSizeEvent& evt);
    void on_have_sample(SpekHaveSampleEvent& evt);
    void on_dpi_changed(wxDPIChangedEvent& event);
    void render(wxDC& dc);

    void start();
    void stop();

    void create_palette();
    int palette_width() const { return this->FromDIP(10); }

    Fonts create_fonts(wxDC& dc) const;
    Layout calculate_layout(wxDC& dc, const Fonts& fonts, const wxSize& size) const;

    // Width of the plot for the current window size.
    int plot_width() const;

    std::unique_ptr<SpekPipeline> pipeline;
    int streams;
    int stream;
    int channels;
    int channel;
    WindowFunction window_function;
    wxString path;
    wxString desc;
    double duration;
    int sample_rate;
    enum palette palette;
    wxImage palette_image;
    wxImage image;
    int prev_width;
    int fft_bits;
    int urange;
    int lrange;

    wxDECLARE_EVENT_TABLE();
};
