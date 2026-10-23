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

#include <cmath>
#include <span>
#include <string>
#include <vector>

#include "spek-audio.h"
#include "spek-fft.h"
#include "spek-pipeline.h"

#include "test.h"

namespace
{

// Runs the whole analysis, one column at a time.
struct Collector
{
    int bands = 0;
    int samples = 0;
    bool done = false;
    std::vector<float> values;

    SpekPipeline::SampleCallback callback()
    {
        return [this](int bands, int sample, std::span<const float> values) {
            if (sample < 0) {
                this->done = true;
                return;
            }
            this->bands = bands;
            this->samples++;
            this->values.assign(values.begin(), values.end());
        };
    }
};

} // namespace

static void test_pipeline_runs()
{
    const int fft_bits = 9; // 512 samples, 257 bands.
    const int samples = 32;

    Audio audio;
    auto file = audio.open(SAMPLES_DIR "/2ch-44100Hz-16bps.wav", 0);
    test("opened", true, file.has_value());
    if (!file) {
        return;
    }

    auto fft = spek_fft_create(fft_bits);
    test("fft", true, fft.has_value());
    if (!fft) {
        return;
    }

    Collector collector;
    SpekPipeline pipeline(
        std::move(*file),
        std::move(*fft),
        0, // stream
        0, // channel
        WindowFunction::DEFAULT,
        samples,
        collector.callback()
    );

    test("no error", false, pipeline.has_error());
    test("bands", (1 << (fft_bits - 1)) + 1, pipeline.bands());
    test("channels", 2, pipeline.channels());
    test("sample rate", 44100, pipeline.sample_rate());
    test("duration", 0.1, pipeline.duration());
    test("desc", true, pipeline.desc().find("PCM") != std::string::npos);

    pipeline.run();

    test("finished", true, collector.done);
    test("bands", (1 << (fft_bits - 1)) + 1, collector.bands);
    // The file is 0.1s long, every column needs its share of the samples.
    test("samples", true, collector.samples > 0 && collector.samples <= samples);
    test("values", static_cast<size_t>(collector.bands), collector.values.size());
    // The sample files are silent, so everything is way below the noise floor.
    test(
        "silence",
        true,
        collector.values.empty() || collector.values.front() < -100.0f
    );
}

static void test_pipeline_errors()
{
    Audio audio;
    auto file = audio.open(SAMPLES_DIR "/no.file", 0);
    test("not opened", false, file.has_value());
    test("error", AudioError::CANNOT_OPEN_FILE, file.error());

    // A file which exists but has an unusable decoder still reports the
    // stream information together with the error.
    auto fft = spek_fft_create(8);
    test("fft", true, fft.has_value());

    Collector collector;
    SpekPipeline pipeline(
        std::unique_ptr<AudioFile>(),
        std::move(*fft),
        0,
        0,
        WindowFunction::DEFAULT,
        0, // No samples, the analysis cannot be run.
        collector.callback()
    );

    test("error", true, pipeline.has_error());
    pipeline.run();
    test("no samples", true, collector.samples == 0);
}

void test_pipeline()
{
    run("pipeline runs", test_pipeline_runs);
    run("pipeline errors", test_pipeline_errors);
}
