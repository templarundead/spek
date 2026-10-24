# vcpkg overlays for the self-contained Linux build

The standard Linux triplets build most ports statically, but deliberately use
the host's X11 libraries. They also let wxWidgets export GTK's dynamic
pkg-config interface. That is not sufficient for an executable linked with
`-static`.

- `triplets/x64-linux-static.cmake` and `arm64-linux-static.cmake` enable
  vcpkg's X11 ports with `X_VCPKG_FORCE_VCPKG_X_LIBRARIES`.
- `ports/wxwidgets` is the baseline 3.2.7 port with static GTK transitive
  dependencies selected when `BUILD_SHARED_LIBS=OFF`.
- `ports/dbus` is the baseline 1.16.2#1 port with support for vcpkg's
  `BUILD_SHARED_LIBS` setting instead of an unconditional shared library.
- `ports/libepoxy` is the baseline 1.5.10#2 port without GL/EGL in
  `Requires.private`: epoxy resolves those entry points through its dispatch
  layer, so a static consumer must not inherit the build host's shared GL/EGL.
- `ports/libxcursor` supplies Xcursor 1.2.3, which is absent from the pinned
  baseline. The source commit and archive hash are pinned in its portfile.

The overlays are also read by Windows builds, but their Linux-only ports and
conditions leave the existing Windows dependency configuration unchanged.
