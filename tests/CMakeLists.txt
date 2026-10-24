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

add_executable(spek-test
    test.cc
    test.h
    test-audio.cc
    test-fft.cc
    test-pipeline.cc
    test-utils.cc
)

target_include_directories(spek-test
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_BINARY_DIR}" # spek-config.h
)

target_link_libraries(spek-test PRIVATE spek-core)

target_compile_definitions(spek-test
    PRIVATE
        SAMPLES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/samples"
)

if(MSVC)
    target_compile_options(spek-test PRIVATE /W3)
else()
    target_compile_options(spek-test PRIVATE -Wall -Wextra)
endif()

set_target_properties(spek-test PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

add_test(NAME spek-test COMMAND spek-test)

if(WIN32)
    # The test binary needs the FFmpeg DLLs next to it to be able to run.
    include(SpekCopyRuntimeDlls)
    spek_copy_runtime_dlls(spek-test)
endif()

if(UNIX AND NOT APPLE AND SPEK_BUILD_GUI)
    # Link the same wxGTK stack as the application. The global in this test
    # runs before main(), covering a failure mode which a normal GUI smoke
    # test cannot reliably expose on every linker/architecture combination.
    add_executable(spek-static-init-test test-static-init.cc)
    target_link_libraries(spek-static-init-test PRIVATE spek-gui)
    set_target_properties(spek-static-init-test PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    add_test(NAME spek-static-init-test COMMAND spek-static-init-test)
endif()
