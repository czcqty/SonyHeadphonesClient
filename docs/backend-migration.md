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
- User testing reports WinUI functionality missing relative to Qt Quick and
  controls that cannot be adjusted or remain disabled. These reports are open
  acceptance blockers; a catalog entry or a successful preview smoke test is
  insufficient evidence to close them.

Branch ownership and build entries are listed in the [branch guide](branches.md).

## Documentation maintenance check (2026-10-15)

During repository documentation cleanup, the existing Windows build trees were
retested: Debug with Qt Quick passed **16/16 CTest suites**; backend-only Release
passed **8/8 suites**. TEMP/TMP pointed to an isolated workspace test directory.
No application source changed or fresh build was performed in this check.
Updated local Markdown links and bilingual catalog validation also passed.
The native toolkit smoke results below remain the recorded 2026-10-14 results;
this maintenance check does not add platform or real-device acceptance.

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
- Linux BlueZ/hardware checks and real Plasma/GNOME/Xfce desktop validation.
  The Ubuntu/Debian backend and KF6 build checks are covered by the native package
  follow-up below; the supported backend minimum remains Qt 6.5.
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
2. **Linux**: BlueZ and real Plasma/GNOME/Xfce sessions. Native Ubuntu/Debian
   backend and KF6 builds are now automated checks, not desktop acceptance.
   Verify compositor/Wayland popup focus,
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

## Native installer and recovery follow-up (2026-10-19)

**Migration is not complete.** The checks below cover automated installation,
transport and simulated controls only. User testing has not accepted the native
migration; observed failures must be reproduced and fixed before acceptance.
Hosted CI success, when available, does not close the real desktop/device gates.

Shared changes live on main and are merged into WinUI, KDE, GNOME and Xfce.
The backend still uses Qt Core and protocol v1. Qt Quick remains enabled by
default; no frontend or hardware support was removed.

- Native transports wait for a valid initial snapshot, time out startup after ten
  seconds, disable commands and clear stale state on failure, and offer explicit
  retry. Retry retires the old child first and does not replay queued commands or
  reconnect devices. Normal exit allows two seconds for EOF shutdown. Diagnostics
  are bounded and preserve UTF-8 across reads. GTK shares the Python transport;
  C# and KDE use their own native process APIs against the same contract.
- `MDR_DEPLOY_BACKEND_RUNTIME` defaults to ON. Native Debian packages use system
  Qt; Ubuntu privately bundles Qt Core 6.8.3 and matching ICU without modifying
  KDE/GTK library search paths. Installed adapters find backend/catalog resources
  without a source checkout or a required `--backend` parameter.
- WinUI uses a dedicated backend build, self-contained Release publish, and the
  generated application PRI resource map. Its NSIS installer is per-user, has
  separate identity from Qt, live-mode menu shortcuts, upgrade and file-list-based
  uninstall. User preferences and unrelated files are preserved. Tray recovery
  keeps settings reachable when the shell tray disappears/reappears.
- Ubuntu 24.04 uses KF5; Debian 13 uses Qt 6.8.2/KF6 6.13. Each emits backend and
  common DEBs plus KDE/GNOME/Xfce adapters with matching-version dependencies.
  Installed menu entries explicitly pass `--live`; CLI defaults remain isolated
  preview. No autostart service is added. Xfce creates XEmbed icons only on X11;
  Wayland uses ordinary windows and no status-notifier indicator is claimed.
- Manual native CI workflows build, test, package, install/reinstall/uninstall and
  retain artifacts/reports. Wrong-branch Linux adapter selections fail before SDK
  provisioning. Hosted runs are recorded below separately from the earlier local
  checks. See [build and package instructions](../packaging/native/README.md).

### Recorded automated evidence

- Windows/MSVC/Qt 6.11: backend-only Release **9/9 CTest suites pass**; Qt Quick
  Debug **17/17 suites pass**. Shared Python transport has **10 deterministic
  failure/recovery cases**, also passing on Linux; C# and KDE transport tests
  cover missing/failed children, timeout, incompatible/malformed output, bounded
  diagnostics, ordered writes, restart without replay, and child shutdown.
- Windows NSIS package installs and reinstalls, passes English/Chinese native
  widget and restart smoke tests using default child lookup with PATH restricted
  to System32 and an unavailable SDK root, then uninstalls. Both reports contain
  `ok: true, sentCommands: 6`. An unrelated installed-directory sentinel survives.
  This is an installation/runtime isolation check, not a fresh Windows OS image.
- Ubuntu 24.04 Qt 6.8.3 backend: **9/9 CTest suites pass**. All three installed
  adapters pass English/Chinese native-control and recovery checks, reinstall and
  uninstall with isolated preferences. Dependency inspection finds Qt Core as the
  backend's only Qt module, resolving from its private installed directory.
- Debian 13 Qt 6.8.2 backend: **9/9 CTest suites pass**; KF6 builds and its native
  transport test passes. All three installed adapters pass English/Chinese
  control/recovery smoke tests, reinstall and uninstall in the isolated Debian
  root filesystem. Dependency inspection finds Qt Core as the backend's only Qt
  module, resolving from the distro runtime. Native DEBs and manifests are generated.
- Xfce's fallback also passes the automated control/restart smoke on the available
  WSLg Wayland backend with GTK criticals treated as failures. This does not
  establish an actual Xfce Wayland desktop or tray implementation.

Real headphones/BlueZ, real desktop focus and tray behavior, accessibility,
multiple monitors/DPI, Windows 10, clean-machine OS checks and signing remain
release gates. Xvfb/WSLg and a restricted development-machine PATH do not replace
those checks. macOS remains outside this follow-up.

