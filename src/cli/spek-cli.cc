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
#include <atomic>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

extern "C" {
#include <libavutil/log.h>
}

#include "spek-audio.h"
#include "spek-fft.h"
#include "spek-i18n.h"
#include "spek-image.h"
#include "spek-palette.h"
#include "spek-pipeline.h"
#include "spek-utils.h"

#include "spek-cli.h"

namespace spek
{

namespace
{

constexpr int DEFAULT_WIDTH = 1024;
constexpr int DEFAULT_FFT_BITS = 11;
constexpr double DEFAULT_MIN_DB = -120.0;
constexpr double DEFAULT_MAX_DB = 0.0;

struct Options
{
    bool help = false;
    bool version = false;
    std::string input;
    std::string output;
    int width = DEFAULT_WIDTH;
    int height = 0; // 0 = one pixel per FFT band.
    int fft_bits = DEFAULT_FFT_BITS;
    int stream = 0;
    int channel = 0;
    double min_db = DEFAULT_MIN_DB;
    double max_db = DEFAULT_MAX_DB;
    enum palette palette = PALETTE_DEFAULT;
    WindowFunction window = WindowFunction::DEFAULT;
    bool recursive = false;
    unsigned jobs = 1;
    bool quiet = false;
};

void print_usage(std::ostream& os)
{
    os << spek_format(
        spek_tr("Usage: %s [OPTION]... [FILE]\n").c_str(), SPEK_NAME
    );
    os << spek_tr("Analyse audio files and save the spectrograms as PNG images.\n") << "\n";
    os << spek_tr("Options:\n");
    os << spek_tr("  -c, --cli               Run without the graphical interface.\n");
    os << spek_tr(
        "  -i, --input <path>      Audio file or directory to analyse.\n"
    );
    os << spek_tr(
        "  -o, --output <path>     PNG file, or a directory for batch processing.\n"
    );
    os << spek_tr("  -w, --width <n>         Width of the image in pixels.\n");
    os << spek_tr("  -H, --height <n>        Height of the image in pixels.\n");
    os << spek_tr("      --fft-bits <n>      Size of the FFT window, 2^n samples.\n");
    os << spek_tr("      --stream <n>        Audio stream to analyse, zero-based.\n");
    os << spek_tr("      --channel <n>       Audio channel to analyse, zero-based.\n");
    os << spek_tr("      --min-db <n>        Lower bound of the spectral density range.\n");
    os << spek_tr("      --max-db <n>        Upper bound of the spectral density range.\n");
    os << spek_tr("      --palette <name>    Palette: sox, spectrum or mono.\n");
    os << spek_tr("      --window <name>     Window function: hann, hamming or blackman-harris.\n");
    os << spek_tr("  -r, --recursive         Recurse into sub-directories.\n");
    os << spek_tr("  -j, --jobs <n>          Number of files to analyse in parallel.\n");
    os << spek_tr("  -q, --quiet             Do not print progress information.\n");
    os << spek_tr("  -h, --help              Show this help message.\n");
    os << spek_tr("  -V, --version           Display the version and exit.\n");
}

// Matches "--foo=bar", returns the value or nullopt when the name differs.
std::optional<std::string> match_option(std::string_view arg, std::string_view name)
{
    if (name.size() == 1) {
        return std::nullopt;
    }
    if (!arg.starts_with(name)) {
        return std::nullopt;
    }
    if (arg.size() == name.size()) {
        return std::string();
    }
    if (arg[name.size()] != '=') {
        return std::nullopt;
    }
    return std::string(arg.substr(name.size() + 1));
}

// Returns the value of an option, either the next argument or the one
// attached with "=". Prints an error and quits when the value is missing.
std::optional<std::string> option_value(
    int argc, char **argv, int& i, std::string_view name, std::optional<std::string> inline_value
)
{
    if (inline_value) {
        if (inline_value->empty()) {
            std::cerr << spek_format(spek_tr("%s: option \"%s\" requires a value\n").c_str(),
                                     SPEK_NAME, std::string(name).c_str());
            return std::nullopt;
        }
        return inline_value;
    }

    if (i + 1 >= argc) {
        std::cerr << spek_format(
            spek_tr("%s: option \"%s\" requires a value\n").c_str(),
            SPEK_NAME,
            std::string(name).c_str()
        );
        return std::nullopt;
    }

    return std::string(argv[++i]);
}

std::optional<int> parse_int(std::string_view value)
{
    // std::from_chars leaves the value untouched when parsing fails.
    int result = 0;
    const char *first = value.data();
    const char *last = first + value.size();
    auto [end, ec] = std::from_chars(first, last, result);
    if (ec != std::errc{} || end != last) {
        return std::nullopt;
    }
    return result;
}

std::optional<double> parse_double(std::string_view value)
{
    // std::from_chars for floating point values is not implemented by all
    // standard libraries yet, strtod() is good enough for command line parsing.
    const std::string text(value);
    char *end = nullptr;
    const double result = strtod(text.c_str(), &end);
    if (end != text.c_str() + text.size()) {
        return std::nullopt;
    }
    return result;
}

std::string to_lower(std::string_view value)
{
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

// Parses the command line. Returns nullopt when the arguments are invalid.
std::optional<Options> parse_options(int argc, char **argv)
{
    Options options;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);

        auto flag = [&](std::string_view short_name, std::string_view long_name) {
            if (short_name.size() == 2) {
                return arg == short_name || arg == long_name || match_option(arg, long_name).has_value();
            }
            return arg == short_name || arg == long_name;
        };

        auto value = [&](std::string_view short_name, std::string_view long_name)
            -> std::optional<std::string> {
            std::optional<std::string> inline_value;
            if (arg == long_name || (short_name.size() == 2 && arg == short_name)) {
                inline_value = std::nullopt;
                return option_value(argc, argv, i, long_name, inline_value);
            }
            if (short_name.size() == 2 && arg.starts_with(short_name) && arg.size() > 2) {
                return std::string(arg.substr(2));
            }
            inline_value = match_option(arg, long_name);
            if (!inline_value) {
                return std::nullopt;
            }
            return option_value(argc, argv, i, long_name, inline_value);
        };

        if (flag("-h", "--help")) {
            options.help = true;
        } else if (flag("-V", "--version")) {
            options.version = true;
        } else if (flag("-c", "--cli")) {
            // Nothing to do, the presence of the option selects the CLI.
        } else if (flag("-q", "--quiet")) {
            options.quiet = true;
        } else if (flag("-r", "--recursive")) {
            options.recursive = true;
        } else if (arg == "-i" || arg == "--input" || match_option(arg, "--input")) {
            auto v = value("-i", "--input");
            if (!v) return std::nullopt;
            options.input = *v;
        } else if (arg == "-o" || arg == "--output" || match_option(arg, "--output")) {
            auto v = value("-o", "--output");
            if (!v) return std::nullopt;
            options.output = *v;
        } else if (arg == "-w" || arg == "--width" || match_option(arg, "--width")) {
            auto v = value("-w", "--width");
            if (!v) return std::nullopt;
            if (auto n = parse_int(*v)) options.width = *n;
            else return std::nullopt;
        } else if (arg == "-H" || arg == "--height" || match_option(arg, "--height")) {
            auto v = value("-H", "--height");
            if (!v) return std::nullopt;
            if (auto n = parse_int(*v)) options.height = *n;
            else return std::nullopt;
        } else if (arg == "--fft-bits" || match_option(arg, "--fft-bits")) {
            auto v = value("", "--fft-bits");
            if (!v) return std::nullopt;
            if (auto n = parse_int(*v)) options.fft_bits = *n;
            else return std::nullopt;
        } else if (arg == "--stream" || match_option(arg, "--stream")) {
            auto v = value("", "--stream");
            if (!v) return std::nullopt;
            if (auto n = parse_int(*v)) options.stream = *n;
            else return std::nullopt;
        } else if (arg == "--channel" || match_option(arg, "--channel")) {
            auto v = value("", "--channel");
            if (!v) return std::nullopt;
            if (auto n = parse_int(*v)) options.channel = *n;
            else return std::nullopt;
        } else if (arg == "--min-db" || match_option(arg, "--min-db")) {
            auto v = value("", "--min-db");
            if (!v) return std::nullopt;
            if (auto d = parse_double(*v)) options.min_db = *d;
            else return std::nullopt;
        } else if (arg == "--max-db" || match_option(arg, "--max-db")) {
            auto v = value("", "--max-db");
            if (!v) return std::nullopt;
            if (auto d = parse_double(*v)) options.max_db = *d;
            else return std::nullopt;
        } else if (arg == "--palette" || match_option(arg, "--palette")) {
            auto v = value("", "--palette");
            if (!v) return std::nullopt;
            const std::string name = to_lower(*v);
            if (name == "sox") {
                options.palette = PALETTE_SOX;
            } else if (name == "spectrum") {
                options.palette = PALETTE_SPECTRUM;
            } else if (name == "mono") {
                options.palette = PALETTE_MONO;
            } else {
                std::cerr << spek_format(
                    spek_tr("%s: unknown palette \"%s\"\n").c_str(), SPEK_NAME, v->c_str()
                );
                return std::nullopt;
            }
        } else if (arg == "--window" || match_option(arg, "--window")) {
            auto v = value("", "--window");
            if (!v) return std::nullopt;
            const std::string name = to_lower(*v);
            if (name == "hann") {
                options.window = WindowFunction::HANN;
            } else if (name == "hamming") {
                options.window = WindowFunction::HAMMING;
            } else if (name == "blackman-harris" || name == "blackman_harris") {
                options.window = WindowFunction::BLACKMAN_HARRIS;
            } else {
                std::cerr << spek_format(
                    spek_tr("%s: unknown window function \"%s\"\n").c_str(), SPEK_NAME, v->c_str()
                );
                return std::nullopt;
            }
        } else if (arg == "-j" || arg == "--jobs" || match_option(arg, "--jobs")) {
            auto v = value("-j", "--jobs");
            if (!v) return std::nullopt;
            if (auto n = parse_int(*v)) options.jobs = static_cast<unsigned>(*n);
            else return std::nullopt;
        } else if (arg.size() > 1 && arg[0] == '-') {
            std::cerr << spek_format(
                spek_tr("%s: unknown option \"%s\"\n").c_str(), SPEK_NAME, std::string(arg).c_str()
            );
            return std::nullopt;
        } else if (options.input.empty()) {
            options.input = std::string(arg);
        } else {
            std::cerr << spek_format(
                spek_tr("%s: unexpected argument \"%s\"\n").c_str(),
                SPEK_NAME,
                std::string(arg).c_str()
            );
            return std::nullopt;
        }
    }

