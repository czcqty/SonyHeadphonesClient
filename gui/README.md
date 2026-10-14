# Qt Quick desktop client

This directory contains the project's only desktop GUI: a compact tray-first Qt
Quick/QML application backed by C++ ViewModels and `libmdr`.

See the root README for tray lifecycle, supported desktop prerequisites, and
compact-panel navigation. `PanelController` owns window presentation; protocol
and persistent settings remain in the existing session and ViewModels.

## Targets

- `SonyHeadphonesClient`: application executable
- `SonyHeadphonesClient_viewmodel_tests`: profile, route, capability-gating,
  and desired-state mapping tests
- `SonyHeadphonesClient_backend_tests`: coroutine ownership and MDR v1
  transport regression tests
- `SonyHeadphonesClient_qml_tests`: QML/QuickTest contract tests
- `SonyHeadphonesClient_visual_diff`: deterministic pixel comparison utility
- `run_gui_tests`: runs all registered `gui-*` CTest suites, including the
  Python metadata validators when Python is found
- `capture_qml_baselines`: captures the Windows route/state/locale/theme matrix

## Structure

- `quick/PlatformAppearance.*` owns desktop appearance, system notifications,
  three-state theme preferences through Qt system APIs.
- `quick/AppController.*` owns route, locale preference, preview state, profile selection,
  connection state, and the domain ViewModels.
- `quick/MdrSession.*` owns asynchronous device scan/connect, MDR v2-to-v1
  fallback, polling, synchronization, and dirty-property commits.
- `quick/ViewModels.*` exposes typed `Q_PROPERTY` values and capability-gated
  write operations to QML.
- `quick/DeviceProfileRepository.*` provides deterministic device profiles for
  UI development without hardware.
- `DeviceCapabilities.*` maps protocol support tables to features available to
  both the session and ViewModels.
- `qml/` contains routes, reusable controls, the translation singleton, and
  the measured design-token singleton.
- `translations/` contains the committed Simplified Chinese Qt Linguist
  catalog; English strings live in `qml/I18n.qml`.
- `assets/icons/` contains the canonical SVG product icon and its generated
  PNG, ICO, and ICNS variants.
- `assets/devices/` contains device artwork.
- `tests/` contains ViewModel, backend, QuickTest, capture, and visual-diff
  tooling.

The data flow is:

```text
platform Bluetooth
  → libmdr
  → MdrSession
  → AppController / typed ViewModels
  → QML
```

User actions take the reverse path. A ViewModel first checks the relevant
capability, updates `MDRProperty::desired`, and lets `MdrSession` submit dirty
values through the existing polling state machine. Unsupported operations must
not modify desired state or emit a protocol command.

The Devices page reads the backend paired-device list and calls the existing
capability-gated pairing, source-switch, connect/disconnect, and unpair bindings.
Preview profiles are deterministic fixtures, not evidence of hardware validation.

## Preview interface

```text
SonyHeadphonesClient
  --preview-desktop <windows|macos|kde|gnome|xfce|linux>
  --preview-profile <profile>
  --preview-route <route>
  --preview-state <state>
  --preview-theme <system|light|dark>
  --preview-size <WIDTHxHEIGHT>
  --locale <en|zh_CN>
  [--capture <output.png>]
```

Profiles:

```text
WF-1000XM5
WF-1000XM6
WF-C510
WF-L910
WF-LS900N
WH-1000XM5
WH-1000XM6
WH-CH720N
generic-max
generic-tws
generic-headband
generic-v1
```

Preview profiles prove UI behavior and capability mapping only. They are not
evidence that a physical model has passed read, write, reconnect, or
persistence testing.

Supplying any `--preview-*` option enables preview mode. `--capture` only saves
the current window; combine it with `--preview-profile` for a hardware-free,
deterministic capture.

## Windows development runtime

The CMake target copies Qt's Windows style, its Fusion/Basic fallbacks, native
style plugin and Windows platform plugin beside the executable. `AppButton`,
`AppSwitch`, `AppSlider`, `AppComboBox`, `AppScrollBar` and `AppBusyIndicator` are
thin API wrappers around official Qt Quick Controls. They do not override
backgrounds, handles, indicators, delegates or content items. Radio buttons,
item delegates and toolbars likewise use the official controls.

The application leaves style selection to Qt (`Windows`, `macOS`, or `Fusion`
on Linux) and honors `QT_QUICK_CONTROLS_STYLE`. No independent Adwaita/Breeze/Xfce
skin is maintained. Manual color-scheme overrides require Qt 6.8+ and platform
support; earlier supported Qt releases retain system appearance.

This post-build deployment is for development. Release packaging should use a
Release build, include the MSVC redistributable, and be tested on a machine
without a Qt or Visual Studio installation.

## Localization and icon maintenance

UI-owned text must go through `I18n.qml`; user/device names, playback metadata,
MAC addresses, and raw protocol details remain untranslated. Run these checks
after adding or changing text or product artwork:

```bash
python packaging/validate_translations.py
python packaging/generate_icons.py --validate-only
```

To regenerate platform icons from `assets/icons/app-icon.svg`, install Pillow
and CairoSVG and run `python packaging/generate_icons.py` from the repository
root.

English source strings live in `qml/I18n.qml`. Simplified Chinese translations
live in `translations/SonyHeadphonesClient_zh_CN.ts`, are compiled to a Qt
catalog, and are applied at runtime by `AppController`.

## Visual acceptance

`CapturePreviewBaselines.ps1` produces deterministic captures at 360 × 600,
412 × 780, and 720 × 900 logical pixels for the four primary profiles.
`SonyHeadphonesClient_visual_diff` compares equal-sized images,
supports repeatable `--ignore x,y,w,h` masks, and writes a red difference
overlay.

Static APK measurements are documented under `docs/reference/`. Visual parity
remains `reference-blocked` until the implementation and official application
are captured at the same route, locale, device profile, viewport, and state.

The pre-panel local Debug baseline contained original six CTest entries: ViewModel,
backend, QML QuickTest, localization-catalog, application-icon, and package-
metadata checks. This does not replace real-device or cross-platform release
verification.

## Panel verification

`gui-panel-lifecycle` covers hidden tray startup, visible fallback, transient popup
focus, dismissal on outside focus, and work-area clamping. QML tests instantiate
the internal pages at 360 × 480. These offscreen tests do not emulate an actual
Windows/macOS shell, GNOME extension, KDE/Xfce panel, or physical headset.

## Desktop appearance validation

`gui-platform-appearance` covers preference migration, preview isolation,
desktop metadata and live Qt color-scheme notifications. QML tests instantiate
all pages using the host's official style. `--preview-desktop` is retained for
CLI compatibility, but does not emulate a foreign system's style. Capture on
each target OS; Windows screenshots cannot validate macOS or Linux appearance.

UI policy: prefer official controls and installed style providers. Add custom
drawing only when an application-specific visualization needs it, not to replace
a standard button, slider, switch or menu.
