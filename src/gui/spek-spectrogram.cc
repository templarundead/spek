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

#include "spek-config.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <expected>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include <wx/dcbuffer.h>
#include <wx/settings.h>

#include "spek-audio.h"
#include "spek-events.h"
#include "spek-fft.h"
#include "spek-platform.h"
#include "spek-ruler.h"

#include "spek-spectrogram.h"

struct SpekSpectrogram::OpenRequest
{
    using Result = std::expected<std::unique_ptr<AudioFile>, AudioError>;

    std::mutex mutex;
    bool finished = false;
    int samples = 0;
    std::string file_name;
    int stream = 0;
    Result result = std::unexpected(AudioError::CANNOT_OPEN_FILE);
};

wxBEGIN_EVENT_TABLE(SpekSpectrogram, wxWindow)
    EVT_CHAR(SpekSpectrogram::on_char)
    EVT_PAINT(SpekSpectrogram::on_paint)
    EVT_SIZE(SpekSpectrogram::on_size)
    EVT_DPI_CHANGED(SpekSpectrogram::on_dpi_changed)
    SPEK_EVT_HAVE_SAMPLE(SpekSpectrogram::on_have_sample)
wxEND_EVENT_TABLE()

constexpr int MIN_RANGE = -140;
constexpr int MAX_RANGE = 0;
constexpr int FFT_BITS = 11;
constexpr int MIN_FFT_BITS = 8;
constexpr int MAX_FFT_BITS = 14;
constexpr int URANGE = 0;
constexpr int LRANGE = -120;

// Forward declarations.
static wxString trim(wxDC& dc, const wxString& s, int length, bool trim_end);
static int bits_to_bands(int bits);

static wxString time_formatter(int unit)
{
    // TODO: i18n
    return wxString::Format("%d:%02d", unit / 60, unit % 60);
}

static wxString freq_formatter(int unit)
{
    return wxString::Format(_("%d kHz"), unit / 1000);
}

static wxString density_formatter(int unit)
{
    return wxString::Format(_("%d dB"), -unit);
}

SpekSpectrogram::SpekSpectrogram(wxFrame *parent) :
    wxWindow(
        parent, -1, wxDefaultPosition, wxDefaultSize,
        wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS
    ),
    open_timer(this),
    streams(0),
    stream(0),
    channels(0),
    channel(0),
    window_function(WindowFunction::DEFAULT),
    duration(0.0),
    sample_rate(0),
    palette(PALETTE_DEFAULT),
    palette_image(),
    image(1, 1),
    prev_width(-1),
    fft_bits(FFT_BITS),
    urange(URANGE),
    lrange(LRANGE)
{
    this->Bind(wxEVT_TIMER, &SpekSpectrogram::on_open_timer, this);
    this->create_palette();

    // Never let the window become so small that the labels around the plot
    // leave no room for the spectrogram itself.
    wxBitmap bitmap(1, 1);
    wxMemoryDC dc(bitmap);
    const Fonts fonts = this->create_fonts(dc);
    const Layout layout = this->calculate_layout(dc, fonts, wxSize(0, 0));
    const int min_plot = this->FromDIP(100);
    SetMinClientSize(wxSize(
        layout.lpad + layout.rpad + min_plot,
        layout.tpad + layout.bpad + min_plot
    ));

    SetDoubleBuffered(true);
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetFocus();
}

SpekSpectrogram::~SpekSpectrogram()
{
    this->stop();
}

void SpekSpectrogram::open(const wxString& path)
{
    this->path = path;
    this->stream = 0;
    this->channel = 0;
    this->streams = 0;
    this->channels = 0;
    this->duration = 0.0;
    this->sample_rate = 0;
    this->start();
    Refresh();
}

void SpekSpectrogram::save(const wxString& path)
{
    wxSize size = GetClientSize();
    wxBitmap bitmap(size.GetWidth(), size.GetHeight());
    wxMemoryDC dc(bitmap);
    this->render(dc);
    bitmap.SaveFile(path, wxBITMAP_TYPE_PNG);
}

