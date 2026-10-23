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

set(SPEK_ICON_THEME_DIR "${CMAKE_INSTALL_DATADIR}/icons/hicolor")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/spek.desktop.in"
    "${CMAKE_CURRENT_BINARY_DIR}/spek.desktop"
    @ONLY
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/spek.metainfo.xml.in"
    "${CMAKE_CURRENT_BINARY_DIR}/spek.metainfo.xml"
    @ONLY
)

install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/spek.desktop"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
)

install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/spek.metainfo.xml"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo"
)

foreach(size 16 22 24 32 48)
    install(
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/icons/${size}x${size}/spek.png"
        DESTINATION "${SPEK_ICON_THEME_DIR}/${size}x${size}/apps"
        RENAME spek.png
    )
endforeach()

install(
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/icons/scalable/spek.svg"
    DESTINATION "${SPEK_ICON_THEME_DIR}/scalable/apps"
)
