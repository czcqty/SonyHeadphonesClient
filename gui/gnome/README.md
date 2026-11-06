# GNOME presentation

The GNOME functional migration is complete on the `gnome` branch. The
installed Ubuntu 24.04 and Debian 13 clients passed English and Chinese
acceptance across 12 preview profiles and all 45 commands. Physical headset
and workstation verification remains open. [Linux acceptance record](../../docs/linux-acceptance-2026-11-07.md)

Python 3 + PyGObject, GTK 4.10+ and libadwaita 1.4+. The actual controls are
`Adw.ApplicationWindow`, `HeaderBar`, `PreferencesPage`, `PreferencesGroup`,
`ActionRow`, `ComboRow`, `Gtk.Switch`, `Gtk.Scale` and `Gtk.SpinButton`.
The navigation uses a symbol and label for each page. Its libadwaita
`OverlaySplitView` collapses to a header menu on narrow windows.

```sh
# Debian/Ubuntu runtime dependencies
sudo apt install python3-gi gir1.2-gtk-4.0 gir1.2-adw-1
python3 gui/gnome/main.py --backend "$PWD/build/backend/SoundConnectBackend"
python3 gui/gnome/main.py --backend "$PWD/build/backend/SoundConnectBackend" --panel
```

Build the backend first as documented in [../native/README.md](../native/README.md).
`--locale en|zh_CN`, `--route`, `--profile` and explicit `--live` are supported.
Stock GNOME provides ordinary application windows. Open quick controls from
the settings header, Ctrl+Alt+M or `--panel`. Escape dismisses a picker. Opening
settings or repeating the shortcut keeps the same quick window available.
The panel's × closes it. If it was the only window, settings becomes reachable.
Repeated live launcher invocations bring forward the primary process's window.
Ctrl+Q quits the application and closes its backend.

Use the installed `soundconnect-gnome` command or run these preview checks from
the source tree with a Linux backend:

```sh
for locale in en zh_CN; do
  for check in --smoke-test --parity-test; do
    xvfb-run -a dbus-run-session -- /usr/bin/python3 gui/gnome/main.py \
      --backend /absolute/path/to/SoundConnectBackend --locale "$locale" "$check"
  done
done
```

The parity check drives the GTK controls against every profile advertised by
`app.describe`. It checks single writes and their reply snapshots, unavailable
controls, stale state during selection, real picker and confirmation widgets,
discovery, multipoint actions, preference changes and recovery after a missing
child. It also injects a disconnected multipoint report to exercise the row for
connecting a paired host. Bluetooth and multi-host audio need real devices.

## Local verification (2026-11-07)

The checks used the Linux backend with GTK 4.14/libadwaita 1.5 on Ubuntu and
GTK 4.18/libadwaita 1.7 on Debian.

- English and Simplified Chinese each passed all 12 preview profiles and all 45
  commands. The installed check verified 1,414 writes per language with
  `SOUNDCONNECT_TEST_X11_POINTER=1` on a dedicated Xvfb display. It clicked
  a switch and a slider, and verified one write when a drag ended.
- Both language smoke tests passed on Xvfb. The English smoke also passed on the
  WSLg Wayland display.
- XTest delivered Escape and Ctrl+Alt+M/P/N to the application windows.
  Reopening quick controls or opening settings kept the panel visible, × closed
  it, and playback and next each sent one request. Compact navigation opened
  at 450 × 550. Three separate live launcher invocations reused the same
  settings window, quick window and backend PID. Quit reaped the child.
- Debian 13 GNOME Shell/Mutter 48.7 ran as a nested Wayland compositor.
  English lifecycle and Chinese 12-profile control tests passed. Shell-level
  key delivery, panel focus/lifetime and moving settings between virtual
  1920 × 1080 outputs at scale factors 1 and 2 passed. Normal login, power,
  authentication and Bluetooth services were outside this isolated session.
- Physical GNOME Shell sessions, real Bluetooth/headphones and mixed-DPI
  multiple monitors remain in the [desktop acceptance gate](../../docs/backend-migration.md).

## Preferences and shortcuts

- The application-settings page supports live persistent language and
  system/light/dark, including migration of the old Qt preferences. Preview
  changes are intentionally session-only. `--locale` / `--theme` override the
  initial session without saving. Both device pages and dynamic choices/errors
  update when switching English/Chinese.
- Ctrl+Alt+P / N / M invoke play/pause, next track and quick controls within
  the application, preserving the previous Linux Qt shortcut scope.
- Native dialogs confirm power-off and unpair; cancellation sends no command.

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
