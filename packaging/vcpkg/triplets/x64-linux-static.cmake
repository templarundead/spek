set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# The stock X11 ports are empty on Linux unless this is set. Empty packages
# make pkg-config select the build host's shared libraries, which cannot be
# used in the self-contained executable.
set(X_VCPKG_FORCE_VCPKG_X_LIBRARIES ON)
