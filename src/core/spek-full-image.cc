// Copyright (C) 2010-2013 Alexander Kojevnikov and contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "spek-config.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Keep the rasterizer private to this translation unit. It works on the
// embedded, trusted font and does not need a GUI, Fontconfig or a display.
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4505)
#endif
#include "third_party/stb_truetype.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

#include "spek-ruler-ticks.h"

#include "spek-full-image.h"

namespace
{

#include "spek-font-data.inc"

constexpr uint32_t BACKGROUND = 0x202020;
constexpr uint32_t FOREGROUND = 0xF0F0F0;
constexpr int FONT_LARGE = 20;
constexpr int FONT_NORMAL = 13;
constexpr int FONT_SMALL = 12;

struct Character
{
    uint32_t value;
    std::size_t length;
};

Character next_character(std::string_view text)
{
    if (text.empty()) {
        return {0, 0};
    }

    const auto first = static_cast<unsigned char>(text[0]);
    if (first < 0x80) {
        return {first, 1};
    }

    std::size_t length = 0;
    uint32_t value = 0;
    uint32_t minimum = 0;
    if (first >= 0xC2 && first <= 0xDF) {
        length = 2;
        value = first & 0x1F;
        minimum = 0x80;
    } else if (first >= 0xE0 && first <= 0xEF) {
        length = 3;
        value = first & 0x0F;
        minimum = 0x800;
    } else if (first >= 0xF0 && first <= 0xF4) {
        length = 4;
        value = first & 0x07;
        minimum = 0x10000;
    } else {
        return {'?', 1};
    }

    if (text.size() < length) {
        return {'?', 1};
    }
    for (std::size_t i = 1; i < length; ++i) {
        const auto next = static_cast<unsigned char>(text[i]);
        if ((next & 0xC0) != 0x80) {
            return {'?', 1};
        }
        value = (value << 6) | (next & 0x3F);
    }
    if (value < minimum || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) {
        return {'?', 1};
    }
    return {value, length};
}

class Font
{
public:
    Font()
    {
        const int offset = stbtt_GetFontOffsetForIndex(spek_font_data, 0);
        if (offset < 0 || !stbtt_InitFont(&this->info, spek_font_data, offset)) {
            return;
        }
        stbtt_GetFontVMetrics(&this->info, &this->ascent, &this->descent, &this->line_gap);
        this->ready = true;
    }

    bool valid() const { return this->ready; }

    float scale(int pixel_height) const
    {
        return stbtt_ScaleForPixelHeight(&this->info, static_cast<float>(pixel_height));
    }

    int line_height(int pixel_height) const
    {
        if (!this->ready) {
            return pixel_height;
        }
        return std::max(1, static_cast<int>(std::ceil(
            (this->ascent - this->descent + this->line_gap) * this->scale(pixel_height)
        )));
    }

    uint32_t supported(uint32_t codepoint) const
    {
        if (codepoint == 0xA0 || codepoint == '\t') {
            codepoint = ' ';
        }
        if (codepoint != '?'
            && stbtt_FindGlyphIndex(&this->info, static_cast<int>(codepoint)) == 0) {
            return '?';
        }
        return codepoint;
    }

    int width(std::string_view text, int pixel_height = FONT_NORMAL) const
    {
        if (!this->ready) {
            return 0;
        }
        const float scale = this->scale(pixel_height);
        double result = 0.0;
        uint32_t previous = 0;
        while (!text.empty()) {
            const Character character = next_character(text);
            text.remove_prefix(character.length);
            const uint32_t codepoint = this->supported(character.value);
            if (previous != 0) {
                result += stbtt_GetCodepointKernAdvance(
                    &this->info, static_cast<int>(previous), static_cast<int>(codepoint)
                ) * scale;
            }
            int advance = 0;
            int left_side_bearing = 0;
            stbtt_GetCodepointHMetrics(
                &this->info, static_cast<int>(codepoint), &advance, &left_side_bearing
            );
            result += advance * scale;
            previous = codepoint;
            if (result >= std::numeric_limits<int>::max()) {
                return std::numeric_limits<int>::max();
            }
        }
        return std::max(0, static_cast<int>(std::ceil(result)));
    }

