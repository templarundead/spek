vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO NoMercy-CI-Mirrors/libxcursor
    REF 8ae6cf61b18ecddf26f72a07bbfbd1ea9f022c36 # libXcursor-1.2.3
    SHA512 26a3320bb4b688018ee606cf1f67ee8786fc2d3ba7fac021fb4d844a7090c93d9fa8bbacea73234b51a4b758955405c359c4b3fe0ffb36c59f6d1ea4f5fed3d0
    HEAD_REF master
)

set(ENV{ACLOCAL} "aclocal -I \"${CURRENT_INSTALLED_DIR}/share/xorg/aclocal/\"")

vcpkg_configure_make(
    SOURCE_PATH "${SOURCE_PATH}"
    AUTOCONFIG
    OPTIONS xorg_cv_malloc0_returns_null=yes
)
vcpkg_install_make()
vcpkg_fixup_pkgconfig()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING")
