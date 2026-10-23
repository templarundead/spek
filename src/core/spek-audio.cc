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

#include <cassert>
#include <cstring>
#include <string>
#include <utility>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/mathematics.h>
#include <libavutil/mem.h>
#include <libavutil/samplefmt.h>
}

#include "spek-audio.h"
#include "spek-i18n.h"

namespace
{

// RAII wrappers around the FFmpeg objects we own.
struct FormatDeleter
{
    void operator()(AVFormatContext *ctx) const noexcept { avformat_close_input(&ctx); }
};

struct CodecDeleter
{
    void operator()(AVCodecContext *ctx) const noexcept { avcodec_free_context(&ctx); }
};

struct PacketDeleter
{
    void operator()(AVPacket *packet) const noexcept { av_packet_free(&packet); }
};

struct FrameDeleter
{
    void operator()(AVFrame *frame) const noexcept { av_frame_free(&frame); }
};

struct BufferDeleter
{
    void operator()(float *buffer) const noexcept { av_free(buffer); }
};

using FormatPtr = std::unique_ptr<AVFormatContext, FormatDeleter>;
using CodecPtr = std::unique_ptr<AVCodecContext, CodecDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;
using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;
using SampleBuffer = std::unique_ptr<float[], BufferDeleter>;

class AudioFileImpl final : public AudioFile
{
public:
    AudioFileImpl(
        AudioError error, FormatPtr format_context, CodecPtr codec_context, int audio_stream,
        const std::string& codec_name, int bit_rate, int sample_rate, int bits_per_sample,
        int streams, int channels, double duration
    );
    ~AudioFileImpl() override = default;

    void start(int channel, int samples) override;
    int read() override;

    AudioError get_error() const override { return this->error; }
    std::string get_codec_name() const override { return this->codec_name; }
    int get_bit_rate() const override { return this->bit_rate; }
    int get_sample_rate() const override { return this->sample_rate; }
    int get_bits_per_sample() const override { return this->bits_per_sample; }
    int get_streams() const override { return this->streams; }
    int get_channels() const override { return this->channels; }
    double get_duration() const override { return this->duration; }
    std::span<const float> get_buffer() const override
    {
        return std::span<const float>(this->buffer.get(), this->buffer_len);
    }
    int64_t get_frames_per_interval() const override { return this->frames_per_interval; }
    int64_t get_error_per_interval() const override { return this->error_per_interval; }
    int64_t get_error_base() const override { return this->error_base; }

private:
    AudioError error;
    FormatPtr format_context;
    CodecPtr codec_context;
    int audio_stream;
    std::string codec_name;
    int bit_rate;
    int sample_rate;
    int bits_per_sample;
    int streams;
    int channels;
    double duration;

    int channel = 0;

