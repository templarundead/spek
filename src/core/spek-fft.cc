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
#include <utility>

extern "C" {
#include <libavutil/mem.h>
#include <libavutil/tx.h>
}

#include "spek-fft.h"

namespace
{

class FFTPlanImpl final : public FFTPlan
{
public:
    explicit FFTPlanImpl(int nbits);
    ~FFTPlanImpl() override;

protected:
    bool do_initialise() override;
    void transform(std::span<float> input, std::span<float> spectrum) override;

private:
    int len;
    AVTXContext *ctx;
    av_tx_fn tx;
};

FFTPlanImpl::FFTPlanImpl(int nbits) :
    FFTPlan(1 << nbits, (1 << (nbits - 1)) + 1), len(1 << nbits), ctx(nullptr), tx(nullptr)
{
}

FFTPlanImpl::~FFTPlanImpl()
{
    if (this->tx != nullptr) {
        av_tx_uninit(&this->ctx);
    }
}

bool FFTPlanImpl::do_initialise()
{
    // A scale of 1.0 gives the same unnormalised transform avfft used to
    // return, the magnitudes are normalised in execute() below.
    const float scale = 1.0f;
    const int ret = av_tx_init(&this->ctx, &this->tx, AV_TX_FLOAT_RDFT, 0, this->len, &scale, 0);
    if (ret < 0 || this->tx == nullptr) {
        return false;
    }
    return true;
}

void FFTPlanImpl::transform(std::span<float> input, std::span<float> spectrum)
{
    // The output is a standard complex spectrum: bins 0 .. len / 2,
    // each of them an interleaved (re, im) pair.
    this->tx(this->ctx, spectrum.data(), input.data(), sizeof(float));
}

} // namespace

void FFTPlan::AvDeleter::operator()(float *ptr) const noexcept
{
    av_free(ptr);
}

FFTPlan::FFTPlan(int input_size, int output_size) :
    input_size_(input_size), output_(static_cast<size_t>(output_size), 0.0f)
{
}

FFTError FFTPlan::initialise()
{
    if (!this->allocate()) {
        return FFTError::OUT_OF_MEMORY;
    }
    if (!this->do_initialise()) {
        return FFTError::CANNOT_INIT;
    }
    return FFTError::OK;
}

bool FFTPlan::allocate()
{
    // FFmpeg uses various assembly optimisations which expect the data to be
    // aligned by up to 32 bytes (e.g. AVX), hence av_malloc() and friends.
    this->input_.reset(
        static_cast<float *>(av_malloc(sizeof(float) * static_cast<size_t>(this->input_size_)))
    );
    // The spectrum holds len / 2 + 1 complex values, i.e. len + 2 floats.
    this->spectrum_.reset(static_cast<float *>(
        av_malloc(sizeof(float) * (static_cast<size_t>(this->input_size_) + 2))
    ));

    if (!this->input_ || !this->spectrum_) {
        return false;
    }

    this->valid_ = true;
    return true;
}

void FFTPlan::execute()
{
    if (!this->valid_) {
        return;
    }

    const std::span<float> spectrum(this->spectrum_.get(), static_cast<size_t>(this->input_size_) + 2);
    this->transform(this->input(), spectrum);

    // Calculate magnitudes.
    const int n = this->input_size_;
    const float n2 = static_cast<float>(n) * static_cast<float>(n);
    const int bins = n / 2;

    auto to_db = [n2](float re, float im) {
        return 10.0f * log10f((re * re + im * im) / n2);
    };

    // DC (bin 0) and Nyquist (bin n / 2) are real-only, the imaginary part
    // of both bins is zero.
    this->output_[0] = to_db(spectrum[0], 0.0f);
    this->output_[static_cast<size_t>(bins)] = to_db(spectrum[static_cast<size_t>(n)], 0.0f);

    for (int i = 1; i < bins; ++i) {
        const size_t j = static_cast<size_t>(i) * 2;
        this->output_[static_cast<size_t>(i)] = to_db(spectrum[j], spectrum[j + 1]);
    }
}

std::expected<std::unique_ptr<FFTPlan>, FFTError> spek_fft_create(int nbits)
{
    if (nbits < SPEK_FFT_MIN_BITS || nbits > SPEK_FFT_MAX_BITS) {
        return std::unexpected(FFTError::INVALID_SIZE);
    }

    auto plan = std::make_unique<FFTPlanImpl>(nbits);
    if (const FFTError error = plan->initialise(); error != FFTError::OK) {
        return std::unexpected(error);
    }

    return plan;
}
