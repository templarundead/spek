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
#include <cassert>
#include <cmath>
#include <numbers>

#include "spek-audio.h"
#include "spek-fft.h"
#include "spek-i18n.h"
#include "spek-utils.h"

#include "spek-pipeline.h"

namespace
{

constexpr int NFFT = 64; // Number of FFTs to pre-fetch.

float get_window(WindowFunction f, int i, std::span<const float> coss, int n)
{
    switch (f) {
    case WindowFunction::HANN:
        return 0.5f * (1.0f - coss[i]);
    case WindowFunction::HAMMING:
        return 0.53836f - 0.46164f * coss[i];
    case WindowFunction::BLACKMAN_HARRIS:
        return 0.35875f - 0.48829f * coss[i] + 0.14128f * coss[2 * i % n]
            - 0.01168f * coss[3 * i % n];
    default:
        assert(false);
        return 0.0f;
    }
}

} // namespace

SpekPipeline::SpekPipeline(
    std::unique_ptr<AudioFile> file,
    std::unique_ptr<FFTPlan> fft,
    int stream,
    int channel,
    WindowFunction window_function,
    int samples,
    SampleCallback cb
) :
    file(std::move(file)),
    fft(std::move(fft)),
    stream(stream),
    channel(channel),
    window_function(window_function),
    samples(samples),
    cb(std::move(cb)),
    nfft(0),
    input_size(0),
    input_pos(0),
    worker_done(false),
    quit(false)
{
    if (!this->has_error()) {
        this->nfft = this->fft->input_size();
        this->coss.resize(static_cast<size_t>(this->nfft));

        const float cf = 2.0f * std::numbers::pi_v<float> / (this->nfft - 1.0f);
        for (int i = 0; i < this->nfft; ++i) {
            this->coss[static_cast<size_t>(i)] = std::cos(cf * static_cast<float>(i));
        }

        this->input_size = this->nfft * (NFFT * 2 + 1);
        this->input.resize(static_cast<size_t>(this->input_size), 0.0f);
        this->output.resize(static_cast<size_t>(this->fft->output_size()), 0.0f);

        this->file->start(this->channel, this->samples);
    }
}

SpekPipeline::~SpekPipeline()
{
    this->stop();
}

void SpekPipeline::start()
{
    if (this->has_error() || this->reader_thread.joinable()) {
        return;
    }

    this->input_pos = 0;
    this->worker_done = false;
    this->quit.store(false);

    this->worker_thread = std::thread(&SpekPipeline::worker_func, this);
    this->reader_thread = std::thread(&SpekPipeline::reader_func, this);
}

void SpekPipeline::wait()
{
    this->join_threads();
}

void SpekPipeline::run()
{
    this->start();
    this->wait();
}

void SpekPipeline::stop()
{
    this->quit.store(true);

    {
        std::lock_guard<std::mutex> lock(this->worker_mutex);
        this->input_pos = -1;
    }
    this->worker_cond.notify_all();

    this->join_threads();
}

void SpekPipeline::join_threads()
{
    // The worker thread is always joined by the reader thread, which is the
    // only place where it is known to have finished.
    if (this->reader_thread.joinable()) {
        if (this->reader_thread.get_id() == std::this_thread::get_id()) {
            // Called from the reader thread itself (e.g. from the callback),
            // let it finish on its own.
            this->reader_thread.detach();
        } else {
            this->reader_thread.join();
        }
    }
}

bool SpekPipeline::has_error() const
{
    return !this->file || !!this->file->get_error() || !this->fft || !this->fft->valid()
        || this->samples <= 0;
}

std::string SpekPipeline::desc() const
{
    std::vector<std::string> items;

    if (!this->file->get_codec_name().empty()) {
        items.push_back(this->file->get_codec_name());
    }

    if (this->file->get_bit_rate()) {
        items.push_back(spek_format(
            spek_tr("%d kbps").c_str(), (this->file->get_bit_rate() + 500) / 1000
        ));
    }

    if (this->file->get_sample_rate()) {
        items.push_back(spek_format(spek_tr("%d Hz").c_str(), this->file->get_sample_rate()));
    }

    // Include bits per sample only if there is no bitrate.
    if (this->file->get_bits_per_sample() && !this->file->get_bit_rate()) {
        const int bps = this->file->get_bits_per_sample();
        items.push_back(spek_format(
            spek_tr_n("%d bit", "%d bits", static_cast<unsigned long>(bps)).c_str(), bps
        ));
    }

    if (this->file->get_channels()) {
        items.push_back(spek_format(
            // TRANSLATORS: first %d is the current channel, second %d is the total number.
            "channel %d / %d",
            this->channel + 1,
            this->file->get_channels()
        ));
    }

    if (!this->file->get_error()) {
        items.push_back(spek_format("W:%i", this->nfft));

        std::string window_function_name;
        switch (this->window_function) {
        case WindowFunction::HANN:
            window_function_name = "Hann";
            break;
        case WindowFunction::HAMMING:
            window_function_name = "Hamming";
            break;
        case WindowFunction::BLACKMAN_HARRIS:
            window_function_name = "Blackman–Harris";
            break;
        default:
            assert(false);
        }
        if (!window_function_name.empty()) {
            items.push_back("F:" + window_function_name);
        }
    }

    std::string desc;
    for (const auto& item : items) {
        if (!desc.empty()) {
            desc.append(", ");
        }
        desc.append(item);
    }

    const std::string error_string = spek_audio_error_message(this->file->get_error());
    if (desc.empty()) {
        desc = error_string;
    } else if (!error_string.empty()) {
        desc = spek_format(
            // TRANSLATORS: first %s is the error message, second %s is stream description.
            spek_tr("%s: %s").c_str(),
            error_string.c_str(),
            desc.c_str()
        );
    } else if (this->stream < this->file->get_streams()) {
        desc = spek_format(
            // TRANSLATORS: first %d is the stream number, second %d is the
            // total number of streams, %s is the stream description.
            spek_tr("Stream %d / %d: %s").c_str(),
            this->stream + 1,
            this->file->get_streams(),
            desc.c_str()
        );
    }

    return desc;
}

