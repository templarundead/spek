# Copyright (C) 2010-2013 Alexander Kojevnikov and contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# The toolkit-agnostic core: audio decoding, FFT, the analysis pipeline
# and the image writer. Nothing here may depend on wxWidgets.
add_library(spek-core STATIC
    spek-audio.cc
    spek-audio.h
    spek-fft.cc
    spek-fft.h
    spek-font8x8.inc
    spek-full-image.cc
    spek-full-image.h
    spek-i18n.cc
    spek-i18n.h
    spek-image.cc
    spek-image.h
    spek-palette.cc
    spek-palette.h
    spek-pipeline.cc
    spek-pipeline.h
    spek-png.cc
    spek-png.h
    spek-ruler-ticks.cc
    spek-ruler-ticks.h
    spek-utils.cc
    spek-utils.h
)

target_include_directories(spek-core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_BINARY_DIR}" # spek-config.h
)

target_link_libraries(spek-core
    PUBLIC
        FFmpeg::avformat
        FFmpeg::avcodec
        FFmpeg::avutil
        Threads::Threads
)

if(FFmpeg_SWRESAMPLE_FOUND)
    target_link_libraries(spek-core PUBLIC FFmpeg::swresample)
endif()

if(ZLIB_FOUND)
    target_link_libraries(spek-core PRIVATE ZLIB::ZLIB)
endif()

if(Intl_FOUND)
    target_link_libraries(spek-core PRIVATE Intl::Intl)
endif()

if(MSVC)
    target_compile_options(spek-core PRIVATE /W4)
else()
    target_compile_options(spek-core PRIVATE -Wall -Wextra)
endif()

set_target_properties(spek-core PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
