# KDE presentation

The KDE functional migration is complete on the `kde` branch. The
installed Ubuntu 24.04 and Debian 13 clients passed English and Chinese
acceptance across 12 preview profiles and all 45 commands. Physical headset
and workstation verification remains open. [Linux acceptance record](../../docs/linux-acceptance-2026-11-07.md)

C++ Qt Widgets + KDE Frameworks: actual `KPageWidget` settings navigation,
`QCheckBox`, `QSlider`, `QComboBox`, native menus, `QScrollArea` and
`KStatusNotifierItem`. This is KDE's own toolkit family. No hand-painted Breeze
skin or forced QStyle. Plasma supplies its configured widget style, font and
palette. The frontend supports KF6/Qt6 and KF5/Qt5; the device helper independently
retains the repository's Qt 6.5+ requirement.

```sh
# Plasma 5 / Ubuntu 24.04 example; Plasma 6 needs the KF6 equivalents.
sudo apt install cmake g++ qtbase5-dev libkf5widgetsaddons-dev libkf5notifications-dev libkf5configwidgets-dev libkf5wayland-dev breeze libqt5svg5
cmake -S gui/kde -B build/kde
cmake --build build/kde --parallel
build/kde/SonyHeadphonesClientKDE --backend "$PWD/build/backend/SoundConnectBackend"
```

Build the shared host using [../native/README.md](../native/README.md). Optional
`--panel`, `--route`, `--profile`, `--locale en|zh_CN` and explicit `--live`.
The tray and Ctrl+Alt+M open the same quick-controls window. Its Close
button dismisses it. Opening settings, opening a picker, pressing Escape after
the picker closes, or activating the tray again keeps the panel available.
It uses the display and corner reported by the tray item, with panel
reservations from Plasma's strut manager. Plasma Wayland receives a position
request through KWayland. If the host has yet to report an item position,
the adapter uses the reserved screen edge. When several displays have
panels, the settings display takes priority until tray activation identifies
the actual display. Closing settings without a tray host exits the application.

Settings uses KDE's icon list, or a native page picker at narrow widths.
Both windows display the application icon. The full and quick layouts use
the shared binding catalog and backend state for device controls.
The safe-listening picker indicates when the device has yet to report its
state. Unsupported controls expose a localized explanation. Paired-device
actions use the connection and playback flags reported for each device.

```sh
xvfb-run -a dbus-run-session -- build/kde/SonyHeadphonesClientKDE --backend /absolute/backend --smoke-test
xvfb-run -a dbus-run-session -- build/kde/SonyHeadphonesClientKDE --backend /absolute/backend --locale zh_CN --parity-test
```

Run both checks with `--locale en` and `--locale zh_CN`. Smoke exercises
compact and expanded navigation, icons, panel lifetime, shortcuts, theme
and silent refresh. Parity exercises the actual controls for all profiles
and verifies the backend reply to each request. It also checks confirmed and
canceled actions, open pickers, unknown safe listening, lost state and recovery.

## Plasma verification (2026-11-07)

On nested Plasma 6, both X11 and Wayland checks placed quick controls at the
expected corners for top and bottom panels. After plasmashell restarted,
the existing backend stayed alive and the tray reopened the same quick panel.
Physical mixed-DPI displays, a full login session and real BlueZ/headphones
remain on the [workstation checklist](../../docs/linux-acceptance-2026-11-07.md#bluetooth-and-outstanding-checks).

## Preferences and shortcuts

- The application-settings page supports live persistent language and
  system/light/dark, including migration of the old Qt preferences. Preview
  changes are intentionally session-only. `--locale` / `--theme` override the
  initial session without saving. Both device pages and dynamic choices/errors
  update when switching English/Chinese.
- Ctrl+Alt+P / N / M invoke play/pause, next track and quick controls within
  the application, preserving the previous Linux Qt shortcut scope.
- Native dialogs confirm power-off and unpair; cancellation sends no command.

Manual modes use installed official BreezeLight/BreezeDark schemes through
KColorSchemeManager; system mode restores KDE tracking. No palette is painted
by the adapter. KF5 5.89+ or KF6 ColorScheme is required for this integration.

## Installed packages and recovery

Ubuntu 24.04 and Debian 13 DEB recipes are documented in the
[shared native packaging guide](../../packaging/native/README.md). Installed
launchers find the shared backend and catalog without a source checkout or
`--backend`; the explicit override remains available. Menu entries pass `--live`,
while a command without that flag remains an isolated preview.

Commands remain disabled until a valid initial snapshot arrives. Startup has a
ten-second timeout; backend failure clears old state and exposes Retry startup
in full settings. Retry obtains fresh state without replaying commands or
reconnecting headphones. Normal exit drains stdin/EOF, allowing two seconds
before terminating the owned child.
