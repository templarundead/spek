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

# The command line interface: analyses files (or whole directories) and
# writes the spectrograms out as PNG images.
add_library(spek-cli STATIC
    spek-cli.cc
    spek-cli.h
)

target_include_directories(spek-cli
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_BINARY_DIR}" # spek-config.h
)

target_link_libraries(spek-cli
    PUBLIC
        spek-core
    PRIVATE
        Threads::Threads
)

if(MSVC)
    target_compile_options(spek-cli PRIVATE /W4)
else()
    target_compile_options(spek-cli PRIVATE -Wall -Wextra)
endif()

set_target_properties(spek-cli PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
