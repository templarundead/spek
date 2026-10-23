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

cmake_minimum_required(VERSION 3.25)

project(spek
    VERSION 0.8.5
    DESCRIPTION "Acoustic spectrum analyser"
    HOMEPAGE_URL "https://www.spek.cc/"
    LANGUAGES CXX
)

# The project is written in C++23.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "RelWithDebInfo" "MinSizeRel"
    )
endif()

option(SPEK_BUILD_GUI "Build the wxWidgets graphical interface" ON)
option(SPEK_BUILD_TESTS "Build the test suite" ON)
option(SPEK_ENABLE_NLS "Enable translations" ON)
option(SPEK_INSTALL "Generate the installation rules" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(GNUInstallDirs)

# Platform detection, mirrors what configure.ac used to define.
if(WIN32)
    set(OS_WIN TRUE)
elseif(APPLE)
    set(OS_OSX TRUE)
else()
    set(OS_UNIX TRUE)
endif()

# Threads are used by the pipeline and by the batch mode of the CLI.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Audio decoding and the FFT come from FFmpeg. Note that libavutil also
# provides the transforms (libavutil/tx.h), the deprecated libavcodec/avfft.h
# header no longer exists as of FFmpeg 8.
find_package(FFmpeg REQUIRED COMPONENTS AVFORMAT AVCODEC AVUTIL
    OPTIONAL_COMPONENTS SWRESAMPLE
)

if(FFmpeg_AVUTIL_VERSION AND FFmpeg_AVUTIL_VERSION VERSION_LESS 57.28.100)
    message(WARNING
        "libavutil ${FFmpeg_AVUTIL_VERSION} is rather old, "
        "libavutil >= 57.28.100 (FFmpeg 5.1) is recommended."
    )
endif()

# zlib is used to compress the PNG files written by the CLI. Without it the
# images are still valid, they are just not compressed.
find_package(ZLIB)

if(SPEK_ENABLE_NLS)
    find_package(Intl)
endif()

if(SPEK_BUILD_GUI)
    # wxWidgets 3.2 or newer: needed for High-DPI support and for the
    # dark mode detection.
    if(TARGET wxWidgets::core)
        # Already found, e.g. by a superproject.
    elseif(DEFINED VCPKG_TARGET_TRIPLET OR DEFINED VCPKG_CHAINLOAD_TOOLCHAIN_FILE
           OR CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
        # vcpkg builds wxWidgets with CMake and installs a config package with
        # imported targets.
        find_package(wxWidgets 3.2 CONFIG REQUIRED COMPONENTS core base)
    else()
        find_package(wxWidgets 3.2 REQUIRED COMPONENTS core base)
    endif()
endif()

if(SPEK_BUILD_TESTS)
    enable_testing()
endif()

if(ZLIB_FOUND)
    set(SPEK_HAVE_ZLIB 1)
else()
    set(SPEK_HAVE_ZLIB 0)
endif()

if(Intl_FOUND)
    set(SPEK_HAVE_INTL 1)
else()
    set(SPEK_HAVE_INTL 0)
endif()

if(SPEK_BUILD_GUI)
    set(SPEK_BUILD_GUI_VALUE 1)
else()
    set(SPEK_BUILD_GUI_VALUE 0)
endif()

set(SPEK_PROGRAM_NAME "spek")
set(SPEK_GETTEXT_PACKAGE "spek")
set(SPEK_LOCALE_DIR "${CMAKE_INSTALL_FULL_LOCALEDIR}")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/spek-config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/spek-config.h"
    @ONLY
)

add_subdirectory(src)

if(SPEK_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(SPEK_INSTALL)
    add_subdirectory(data)
    install(FILES man/spek.1 DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
    install(DIRECTORY lic/ DESTINATION "${CMAKE_INSTALL_DOCDIR}/lic")
    install(FILES
        CREDITS.md
        INSTALL.md
        LICENSE
        MANUAL.md
        README.md
        DESTINATION "${CMAKE_INSTALL_DOCDIR}"
    )
endif()

if(SPEK_ENABLE_NLS AND Intl_FOUND)
    add_subdirectory(po)
endif()

message(STATUS "")
message(STATUS "${PROJECT_NAME}-${PROJECT_VERSION}")
message(STATUS "")
message(STATUS "    Install prefix:    ${CMAKE_INSTALL_PREFIX}")
message(STATUS "    C++ compiler:      ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "    C++ standard:      C++${CMAKE_CXX_STANDARD}")
message(STATUS "    Build type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "    Operating system:  ${CMAKE_SYSTEM_NAME}")
message(STATUS "    FFmpeg:            ${FFmpeg_VERSION}")
message(STATUS "    zlib:              ${ZLIB_FOUND}")
message(STATUS "    libintl:           ${Intl_FOUND}")
message(STATUS "    Graphical UI:      ${SPEK_BUILD_GUI}")
message(STATUS "")
