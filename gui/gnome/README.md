# GNOME native prototype

Python 3 + PyGObject, GTK 4.10+ and libadwaita 1.4+. The actual controls are
`Adw.ApplicationWindow`, `HeaderBar`, `PreferencesPage`, `PreferencesGroup`,
`ActionRow`, `ComboRow`, `Gtk.Switch`, `Gtk.Scale` and `StackSidebar`. No custom
CSS or painted replacements. System appearance comes from libadwaita.

```sh
# Debian/Ubuntu runtime dependencies
sudo apt install python3-gi gir1.2-gtk-4.0 gir1.2-adw-1
python3 gui/gnome/main.py --backend "$PWD/build/gui/SonyHeadphonesClient_native_backend"
python3 gui/gnome/main.py --backend "$PWD/build/gui/SonyHeadphonesClient_native_backend" --panel
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
