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

#include <cstddef>
#include <expected>
#include <memory>
#include <ostream>
#include <span>
#include <vector>

// The minimum and the maximum number of bits supported by a plan,
// i.e. the plan size is between 2^SPEK_FFT_MIN_BITS and 2^SPEK_FFT_MAX_BITS.
constexpr int SPEK_FFT_MIN_BITS = 2;
constexpr int SPEK_FFT_MAX_BITS = 24;

enum class FFTError
{
    OK,
    INVALID_SIZE, // The number of bits is out of the supported range.
    OUT_OF_MEMORY,
    CANNOT_INIT, // FFmpeg could not create a transform for this size.
};

// A real-to-complex FFT plan. The transform itself is provided by
// libavutil/tx.h which picks the fastest implementation available
// (including SIMD and, where available, hardware acceleration).
//
// Note: the deprecated libavcodec/avfft.h header has been removed in
// FFmpeg 8, av_tx is its supported replacement.
class FFTPlan
{
public:
    virtual ~FFTPlan() = default;

    // Number of real samples consumed by the transform: 2^nbits.
    int input_size() const noexcept { return this->input_size_; }

    // Number of magnitude bins produced by the transform: 2^(nbits - 1) + 1.
    int output_size() const noexcept { return static_cast<int>(this->output_.size()); }

    // Input buffer, aligned for the SIMD code used by FFmpeg.
    // Empty when the plan could not be created.
    std::span<float> input() noexcept
    {
        return std::span<float>(this->input_.get(), this->valid_ ? this->input_size_ : 0);
    }

    // Magnitudes in dB, as produced by the last execute() call.
    std::span<const float> output() const noexcept { return this->output_; }

    // False when the plan could not be created, execute() is a no-op then.
    bool valid() const noexcept { return this->valid_; }

    // Allocates the buffers and lets the implementation set itself up.
    // Returns FFTError::OK when the plan is ready to be used.
    FFTError initialise();

    // Runs the transform and fills the output buffer with magnitudes in dB.
    void execute();

protected:
    FFTPlan(int input_size, int output_size);

    // Allocates the aligned input and scratch buffers.
    bool allocate();

    // Called once after allocate(), lets the implementation set itself up.
    virtual bool do_initialise() { return true; }

    // Transforms `input` (input_size() floats) into `spectrum`, which has
    // room for input_size() + 2 floats and receives input_size() / 2 + 1
    // interleaved (re, im) pairs.
    virtual void transform(std::span<float> input, std::span<float> spectrum) = 0;

    bool valid_ = false;
    int input_size_ = 0;
    std::vector<float> output_;

private:
    struct AvDeleter
    {
        void operator()(float *ptr) const noexcept;
    };

    using AvBuffer = std::unique_ptr<float[], AvDeleter>;

    AvBuffer input_;
    AvBuffer spectrum_;
};

// Creates a plan for 2^nbits input samples.
[[nodiscard]] std::expected<std::unique_ptr<FFTPlan>, FFTError> spek_fft_create(int nbits);

inline std::ostream& operator<<(std::ostream& os, FFTError error)
{
    return os << static_cast<int>(error);
}