    PacketPtr packet;
    FramePtr frame;
    int buffer_len = 0;
    SampleBuffer buffer;
    // TODO: these guys don't belong here, move them somewhere else when revamping the pipeline
    int64_t frames_per_interval = 0;
    int64_t error_per_interval = 0;
    int64_t error_base = 0;
};

AudioFileImpl::AudioFileImpl(
    AudioError error, FormatPtr format_context, CodecPtr codec_context, int audio_stream,
    const std::string& codec_name, int bit_rate, int sample_rate, int bits_per_sample, int streams,
    int channels, double duration
) :
    error(error),
    format_context(std::move(format_context)),
    codec_context(std::move(codec_context)),
    audio_stream(audio_stream),
    codec_name(codec_name),
    bit_rate(bit_rate),
    sample_rate(sample_rate),
    bits_per_sample(bits_per_sample),
    streams(streams),
    channels(channels),
    duration(duration)
{
    this->packet = PacketPtr(av_packet_alloc());
    this->frame = FramePtr(av_frame_alloc());
}

void AudioFileImpl::start(int channel, int samples)
{
    this->channel = channel;
    if (channel < 0 || channel >= this->channels) {
        assert(false);
        this->error = AudioError::NO_CHANNELS;
    }

    AVStream *stream = this->format_context->streams[this->audio_stream];
    int64_t rate = this->sample_rate * (int64_t)stream->time_base.num;
    int64_t duration = (int64_t)(this->duration * stream->time_base.den / stream->time_base.num);
    this->error_base = samples * (int64_t)stream->time_base.den;
    this->frames_per_interval = av_rescale_rnd(duration, rate, this->error_base, AV_ROUND_DOWN);
    this->error_per_interval = (duration * rate) % this->error_base;
}

int AudioFileImpl::read()
{
    if (!!this->error) {
        // Stop on error.
        return -1;
    }

    // This runs a state machine to allow incremental calls to decode and return the next chunk of
    // samples.
    // FFmpeg docs: https://ffmpeg.org/doxygen/7.1/group__lavc__decoding.html
    for (;;) {
        if (!this->packet) {
            // Finished decoding.
            return 0;
        }

        // Read the next packet.
        int ret = 0;
        while ((ret = av_read_frame(this->format_context.get(), this->packet.get())) >= 0) {
            if (this->packet->stream_index == this->audio_stream) {
                break;
            }
            av_packet_unref(this->packet.get());
        }

        if (ret < 0) {
            // End of file or error, empty the packet to flush the decoder.
            av_packet_unref(this->packet.get());
            this->packet.reset();
        }

        ret = avcodec_send_packet(this->codec_context.get(), this->packet.get());
        if (this->packet) {
            av_packet_unref(this->packet.get());
        }
        if (ret < 0) {
            // Skip the packet.
            continue;
        }

        // The packet can contain multiple frames, read all of them.
        int total_samples = 0;
        while ((ret = avcodec_receive_frame(this->codec_context.get(), this->frame.get())) >= 0) {
            int samples = this->frame->nb_samples;
            if (samples == 0) {
                // Occasionally the frame has no samples, move on to the next one.
                av_frame_unref(this->frame.get());
                continue;
            }
            // We have the data, normalise and write to the buffer.
            if ((total_samples + samples) > this->buffer_len) {
                auto new_buffer = SampleBuffer(static_cast<float *>(
                    av_malloc(sizeof(float) * (total_samples + samples))
                ));
                if (!new_buffer) {
                    return -1;
                }
                if (total_samples > 0) {
                    memcpy(
                        new_buffer.get(), this->buffer.get(), sizeof(float) * total_samples
                    );
                }
                this->buffer = std::move(new_buffer);
                this->buffer_len = total_samples + samples;
            }

            AVSampleFormat format = static_cast<AVSampleFormat>(this->frame->format);
            int is_planar = av_sample_fmt_is_planar(format);
            for (int sample = 0; sample < samples; ++sample) {
                uint8_t *data;
                int offset;
                if (is_planar) {
                    data = this->frame->data[this->channel];
                    offset = sample;
                } else {
                    data = this->frame->data[0];
                    offset = sample * this->channels;
                }
                float value;
                switch (format) {
                case AV_SAMPLE_FMT_S16:
                case AV_SAMPLE_FMT_S16P:
                    value = reinterpret_cast<int16_t *>(data)[offset]
                        / static_cast<float>(INT16_MAX);
                    break;
                case AV_SAMPLE_FMT_S32:
                case AV_SAMPLE_FMT_S32P:
                    value = reinterpret_cast<int32_t *>(data)[offset]
                        / static_cast<float>(INT32_MAX);
                    break;
                case AV_SAMPLE_FMT_FLT:
                case AV_SAMPLE_FMT_FLTP:
                    value = reinterpret_cast<float *>(data)[offset];
                    break;
                case AV_SAMPLE_FMT_DBL:
                case AV_SAMPLE_FMT_DBLP:
                    value = reinterpret_cast<double *>(data)[offset];
                    break;
                default:
                    value = 0.0f;
                    break;
                }
                this->buffer[total_samples + sample] = value;
            }
            total_samples += samples;
            av_frame_unref(this->frame.get());
        }
        if (total_samples > 0) {
            return total_samples;
        }
        // Error during decoding or EOF, skip the packet.
    }
}

} // namespace

std::string spek_audio_error_message(AudioError error)
{
    switch (error) {
    case AudioError::CANNOT_OPEN_FILE:
        return spek_tr("Cannot open input file");
    case AudioError::NO_STREAMS:
        return spek_tr("Cannot find stream info");
    case AudioError::NO_AUDIO:
        return spek_tr("The file contains no audio streams");
    case AudioError::NO_DECODER:
        return spek_tr("Cannot find decoder");
    case AudioError::NO_DURATION:
        return spek_tr("Unknown duration");
    case AudioError::NO_CHANNELS:
        return spek_tr("No audio channels");
    case AudioError::CANNOT_OPEN_DECODER:
        return spek_tr("Cannot open decoder");
    case AudioError::BAD_SAMPLE_FORMAT:
        return spek_tr("Unsupported sample format");
    case AudioError::OK:
        break;
    }

    return {};
}

