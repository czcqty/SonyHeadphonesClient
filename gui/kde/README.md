# KDE native prototype

C++ Qt Widgets + KDE Frameworks: actual `KPageWidget` settings navigation,
`QCheckBox`, `QSlider`, `QComboBox`, native menus, `QScrollArea` and
`KStatusNotifierItem`. This is KDE's own toolkit family. No hand-painted Breeze
skin or forced QStyle. Plasma supplies its configured widget style, font and
palette. The frontend supports KF6/Qt6 and KF5/Qt5; the device helper independently
retains the repository's Qt 6.5+ requirement.

```sh
# Plasma 5 / Ubuntu 24.04 example; Plasma 6 needs the KF6 equivalents.
sudo apt install cmake g++ qtbase5-dev libkf5widgetsaddons-dev libkf5notifications-dev libkf5configwidgets-dev
cmake -S gui/kde -B build/kde
cmake --build build/kde --parallel
build/kde/SoundConnectKDE --backend "$PWD/build/backend/SoundConnectBackend"
```

Build the shared host using [../native/README.md](../native/README.md). Optional
`--panel`, `--route`, `--profile`, `--locale en|zh_CN` and explicit `--live`.
The separately laid out quick panel is a native popup. KDE's native tray menu
opens it or full settings. Closing settings hides to tray only when a host is
available; otherwise it quits. Qt handles popup Escape, focus dismissal and
nested combo popups. Popup screen bounds are clamped to the selected display.

```sh
xvfb-run -a dbus-run-session -- build/kde/SoundConnectKDE --backend /absolute/backend --smoke-test
```

The smoke test instantiates all pages and checks startup/state refresh isolation
and one write per toggle/slider update. A KDE Plasma session, KF6 build, tray
positioning on Wayland/multiple displays, keyboard/accessibility and real-device
acceptance are still required. No target desktop appearance is inferred from
Xvfb. See the shared README for remaining prototype feature and packaging work.

## Local verification (2026-10-14)

Ubuntu 24.04 WSL: GCC 13.3, Qt 5.15.13 and KDE Frameworks 5.115 build succeeds.
The Xvfb/DBus native-control smoke test passes for all nine pages, zero startup
writes, silent state refresh and one write per toggle/slider update. The host was
the real shared C++ Windows executable over WSL interoperability pipes in isolated
simulation; this does not verify Linux Bluetooth or a Plasma desktop session.

## Shared backend migration

The adapter now consumes the main branch's `SoundConnectBackend` and
`clients/common/catalog.json`. The former branch-local backend source and
catalog copies have been removed. Advanced device operations are present in
the shared catalog; application/desktop parity still follows the migration gate.
Native widget smoke test passed again against the new standalone backend.

## Preference and desktop follow-up

- The application-settings page supports live persistent language and
  system/light/dark, including migration of the old Qt preferences. Preview
  changes are intentionally session-only. `--locale` / `--theme` override the
  initial session without saving. Both device pages and dynamic choices/errors
  update when switching English/Chinese.
- Ctrl+Alt+P / N / M invoke play/pause, next track and quick controls within
  the application, preserving the previous Linux Qt shortcut scope.
- Native dialogs confirm power-off and unpair; cancellation sends no command.
  Separate quick/full layouts, native menu styling and Qt Quick removal gates
  remain unchanged.

Manual modes use installed official BreezeLight/BreezeDark schemes through
KColorSchemeManager; system mode restores KDE tracking. No palette is painted
by the adapter. KF5 5.89+ or KF6 ColorScheme is required for this integration.
