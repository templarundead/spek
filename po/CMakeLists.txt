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

# Compiles and installs the message catalogues. Requires msgfmt from
# GNU gettext, the build simply skips the translations when it is missing.
find_program(MSGFMT_EXECUTABLE msgfmt)

if(NOT MSGFMT_EXECUTABLE)
    message(STATUS "msgfmt not found, the translations will not be installed")
    return()
endif()

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/LINGUAS" linguas
     REGEX "^[^#]")
set(languages "")
foreach(line IN LISTS linguas)
    string(STRIP "${line}" language)
    if(language STREQUAL "")
        continue()
    endif()
    list(APPEND languages "${language}")
endforeach()

set(compiled "")
foreach(language IN LISTS languages)
    set(po "${CMAKE_CURRENT_SOURCE_DIR}/${language}.po")
    if(NOT EXISTS "${po}")
        continue()
    endif()

    set(mo "${CMAKE_CURRENT_BINARY_DIR}/${language}.mo")
    add_custom_command(
        OUTPUT "${mo}"
        COMMAND "${MSGFMT_EXECUTABLE}" --check --output-file "${mo}" "${po}"
        DEPENDS "${po}"
        VERBATIM
    )
    list(APPEND compiled "${mo}")

    install(
        FILES "${mo}"
        DESTINATION "${CMAKE_INSTALL_LOCALEDIR}/${language}/LC_MESSAGES"
        RENAME "${SPEK_GETTEXT_PACKAGE}.mo"
    )
endforeach()

if(compiled)
    add_custom_target(spek-translations ALL DEPENDS ${compiled})
endif()
