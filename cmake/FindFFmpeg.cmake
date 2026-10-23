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

#[=======================================================================[.rst:
FindFFmpeg
----------

Finds the FFmpeg libraries.

.. code-block:: cmake

  find_package(FFmpeg [REQUIRED] [COMPONENTS <components>...])

Supported components: ``AVCODEC``, ``AVFORMAT``, ``AVUTIL``, ``SWRESAMPLE``,
``SWSCALE``, ``AVFILTER``, ``AVDEVICE``.  With no components given, all of
them are searched for.

Imported targets
^^^^^^^^^^^^^^^^

``FFmpeg::avcodec``, ``FFmpeg::avformat``, ``FFmpeg::avutil``,
``FFmpeg::swresample``, ``FFmpeg::swscale``, ``FFmpeg::avfilter``,
``FFmpeg::avdevice``.

Result variables
^^^^^^^^^^^^^^^^

``FFmpeg_FOUND``
  True when all the requested components were found.
``FFmpeg_VERSION``
  The version of libavutil, e.g. ``59.39.100``.
``FFmpeg_<COMPONENT>_VERSION``
  The version of the individual component.
#]=======================================================================]

set(FFmpeg_ALL_COMPONENTS AVCODEC AVFORMAT AVUTIL SWRESAMPLE SWSCALE AVFILTER AVDEVICE)

if(NOT FFmpeg_FIND_COMPONENTS)
    set(FFmpeg_FIND_COMPONENTS ${FFmpeg_ALL_COMPONENTS})
endif()

find_package(PkgConfig QUIET)

# Reads the version of a component from its main header.
function(_ffmpeg_component_version component header_dir out_var)
    string(TOLOWER "${component}" lower)

    # FFmpeg keeps the version macros in <libav*>/version.h.
    foreach(header IN ITEMS
        "${FFmpeg_${component}_INCLUDE_DIR}/${header_dir}/version.h"
        "${FFmpeg_${component}_INCLUDE_DIR}/${header_dir}/${lower}.h"
    )
        if(NOT EXISTS "${header}")
            continue()
        endif()

        file(STRINGS "${header}" lines
             REGEX "^#define +LIB${component}_VERSION_(MAJOR|MINOR|MICRO)")
        if(NOT lines)
            continue()
        endif()

        set(major 0)
        set(minor 0)
        set(micro 0)
        foreach(line IN LISTS lines)
            if(line MATCHES "VERSION_MAJOR +([0-9]+)")
                set(major "${CMAKE_MATCH_1}")
            elseif(line MATCHES "VERSION_MINOR +([0-9]+)")
                set(minor "${CMAKE_MATCH_1}")
            elseif(line MATCHES "VERSION_MICRO +([0-9]+)")
                set(micro "${CMAKE_MATCH_1}")
            endif()
        endforeach()

        set(${out_var} "${major}.${minor}.${micro}" PARENT_SCOPE)
        return()
    endforeach()
endfunction()

foreach(component IN LISTS FFmpeg_ALL_COMPONENTS)
    if(NOT component IN_LIST FFmpeg_FIND_COMPONENTS)
        continue()
    endif()

    string(TOLOWER "${component}" lower)

    if(PKG_CONFIG_FOUND)
        pkg_check_modules(PC_FFmpeg_${component} QUIET "lib${lower}")
    endif()

    find_path(FFmpeg_${component}_INCLUDE_DIR
        NAMES "lib${lower}/${lower}.h"
        HINTS
            ${PC_FFmpeg_${component}_INCLUDE_DIRS}
            ENV FFMPEG_ROOT
            ${FFMPEG_ROOT}
            ENV FFmpeg_ROOT
        PATH_SUFFIXES include
    )

    find_library(FFmpeg_${component}_LIBRARY
        NAMES "${lower}" "lib${lower}"
        HINTS
            ${PC_FFmpeg_${component}_LIBRARY_DIRS}
            ENV FFMPEG_ROOT
            ${FFMPEG_ROOT}
            ENV FFmpeg_ROOT
        PATH_SUFFIXES lib lib64
    )

    _ffmpeg_component_version(${component} "lib${lower}" FFmpeg_${component}_VERSION)

    if(FFmpeg_${component}_INCLUDE_DIR AND FFmpeg_${component}_LIBRARY)
        set(FFmpeg_${component}_FOUND TRUE)

        if(NOT TARGET FFmpeg::${lower})
            add_library(FFmpeg::${lower} UNKNOWN IMPORTED)
            set_target_properties(FFmpeg::${lower} PROPERTIES
                IMPORTED_LOCATION "${FFmpeg_${component}_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${FFmpeg_${component}_INCLUDE_DIR}"
            )
            # FFmpeg is written in C, make sure the headers are included as such.
            set_property(TARGET FFmpeg::${lower} PROPERTY IMPORTED_LINK_INTERFACE_LANGUAGES C)
        endif()
    else()
        set(FFmpeg_${component}_FOUND FALSE)
    endif()

    mark_as_advanced(FFmpeg_${component}_INCLUDE_DIR FFmpeg_${component}_LIBRARY)
endforeach()

# The version of the bundle is the version of libavutil.
if(FFmpeg_AVUTIL_VERSION)
    set(FFmpeg_VERSION "${FFmpeg_AVUTIL_VERSION}")
endif()

include(FindPackageHandleStandardArgs)

list(GET FFmpeg_FIND_COMPONENTS 0 FFmpeg_FIRST_COMPONENT)
find_package_handle_standard_args(FFmpeg
    REQUIRED_VARS "FFmpeg_${FFmpeg_FIRST_COMPONENT}_LIBRARY" "FFmpeg_VERSION"
    HANDLE_COMPONENTS
)