int SpekPipeline::streams() const
{
    return this->file->get_streams();
}

int SpekPipeline::channels() const
{
    return this->file->get_channels();
}

double SpekPipeline::duration() const
{
    return this->file->get_duration();
}

int SpekPipeline::sample_rate() const
{
    return this->file->get_sample_rate();
}

int SpekPipeline::bands() const
{
    return this->fft ? this->fft->output_size() : 0;
}

void SpekPipeline::reader_func()
{
    int pos = 0;
    int prev_pos = 0;
    int len;

    while ((len = this->file->read()) > 0) {
        if (this->quit.load()) {
            break;
        }

        const std::span<const float> buffer = this->file->get_buffer();
        auto it = buffer.begin();
        while (len-- > 0) {
            this->input[static_cast<size_t>(pos)] = *it++;
            pos = (pos + 1) % this->input_size;

            // Wake up the worker if we have enough data.
            if ((pos > prev_pos ? pos : pos + this->input_size) - prev_pos == this->nfft * NFFT) {
                this->reader_sync(prev_pos = pos);
            }
        }
        assert(len == -1);
    }

    if (pos != prev_pos) {
        // Process the remaining data.
        this->reader_sync(pos);
    }

    // Force the worker to quit.
    this->reader_sync(-1);
    this->worker_thread.join();

    // Notify the client.
    if (this->cb) {
        this->cb(this->fft->output_size(), -1, {});
    }
}

void SpekPipeline::reader_sync(int pos)
{
    {
        std::unique_lock<std::mutex> lock(this->reader_mutex);
        this->reader_cond.wait(lock, [this] { return this->worker_done || this->quit.load(); });
        this->worker_done = false;
    }

    {
        std::lock_guard<std::mutex> lock(this->worker_mutex);
        this->input_pos = pos;
    }
    this->worker_cond.notify_one();
}

void SpekPipeline::worker_func()
{
    int sample = 0;
    int64_t frames = 0;
    int64_t num_fft = 0;
    int64_t acc_error = 0;
    int head = 0;
    int tail = 0;
    int prev_head = 0;

    std::span<float> output(this->output);
    std::ranges::fill(output, 0.0f);

    while (true) {
        {
            std::lock_guard<std::mutex> lock(this->reader_mutex);
            this->worker_done = true;
        }
        this->reader_cond.notify_one();

        {
            std::unique_lock<std::mutex> lock(this->worker_mutex);
            this->worker_cond.wait(
                lock, [this, tail] { return this->input_pos != tail || this->quit.load(); }
            );
            tail = this->input_pos;
        }

        if (tail == -1 || this->quit.load()) {
            return;
        }

        while (true) {
            head = (head + 1) % this->input_size;
            if (head == tail) {
                head = prev_head;
                break;
            }
            frames++;

            // If we have enough frames for an FFT or we have
            // all frames required for the interval run and FFT.
            bool int_full = acc_error < this->file->get_error_base()
                && frames == this->file->get_frames_per_interval();
            bool int_over = acc_error >= this->file->get_error_base()
                && frames == 1 + this->file->get_frames_per_interval();

            if (frames % this->nfft == 0 || ((int_full || int_over) && num_fft == 0)) {
                prev_head = head;
                std::span<float> input(this->fft->input());
                for (int i = 0; i < this->nfft; i++) {
                    float val = this->input[static_cast<size_t>(
                        (this->input_size + head - this->nfft + i) % this->input_size
                    )];
                    val *= get_window(this->window_function, i, this->coss, this->nfft);
                    input[static_cast<size_t>(i)] = val;
                }
                this->fft->execute();
                num_fft++;
                const std::span<const float> fft_output = this->fft->output();
                for (size_t i = 0; i < output.size(); i++) {
                    output[i] += fft_output[i];
                }
            }

            // Do we have the FFTs for one interval?
            if (int_full || int_over) {
                if (int_over) {
                    acc_error -= this->file->get_error_base();
                } else {
                    acc_error += this->file->get_error_per_interval();
                }

                for (auto& value : output) {
                    value /= static_cast<float>(num_fft);
                }

                if (sample == this->samples) {
                    break;
                }
                if (this->cb) {
                    this->cb(static_cast<int>(output.size()), sample++, output);
                }

                std::ranges::fill(output, 0.0f);
                frames = 0;
                num_fft = 0;
            }
        }
    }
}
