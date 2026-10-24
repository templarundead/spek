# Installation instructions

## Windows

Download section offers two packages: an MSI installer and a ZIP archive. To
install Spek, download the MSI installer, double-click it and follow the
instructions.

If you don't want to use the installer, you can download the ZIP archive, unpack
it somewhere on your disk and run `Spek\spek.exe`.

## Mac OS X

Spek for Mac OS X is available in the download section. Download and open the
DMG package, then drag the Spek icon to Applications.

## *BSD and GNU/Linux

### Binary packages

 * Arch: [spek](https://aur.archlinux.org/packages/spek/) and
   [spek-git](https://aur.archlinux.org/packages/spek-git/)
 * Debian: [spek](https://packages.debian.org/search?keywords=spek)
 * Fedora: [RPMFusion package](https://bugzilla.rpmfusion.org/show_bug.cgi?id=1718)
 * FreeBSD: [audio/spek](https://www.freshports.org/audio/spek/)
 * Gentoo: [media-sound/spek](https://packages.gentoo.org/packages/media-sound/spek/)
 * Ubuntu: [spek](http://packages.ubuntu.com/search?keywords=spek)

### Building from the sources

Spek is built with [CMake](https://cmake.org/) and requires a compiler with
C++23 support, e.g. GCC 13+, Clang 19+ or Visual Studio 2022 17.6+. Older
Clang versions cannot be used because their standard library has no
`std::expected`:

    git clone https://github.com/templarundead/spek.git
    cd spek
    cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
    cmake --build build

Run the tests with:

    ctest --test-dir build

To start Spek, run:

    ./build/src/spek

Or install it with:

    sudo cmake --install build

#### Dependencies

 * [wxWidgets](https://www.wxwidgets.org/) >= 3.2
 * [FFmpeg](https://ffmpeg.org/) >= 5.1 (`libavformat`, `libavcodec`,
   `libavutil` and `libswresample`)
 * [zlib](https://zlib.net/) (optional, used to compress the PNG files written
   by the command line interface)
 * [gettext](https://www.gnu.org/software/gettext/) (optional, for the
   translations)

On Debian/Ubuntu:

    sudo apt-get install cmake libwxgtk3.2-dev libavcodec-dev libavformat-dev \
        libavutil-dev libswresample-dev zlib1g-dev gettext

On macOS with Homebrew:

    brew install cmake pkg-config ffmpeg wxwidgets gettext

#### Build options

| Option                | Default | Description                                     |
| --------------------- | ------- | ----------------------------------------------- |
| `SPEK_BUILD_GUI`      | `ON`    | Build the wxWidgets graphical interface.        |
| `SPEK_BUILD_TESTS`    | `ON`    | Build the test suite.                           |
| `SPEK_ENABLE_NLS`     | `ON`    | Enable the translations.                        |
| `SPEK_INSTALL`        | `ON`    | Generate the installation rules.                |
| `CMAKE_BUILD_TYPE`    | `Release` | `Debug`, `Release`, `RelWithDebInfo`, `MinSizeRel` |

#### Building with vcpkg

The repository ships a [vcpkg](https://vcpkg.io/) manifest, so the dependencies
can be fetched and built automatically on any platform:

    cmake -B build -S . \
        -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
    cmake --build build

The GitHub Actions workflow (`.github/workflows/build.yml`) uses vcpkg on
Windows, where there is no system package manager.

Add `-DVCPKG_TARGET_TRIPLET=x64-windows-static` to link wxWidgets, FFmpeg and
the runtime library statically: the result is a single `spek.exe` with no DLL
next to it, which is how the `spek-cli-gui-windows-static` artefact is built.
