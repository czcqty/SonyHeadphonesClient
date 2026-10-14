# GNOME native prototype

Python 3 + PyGObject, GTK 4.10+ and libadwaita 1.4+. The actual controls are
`Adw.ApplicationWindow`, `HeaderBar`, `PreferencesPage`, `PreferencesGroup`,
`ActionRow`, `ComboRow`, `Gtk.Switch`, `Gtk.Scale` and `StackSidebar`. No custom
CSS or painted replacements. System appearance comes from libadwaita.

```sh
# Debian/Ubuntu runtime dependencies
sudo apt install python3-gi gir1.2-gtk-4.0 gir1.2-adw-1
python3 gui/gnome/main.py --backend "$PWD/build/backend/SoundConnectBackend"
python3 gui/gnome/main.py --backend "$PWD/build/backend/SoundConnectBackend" --panel
```

Build the backend first as documented in [../native/README.md](../native/README.md).
`--locale en|zh_CN`, `--route`, `--profile` and explicit `--live` are supported.
GNOME does not provide a default app tray: quick controls use their own small
native window, reachable from the settings header or `--panel`. No Shell extension
is required. Escape closes quick controls, and All settings opens the full window.
Closing the main window quits the prototype and its backend. Settings are session-only.

Optional isolated smoke test (never combine with `--live`):

```sh
xvfb-run -a python3 gui/gnome/main.py --backend /absolute/backend --smoke-test
```

See the common README for feature-parity limitations. GNOME Shell/Wayland,
real headset, accessibility, multiple displays and packaging remain acceptance
work; Xvfb tests are not a substitute for those checks.

## Local verification (2026-10-14)

Ubuntu 24.04 WSL: GTK 4.14.5/libadwaita 1.5 native UI starts and passes the Xvfb
smoke test for all pages, zero startup writes, silent refresh and one write per
toggle/slider commit. The host was the real shared Windows C++ executable over
WSL interoperability pipes in isolated simulation. This does not validate the
Linux Bluetooth backend or GNOME Shell/Wayland integration.

## Shared backend migration

The adapter now consumes the main branch's `SoundConnectBackend` and
`clients/common/catalog.json`. The former branch-local backend source and
catalog copies have been removed. Advanced device operations are present in
the shared catalog; application/desktop parity still follows the migration gate.
Native widget smoke test passed again against the new standalone backend.
