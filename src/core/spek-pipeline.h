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

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

class AudioFile;
class FFTPlan;

enum class WindowFunction
{
    HANN,
    HAMMING,
    BLACKMAN_HARRIS,
    COUNT,
    DEFAULT = HANN,
};

// Decodes an audio file and runs the FFTs, one column at a time.
//
// The pipeline is toolkit-agnostic: it never touches wxWidgets, which makes
// it possible to reuse it from the command line interface.
class SpekPipeline
{
public:
    // Called once per spectrogram column. `sample` is the column index, it is
    // -1 (with empty `values`) once the analysis is over.
    //
    // The callback is invoked from the reader thread, so it must be thread
    // safe and it must not call stop() unless it is prepared to be detached.
    using SampleCallback = std::function<void(int bands, int sample, std::span<const float> values)>;

    SpekPipeline(
        std::unique_ptr<AudioFile> file,
        std::unique_ptr<FFTPlan> fft,
        int stream,
        int channel,
        WindowFunction window_function,
        int samples,
        SampleCallback cb
    );

    ~SpekPipeline();

    SpekPipeline(const SpekPipeline&) = delete;
    SpekPipeline& operator=(const SpekPipeline&) = delete;

    // Starts the reader and the worker threads, returns immediately.
    void start();

    // Waits until the analysis is over.
    void wait();

    // Starts the analysis and waits until it is over.
    void run();

    // Asks the pipeline to stop and waits for the threads to finish.
    // Safe to call from any thread, including the reader thread.
    void stop();

    // True when the analysis cannot be run, see desc() for the reason.
    bool has_error() const;

    // A description of the audio stream, or the error which occurred.
    std::string desc() const;

    int streams() const;
    int channels() const;
    double duration() const;
    int sample_rate() const;
    int bands() const;

private:
    void reader_func();
    void worker_func();
    void reader_sync(int pos);
    void join_threads();

    std::unique_ptr<AudioFile> file;
    std::unique_ptr<FFTPlan> fft;
    int stream;
    int channel;
    WindowFunction window_function;
    int samples;
    SampleCallback cb;

    std::vector<float> coss; // Pre-computed cos table.
    int nfft; // Size of the FFT transform.
    int input_size;
    int input_pos;
    std::vector<float> input;
    std::vector<float> output;

    std::thread reader_thread;
    std::thread worker_thread;
    std::mutex reader_mutex;
    std::condition_variable reader_cond;
    std::mutex worker_mutex;
    std::condition_variable worker_cond;
    bool worker_done;
    std::atomic<bool> quit;
};