SpekSpectrogram::Fonts SpekSpectrogram::create_fonts(wxDC& dc) const
{
    const double scale = spek_platform_font_scale();

    Fonts fonts;
    fonts.normal = wxFont(
        (int)round(9 * scale),
        wxFONTFAMILY_SWISS,
        wxFONTSTYLE_NORMAL,
        wxFONTWEIGHT_NORMAL
    );
    fonts.large = wxFont(fonts.normal);
    fonts.large.SetPointSize((int)round(10 * scale));
    fonts.large.SetWeight(wxFONTWEIGHT_BOLD);
    fonts.small_font = wxFont(fonts.normal);
    fonts.small_font.SetPointSize((int)round(8 * scale));

    dc.SetFont(fonts.normal);
    fonts.normal_height = dc.GetTextExtent("dummy").GetHeight();
    dc.SetFont(fonts.large);
    fonts.large_height = dc.GetTextExtent("dummy").GetHeight();
    dc.SetFont(fonts.small_font);
    fonts.small_height = dc.GetTextExtent("dummy").GetHeight();

    return fonts;
}

SpekSpectrogram::Layout SpekSpectrogram::calculate_layout(
    wxDC& dc, const Fonts& fonts, const wxSize& size
) const
{
    // Sizes which don't depend on the text, scaled for High-DPI displays.
    const int gap = this->FromDIP(10);
    const int ruler = this->FromDIP(10);
    const int tick = this->FromDIP(4);

    // The labels are drawn outside of the plot, measure them to find out
    // how much space they need. Both strings are a bit wider than what the
    // rulers normally draw, so that there is some slack for longer values.
    dc.SetFont(fonts.small_font);
    // TRANSLATORS: keep "000" unchanged, it's used to calc the text width.
    const int freq_width = dc.GetTextExtent(_("000 kHz")).GetWidth();
    // TRANSLATORS: keep "-000" unchanged, it's used to calc the text width.
    const int density_width = dc.GetTextExtent(_("-000 dB")).GetWidth();

    Layout layout;
    layout.gap = gap;
    layout.ruler = ruler;
    layout.tick = tick;
    // Left: the frequency labels.
    layout.lpad = gap + freq_width + gap;
    // Right: the palette followed by the spectral density labels.
    layout.rpad = gap + ruler + gap + density_width + gap;
    // Top: the file name above the description, in large and normal fonts.
    layout.tpad = gap + fonts.large_height + gap + fonts.normal_height + gap;
    // Bottom: the time labels with their ticks.
    layout.bpad = gap + std::max(tick, gap + fonts.small_height) + gap;

    // Never let the paddings eat into the plot, whatever the window size is.
    layout.width = std::max(0, size.GetWidth() - layout.lpad - layout.rpad);
    layout.height = std::max(0, size.GetHeight() - layout.tpad - layout.bpad);

    return layout;
}

int SpekSpectrogram::plot_width() const
{
    wxBitmap bitmap(1, 1);
    wxMemoryDC dc(bitmap);
    const Fonts fonts = this->create_fonts(dc);
    return this->calculate_layout(dc, fonts, GetClientSize()).width;
}

wxSize SpekSpectrogram::DoGetBestClientSize() const
{
    wxBitmap bitmap(1, 1);
    wxMemoryDC dc(bitmap);
    const Fonts fonts = this->create_fonts(dc);
    const Layout layout = this->calculate_layout(dc, fonts, wxSize(0, 0));

    dc.SetFont(fonts.large);
    const int name_width = dc.GetTextExtent(this->path).GetWidth();
    dc.SetFont(fonts.normal);
    const int desc_width = dc.GetTextExtent(this->desc).GetWidth();

    // The sizer can now ask for enough room for the complete file name. The
    // frame only grows, so opening a short file never undoes a user's resize.
    const int min_plot = this->FromDIP(100);
    const int plot_width = std::max({min_plot, name_width, desc_width});
    return wxSize(
        layout.lpad + plot_width + layout.rpad,
        layout.tpad + min_plot + layout.bpad
    );
}

