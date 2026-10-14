# Xfce native prototype

Python 3 + PyGObject, GTK 3.24 and libxfce4ui 4.18+. Uses the actual Xfce
`TitledDialog`, GTK notebook, check buttons, scales, combo boxes and native menu.
Appearance/fonts come from Xfce's GTK settings; no custom CSS or control painting.

```sh
sudo apt install python3-gi gir1.2-gtk-3.0 gir1.2-libxfce4ui-2.0
python3 gui/xfce/main.py --backend "$PWD/build/backend/SoundConnectBackend"
```

Build the host using [../native/README.md](../native/README.md). Supports
`--panel`, `--route`, `--locale en|zh_CN`, `--profile` and explicit `--live`.
The full settings dialog and quick-control window have independent layouts.
GTK's XEmbed status icon supports Xfce's X11 notification area and a native context
menu. If no tray host embeds the icon, closing the window exits normally. Escape
closes the quick window; the settings link restores the full dialog. The quick
window remains visible on focus loss in this prototype, so combo-box interaction
cannot accidentally dismiss it. Wayland tray/positioning remains future work.

```sh
xvfb-run -a python3 gui/xfce/main.py --backend /absolute/backend --smoke-test
```

The smoke check verifies page creation, zero startup writes, state refresh, and
one command for a toggle/slider commit. Xfce panel integration, actual desktop
scaling, accessibility and live hardware acceptance remain separate checks.
See the common README for remaining feature-parity and packaging work.

## Local verification (2026-10-14)

Ubuntu 24.04 WSL: GTK 3.24.41 and libxfce4ui 4.18.4 start successfully. The Xvfb
test passes with GTK criticals treated as fatal, covering nine pages, native
dialog actions, zero startup writes and single toggle/slider commits. GTK's
legacy StatusIcon reports its expected deprecation warnings. The shared C++
Windows host ran in simulation via WSL pipes; Linux Bluetooth, an actual Xfce
panel and Wayland tray behavior were not validated.

## Shared backend migration

The adapter now consumes the main branch's `SoundConnectBackend` and
`clients/common/catalog.json`. The former branch-local backend source and
catalog copies have been removed. Advanced device operations are present in
the shared catalog; application/desktop parity still follows the migration gate.
Native widget smoke test passed again against the new standalone backend.
