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
    # The application icon and, through wx/msw/wx.rc, the per-monitor DPI
    # awareness manifest that makes wxWidgets aware of High-DPI screens.
    # Resource objects cannot live in a static library, so the script is
    # compiled into the executable itself.
    target_sources(spek PRIVATE "${PROJECT_SOURCE_DIR}/dist/win/spek.rc")
    set_source_files_properties(
        "${PROJECT_SOURCE_DIR}/dist/win/spek.rc"
        PROPERTIES INCLUDE_DIRECTORIES "${PROJECT_SOURCE_DIR}/dist/win"
    )
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