    return options;
}

// Collects the audio files to analyse.
std::vector<std::filesystem::path> collect_files(const Options& options, std::string& error)
{
    namespace fs = std::filesystem;

    std::vector<fs::path> files;

    if (options.input.empty()) {
        error = spek_tr("no input file, see --help");
        return files;
    }

    std::error_code ec;
    if (!fs::exists(options.input, ec)) {
        error = spek_format(
            spek_tr("cannot open input file: %s").c_str(), options.input.c_str()
        );
        return files;
    }

    if (fs::is_directory(options.input, ec)) {
        if (options.recursive) {
            for (const auto& entry : fs::recursive_directory_iterator(options.input, ec)) {
                if (entry.is_regular_file() && spek_has_audio_extension(entry.path().string())) {
                    files.push_back(entry.path());
                }
            }
        } else {
            for (const auto& entry : fs::directory_iterator(options.input, ec)) {
                if (entry.is_regular_file() && spek_has_audio_extension(entry.path().string())) {
                    files.push_back(entry.path());
                }
            }
        }
        std::ranges::sort(files);
    } else {
        files.push_back(fs::path(options.input));
    }

    if (files.empty()) {
        error = spek_tr("no audio files found");
    }

    return files;
}

// Works out where the spectrogram of `input` has to be saved.
std::filesystem::path output_path(
    const std::filesystem::path& input, const Options& options, bool batch
)
{
    namespace fs = std::filesystem;

    std::error_code ec;

    // In batch mode the whole file name is kept: several formats can share
    // the same base name, e.g. song.flac and song.mp3.
    const std::string name = batch ? input.filename().string() : input.stem().string();

    if (batch || options.output.empty()) {
        fs::path dir;
        if (!options.output.empty()) {
            dir = fs::path(options.output);
        } else if (fs::is_directory(input, ec)) {
            dir = input;
        } else {
            dir = input.parent_path();
        }
        if (dir.empty()) {
            dir = fs::path(".");
        }
        return dir / (name + ".png");
    }

    const fs::path output(options.output);
    if (fs::is_directory(output, ec)) {
        return output / (name + ".png");
    }

    return output;
}

// Renders the spectrogram of one file into `image`.
// Returns an empty string on success or the error message otherwise.
std::string render(
    const std::filesystem::path& input, const Options& options, SpekImage& image, std::string& desc
)
{
    Audio audio;
    auto file = audio.open(input.string(), options.stream);
    if (!file) {
        return spek_audio_error_message(file.error());
    }

    auto fft = spek_fft_create(options.fft_bits);
    if (!fft) {
        return spek_tr("cannot create the FFT plan");
    }

    const int bands = (*fft)->output_size();
    const int width = std::max(options.width, 1);

    SpekImage spectrogram(width, bands);

    const double range = options.max_db - options.min_db;
    const double min_db = options.min_db;
    const enum palette palette = options.palette;
    std::vector<uint32_t> column(static_cast<size_t>(bands));

    const auto callback = [&spectrogram, &column, range, min_db, palette](
                              int sample_bands, int sample, std::span<const float> values
                          ) {
        if (sample < 0 || sample >= spectrogram.width()) {
            return;
        }

        column.resize(static_cast<size_t>(sample_bands));
        for (int y = 0; y < sample_bands; ++y) {
            const double value = std::clamp(
                static_cast<double>(values[static_cast<size_t>(y)]), min_db, min_db + range
            );
            const double level = range > 0.0 ? (value - min_db) / range : 0.0;
            column[static_cast<size_t>(sample_bands - y - 1)] = spek_palette(palette, level);
        }
        spectrogram.set_column(sample, column);
    };

    SpekPipeline pipeline(
        std::move(*file),
        std::move(*fft),
        options.stream,
        options.channel,
        options.window,
        width,
        callback
    );

    desc = pipeline.desc();
    pipeline.run();

    if (pipeline.has_error()) {
        return pipeline.desc();
    }

    const int height = options.height > 0 ? options.height : bands;
    if (height == bands) {
        image = std::move(spectrogram);
    } else {
        // Scale the spectrogram vertically to the requested height.
        image.create(width, height);
        std::vector<uint32_t> column(static_cast<size_t>(height));
        for (int x = 0; x < width; ++x) {
            for (int y = 0; y < height; ++y) {
                const int src_y = std::min(bands - 1, y * bands / height);
                const size_t offset =
                    (static_cast<size_t>(src_y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 3;
                const std::span<const uint8_t> pixels = spectrogram.pixels();
                column[static_cast<size_t>(y)] =
                    (static_cast<uint32_t>(pixels[offset]) << 16)
                    | (static_cast<uint32_t>(pixels[offset + 1]) << 8)
                    | static_cast<uint32_t>(pixels[offset + 2]);
            }
            image.set_column(x, column);
        }
    }

    return {};
}

// Analyses one file and saves its spectrogram.
// Returns an empty string on success or the error message otherwise.
std::string process_file(
    const std::filesystem::path& input,
    const Options& options,
    bool batch,
    std::string& path,
    std::string& desc
)
{
    SpekImage image;

    const std::string error = render(input, options, image, desc);
    if (!error.empty()) {
        return error;
    }

    const std::filesystem::path output = output_path(input, options, batch);
    path = output.string();

    std::error_code ec;
    if (output.has_parent_path()) {
        std::filesystem::create_directories(output.parent_path(), ec);
    }

    const auto result = image.save_png(output.string());
    if (!result) {
        return spek_tr("cannot save the image");
    }

    return {};
}

} // namespace

bool wants_cli(int argc, char **argv)
{
    // Keep this list in sync with parse_options(). Every console-only option,
    // not just --cli itself, must bypass wxEntry(): otherwise a valid headless
    // command such as `spek --input=file --output=image.png` starts wxGTK.
    static constexpr std::string_view short_options[] = {
        "-c", "-h", "-V", "-i", "-o", "-w", "-H", "-r", "-j", "-q",
    };
    static constexpr std::string_view long_options[] = {
        "--cli", "--help", "--version", "--input", "--output", "--width",
        "--height", "--fft-bits", "--stream", "--channel", "--min-db", "--max-db",
        "--palette", "--window", "--recursive", "--jobs", "--quiet",
    };

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        for (const auto name : short_options) {
            if (arg == name) {
                return true;
            }
        }
        for (const auto name : long_options) {
            if (arg == name
                || (arg.size() > name.size() && arg.starts_with(name) && arg[name.size()] == '=')) {
                return true;
            }
        }
    }

    return false;
}

int run_cli(int argc, char **argv)
{
    // FFmpeg is very chatty about the formats it has to guess, only report
    // the real errors.
    av_log_set_level(AV_LOG_ERROR);

    const auto options = parse_options(argc, argv);
    if (!options) {
        std::cerr << spek_format(spek_tr("Try \"%s --help\" for more information.\n").c_str(), SPEK_NAME);
        return 2;
    }

    if (options->help) {
        print_usage(std::cout);
        return 0;
    }

    if (options->version) {
        // TRANSLATORS: the %s is the package version.
        std::cout << spek_format(spek_tr("Spek version %s").c_str(), SPEK_VERSION) << "\n";
        return 0;
    }

    std::string error;
    const std::vector<std::filesystem::path> files = collect_files(*options, error);
    if (!error.empty()) {
        std::cerr << SPEK_NAME << ": " << error << "\n";
        return 1;
    }

    const bool batch = files.size() > 1
        || std::filesystem::is_directory(std::filesystem::path(options->input));

    const unsigned jobs = std::clamp(
        options->jobs == 0 ? std::thread::hardware_concurrency() : options->jobs,
        1u,
        std::max(1u, static_cast<unsigned>(files.size()))
    );

    std::vector<std::string> messages(files.size());
    std::atomic<size_t> next_file(0);
    std::atomic<int> failures(0);

    const auto worker = [&]() {
        while (true) {
            const size_t index = next_file.fetch_add(1);
            if (index >= files.size()) {
                return;
            }

            std::string path;
            std::string desc;
            const std::string error = process_file(files[index], *options, batch, path, desc);

            if (error.empty()) {
                messages[index] = spek_format(
                    spek_tr("%s: saved %s\n").c_str(),
                    files[index].string().c_str(),
                    path.c_str()
                );
                if (!desc.empty()) {
                    messages[index] += spek_format("  %s\n", desc.c_str());
                }
            } else {
                messages[index] = spek_format(
                    spek_tr("%s: %s\n").c_str(), files[index].string().c_str(), error.c_str()
                );
                failures.fetch_add(1);
            }
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(jobs);
    for (unsigned i = 0; i < jobs; ++i) {
        threads.emplace_back(worker);
    }
    for (auto& thread : threads) {
        thread.join();
    }

    // Print the results in the order the files were found.
    if (!options->quiet) {
        for (const auto& message : messages) {
            std::cout << message;
        }
        std::cout.flush();
    }

    if (failures.load() > 0) {
        if (!options->quiet) {
            std::cerr << spek_format(
                spek_tr("%s: %d of %d files could not be analysed\n").c_str(),
                SPEK_NAME,
                failures.load(),
                static_cast<int>(files.size())
            );
        }
        return 1;
    }

    return 0;
}

} // namespace spek