void SpekSpectrogram::on_char(wxKeyEvent& evt)
{
    switch (evt.GetKeyCode()) {
    case 'c':
        if (this->channels) {
            this->channel = (this->channel + 1) % this->channels;
        }
        break;
    case 'C':
        if (this->channels) {
            this->channel = (this->channel - 1 + this->channels) % this->channels;
        }
        break;
    case 'f':
        this->window_function =
            static_cast<WindowFunction>((static_cast<int>(this->window_function) + 1)
                                        % static_cast<int>(WindowFunction::COUNT));
        break;
    case 'F':
        this->window_function = static_cast<WindowFunction>(
            (static_cast<int>(this->window_function) - 1 + static_cast<int>(WindowFunction::COUNT))
            % static_cast<int>(WindowFunction::COUNT)
        );
        break;
    case 'l':
        this->lrange = std::min(this->lrange + 1, this->urange - 1);
        break;
    case 'L':
        this->lrange = std::max(this->lrange - 1, MIN_RANGE);
        break;
    case 'p':
        this->palette =
            static_cast<enum palette>((this->palette + 1) % PALETTE_COUNT);
        this->create_palette();
        break;
    case 'P':
        this->palette =
            static_cast<enum palette>((this->palette - 1 + PALETTE_COUNT) % PALETTE_COUNT);
        this->create_palette();
        break;
    case 's':
        if (this->streams) {
            this->stream = (this->stream + 1) % this->streams;
        }
        break;
    case 'S':
        if (this->streams) {
            this->stream = (this->stream - 1 + this->streams) % this->streams;
        }
        break;
    case 'u':
        this->urange = std::min(this->urange + 1, MAX_RANGE);
        break;
    case 'U':
        this->urange = std::max(this->urange - 1, this->lrange + 1);
        break;
    case 'w':
        this->fft_bits = std::min(this->fft_bits + 1, MAX_FFT_BITS);
        this->create_palette();
        break;
    case 'W':
        this->fft_bits = std::max(this->fft_bits - 1, MIN_FFT_BITS);
        this->create_palette();
        break;
    default:
        evt.Skip();
        return;
    }

    this->start();
    Refresh();
}

void SpekSpectrogram::on_paint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    this->render(dc);
}

void SpekSpectrogram::on_size(wxSizeEvent&)
{
    const wxSize size = GetClientSize();
    const bool width_changed = this->prev_width != size.GetWidth();
    this->prev_width = size.GetWidth();

    if (width_changed) {
        this->start();
    }
}

void SpekSpectrogram::on_dpi_changed(wxDPIChangedEvent&)
{
    // The layout is recalculated on every repaint, but the size of the plot
    // in pixels has changed, so the analysis has to be run again.
    this->start();
    Refresh();
}

void SpekSpectrogram::on_have_sample(SpekHaveSampleEvent& event)
{
    const int bands = event.get_bands();
    const int sample = event.get_sample();
    const std::span<const float> values = event.get_values();

    if (sample == -1) {
        this->stop();
        return;
    }

    // Ignore the samples of an analysis which has been restarted already.
    if (sample >= this->image.GetWidth() || bands != this->image.GetHeight()) {
        return;
    }

    const double range = this->urange - this->lrange;
    for (int y = 0; y < bands; y++) {
        const double value = std::clamp(
            static_cast<double>(values[static_cast<size_t>(y)]),
            static_cast<double>(this->lrange),
            static_cast<double>(this->urange)
        );
        const double level = range > 0.0 ? (value - this->lrange) / range : 0.0;
        const uint32_t color = spek_palette(this->palette, level);
        this->image.SetRGB(
            sample,
            bands - y - 1,
            static_cast<uint8_t>(color >> 16),
            static_cast<uint8_t>((color >> 8) & 0xFF),
            static_cast<uint8_t>(color & 0xFF)
        );
    }

    // TODO: refresh only one pixel column
    this->Refresh();
}

