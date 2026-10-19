# Native desktop packages

Adapters remain on `codex/winui`, `codex/kde`, `codex/gnome`, and `codex/xfce`.
Shared transport, catalog and Linux recipes live on main. Merge main before
building an adapter; do not copy device/backend implementations. Qt Quick stays
available until the platform and hardware migration gates pass.

## Linux amd64

Build separately on Ubuntu 24.04 and Debian 13. Run dependency installation and
install/uninstall checks only in disposable distro containers/root filesystems:

```sh
bash packaging/native/linux_dependencies.sh # requires root
# Ubuntu only, after the script installs the pinned private SDK:
export PATH=/opt/soundconnect-tools/bin:$PATH
export SOUNDCONNECT_QT_ROOT=/opt/qt/6.8.3/gcc_64
bash packaging/native/build_linux.sh kde # or gnome / xfce, on its branch
bash packaging/native/verify_linux.sh build/native-linux/packages kde
```

CMake 3.31+ is required. Ubuntu's private Qt 6.8.3 SDK includes `qtbase` and the
matching `icu` archive, and is used only by the backend. KDE uses distro Qt/KF5.
Debian 13 uses system Qt 6.8/KF6. The backend minimum remains Qt 6.5.

Each build emits `soundconnect-common`, `soundconnect-backend`, and its adapter
DEB, plus a source/dependency manifest. Adapters depend on matching shared-package
versions. All three adapters may be installed together. Runtime files use
`/usr/share/soundconnect/` and `/usr/libexec/soundconnect/`; commands are
`soundconnect-kde`, `soundconnect-gnome`, `soundconnect-xfce`.

Installed menu entries explicitly pass `--live`; CLI launches without it remain
isolated previews. `--backend` overrides the installed host path. Ubuntu bundles
Qt Core/ICU privately with executable-relative library lookup; Debian uses system
dependencies. GTK/KDE library search paths are not changed. No daemon or autostart
service is installed.

`verify_linux.sh` installs, reinstalls and removes packages. It tests both
languages outside the checkout without a backend override or SDK path and checks
isolated preferences survive removal. Xvfb provides widget/packaging evidence.
The manual native Linux workflow runs Ubuntu 24.04 and Debian 13 container jobs
and uploads packages and reports.

## Windows x64

Use the WinUI branch's `gui/winui/Build.ps1`, `Package.ps1`, and `Verify.ps1`.
The build publishes a self-contained app and installs the shared Release backend
and runtime into it. It requires a dedicated backend build directory and rejects
caches belonging to another checkout or the transitional Qt Quick build. The
generated application PRI must be included in the published directory.

The per-user NSIS installer provides a live-mode Start-menu shortcut, optional
desktop shortcut, upgrade and uninstall, using its own installation identity.
Uninstall removes listed application files and empty directories, preserving
preferences and unrelated files. Diagnostics use the user's SoundConnect data
directory. The WinUI branch includes a manual native packaging workflow.

## Child recovery

Commands wait for a valid version-1 initial snapshot; startup times out after ten
seconds. Backend failure clears stale state and exposes retry and bounded
diagnostics. Retry retires the old child, discards old callbacks/queued commands,
and obtains fresh state. Device commands are not replayed and headphones are not
automatically reconnected. Normal exit closes stdin and allows two seconds for
EOF shutdown before terminating the owned child.

Real desktop, BlueZ/headphone, accessibility, Windows 10 and multiple-display
checks remain in [the migration gates](../../docs/backend-migration.md).