    stbtt_fontinfo info{};
    int ascent = 0;
    int descent = 0;
    int line_gap = 0;

private:
    bool ready = false;
};

class Canvas
{
public:
    Canvas(SpekImage& image, const Font& font) : image(image), font(font) {}

    int text_width(std::string_view value, int pixel_height = FONT_NORMAL) const
    {
        return this->font.width(value, pixel_height);
    }

    int line_height(int pixel_height) const
    {
        return this->font.line_height(pixel_height);
    }

    void fill_rect(int x, int y, int width, int height, uint32_t color = FOREGROUND)
    {
        if (width <= 0 || height <= 0) {
            return;
        }
        const long long right = static_cast<long long>(x) + width;
        const long long bottom = static_cast<long long>(y) + height;
        const int x0 = std::max(x, 0);
        const int y0 = std::max(y, 0);
        const int x1 = static_cast<int>(std::clamp<long long>(right, 0, this->image.width()));
        const int y1 = static_cast<int>(std::clamp<long long>(bottom, 0, this->image.height()));
        for (int py = y0; py < y1; ++py) {
            for (int px = x0; px < x1; ++px) {
                this->image.set_pixel(px, py, color);
            }
        }
    }

    void text(int x, int y, std::string_view value, int pixel_height = FONT_NORMAL, bool bold = false)
    {
        if (!this->font.valid()) {
            return;
        }

        const float scale = this->font.scale(pixel_height);
        const int baseline = y + static_cast<int>(std::lround(this->font.ascent * scale));
        double pen_x = x;
        uint32_t previous = 0;
        while (!value.empty()) {
            const Character character = next_character(value);
            value.remove_prefix(character.length);
            const uint32_t codepoint = this->font.supported(character.value);
            if (previous != 0) {
                pen_x += stbtt_GetCodepointKernAdvance(
                    &this->font.info, static_cast<int>(previous), static_cast<int>(codepoint)
                ) * scale;
            }

            int x0 = 0;
            int y0 = 0;
            int x1 = 0;
            int y1 = 0;
            stbtt_GetCodepointBitmapBox(
                &this->font.info, static_cast<int>(codepoint), scale, scale, &x0, &y0, &x1, &y1
            );
            const int width = x1 - x0;
            const int height = y1 - y0;
            if (width > 0 && height > 0) {
                std::vector<unsigned char> alpha(static_cast<std::size_t>(width) * height);
                stbtt_MakeCodepointBitmap(
                    &this->font.info,
                    alpha.data(),
                    width,
                    height,
                    width,
                    scale,
                    scale,
                    static_cast<int>(codepoint)
                );
                const int origin_x = static_cast<int>(std::lround(pen_x)) + x0;
                const int origin_y = baseline + y0;
                for (int row = 0; row < height; ++row) {
                    for (int column = 0; column < width; ++column) {
                        const unsigned char coverage = alpha[
                            static_cast<std::size_t>(row) * width + column
                        ];
                        this->blend_pixel(origin_x + column, origin_y + row, coverage);
                        if (bold) {
                            this->blend_pixel(origin_x + column + 1, origin_y + row, coverage);
                        }
                    }
                }
            }

            int advance = 0;
            int left_side_bearing = 0;
            stbtt_GetCodepointHMetrics(
                &this->font.info, static_cast<int>(codepoint), &advance, &left_side_bearing
            );
            pen_x += advance * scale;
            previous = codepoint;
        }
    }

    void rectangle(int x, int y, int width, int height)
    {
        if (width <= 0 || height <= 0) {
            return;
        }
        fill_rect(x, y, width, 1);
        fill_rect(x, y + height - 1, width, 1);
        fill_rect(x, y, 1, height);
        fill_rect(x + width - 1, y, 1, height);
    }

private:
    void blend_pixel(int x, int y, unsigned char coverage)
    {
        if (coverage == 0 || x < 0 || y < 0 || x >= this->image.width() || y >= this->image.height()) {
            return;
        }
        const uint32_t destination = this->image.pixel(x, y);
        const unsigned int inverse = 255 - coverage;
        const auto channel = [coverage, inverse](uint32_t foreground, uint32_t background, int shift) {
            const unsigned int fg = (foreground >> shift) & 0xFF;
            const unsigned int bg = (background >> shift) & 0xFF;
            return static_cast<uint32_t>((fg * coverage + bg * inverse + 127) / 255);
        };
        const uint32_t color = (channel(FOREGROUND, destination, 16) << 16)
            | (channel(FOREGROUND, destination, 8) << 8)
            | channel(FOREGROUND, destination, 0);
        this->image.set_pixel(x, y, color);
    }