void SpekSpectrogram::render(wxDC& dc)
{
    const wxSize size = GetClientSize();
    const int w = size.GetWidth();

    // Follow the theme of the operating system, including its dark mode.
    const wxColour background = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    const wxColour foreground = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);

    // Initialise.
    dc.SetBackground(wxBrush(background));
    dc.SetBackgroundMode(wxTRANSPARENT);
    dc.SetPen(wxPen(foreground));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.SetTextForeground(foreground);

    const Fonts fonts = this->create_fonts(dc);
    const Layout layout = this->calculate_layout(dc, fonts, size);

    // Clean the background.
    dc.Clear();

    // The file name is drawn above the description, which sits right on top
    // of the plot. Both are clamped so that they are never cut off.
    const int desc_y = std::max(layout.gap, layout.tpad - layout.gap - fonts.normal_height);
    const int name_y = std::max(layout.gap, desc_y - layout.gap - fonts.large_height);

    // Spek version
    dc.SetFont(fonts.large);
    const wxString package_name(SPEK_DISPLAY_NAME);
    dc.DrawText(package_name, w - layout.rpad + layout.gap, name_y);
    const int package_name_width = dc.GetTextExtent(package_name + " ").GetWidth();
    dc.SetFont(fonts.small_font);
    dc.DrawText(
        SPEK_VERSION,
        w - layout.rpad + layout.gap + package_name_width,
        desc_y + fonts.normal_height - fonts.small_height
    );

    const bool has_image = this->image.GetWidth() > 1 && this->image.GetHeight() > 1;

    if (layout.width > 0 && layout.height > 0) {
        if (has_image) {
            // Draw the spectrogram.
            wxBitmap bmp(this->image.Scale(layout.width, layout.height));
            dc.DrawBitmap(bmp, layout.lpad, layout.tpad);

            // File name.
            dc.SetFont(fonts.large);
            dc.DrawText(
                trim(dc, this->path, layout.width, false),
                layout.lpad,
                name_y
            );

            // File properties.
            dc.SetFont(fonts.normal);
            dc.DrawText(
                trim(dc, this->desc, layout.width, true),
                layout.lpad,
                desc_y
            );
        }

        // Prepare to draw the rulers.
        dc.SetFont(fonts.small_font);

        if (this->duration) {
            // Time ruler.
            const int time_factors[] = {
                1, 2, 5, 10, 20, 30, 1*60, 2*60, 5*60, 10*60, 20*60, 30*60, 0
            };
            SpekRuler time_ruler(
                layout.lpad,
                layout.tpad + layout.height,
                SpekRuler::BOTTOM,
                // TODO: i18n
                "00:00",
                time_factors,
                0,
                (int)this->duration,
                1.5,
                layout.width / this->duration,
                0.0,
                time_formatter,
                layout.gap,
                layout.tick
            );
            time_ruler.draw(dc);
        }

        if (this->sample_rate) {
            // Frequency ruler.
            const int freq = this->sample_rate / 2;
            const int freq_factors[] = {1000, 2000, 5000, 10000, 20000, 0};
            SpekRuler freq_ruler(
                layout.lpad,
                layout.tpad,
                SpekRuler::LEFT,
                // TRANSLATORS: keep "00" unchanged, it's used to calc the text width
                _("00 kHz"),
                freq_factors,
                0,
                freq,
                3.0,
                layout.height / (double)freq,
                0.0,
                freq_formatter,
                layout.gap,
                layout.tick
            );
            freq_ruler.draw(dc);
        }

        // The palette.
        wxBitmap bmp(this->palette_image.Scale(layout.ruler, layout.height + 1));
        dc.DrawBitmap(bmp, w - layout.rpad + layout.gap, layout.tpad);

        // Spectral density.
        const int density_factors[] = {1, 2, 5, 10, 20, 50, 0};
        SpekRuler density_ruler(
            w - layout.rpad + layout.gap + layout.ruler,
            layout.tpad,
            SpekRuler::RIGHT,
            // TRANSLATORS: keep "-00" unchanged, it's used to calc the text width
            _("-00 dB"),
            density_factors,
            -this->urange,
            -this->lrange,
            3.0,
            layout.height / (double)(this->lrange - this->urange),
            layout.height,
            density_formatter,
            layout.gap,
            layout.tick
        );
        density_ruler.draw(dc);
    }

    // Border around the spectrogram.
    dc.DrawRectangle(layout.lpad, layout.tpad, layout.width, layout.height);
}

void SpekSpectrogram::start()
{
    if (this->path.IsEmpty()) {
        return;
    }

    auto pending = this->open_request;
    this->stop();

    // The number of samples is the number of pixels available for the image.
    // The number of bands is fixed, FFT results are very different for
    // different values but we need some consistency.
    const int samples = this->plot_width();
    if (samples <= 0) {
        this->image.Create(1, 1);
        return;
    }

    const int bands = bits_to_bands(this->fft_bits);
    this->image.Create(samples, bands);

    const std::string file_name(this->path.utf8_str());
    const int stream = this->stream;

    // Resizing the window changes only the number of output columns. Reuse an
    // outstanding open for the same input instead of starting another network
    // request for every wxEVT_SIZE while the cloud file is being fetched.
    if (pending && pending->file_name == file_name && pending->stream == stream) {
        {
            std::lock_guard<std::mutex> lock(pending->mutex);
            pending->samples = samples;
        }
        this->open_request = pending;
        this->open_timer.Start(25);
        return;
    }

    // avformat_open_input(), which is called by Audio::open(), can block for
    // a long time for a cloud or network file. Keep it and all FFmpeg calls
    // in a worker which knows nothing about wx or this window. The timer only
    // transfers the completed result back to the GUI thread.
    auto request = std::make_shared<OpenRequest>();
    request->samples = samples;
    request->file_name = file_name;
    request->stream = stream;
    this->open_request = request;
    std::thread([request, file_name, stream]() {
        Audio audio;
        auto result = audio.open(file_name, stream);
        std::lock_guard<std::mutex> lock(request->mutex);
        request->result = std::move(result);
        request->finished = true;
    }).detach();
    this->open_timer.Start(25);
}

