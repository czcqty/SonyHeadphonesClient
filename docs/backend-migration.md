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

- Actual desktop lifecycle and accessibility acceptance. Preference migration,
  bilingual choices/errors and baseline shortcuts are now implemented, but the
  automated checks below do not replace platform-specific desktop acceptance.
- macOS SDK build and actual AppKit runtime checks (no macOS host is available).
- Linux Qt 6.5+ backend build, BlueZ/hardware checks, KDE Frameworks 6, and real
  Plasma/GNOME/Xfce desktop validation. Installed WSL Qt is 6.4; the supported
  minimum is not lowered to make this environment pass.
- Real headphones, disconnect/reconnect and protocol fallback, clean-machine
  packaging, multi-monitor/DPI, accessibility and attached popups on each OS.

No Qt Quick source deletion is performed before these gates pass. The default
build keeps the transitional UI; `MDR_BUILD_QT_FRONTEND=OFF` is the validated
headless build path. The target backend-only main branch is not yet the default.

## Native application follow-up (2026-10-14)

Implemented on main and consumed by all five native branches:

- Shared Qt Core `ApplicationPreferences` uses the existing application's
  QSettings identity, including the macOS organization domain. Existing locale
  and themeMode take precedence; legacy darkMode migrates to light/dark; new
  users get system theme and system language. Preview neither reads nor writes
  user settings. CLI locale/theme are non-saving session overrides.
- The version-1 contract adds `preferences` snapshots and two explicit commands,
  available while disconnected. Device logic/commands and libmdr are unchanged.
- One shared bilingual catalog covers equalizer IDs, touch options, connection
  error codes, parameterized messages, protocol errors and boolean values. Device
  names, media metadata and opaque OS diagnostic details are preserved verbatim.
- Every native branch exposes an application-settings page. WinUI uses
  ElementTheme, AppKit NSAppearance, GNOME AdwStyleManager, KDE the official
  Breeze schemes via KColorSchemeManager, Xfce GTK's dark-variant preference.
  Restoring system mode delegates to each framework. GTK themes without a
  corresponding variant are not replaced with custom styling.
- Baseline shortcut scope is preserved: Ctrl+Alt+P / N / M globally on Windows
  in live mode; application-local on Linux and Control+Option on AppKit, as with
  the previous non-Windows Qt shortcuts. Windows registers a message-only native
  window, releases registrations on exit and reports conflicts with local fallback.
- Native tray menus include playback/next/disconnect/settings/quit. GNOME uses
  ordinary quick/full windows because stock GNOME has no tray. WinUI keeps full
  settings reachable if the shell tray disappears. Xfce defers focus dismissal
  while combos/menus own a grab, positions against the X11 tray/workarea, and
  falls back to ordinary windows without a tray. KDE delegates popup stacking
  and Escape to Qt; AppKit uses a transient NSPopover and native status menu.
- Power-off/unpair confirmations use each framework's dialogs. Cancel sends no
  command. Native control binding updates remain silent.

### Current environment verification

- Windows Debug with Qt Quick retained: **16/16 CTest suites pass**.
- Windows Release with `MDR_BUILD_QT_FRONTEND=OFF`: **8/8 suites pass**.
- Backend tests cover old-preference precedence, migration, reload/persistence,
  invalid values, disconnected preference updates, preview isolation and CLI
  overrides. Tests use isolated temporary settings, not the user's settings.
- Device regression still compares 38 commands across all 12 profiles (456
  comparisons). The native catalog checks 41 device/connection plus two preference
  commands; 138 enabled writes pass across four profiles.
- WinUI .NET 8 / Windows App SDK 1.8 builds. English and Chinese native smoke
  checks cover preference roundtrips, live relabeling, actual theme requests,
  dynamic strings, toggle/slider single writes, shortcut dispatch/registration,
  popup discovery and cancellation. The packaged backend is found via the default
  child path. Global key dispatch is tested via its native message handler; this
  does not claim manual keyboard or desktop-wide shortcut acceptance.
- KDE Qt5/KF5, GNOME GTK4/libadwaita and Xfce GTK3/libxfce4ui smoke checks pass in
  WSL/Xvfb: all pages, preference changes, both languages, native dark theme,
  shortcut dispatch, window fallback and dialog cancellation. They use the
  Windows simulation backend through pipes, not Linux Bluetooth.
- Refreshed Windows backend ZIP extracts and passes process/catalog tests with
  PATH restricted to Windows System32. Qt6Core.dll is the only bundled Qt module.
- macOS build-script shell syntax and package plist remain checkable, but **Swift
  source has not been compiled or run** because no macOS SDK/runtime is available.
- Environment diagnostics: NuGet NU1900 (offline vulnerability metadata), GTK
  StatusIcon deprecation/no-XEmbed-host warnings under Xvfb, missing desktop portal
  services under WSL. The sandbox Release IPO probe reports MSVC PDB access failure
  and uses the project's supported non-IPO fallback; build/tests/package succeed.

### Explicit remaining release gates

1. **macOS**: compile Swift/AppKit and Qt backend on a Mac; native menu-bar/popup,
   theme switching, preference-file migration, keyboard/VoiceOver, permission and
   packaging/signing/notarization acceptance. AppKit changes are source-only here.
2. **Linux**: Qt 6.5+ backend build (installed WSL Qt is 6.4.2), BlueZ, KF6 build,
   and real Plasma/GNOME/Xfce sessions. Verify compositor/Wayland popup focus,
   attached menus, tray disappearance/reappearance and system-theme changes.
   Xfce currently supplies an XEmbed tray on X11; **no Wayland status-notifier
   indicator is implemented**. Its no-tray ordinary-window fallback must be tested
   on the target Wayland session. This is a known platform limitation, not a
   successful Wayland tray check.
3. **Windows**: Windows 10 and clean-machine installation; real Explorer/taskbar
   placement, high contrast/reduced effects, actual global key presses/conflicts,
   tray restart, multi-monitor/DPI, Narrator and minimum-size visual checks.
4. **All real devices**: Bluetooth discovery, pairing, connection cancellation,
   reconnect/recovery, MDR fallback, device-reported capability choices and actual
   headphone acknowledgements. Simulation does not establish hardware correctness.
5. **All desktops**: manual Chinese/English, light/dark/system, minimum-size,
   accessibility and multi-display acceptance across every page. Automated native
   widgets and image previews do not replace these checks.

Qt Quick is still present and enabled by default. No Qt Quick source, platform
support, protocol mapping or backend Qt dependency was removed in this follow-up.