    SpekImage& image;
    const Font& font;
};

enum class RulerPosition
{
    RIGHT,
    BOTTOM,
    LEFT,
};

using Formatter = std::string (*)(int unit);

std::string format_time(int unit)
{
    const int minutes = unit / 60;
    const int seconds = unit % 60;
    const std::string seconds_string = std::to_string(seconds);
    return std::to_string(minutes) + ":" + (seconds < 10 ? "0" : "") + seconds_string;
}

std::string format_frequency(int unit)
{
    return std::to_string(unit / 1000) + " kHz";
}

std::string format_density(int unit)
{
    return std::to_string(-unit) + " dB";
}

void draw_ruler(
    Canvas& canvas,
    int x,
    int y,
    RulerPosition position,
    std::string_view sample_label,
    std::span<const int> factors,
    int min_units,
    int max_units,
    double spacing,
    double scale,
    double offset,
    Formatter formatter,
    int gap,
    int tick_length
)
{
    const bool horizontal = position == RulerPosition::BOTTOM;
    const int extent = horizontal
        ? canvas.text_width(sample_label, FONT_SMALL)
        : canvas.line_height(FONT_SMALL);
    const auto ticks = spek_ruler_ticks(
        factors, min_units, max_units, scale, spacing, extent
    );

    for (const int tick : ticks) {
        const std::string label = formatter(tick);
        const int value = horizontal ? tick : max_units + min_units - tick;
        const int p = static_cast<int>(offset + scale * (value - min_units));
        const int width = canvas.text_width(label, FONT_SMALL);

        if (position == RulerPosition::BOTTOM) {
            canvas.text(x + p - width / 2, y + gap, label, FONT_SMALL);
            canvas.fill_rect(x + p, y, 1, tick_length);
        } else if (position == RulerPosition::RIGHT) {
            canvas.text(
                x + gap, y + p - canvas.line_height(FONT_SMALL) / 2, label, FONT_SMALL
            );
            canvas.fill_rect(x, y + p, tick_length, 1);
        } else {
            canvas.text(
                x - width - gap, y + p - canvas.line_height(FONT_SMALL) / 2, label, FONT_SMALL
            );
            canvas.fill_rect(x - tick_length, y + p, tick_length, 1);
        }
    }
}

void draw_scaled(Canvas& canvas, const SpekImage& source, int x, int y, int width, int height)
{
    if (source.empty() || width <= 0 || height <= 0) {
        return;
    }
    const auto pixels = source.pixels();
    for (int dy = 0; dy < height; ++dy) {
        const int sy = std::min(
            source.height() - 1,
            static_cast<int>(static_cast<int64_t>(dy) * source.height() / height)
        );
        for (int dx = 0; dx < width; ++dx) {
            const int sx = std::min(
                source.width() - 1,
                static_cast<int>(static_cast<int64_t>(dx) * source.width() / width)
            );
            const std::size_t offset =
                (static_cast<std::size_t>(sy) * source.width() + sx) * 3;
            const uint32_t color = (static_cast<uint32_t>(pixels[offset]) << 16)
                | (static_cast<uint32_t>(pixels[offset + 1]) << 8)
                | static_cast<uint32_t>(pixels[offset + 2]);
            canvas.fill_rect(x + dx, y + dy, 1, 1, color);
        }
    }
}

} // namespace

