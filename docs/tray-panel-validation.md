# Tray panel implementation and validation

Implemented against checkout `1095bcd`, without changing `libmdr`, the session
state machine, ViewModel command mappings, or persistent setting keys.

## Behavior

- With a tray available at startup, launch hidden. Primary activation opens the
  compact panel; external focus dismisses it. Owned transient windows and the
  tray context menu do not prematurely dismiss the panel.
- Without a tray, show the same compact UI as a normal window and keep it visible
  on focus loss. The footer always exposes Quit. No autostart is added.
- Preserve existing playback and panel shortcuts. Windows retains global hotkeys;
  other platforms retain application shortcuts.
- Home exposes battery/connection status, playback, volume and noise controls.
  Equalizer, sound, headphone behavior, touch assignments, voice guidance and
  device management remain internal pages. The old `settings` preview route is
  an alias for Sound, not an all-settings window.
- Desktop-specific Qt styling covers Windows, macOS, KDE, GNOME and Xfce.
  It uses Qt-rendered controls; it does not claim to use system-native controls.
- Existing settings-page bindings were compared before/after: none removed.

## Executed in the prepared Linux cloud environment

Existing GCC 14.2 / Qt 6.8.2 / CMake environment; no dependency installation.

```sh
source /workspace/cloud-setup/activate.sh
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
git diff --check
```

Final result: **7/7 CTest suites passed**: ViewModel, backend, panel lifecycle,
QML, translation catalog, application icons, package metadata.

Panel lifecycle tests exercise hidden startup, fallback reachability, owned
transient focus, outside-focus dismissal, and negative-coordinate/multi-screen
work-area clamping. QML tests instantiate all nine public routes at 360 × 480.

Twenty offscreen captures are in `build/panel-review/`, with command outcomes in
`captures.log`: nine routes in Chinese/light/412 × 600 and English/dark/360 × 480,
plus an Xfce appearance preview and an actual non-preview no-tray startup.
Every PNG's signature and dimensions were checked; representative home,
connection, equalizer, sound, behavior, noise and device pages were inspected.
Preview captures produced no QML warnings. The no-tray capture and lifecycle
tests emit the expected offscreen-plugin warning that `raise()` is unsupported.

Examples:

- `build/panel-review/home-zh_CN-light.png`
- `build/panel-review/equalizer-en-dark.png`
- `build/panel-review/no-tray-startup.png`

## Not verified here

- Windows/macOS compilation, packaging, actual tray clicks, activation and focus.
- Actual KDE/GNOME/Xfce panels, compositor positioning, taskbar edges, DPI changes
  and Wayland activation/placement. Appearance environment variables are not a
  substitute for desktop-session tests.
- Physical Bluetooth discovery, headset connection/reconnection, settings writes,
  hardware capability correctness, or device persistence.

GNOME requires an enabled compatible tray extension or equivalent distribution
support. Xfce requires its panel tray component; KDE requires a working tray host.
No extension or desktop configuration is installed automatically. Tray availability
is selected at startup; a later desktop-host change is not a tested transition.

No new product decision is required for the implemented scope. Changes are local,
uncommitted and unpublished; no PR was created.
