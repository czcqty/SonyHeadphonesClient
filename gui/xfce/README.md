# Xfce native client

The Xfce branch provides a GTK 3.24 and libxfce4ui 4.18+ presentation for
`SoundConnectBackend`. The full window uses the Xfce settings dialog, stock
GTK controls and the desktop's GTK theme. Each notebook tab has its own page
symbol and translated label. Narrow windows keep the symbols and tooltips.

## Run

Install the matching common, backend and Xfce packages described in the
[Linux packaging guide](../../packaging/native/README.md). The installed
desktop entry starts live mode. Running `soundconnect-xfce` from the shell
starts an isolated preview. For a source checkout, build the shared backend
and supply its path explicitly.

```sh
sudo apt install python3-gi gir1.2-gtk-3.0 gir1.2-libxfce4ui-2.0
/usr/bin/python3 gui/xfce/main.py --backend "$PWD/build/backend/SoundConnectBackend"
```

`--panel` opens quick controls directly. `--route`, `--profile`,
`--locale en|zh_CN` and `--theme system|light|dark` select a starting
page, preview profile, language and appearance. Live preferences use the
shared backend's existing settings. Preview changes apply to the current
process only.

## Desktop and controls

On X11 the XEmbed notification area supplies the icon's position. The
quick window opens in the nearest corner of that monitor's work area.
Activating the tray again or pressing the application-local Ctrl+Alt+M
brings the existing panel forward. Focus changes, Escape and opening full
settings leave the panel open. Its visible × closes it. Application Quit
ends both windows and the private backend. If the tray goes away while
settings are hidden, settings reopen. Without XEmbed, including on Wayland,
ordinary windows remain reachable and placement belongs to the compositor.

Quick controls provide playback, volume, noise mode and ambient adjustments.
Full settings also support direct numeric entry, device-reported choices and
per-device actions. Confirming power off or unpair submits one request.
Canceling submits none. Buttons reflect connection state, capabilities and
pending responses. A failed backend clears device state and exposes Retry in
settings. Ctrl+Alt+P and Ctrl+Alt+N control playback within the app.

## Validation

From an installed environment, run each test with a Linux backend under
X11. For a source checkout, use the Python entry above with the same flags.

```sh
for locale in en zh_CN; do
  for suite in smoke parity; do
    G_DEBUG=fatal-criticals xvfb-run -a dbus-run-session -- \
      soundconnect-xfce --locale "$locale" "--$suite-test" || exit 1
  done
done
```

Parity exercises GTK controls against every profile and every declared
operation of the preview backend, including unknown safe-listening state,
numeric edits, unavailable features, confirmation, device rows, recovery
and quick-panel lifetime. Per-device switching also uses reported-state
fixtures. Xvfb provides a virtual X11 display. Physical BlueZ/headset
behavior, compositor placement, desktop tray recovery, mixed-DPI layouts
and Wayland fallback need the target environments listed in the
[migration checklist](../../docs/backend-migration.md).