SpekImage spek_render_full_image(const SpekImage& spectrogram, const SpekFullImageInfo& info)
{
    if (spectrogram.empty()) {
        return {};
    }

    const Font font;
    if (!font.valid()) {
        return {};
    }

    constexpr int gap = 10;
    constexpr int ruler = 10;
    constexpr int tick = 4;
    const int large_height = font.line_height(FONT_LARGE);
    const int normal_height = font.line_height(FONT_NORMAL);
    const int small_height = font.line_height(FONT_SMALL);

    const int frequency_width = font.width("000 kHz", FONT_SMALL);
    const int density_width = font.width("-000 dB", FONT_SMALL);
    const std::string package_name = std::string(SPEK_DISPLAY_NAME) + " ";
    const int version_width = font.width(package_name) + font.width(SPEK_VERSION);

    const int lpad = gap + frequency_width + gap;
    const int rpad = std::max(
        gap + ruler + gap + density_width + gap,
        gap + version_width + gap
    );
    const int tpad = gap + large_height + gap + normal_height + gap;
    const int bpad = gap + std::max(tick, gap + small_height) + gap;

    // Grow the plot instead of ellipsising a long file name or description.
    const int plot_width = std::max({
        spectrogram.width(),
        font.width(info.file_name, FONT_LARGE),
        font.width(info.description),
    });
    const int plot_height = spectrogram.height();

    SpekImage result(lpad + plot_width + rpad, tpad + plot_height + bpad);
    Canvas canvas(result, font);
    // Clear the complete backing store before drawing any rulers or labels so
    // no uninitialized or stale stripes can remain around the plot.
    canvas.fill_rect(0, 0, result.width(), result.height(), BACKGROUND);

    const int name_y = gap;
    const int desc_y = name_y + large_height + gap;
    canvas.text(lpad, name_y, info.file_name, FONT_LARGE, true);
    canvas.text(lpad, desc_y, info.description);

    const int package_x = result.width() - rpad + gap;
    canvas.text(package_x, name_y, SPEK_DISPLAY_NAME, FONT_NORMAL, true);
    canvas.text(package_x + font.width(package_name), desc_y, SPEK_VERSION);

    draw_scaled(canvas, spectrogram, lpad, tpad, plot_width, plot_height);

    if (std::isfinite(info.duration) && info.duration >= 1.0) {
        constexpr std::array factors{
            1, 2, 5, 10, 20, 30, 1 * 60, 2 * 60, 5 * 60, 10 * 60, 20 * 60, 30 * 60, 0
        };
        const int duration = static_cast<int>(std::min(
            info.duration, static_cast<double>(std::numeric_limits<int>::max())
        ));
        draw_ruler(
            canvas,
            lpad,
            tpad + plot_height,
            RulerPosition::BOTTOM,
            "00:00",
            factors,
            0,
            duration,
            1.5,
            plot_width / info.duration,
            0.0,
            format_time,
            gap,
            tick
        );
    }

    const int frequency = info.sample_rate / 2;
    if (frequency > 0) {
        constexpr std::array factors{1000, 2000, 5000, 10000, 20000, 0};
        draw_ruler(
            canvas,
            lpad,
            tpad,
            RulerPosition::LEFT,
            "00 kHz",
            factors,
            0,
            frequency,
            3.0,
            plot_height / static_cast<double>(frequency),
            0.0,
            format_frequency,
            gap,
            tick
        );
    }

    const int palette_x = result.width() - rpad + gap;
    for (int y = 0; y <= plot_height; ++y) {
        // Match SpekSpectrogram::create_palette(): its brightest source row
        // is just below 1.0, which also matters for the spectrum palette.
        const double level = (plot_height - y) / static_cast<double>(plot_height + 1);
        canvas.fill_rect(
            palette_x, tpad + y, ruler, 1, spek_palette(info.palette, level)
        );
    }

    if (std::isfinite(info.min_db) && std::isfinite(info.max_db)
        && info.max_db > info.min_db) {
        constexpr std::array factors{1, 2, 5, 10, 20, 50, 0};
        draw_ruler(
            canvas,
            palette_x + ruler,
            tpad,
            RulerPosition::RIGHT,
            "-00 dB",
            factors,
            static_cast<int>(-info.max_db),
            static_cast<int>(-info.min_db),
            3.0,
            plot_height / (info.min_db - info.max_db),
            plot_height,
            format_density,
            gap,
            tick
        );
    }

    canvas.rectangle(lpad, tpad, plot_width, plot_height);
    return result;
}
