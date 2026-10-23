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
#include <cmath>
#include <numbers>
#include <ranges>

#include "spek-fft.h"

#include "test.h"

// FFT sizes that the transform can actually handle.
static const int FFT_BITS_MIN = 4;
static const int FFT_BITS_MAX = 15;

namespace
{

constexpr float SILENCE = -1e12f;

bool is_silence(std::span<const float> values)
{
    return std::ranges::all_of(values, [](float value) { return value < SILENCE; });
}

} // namespace

static void test_sizes()
{
    // Sizes outside of the supported range are rejected.
    test("too small", FFTError::INVALID_SIZE, spek_fft_create(SPEK_FFT_MIN_BITS - 1).error());
    test("too large", FFTError::INVALID_SIZE, spek_fft_create(SPEK_FFT_MAX_BITS + 1).error());

    auto plan = spek_fft_create(SPEK_FFT_MIN_BITS);
    test("created", true, plan.has_value());
    test("valid", true, (*plan)->valid());
}

static void test_const()
{
    for (int nbits = FFT_BITS_MIN; nbits <= FFT_BITS_MAX; ++nbits) {
        auto plan = spek_fft_create(nbits);
        test("created", true, plan.has_value());
        if (!plan) {
            continue;
        }

        test("input size", 1 << nbits, (*plan)->input_size());
        test("output size", (1 << nbits) / 2 + 1, (*plan)->output_size());

        // Test zero input.
        std::ranges::fill((*plan)->input(), 0.0f);
        (*plan)->execute();
        test("silence", true, is_silence((*plan)->output()));

        // Test DC input.
        std::ranges::fill((*plan)->input(), 1.0f);
        (*plan)->execute();
        test("dc component", 0.0f, (*plan)->output()[0]);
        test("silence", true, is_silence((*plan)->output().subspan(1)));
    }
}

static void test_sine()
{
    for (int nbits = FFT_BITS_MIN; nbits <= FFT_BITS_MAX; ++nbits) {
        auto plan = spek_fft_create(nbits);
        if (!plan) {
            continue;
        }

        const int n = (*plan)->input_size();
        for (int k = 1; k < n / 2; k *= 2) {
            for (int i = 0; i < n; ++i) {
                (*plan)->input()[static_cast<size_t>(i)] =
                    static_cast<float>(std::sin(k * i * 2.0 * std::numbers::pi / n));
            }
            (*plan)->execute();
            test("sine", -602, static_cast<int>((*plan)->output()[static_cast<size_t>(k)] * 100));

            // Everything but the k-th bin is silent.
            bool silence = true;
            for (int i = 0; i < (*plan)->output_size(); ++i) {
                if (i == k) {
                    continue;
                }
                if ((*plan)->output()[static_cast<size_t>(i)] > -149.0f) {
                    silence = false;
                    break;
                }
            }
            test("silence", true, silence);
        }
    }
}

// The deprecated libavcodec/avfft.h API and libavutil/tx.h must produce the
// same magnitudes, this is the transform used by the pipeline.
static void test_magnitudes()
{
    auto plan = spek_fft_create(3); // 8 input samples, 5 output bins.
    test("created", true, plan.has_value());
    if (!plan) {
        return;
    }

    // A full scale sine at bin 1.
    for (int i = 0; i < 8; ++i) {
        (*plan)->input()[static_cast<size_t>(i)] =
            static_cast<float>(std::sin(i * 2.0 * std::numbers::pi / 8));
    }
    (*plan)->execute();

    // A sine of amplitude 1 has a magnitude of 1/2 per bin, i.e. -6.02 dB.
    test("bin 1", -602, static_cast<int>((*plan)->output()[1] * 100));
    // The Nyquist bin (bin 4) gets nothing.
    test("nyquist", true, (*plan)->output()[4] < -100.0f);
    // Neither does DC.
    test("dc", true, (*plan)->output()[0] < -100.0f);
}

void test_fft()
{
    run("fft sizes", test_sizes);
    run("fft const", test_const);
    run("fft sine", test_sine);
    run("fft magnitudes", test_magnitudes);
}
