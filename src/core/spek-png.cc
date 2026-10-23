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
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#if SPEK_HAVE_ZLIB
#include <zlib.h>
#endif

#include "spek-png.h"

namespace
{

constexpr std::array<uint32_t, 256> make_crc_table()
{
    std::array<uint32_t, 256> table{};
    for (uint32_t n = 0; n < 256; ++n) {
        uint32_t c = n;
        for (int k = 0; k < 8; ++k) {
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        table[n] = c;
    }
    return table;
}

constexpr std::array<uint32_t, 256> CRC_TABLE = make_crc_table();

uint32_t crc32(std::span<const uint8_t> data)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (const uint8_t byte : data) {
        crc = CRC_TABLE[(crc ^ byte) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

#if !SPEK_HAVE_ZLIB
uint32_t adler32(std::span<const uint8_t> data)
{
    uint32_t a = 1;
    uint32_t b = 0;
    for (const uint8_t byte : data) {
        a = (a + byte) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}
#endif // !SPEK_HAVE_ZLIB

void put_u32_be(std::vector<uint8_t>& out, uint32_t value)
{
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

void write_chunk(
    std::vector<uint8_t>& out, std::string_view type, std::span<const uint8_t> data
)
{
    put_u32_be(out, static_cast<uint32_t>(data.size()));

    const size_t start = out.size();
    out.insert(out.end(), type.begin(), type.end());
    out.insert(out.end(), data.begin(), data.end());

    put_u32_be(out, crc32(std::span<const uint8_t>(out).subspan(start)));
}

// Wraps `data` into a zlib stream. When zlib is available the stream is
// deflate-compressed, otherwise it is emitted as a series of stored blocks
// which is still a perfectly valid (if much larger) zlib stream.
bool zlib_stream(std::span<const uint8_t> data, std::vector<uint8_t>& out)
{
#if SPEK_HAVE_ZLIB
    const uLong bound = compressBound(static_cast<uLong>(data.size()));
    out.resize(static_cast<size_t>(bound));
    uLong size = bound;
    if (compress2(
            out.data(), &size, data.data(), static_cast<uLong>(data.size()), Z_BEST_SPEED
        ) != Z_OK) {
        return false;
    }
    out.resize(static_cast<size_t>(size));
    return true;
#else
    // zlib header: CMF = 32K window, FLG such that (CMF << 8 | FLG) % 31 == 0.
    out.push_back(0x78);
    out.push_back(0x01);

    constexpr size_t MAX_BLOCK = 0xFFFF;
    size_t pos = 0;
    do {
        const size_t chunk = std::min(MAX_BLOCK, data.size() - pos);
        const bool final_block = (pos + chunk) == data.size();
        out.push_back(static_cast<uint8_t>(final_block ? 1 : 0));

        const uint16_t len = static_cast<uint16_t>(chunk);
        out.push_back(static_cast<uint8_t>(len & 0xFF));
        out.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>(~len & 0xFF));
        out.push_back(static_cast<uint8_t>((~len >> 8) & 0xFF));

        out.insert(out.end(), data.begin() + pos, data.begin() + pos + chunk);
        pos += chunk;
    } while (pos < data.size());

    put_u32_be(out, adler32(data));
    return true;
#endif
}

} // namespace

std::expected<void, PngError> spek_png_write(
    std::string_view path, std::span<const uint8_t> rgb, int width, int height
)
{
    if (width <= 0 || height <= 0) {
        return std::unexpected(PngError::BAD_SIZE);
    }
    if (rgb.size() < static_cast<size_t>(width) * static_cast<size_t>(height) * 3) {
        return std::unexpected(PngError::BAD_SIZE);
    }

    // Every scanline is prefixed with its filter type, we use the "Sub"
    // filter which compresses the horizontal gradients of a spectrogram well.
    std::vector<uint8_t> raw;
    raw.reserve(static_cast<size_t>(height) * (static_cast<size_t>(width) * 3 + 1));
    for (int y = 0; y < height; ++y) {
        const size_t row = static_cast<size_t>(y) * static_cast<size_t>(width) * 3;
        raw.push_back(1); // Filter type: Sub.
        for (int x = 0; x < width * 3; ++x) {
            const uint8_t value = rgb[row + static_cast<size_t>(x)];
            const uint8_t left = x >= 3 ? rgb[row + static_cast<size_t>(x) - 3] : 0;
            raw.push_back(static_cast<uint8_t>(value - left));
        }
    }

    std::vector<uint8_t> compressed;
    if (!zlib_stream(raw, compressed)) {
        return std::unexpected(PngError::ENCODE_FAILED);
    }

    std::vector<uint8_t> png;
    png.reserve(compressed.size() + 64);

    const std::array<uint8_t, 8> signature{
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
    };
    png.insert(png.end(), signature.begin(), signature.end());

    std::array<uint8_t, 13> ihdr{};
    ihdr[0] = static_cast<uint8_t>((width >> 24) & 0xFF);
    ihdr[1] = static_cast<uint8_t>((width >> 16) & 0xFF);
    ihdr[2] = static_cast<uint8_t>((width >> 8) & 0xFF);
    ihdr[3] = static_cast<uint8_t>(width & 0xFF);
    ihdr[4] = static_cast<uint8_t>((height >> 24) & 0xFF);
    ihdr[5] = static_cast<uint8_t>((height >> 16) & 0xFF);
    ihdr[6] = static_cast<uint8_t>((height >> 8) & 0xFF);
    ihdr[7] = static_cast<uint8_t>(height & 0xFF);
    ihdr[8] = 8; // Bit depth.
    ihdr[9] = 2; // Colour type: truecolour RGB.
    ihdr[10] = 0; // Compression: deflate.
    ihdr[11] = 0; // Filter method: adaptive filtering.
    ihdr[12] = 0; // Interlace: none.
    write_chunk(png, "IHDR", ihdr);
    write_chunk(png, "IDAT", compressed);
    write_chunk(png, "IEND", {});

    std::ofstream file(std::string(path), std::ios::binary | std::ios::out);
    if (!file) {
        return std::unexpected(PngError::CANNOT_OPEN);
    }
    file.write(reinterpret_cast<const char *>(png.data()), static_cast<std::streamsize>(png.size()));
    if (!file) {
        return std::unexpected(PngError::WRITE_FAILED);
    }

    return {};
}