std::expected<std::unique_ptr<AudioFile>, AudioError> Audio::open(
    std::string_view file_name, int stream
)
{
    const std::string path(file_name);

    AVFormatContext *raw_format_context = nullptr;
    if (avformat_open_input(&raw_format_context, path.c_str(), nullptr, nullptr) < 0) {
        return std::unexpected(AudioError::CANNOT_OPEN_FILE);
    }
    FormatPtr format_context(raw_format_context);

    if (avformat_find_stream_info(format_context.get(), nullptr) < 0) {
        // 24-bit APE returns an error but parses the stream info just fine.
        // TODO: old comment, verify
        if (format_context->nb_streams <= 0) {
            return std::unexpected(AudioError::NO_STREAMS);
        }
    }

    int audio_stream = -1;
    int streams = 0;
    for (unsigned int i = 0; i < format_context->nb_streams; i++) {
        if (format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            if (stream == streams) {
                audio_stream = static_cast<int>(i);
            }
            streams++;
        }
    }
    if (audio_stream == -1) {
        return std::unexpected(AudioError::NO_AUDIO);
    }

    AVStream *avstream = format_context->streams[audio_stream];
    AVCodecParameters *codecpar = avstream->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        return std::unexpected(AudioError::NO_DECODER);
    }

    // We can already fill in the stream info even if the codec won't be able to open it.
    std::string codec_name;
    if (codec->long_name) {
        codec_name = codec->long_name;
    } else if (codec->name) {
        codec_name = codec->name;
    }
    int bit_rate = codecpar->bit_rate;
    int sample_rate = codecpar->sample_rate;
    int bits_per_sample = codecpar->bits_per_raw_sample;
    if (!bits_per_sample) {
        // APE uses bpcs, FLAC uses bprs.
        bits_per_sample = codecpar->bits_per_coded_sample;
    }
    if (codecpar->codec_id == AV_CODEC_ID_AAC || codecpar->codec_id == AV_CODEC_ID_MUSEPACK8 ||
        codecpar->codec_id == AV_CODEC_ID_WMAV1 || codecpar->codec_id == AV_CODEC_ID_WMAV2) {
        // These decoders set both bps and bitrate.
        bits_per_sample = 0;
    }
    if (bits_per_sample) {
        bit_rate = 0;
    }
    int channels = codecpar->ch_layout.nb_channels;

    double duration = 0;
    if (avstream->duration != AV_NOPTS_VALUE) {
        duration = avstream->duration * av_q2d(avstream->time_base);
    } else if (format_context->duration != AV_NOPTS_VALUE) {
        duration = format_context->duration / static_cast<double>(AV_TIME_BASE);
    } else {
        return std::unexpected(AudioError::NO_DURATION);
    }

    if (channels <= 0) {
        return std::unexpected(AudioError::NO_CHANNELS);
    }

    // Allocate a codec context for the decoder.
    CodecPtr codec_context(avcodec_alloc_context3(codec));
    AudioError error = AudioError::CANNOT_OPEN_DECODER;
    if (codec_context) {
        // Copy codec parameters from input stream to output codec context.
        if (avcodec_parameters_to_context(codec_context.get(), codecpar) == 0) {
            // Finally, init the decoder.
            if (avcodec_open2(codec_context.get(), codec, nullptr) == 0) {
                error = AudioError::OK;
            }
        }
    }

    if (!error) {
        AVSampleFormat fmt = static_cast<AVSampleFormat>(codecpar->format);
        if (fmt != AV_SAMPLE_FMT_S16 && fmt != AV_SAMPLE_FMT_S16P &&
            fmt != AV_SAMPLE_FMT_S32 && fmt != AV_SAMPLE_FMT_S32P &&
            fmt != AV_SAMPLE_FMT_FLT && fmt != AV_SAMPLE_FMT_FLTP &&
            fmt != AV_SAMPLE_FMT_DBL && fmt != AV_SAMPLE_FMT_DBLP) {
            error = AudioError::BAD_SAMPLE_FORMAT;
        }
    }

    return std::unique_ptr<AudioFile>(new AudioFileImpl(
        error, std::move(format_context), std::move(codec_context), audio_stream, codec_name,
        bit_rate, sample_rate, bits_per_sample, streams, channels, duration
    ));
}
