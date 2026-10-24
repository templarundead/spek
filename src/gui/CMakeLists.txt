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

# The wxWidgets graphical interface.
add_library(spek-gui STATIC
    spek-artwork.cc
    spek-artwork.h
    spek-events.cc
    spek-events.h
    spek-platform.cc
    spek-platform.h
    spek-preferences.cc
    spek-preferences.h
    spek-preferences-dialog.cc
    spek-preferences-dialog.h
    spek-ruler.cc
    spek-ruler.h
    spek-spectrogram.cc
    spek-spectrogram.h
    spek-window.cc
    spek-window.h
    spek-gui.cc
    spek-gui.h
)

target_include_directories(spek-gui
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_BINARY_DIR}" # spek-config.h
)

target_link_libraries(spek-gui PUBLIC spek-core)

if(APPLE)
    # The AppKit call which brings the window to the front needs
    # Objective-C++, the rest of the interface is plain C++.
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 23)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
    set(CMAKE_OBJCXX_EXTENSIONS OFF)
    target_sources(spek-gui PRIVATE spek-platform-osx.mm)
    target_link_options(spek-gui PUBLIC "SHELL:-framework AppKit")
endif()

if(TARGET wxWidgets::core)
    target_link_libraries(spek-gui PUBLIC wxWidgets::core wxWidgets::base wxWidgets::net)
else()
    include("${wxWidgets_USE_FILE}")
    target_include_directories(spek-gui PUBLIC ${wxWidgets_INCLUDE_DIRS})
    target_link_libraries(spek-gui PUBLIC ${wxWidgets_LIBRARIES})
    target_compile_definitions(spek-gui PUBLIC ${wxWidgets_DEFINITIONS})
endif()

if(MSVC)
    target_compile_options(spek-gui PRIVATE /W3 /permissive-)
else()
    target_compile_options(spek-gui PRIVATE -Wall -Wextra)
endif()

set_target_properties(spek-gui PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