## Hosted CI follow-up (2026-10-19)

The five branches have been pushed. The following hosted runs exercise the
shared code and native package paths; they do not establish functional parity.

| Branch | Run | Result and scope |
| --- | --- | --- |
| main | [39248061259](https://github.com/czcqty/SonyHeadphonesClient/actions/runs/39248061259) | Passed all six Windows/Linux/macOS jobs, Qt Quick ON and OFF; JUnit and test logs retained |
| codex/winui | [39248167461](https://github.com/czcqty/SonyHeadphonesClient/actions/runs/39248167461) | Passed backend 9/9, C# transport, NSIS build, install/reinstall, English/Chinese native smoke and uninstall; installer and evidence retained |
| codex/kde | [39247776913](https://github.com/czcqty/SonyHeadphonesClient/actions/runs/39247776913) | Passed Ubuntu 24.04/KF5 and Debian 13/KF6, backend tests, native transport, DEB install/reinstall/control smoke/uninstall |
| codex/gnome | [39247782161](https://github.com/czcqty/SonyHeadphonesClient/actions/runs/39247782161) | Passed Ubuntu 24.04 and Debian 13 backend tests and DEB install/reinstall/control smoke/uninstall |
| codex/xfce | [39247786506](https://github.com/czcqty/SonyHeadphonesClient/actions/runs/39247786506) | Passed Ubuntu 24.04 and Debian 13 backend tests and DEB install/reinstall/control smoke/uninstall |

Initial failures and fixes:

- WinUI dispatch returned 404 because its workflow existed only on the adapter
  branch. The shared workflow now also exists on main, with a wrong-branch
  preflight. Backend-test failures cannot be masked by the following .NET command.
- All six initial Linux jobs passed backend tests, then failed source provenance
  lookup because the container shell did not inherit checkout's Git ownership
  exception. The workflow trusts its exact checkout path and records the Actions
  source SHA explicitly. All six corrected jobs passed.
- macOS Qt Quick crashed before entering a test under the offscreen platform.
  Its native Controls test now uses Cocoa without overriding the default style;
  the corrected hosted main run passed all 17 Qt Quick CTest suites.
- Windows NSIS rejected the mixed-separator recursive input pattern. The WinUI
  installer now uses Windows path separators for its input and output patterns;
  the corrected hosted installer/installation job passed.

Tested source revisions: main `f461d4b`, WinUI `50f405a`, KDE `8b2802b`, GNOME
`bd3da02`, Xfce `5e65ce1`. Subsequent native merges only add the main Cocoa test
configuration and report retention; this evidence update changes documentation
only. Linux packages and dependency/source manifests are retained in both distro
artifacts for each adapter. The WinUI artifact includes its installer, file
manifest, backend JUnit, native transport log and installed smoke reports.

## WH-1000XM5 mapping and write follow-up (2026-10-22)

The user's WH-1000XM5 (firmware 2.3.1) reports `TOUCH_PANEL_SETTING` in
general-setting slot 1 and `MULTIPOINT_SETTING` in slot 2. The former was
incorrectly treated as capture voice; touch controls exposed only earbud
left/right assignments, and multipoint availability was conflated with paired
device management. These are shared backend/catalog gaps affecting every
adapter and the transitional Qt Quick frontend.

The backend now resolves boolean settings by the reported identity and type,
independent of slot order, and exposes separate touch-panel and multipoint
toggles. Qt Quick navigation includes these capabilities; native adapters use
the shared catalog. WinUI hides earbud assignment controls on panel-only
headphones and omits the unsupported adaptive-noise switch, matching Qt Quick.
The existing noise mode control remains available.

Hardware testing also reproduced a multipoint command that was acknowledged
but not applied. The headset requested fixed-message confirmation 0x07. The
backend now replies only to a matching multipoint alert within ten seconds of
an explicit multipoint write, once per request. It does not acknowledge unrelated
alerts, including firmware updates. Protocol regression tests cover unsolicited,
unrelated, duplicate and expired alerts, as well as the emitted setting/reply
bytes. Capability tests cover slot swaps, unsupported types and absent support.

`backend/tests/verify_live_settings.py` is an explicit opt-in hardware test
(`--allow-writes`). It saves originals before writing, stops the backend,
reconnects with a fresh process to read actual device state, and restores the
originals. Its bounded reconnect retries never replay device commands. Reports
exclude Bluetooth addresses and media metadata. Preview state and a locally
updated value are not accepted as hardware evidence.

Recorded hardware checks: noise off/ambient/NC, touch-panel disable/enable,
DSEE disable/enable and multipoint enable/disable all passed fresh-process readback
and restoration in the combined hardware report. Independently, real WinUI
clicks disabled touch and enabled multipoint; a separate backend then read both
values from the headset and restored touch=true, multipoint=false.

Those WinUI clicks also exposed a malformed JSON response during multipoint
switching, despite all hosted checks passing. Preview stress did not reproduce
the intermittent failure. The host now isolates output from the stdin thread's
implicit cout flushes, writes whole JSON lines through stdio, and routes Linux
DBus errors exclusively to stderr. WinUI explicitly uses UTF-8 for all pipes.
Regression checks cover fragmented input with output backpressure, fragmented
UTF-8/quoted content, and an unavailable Linux DBus. Hardware verification now
fails on malformed JSON instead of silently skipping the offending line.
The exact cause of the observed intermittent frame remains unproven; a further
live UI retest is required before treating this hardening as sufficient.

This does not establish complete migration acceptance. Actual two-host audio
switching, the remaining device features/models, Linux Bluetooth and desktop
behavior, macOS, Windows 10, multi-monitor DPI and accessibility remain open.
Qt Quick remains available and enabled by default.