void SpekSpectrogram::on_open_timer(wxTimerEvent&)
{
    auto request = this->open_request;
    if (!request) {
        this->open_timer.Stop();
        return;
    }

    {
        std::lock_guard<std::mutex> lock(request->mutex);
        if (!request->finished) {
            return;
        }
    }

    this->open_timer.Stop();
    this->open_request.reset();
    this->finish_open(request);
    this->Refresh();
}

void SpekSpectrogram::finish_open(const std::shared_ptr<OpenRequest>& request)
{
    std::unique_lock<std::mutex> lock(request->mutex);
    auto file = std::move(request->result);
    const int samples = request->samples;
    lock.unlock();

    if (!file) {
        this->desc = wxString::FromUTF8(spek_audio_error_message(file.error()).c_str());
        this->duration = 0.0;
        this->sample_rate = 0;
        return;
    }

    auto fft = spek_fft_create(this->fft_bits);
    if (!fft) {
        this->desc = _("Cannot create the FFT plan");
        this->duration = 0.0;
        this->sample_rate = 0;
        return;
    }

    this->pipeline = std::make_unique<SpekPipeline>(
        std::move(*file),
        std::move(*fft),
        this->stream,
        this->channel,
        this->window_function,
        samples,
        [this](int event_bands, int sample, std::span<const float> values) {
            SpekHaveSampleEvent event(event_bands, sample, values);
            wxPostEvent(this, event);
        }
    );

    // TODO: extract conversion into a utility function.
    this->desc = wxString::FromUTF8(this->pipeline->desc().c_str());
    this->streams = this->pipeline->streams();
    this->channels = this->pipeline->channels();
    this->duration = this->pipeline->duration();
    this->sample_rate = this->pipeline->sample_rate();

    this->pipeline->start();
}

void SpekSpectrogram::stop()
{
    this->open_timer.Stop();
    this->open_request.reset();

    if (this->pipeline) {
        // The destructor stops the analysis and waits for the threads.
        this->pipeline.reset();

        // Make sure all have_sample events are processed before returning.
        if (wxApp::GetInstance() != nullptr) {
            wxApp::GetInstance()->ProcessPendingEvents();
        }
    }
}

void SpekSpectrogram::create_palette()
{
    const int ruler = this->palette_width();
    const int bands = bits_to_bands(this->fft_bits);
    this->palette_image.Create(ruler, bands);
    for (int y = 0; y < bands; y++) {
        uint32_t color = spek_palette(this->palette, y / (double)bands);
        this->palette_image.SetRGB(
            wxRect(0, bands - y - 1, ruler, 1),
            static_cast<uint8_t>(color >> 16),
            static_cast<uint8_t>((color >> 8) & 0xFF),
            static_cast<uint8_t>(color & 0xFF)
        );
    }
}

// Trim `s` so that it fits into `length`.
static wxString trim(wxDC& dc, const wxString& s, int length, bool trim_end)
{
    if (length <= 0) {
        return wxEmptyString;
    }

    // Check if the entire string fits.
    wxSize size = dc.GetTextExtent(s);
    if (size.GetWidth() <= length) {
        return s;
    }

    // Binary search FTW!
    wxString fix("...");
    int i = 0;
    int k = s.length();
    while (k - i > 1) {
        int j = (i + k) / 2;
        size = dc.GetTextExtent(trim_end ? s.substr(0, j) + fix : fix + s.substr(j));
        if (trim_end != (size.GetWidth() > length)) {
            i = j;
        } else {
            k = j;
        }
    }

    return trim_end ? s.substr(0, i) + fix : fix + s.substr(k);
}

// TODO: test
static int bits_to_bands(int bits)
{
    return (1 << (bits - 1)) + 1;
}
