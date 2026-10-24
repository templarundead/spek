set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# glibc's iconv_open() loads gconv modules with dlopen(). That is not safe in
# this fully static executable: wxConvLocal is initialized before main() and
# the module loader can SIGSEGV while relocating the gconv DSO. Build GNU
# libiconv into the static dependency set instead, so wx uses its self-contained
# conversion tables and never enters glibc's dynamic gconv loader.
set(X_VCPKG_BUILD_GNU_LIBICONV 1)

# The stock X11 ports are empty on Linux unless this is set. Empty packages
# make pkg-config select the build host's shared libraries, which cannot be
# used in the self-contained executable.
set(X_VCPKG_FORCE_VCPKG_X_LIBRARIES ON)
