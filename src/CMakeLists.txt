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

add_subdirectory(core)
add_subdirectory(cli)

if(SPEK_BUILD_GUI)
    add_subdirectory(gui)
endif()

add_executable(spek
    spek-main.cc
)

if(UNIX AND NOT APPLE AND SPEK_STATIC_LINK)
    # Keep symbols available to the early fatal-signal reporter. The reporter
    # is deliberately part of the executable, not a wx callback, so it also
    # covers failures in static constructors before main().
    target_sources(spek PRIVATE spek-crash.cc)
    target_link_options(spek PRIVATE -rdynamic)
endif()

target_link_libraries(spek PRIVATE spek-cli spek-core)

if(SPEK_BUILD_GUI)
    target_link_libraries(spek PRIVATE spek-gui)
endif()

set_target_properties(spek PROPERTIES
    OUTPUT_NAME "spek"
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

if(WIN32 AND SPEK_BUILD_GUI)
    # Resource objects cannot live in a static library, so the script is
    # compiled into the executable itself. It contains the icons and, for
    # non-MSVC resource compilers, the explicit High-DPI manifest.
    target_sources(spek PRIVATE "${PROJECT_SOURCE_DIR}/dist/win/spek.rc")
    set_source_files_properties(
        "${PROJECT_SOURCE_DIR}/dist/win/spek.rc"
        PROPERTIES INCLUDE_DIRECTORIES
            "${PROJECT_SOURCE_DIR}/dist/win;${PROJECT_SOURCE_DIR}/src"
    )

    if(MSVC)
        # Keep DPI awareness in the Visual Studio project as well as in the
        # manifest so MSVC, clang-cl and the IDE all use the same setting.
        set_property(TARGET spek PROPERTY VS_DPI_AWARE "PerMonitor")
        target_sources(spek PRIVATE "${PROJECT_SOURCE_DIR}/src/app.manifest")
    endif()
endif()

if(WIN32)
    # Copy the FFmpeg / wxWidgets / zlib DLLs next to the executable so that
    # the artefacts can be used straight away.
    include(SpekCopyRuntimeDlls)
    spek_copy_runtime_dlls(spek)
endif()

if(WIN32 AND SPEK_BUILD_GUI)
    # Do not create a console window for normal GUI startup. Command-line
    # modes attach to the parent console explicitly in spek-main.cc.
    set_target_properties(spek PROPERTIES WIN32_EXECUTABLE TRUE)
    if(MSVC)
        # Keep the existing main() entry point while using the Windows GUI
        # subsystem. The CRT still performs normal startup and exit handling.
        target_link_options(spek PRIVATE "/ENTRY:mainCRTStartup")
    endif()
endif()

if(SPEK_INSTALL)
    install(TARGETS spek RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()
