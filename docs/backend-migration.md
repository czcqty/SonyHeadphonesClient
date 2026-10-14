# Shared backend migration

The target is a backend-only main branch, retaining Qt Core for now, with
WinUI, AppKit, KDE, GNOME and Xfce presentations on their existing branches.
Native controls, navigation, window/tray lifetime, accessibility, shortcuts
and appearance belong to presentation adapters. Bluetooth, capabilities,
validation, connection recovery and device operations belong to one backend.

## Removal gate

The existing Qt Quick frontend must remain available until every row below
has been implemented and validated in its native replacement. This is a
functional migration, not just removal of the QML build dependency.

| Area | Required coverage |
| --- | --- |
| Connection | discovery, connection progress/errors, cancellation, disconnect, protocol fallback, recovery |
| Playback | metadata, codec/firmware, all battery and charging states, volume, previous/play/next, power off, device alerts |
| Noise | ANC/off/ambient, ambient level, focus voice, adaptive enable and sensitivity |
| Equalizer | device presets, bands, Clear Bass, unsupported controls |
| Sound/behavior | DSEE, quality, speak-to-chat enable/sensitivity/timeout, wear pause, auto power off choices and wear support, head gestures, listening/cinema/room, adaptive volume, safe listening, capture voice |
| Touch/voice | left/right reported assignments, NC button assignment, voice enable/volume |
| Device management | pairing, reported device list, source switch, connect/disconnect/unpair |
| Application | English/Chinese including dynamic choices/errors, existing preference migration and persistence, system/light/dark, all navigation |
| Desktop | distinct quick panel/full settings, native tray menu, focus/popups/Escape, shortcuts, no-tray fallback, multi-monitor/DPI, startup/shutdown |
| Distribution | per-platform build, packaging, child lookup/lifetime, missing backend/error handling |

Backend command coverage and native widget coverage are separate acceptance
criteria. The backend contract test must cover all business methods for all
device profiles; it does not establish native accessibility or hardware
behavior. Linux windows under Xvfb and a Windows simulation host do not
establish BlueZ or real desktop integration. macOS needs its own SDK/runtime.

## Current status

- Device logic is extracted to `backend/`; protocol command mappings and
  session polling/recovery remain the existing implementation.
- The Qt Quick application is a transitional consumer of that same library.
- `MDR_BUILD_QT_FRONTEND=OFF` builds the standalone Qt Core backend and tests.
- Native presentation parity and real-device desktop checks are still release
  gates. Do not delete the transitional frontend or claim parity from IPC tests.

## Verified on 2026-10-14

- Windows/MSVC/Qt 6.11 Debug: 14/14 CTest suites pass with the transitional Qt
  frontend, including QML, ViewModels, tray lifecycle, appearance, localization,
  backend protocol and original libmdr regressions. Sandbox test runs need
  TEMP/TMP set to a writable workspace test directory for QSettings tests.
- Backend-only configuration: 6/6 suites pass without loading Qt Gui, Quick or
  Widgets. The staged executable passes the process contract test. DLL inspection
  confirms Qt Core is its only Qt module.
- Windows Release CPack ZIP builds successfully (18,250,564 bytes). After
  extraction, process, legacy-client and catalog tests pass with PATH limited
  to Windows System32. The archive includes the MSVC runtime and only Qt Core,
  with no Qt Quick/Gui/Widgets DLLs. This is not a clean-machine hardware test.
- 38 device operations are compared with direct business calls over all 12
  profiles (456 comparisons), including rejection behavior and one changed
  signal for each accepted operation. The shared catalog covers these plus all
  three connection operations; 130 enabled bindings pass on four profiles.
- Process tests cover ordered bursts, strict argument types, malformed/oversized
  frames, old client envelopes, EOF shutdown and preview/live option isolation.
- WinUI builds and its native widget smoke test traverses all device pages,
  confirms no startup/synchronization writes, and emits exactly two commands
  for one toggle and one slider interaction. NuGet's offline vulnerability
  metadata check emits NU1900; compilation succeeds.
- KDE (Qt5/KF5), GNOME (GTK4/libadwaita) and Xfce (GTK3/libxfce4ui) native widget
  smoke tests pass under WSL/Xvfb, using the Windows simulation host through
  private pipes. KDE's own Qt/KDE dependencies are preserved.

## Still required before Qt Quick deletion

- Native application preferences and migration of existing settings, complete
  dynamic option/error localization, shortcut parity and desktop lifecycle
  acceptance. Catalog coverage alone is not proof that these are complete.
- macOS SDK build and actual AppKit runtime checks (no macOS host is available).
- Linux Qt 6.5+ backend build, BlueZ/hardware checks, KDE Frameworks 6, and real
  Plasma/GNOME/Xfce desktop validation. Installed WSL Qt is 6.4; the supported
  minimum is not lowered to make this environment pass.
- Real headphones, disconnect/reconnect and protocol fallback, clean-machine
  packaging, multi-monitor/DPI, accessibility and attached popups on each OS.

No Qt Quick source deletion is performed before these gates pass. The default
build keeps the transitional UI; `MDR_BUILD_QT_FRONTEND=OFF` is the validated
headless build path. The target backend-only main branch is not yet the default.
