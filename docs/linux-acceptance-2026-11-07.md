# Linux native migration acceptance, 2026-11-07

Functional migration is complete on the `kde`, `gnome`, and `xfce` branches.
All three adapters passed native control and installed-package checks using
the Linux `SoundConnectBackend`. The reference is the [Windows functional acceptance
record](windows-acceptance-2026-10-31.md). [Real-headset and workstation
checks](#bluetooth-and-outstanding-checks) remain release gates for all three.

## Tested candidates

| Adapter | Candidate source | Ubuntu 24.04 amd64 | Debian 13 amd64 |
| --- | --- | --- | --- |
| KDE | `b05512f9e764` | Qt 5 / KDE Frameworks 5 | Qt 6 / KDE Frameworks 6 |
| GNOME | `28c3af96a87d` | GTK 4.14 / libadwaita 1.5 | GTK 4.18 / libadwaita 1.7 |
| Xfce | `79857ee08bfe` | GTK 3 / libxfce4ui | GTK 3 / libxfce4ui |

The source was recovered from the Windows archive. Every tracked file in its
main, WinUI and three Linux working trees matched the archived branch revisions
after accounting for Windows line endings. New development lives on the
shared main and three existing adapter branches.

Both Linux backend builds passed all eleven CTest suites. Ubuntu used the private
Qt 6.8.3 SDK and Debian used system Qt Core. The KDE Frameworks 5 and 6 builds each passed both
private-process transport and tray host CTest suites. The native tests traversed all ten settings pages,
quick controls, capability-dependent actions, single edits, confirmations,
language and theme changes, backend loss and retry. Per adapter, each
language covered all 45 distinct commands across the 12 profiles in aggregate. The installed GNOME
runs verified 1,414 writes per language including real XTest clicks and drags.
The KDE runs verified 1,421 requests per language.

### Installed packages

For each of the six adapter/distribution combinations, the installed entry
point ran the smoke and full parity suites in English and Chinese, with its
default child and catalog paths, outside the source tree. Tests used isolated
configuration and data directories. In disposable root filesystems the checks
upgraded `2.0.4-1` to `2.0.4-2` for the same distribution, compared installed
files with the archives after the upgrade and reinstall, then removed the
packages and verified that the seeded preferences survived. The resulting
Linux packages contain an adapter, a common package and the native backend.

Two differences only surfaced in the Debian matrix. A non-reporting headset
can still accept a Safe Listening change. Libadwaita's choice row needed an
explicit unknown entry to show this state without selecting Off. The common
package now installs the application's existing PNG sizes in hicolor alongside
the SVG, and the Xfce package declares its icon theme and SVG loader.

## Desktop observations

- **KDE Plasma 6 with KWin on X11.** The StatusNotifierItem registered with
  the Plasma watcher. On a 1440×900 virtual display with a 56-pixel panel, the
  430×480 quick window appeared at (1002,356) with a bottom panel and (1002,64)
  with a top panel. The tray activation and application shortcut reused the same
  window with the pointer elsewhere. Changing focus, opening settings and
  Escape preserved it. Clicking × closed it. KDE retains its own Qt, KDE
  Frameworks, Breeze and KWayland dependencies. Nested KWin on Wayland
  reported the same quick-window position and size for each panel location and (1002,412)
  with no tray or reserved work area. Repositioning the already visible surface
  required committing the Plasma surface after setting its position. When
  plasmashell exited, kded's watcher still advertised a tray. Checking the
  actual host's D-Bus owner restored the hidden settings window. Restarting
  plasmashell restored tray activation with the same application and backend
  PIDs. Explicit Quit, and closing settings after tray loss, both ended the
  app and its child even with quick controls open.
- **GNOME Shell 48.7 with Mutter, nested Wayland.** Stock GNOME uses ordinary
  quick and settings windows. Keyboard events delivered through the compositor
  exercised Ctrl+Alt+M/P/N and Escape. Reopening quick controls reused the
  same surface, bringing up settings kept it visible, and × closed it. Two
  virtual 1920×1080 outputs exercised 1× and 2× scaling, including moving the
  settings window to the second output. Separate live launcher activations on
  X11 also retained the same windows and child PID. XTest clicks and drags
  exercised the installed GTK volume control. Changing the binding to use the
  range's release gesture fixed missing commits on GTK 4.18. A drag sends
  one write on release and the displayed value comes back from the backend.
- **Xfce with xfwm4 and xfce4-panel on X11.** The XEmbed tray on a 1280×900
  virtual display opened a 440×480 quick window at (840,37) or (840,383) for
  top-right and bottom-right panel positions. Shortcut and repeat tray
  activations, Escape, switching focus and restarting the panel were exercised.
  The existing child survived the panel restart. The full settings window uses
  the normal window role so xfwm4 can raise quick controls over it. Stock
  GtkStatusIcon reproduced a GDK diagnostic during tray-host restart and
  successfully embedded again. WSLg also exercised Xfce's ordinary-window
  fallback on Wayland.

## Bluetooth and outstanding checks

Linux discovery now reads the BlueZ ObjectManager snapshot and distinguishes
an unavailable service, no powered adapter, and an empty successful scan. A
private D-Bus fixture exercised those states, long UTF-8 device names, mixed
powered/off adapters, paired and connected devices, cached unpaired devices,
a rejected scan and recovery in the same live process. A separate cancellation
test delays handle acquisition on the connection worker and checks that the
session closes the handle before publishing a completed cancellation. The GTK
child lifecycle suite also covers a timeout and retry during process launch.
Two read-only refreshes on the WSL system bus returned
`bluetooth-backend-unavailable`. That system exposed no HCI controller.

Physical validation still requires a BlueZ-enabled machine and supported
headphones. Verify discovery, cancellation, protocol fallback, radio off/on,
disconnect/reconnect, device setting readback after restarting the backend,
and multipoint switching between real audio hosts. Also check physical
multi-display placement/scale, login/logout and panel or compositor restart on
the supported desktops, including Xfce's native Wayland session. The
transitional Qt Quick frontend stays enabled while these gates are open.

The [native package guide](../packaging/native/README.md) describes the build,
installed tests and previous-version upgrade check.

## Local evidence

In the WSL delivery, `packages/<distribution>/<adapter>/` contains the exact
DEBs tested, their source manifest and checksums. `acceptance/<distribution>/`
contains CTest XML and per-adapter installed upgrade, parity and uninstall logs.
`acceptance/desktop/` contains the session observations and native screenshots.
